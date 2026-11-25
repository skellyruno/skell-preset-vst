#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

// In-process VST3 hosting proof of integration, Windows only.
// All scanning, creation, state IO, UI and destruction run off the audio thread.
// Published instances are retired only on the message thread, after every
// audio reader has left and the native editor has been destroyed. A bounded
// backlog protects against plugins whose editor or callback never releases.
namespace skellpreset::host {
// Normalize common product-name spellings before loading any third-party code.
// Blocking recursive hosting avoids accidentally nesting this same VST3.
inline bool isSkellPresetProductName(juce::String name) {
    return name.removeCharacters(" -_\t").containsIgnoreCase("skellpreset");
}

// A read-only transport snapshot for third-party effects. Only the audio thread
// updates/reads this object while the nested plugin processes its block.
// Never keep the DAW's ephemeral AudioPlayHead pointer beyond its callback.
class HostedTransport final : public juce::AudioPlayHead {
public:
    void update(juce::Optional<PositionInfo> hostPosition,int chunkOffset,double sampleRate) {
        position=std::move(hostPosition);
        if(!position || chunkOffset<=0 || !std::isfinite(sampleRate) || sampleRate<=0.)return;
        const double elapsed=(double)chunkOffset/sampleRate;
        if(const auto time=position->getTimeInSamples())
            position->setTimeInSamples(*time+chunkOffset);
        if(const auto time=position->getTimeInSeconds())
            position->setTimeInSeconds(*time+elapsed);
        if(const auto bpm=position->getBpm())
            if(std::isfinite(*bpm) && *bpm>0.)
                if(const auto ppq=position->getPpqPosition())
                    position->setPpqPosition(*ppq+elapsed*(*bpm)/60.);
    }
    juce::Optional<PositionInfo> getPosition() const override {return position;}
private:
    juce::Optional<PositionInfo> position;
};

class HostedVST3 final {
    struct Entry {
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        std::atomic<int> latency{0};
        bool prepared=false; // non-realtime lifecycle only
        void release() {
            if(prepared){plugin->releaseResources();prepared=false;}
        }
        ~Entry(){release();}
    };
public:
    static constexpr int slotCount=16;
    static constexpr int maxTotalLatency=65536;
    // One lock-free reader lease per outer audio callback. Sequentially
    // consistent publication, reader entry and retirement checks prevent a
    // reader from observing an old entry after it has been reclaimed.
    class AudioReadScope final {
    public:
        explicit AudioReadScope(HostedVST3& owner) noexcept:host(owner) {
            host.readers.fetch_add(1,std::memory_order_seq_cst);
            for(int i=0;i<slotCount;++i) {
                entries[(size_t)i]=host.active[(size_t)i].load(std::memory_order_seq_cst);
                if(auto* entry=entries[(size_t)i])latency[(size_t)i]=entry->latency.load(std::memory_order_acquire);
            }
        }
        ~AudioReadScope(){host.readers.fetch_sub(1,std::memory_order_seq_cst);}
        AudioReadScope(const AudioReadScope&)=delete;
        AudioReadScope& operator=(const AudioReadScope&)=delete;
        juce::AudioPluginInstance* get(int slot) const noexcept {
            auto* entry=valid(slot)?entries[(size_t)slot]:nullptr;
            return entry?entry->plugin.get():nullptr;
        }
        int getLatency(int slot) const noexcept {return valid(slot)?latency[(size_t)slot]:0;}
    private:
        HostedVST3& host;
        std::array<Entry*,slotCount> entries{};
        std::array<int,slotCount> latency{};
    };
    HostedVST3() {
       #if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
        // JUCE 8 removed AudioPluginFormatManager::addDefaultFormats().
        // Register only VST3 to avoid accidentally loading unsupported formats.
        formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
       #endif
    }

    juce::Result load(int slot,const juce::File& path,double sr,int block) {
        if(!valid(slot))return juce::Result::fail("Invalid rack slot");
        std::lock_guard<std::mutex> lock(lifecycle);
        reclaimRetiredLocked();
       #if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
        if(!path.exists() || !path.hasFileExtension(".vst3"))
            return juce::Result::fail("Choose an installed .vst3 bundle");
        if(owned[(size_t)slot].size()>=8)
            return juce::Result::fail("Hosted insert is still in use. Close its old editor or retry after playback stops.");
        juce::OwnedArray<juce::PluginDescription> descriptions;
        // Some VST3 bundles publish more than one processor, so always scan.
        juce::VST3PluginFormat fallback;
        fallback.findAllTypesForFile(descriptions,path.getFullPathName());
        if(descriptions.isEmpty())return juce::Result::fail("No compatible audio VST3 effect found in bundle");
        juce::PluginDescription* chosen=nullptr;
        for(auto* description:descriptions)
            if(!description->isInstrument) {chosen=description;break;}
        // JUCE VST3 scans may report zero channels until the plug-in has been
        // instantiated; VST3 buses can also be dynamic. Do not reject a
        // valid audio effect using its uninitialized scanner channel counts.
        // Instead validate the real instance's stereo bus layout below.
        if(!chosen)return juce::Result::fail("An audio VST3 effect is required (not an instrument)");
        if(isSkellPresetProductName(chosen->name)
           || isSkellPresetProductName(chosen->descriptiveName)
           || isSkellPresetProductName(path.getFileNameWithoutExtension()))
            return juce::Result::fail("Recursive hosting of Skell-Preset is disabled");
        juce::String error;
        auto instance=formats.createPluginInstance(*chosen,std::max(sr,8000.),
                                                   std::max(block,64),error);
        if(!instance)return juce::Result::fail(error.isNotEmpty()?error:"Could not instantiate VST3");
        if(isSkellPresetProductName(instance->getName()))
            return juce::Result::fail("Recursive hosting of Skell-Preset is disabled");
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        if(!instance->setBusesLayout(stereo))
            return juce::Result::fail("Hosted VST3 must support stereo in/out");
        instance->setPlayHead(&transport); // stable, owned copy of DAW timing
        instance->prepareToPlay(std::max(sr,8000.),std::max(block,64));
        const int latency=instance->getLatencySamples();
        if(latency<0 || latency>maxTotalLatency) {
            instance->releaseResources();
            return juce::Result::fail("Hosted plugin latency exceeds the 65,536-sample safety budget");
        }
        int total=latency;
        for(int i=0;i<slotCount;++i)
            if(i!=slot)total+=getLatency(i);
        if(total>maxTotalLatency) {
            instance->releaseResources();
            return juce::Result::fail("Combined hosted latency exceeds the safe PDC budget");
        }
        auto entry=std::make_unique<Entry>();
        entry->plugin=std::move(instance);
        entry->prepared=true;
        entry->latency.store(latency,std::memory_order_relaxed);
        auto* pointer=entry.get();
        owned[(size_t)slot].push_back(std::move(entry));
        // Prefer the actual plugin product name. Some VST3s (including
        // third-party air/exciter effects) report an empty descriptiveName;
        // never show the generic VST3 INSERT label when loaded successfully.
        const auto display=chosen->descriptiveName.trim();
        names[(size_t)slot]=display.isNotEmpty()?display:
                            (chosen->name.isNotEmpty()?chosen->name:
                             path.getFileNameWithoutExtension());
        locations[(size_t)slot]=path;
        latencies[(size_t)slot].store(latency,std::memory_order_release);
        active[(size_t)slot].store(pointer,std::memory_order_seq_cst);
        reclaimRetiredLocked();
        return juce::Result::ok();
       #else
        (void)path;(void)sr;(void)block;
        return juce::Result::fail("External VST3 hosting is available only in Windows builds");
       #endif
    }
    juce::AudioPluginInstance* get(int slot) const noexcept {
        auto* entry=valid(slot)?active[(size_t)slot].load(std::memory_order_seq_cst):nullptr;
        return entry?entry->plugin.get():nullptr;
    }
    int getLatency(int slot) const noexcept {
        return valid(slot)?latencies[(size_t)slot].load(std::memory_order_acquire):0;
    }
    // Called on the audio thread before every internally chunked block.
    void updateTransport(juce::Optional<juce::AudioPlayHead::PositionInfo> position,
                         int chunkOffset,double sampleRate) {
        transport.update(std::move(position),chunkOffset,sampleRate);
    }
    void pollLatencies() {
        std::lock_guard<std::mutex> lock(lifecycle);
        reclaimRetiredLocked();
        // Report actual latency within the existing PDC safety budget.
        // Dynamic over-budget changes remain unsupported, never allocate
        // additional compensation buffers from the audio callback.
        for(int slot=0;slot<slotCount;++slot)
            if(auto* entry=active[(size_t)slot].load(std::memory_order_seq_cst)) {
                const int value=entry->plugin->getLatencySamples();
                if(value>=0 && value<=maxTotalLatency) {
                    entry->latency.store(value,std::memory_order_release);
                    latencies[(size_t)slot].store(value,std::memory_order_release);
                }
            }
    }
    size_t retainedCountForTest(int slot) {
        std::lock_guard<std::mutex> lock(lifecycle);
        return valid(slot)?owned[(size_t)slot].size():0;
    }
    juce::String getName(int slot) const {
        return valid(slot)?names[(size_t)slot]:juce::String{};
    }
    juce::File getFile(int slot) const {
        return valid(slot)?locations[(size_t)slot]:juce::File{};
    }
    void prepare(double sr,int block) {
        std::lock_guard<std::mutex> lock(lifecycle);
        for(int i=0;i<slotCount;++i) {
            if(auto* entry=active[(size_t)i].load(std::memory_order_seq_cst)) {
                entry->release();
                entry->plugin->prepareToPlay(std::max(sr,8000.),std::max(block,64));
                entry->prepared=true;
                const int latency=juce::jlimit(0,maxTotalLatency,entry->plugin->getLatencySamples());
                entry->latency.store(latency,std::memory_order_release);
                latencies[(size_t)i].store(latency,std::memory_order_release);
            }
        }
    }
    // Called only after the outer host has stopped processing. Also release
    // retired instances; their editors may still be open, so keep ownership.
    void release() {
        std::lock_guard<std::mutex> lock(lifecycle);
        for(auto& entries:owned)
            for(auto& entry:entries)entry->release();
    }
    juce::MemoryBlock getState(int slot) const {
        std::lock_guard<std::mutex> lock(lifecycle);
        juce::MemoryBlock state;
        if(auto* plugin=get(slot))plugin->getStateInformation(state);
        return state;
    }
    void setState(int slot,const juce::MemoryBlock& state) {
        std::lock_guard<std::mutex> lock(lifecycle);
        if(auto* plugin=get(slot))plugin->setStateInformation(state.getData(),(int)state.getSize());
    }
private:
    static bool valid(int slot) noexcept {return slot>=0 && slot<slotCount;}
    juce::AudioPluginFormatManager formats;
    std::array<std::atomic<Entry*>,slotCount> active{};
    std::atomic<unsigned> readers{0};
    mutable std::mutex lifecycle;
    void reclaimRetiredLocked() {
        // Only load/poll on the message thread call this function. Never
        // destroy a third-party editor or plugin on the audio thread.
        if(readers.load(std::memory_order_seq_cst)!=0)return;
        for(int slot=0;slot<slotCount;++slot) {
            auto* current=active[(size_t)slot].load(std::memory_order_seq_cst);
            auto& entries=owned[(size_t)slot];
            entries.erase(std::remove_if(entries.begin(),entries.end(),[current](const auto& entry) {
                return entry.get()!=current && entry->plugin->getActiveEditor()==nullptr;
            }),entries.end());
        }
    }
    std::array<std::atomic<int>,slotCount> latencies{};
    HostedTransport transport; // outlives all owned plug-ins at destruction
    std::array<std::vector<std::unique_ptr<Entry>>,slotCount> owned;
    std::array<juce::String,slotCount> names;
    std::array<juce::File,slotCount> locations;
};
} // namespace skellpreset::host
