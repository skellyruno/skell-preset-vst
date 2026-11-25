#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// In-process VST3 hosting proof of integration, Windows only.
// All scanning, creation, state IO, UI and destruction run off the audio thread.
// Published instances are never destroyed until the outer processor stops,
// avoiding use-after-free during host parameter/slot changes. Replacements per
// slot are capped to prevent unbounded memory retention.
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
public:
    static constexpr int slotCount=16;
    static constexpr int maxTotalLatency=65536;
    HostedVST3() {
       #if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
        // JUCE 8 removed AudioPluginFormatManager::addDefaultFormats().
        // Register only VST3 to avoid accidentally loading unsupported formats.
        formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
       #endif
    }

    // The current FloopHole Windows DLL calls discover(true) in InitDll.
    // Nested in FL Studio, loading it can hang the DAW before JUCE returns.
    static bool needsIsolatedHost(const juce::File& path) {
        const auto name=path.getFileNameWithoutExtension()
            .toLowerCase().removeCharacters(" -_.");
        return name.startsWith("floophole");
    }
    static juce::Result isolatedHostRequired() {
        return juce::Result::fail(
            "FloopHole needs an isolated host: its DLL scans FL Studio plugins during initialization and can freeze FL Studio when nested. Test it in a separate host until Skell has a sandboxed bridge.");
    }
    struct EffectChoice {juce::String identifier,name;};
    static juce::String classKey(const juce::PluginDescription& description) {
        return description.createIdentifierString();
    }
    // Exact class matching, never a name-only guess. Empty key retains the
    // legacy path-only project behavior and single-effect API.
    static const juce::PluginDescription* chooseEffect(
        const juce::OwnedArray<juce::PluginDescription>& descriptions,
        const juce::String& identifier) {
        const juce::PluginDescription* match=nullptr;
        for(auto* description:descriptions){
            if(description->isInstrument)continue;
            if(identifier.isEmpty())return description;
            if(classKey(*description)==identifier){
                if(match)return nullptr; // ambiguous class identity
                match=description;
            }
        }
        return match;
    }
    static juce::Result inspectEffects(const juce::File& path,std::vector<EffectChoice>& choices) {
        choices.clear();
        if(needsIsolatedHost(path))return isolatedHostRequired();
       #if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
        if(!path.exists() || !path.hasFileExtension("vst3"))
            return juce::Result::fail("Choose an installed .vst3 bundle");
        if(isSkellPresetProductName(path.getFileNameWithoutExtension()))
            return juce::Result::fail("Recursive hosting of Skell-Preset is disabled");
        juce::OwnedArray<juce::PluginDescription> descriptions;
        juce::VST3PluginFormat scanner;
        scanner.findAllTypesForFile(descriptions,path.getFullPathName());
        for(auto* d:descriptions){
            if(d->isInstrument || isSkellPresetProductName(d->name) ||
               isSkellPresetProductName(d->descriptiveName))continue;
            const auto key=classKey(*d);
            if(key.isEmpty() || chooseEffect(descriptions,key)!=d)continue;
            choices.push_back({key,d->name.isNotEmpty()?d->name:d->descriptiveName});
        }
        if(choices.empty())return juce::Result::fail("No supported audio effect classes found; check the bridge and licensed FL Studio installation.");
        return juce::Result::ok();
       #else
        (void)path;
        return juce::Result::fail("External VST3 hosting is available only in Windows builds");
       #endif
    }
    // JUCE requires the layout to have one entry for *every* bus, including
    // disabled sidechains. A one-input/one-output BusesLayout wrongly rejects
    // ordinary stereo effects with optional auxiliary buses.
    // Always keep the externally visible chain stereo, and never route
    // unconnected sidechain audio to an uninitialised bus.
    static juce::Result configureMainStereoBuses(juce::AudioProcessor& instance) {
        auto layout=instance.getBusesLayout();
        if(layout.inputBuses.isEmpty() || layout.outputBuses.isEmpty())
            return juce::Result::fail("Hosted effects require main input and output buses");
        layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0,juce::AudioChannelSet::stereo());
        for(int bus=1;bus<layout.inputBuses.size();++bus)
            layout.inputBuses.set(bus,juce::AudioChannelSet::disabled());
        for(int bus=1;bus<layout.outputBuses.size();++bus)
            layout.outputBuses.set(bus,juce::AudioChannelSet::disabled());
        if(!instance.setBusesLayout(layout))
            return juce::Result::fail(
                "Hosted VST3 rejected stereo main buses with auxiliary buses disabled");
        if(instance.getTotalNumInputChannels()!=2 || instance.getTotalNumOutputChannels()!=2)
            return juce::Result::fail("Hosted VST3 main stereo routing is unavailable");
        return juce::Result::ok();
    }
    juce::Result load(int slot,const juce::File& path,double sr,int block,
                      const juce::String& identifier={}) {
        if(!valid(slot))return juce::Result::fail("Invalid rack slot");
        if(needsIsolatedHost(path))return isolatedHostRequired();
       #if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
        if(!path.exists() || !path.hasFileExtension(".vst3"))
            return juce::Result::fail("Choose an installed .vst3 bundle");
        if(isSkellPresetProductName(path.getFileNameWithoutExtension()))
            return juce::Result::fail("Recursive hosting of Skell-Preset is disabled");
        if(owned[(size_t)slot].size()>=8)
            return juce::Result::fail("Restart the DAW session before replacing this insert again");
        juce::OwnedArray<juce::PluginDescription> descriptions;
        // Some VST3 bundles publish more than one processor, so always scan.
        juce::VST3PluginFormat fallback;
        fallback.findAllTypesForFile(descriptions,path.getFullPathName());
        if(descriptions.isEmpty())return juce::Result::fail("No compatible audio VST3 effect found in bundle");
        const auto* chosen=chooseEffect(descriptions,identifier);
        // JUCE VST3 scans may report zero channels until the plug-in has been
        // instantiated; VST3 buses can also be dynamic. Do not reject a
        // valid audio effect using its uninitialized scanner channel counts.
        // Instead validate the real instance's stereo bus layout below.
        if(!chosen)return juce::Result::fail(identifier.isNotEmpty()?
            "Saved effect class is unavailable or ambiguous; no substitute was loaded.":
            "An audio VST3 effect is required (not an instrument)");
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
        const auto buses=configureMainStereoBuses(*instance);
        if(buses.failed())return buses;
        instance->setPlayHead(&transport); // stable, owned copy of DAW timing
        instance->prepareToPlay(std::max(sr,8000.),std::max(block,64));
        const int latency=instance->getLatencySamples();
        if(latency<0 || latency>maxTotalLatency)
            return juce::Result::fail("Hosted plugin latency exceeds the 65,536-sample safety budget");
        int total=latency;
        for(int i=0;i<slotCount;++i)
            if(i!=slot)total+=getLatency(i);
        if(total>maxTotalLatency)
            return juce::Result::fail("Combined hosted latency exceeds the safe PDC budget");
        auto* pointer=instance.get();
        owned[(size_t)slot].push_back(std::move(instance));
        // Prefer the actual plugin product name. Some VST3s (including
        // third-party air/exciter effects) report an empty descriptiveName;
        // never show the generic VST3 INSERT label when loaded successfully.
        const auto display=chosen->descriptiveName.trim();
        names[(size_t)slot]=display.isNotEmpty()?display:
                            (chosen->name.isNotEmpty()?chosen->name:
                             path.getFileNameWithoutExtension());
        locations[(size_t)slot]=path;
        identifiers[(size_t)slot]=classKey(*chosen);
        latencies[(size_t)slot].store(latency,std::memory_order_release);
        active[(size_t)slot].store(pointer,std::memory_order_release);
        return juce::Result::ok();
       #else
        (void)path;(void)sr;(void)block;(void)identifier;
        return juce::Result::fail("External VST3 hosting is available only in Windows builds");
       #endif
    }
    juce::String getIdentifier(int slot) const {
        return valid(slot)?identifiers[(size_t)slot]:juce::String{};
    }
    // Message thread only. Retain ownership so the callback cannot use a
    // freed instance. Never substitute a previously loaded unrelated effect.
    void detach(int slot) {
        if(!valid(slot))return;
        active[(size_t)slot].store(nullptr,std::memory_order_release);
        latencies[(size_t)slot].store(0,std::memory_order_release);
        names[(size_t)slot].clear();
        identifiers[(size_t)slot].clear();
        locations[(size_t)slot]=juce::File{};
    }
    juce::AudioPluginInstance* get(int slot) const noexcept {
        return valid(slot)?active[(size_t)slot].load(std::memory_order_acquire):nullptr;
    }
    int getLatency(int slot) const noexcept {
        return valid(slot)?latencies[(size_t)slot].load(std::memory_order_acquire):0;
    }
    // Called on the audio thread before every internally chunked block.
    void updateTransport(juce::Optional<juce::AudioPlayHead::PositionInfo> position,
                         int chunkOffset,double sampleRate) {
        transport.update(std::move(position),chunkOffset,sampleRate);
    }
    void pollLatencies() noexcept {
        // Message-thread-only check for hosted processors that change PDC
        // after a preset or oversampling switch.
        for(int slot=0;slot<slotCount;++slot)
            if(auto* plugin=get(slot)) {
                const int value=plugin->getLatencySamples();
                if(value>=0 && value<=maxTotalLatency)
                    latencies[(size_t)slot].store(value,std::memory_order_release);
            }
    }
    juce::String getName(int slot) const {
        return valid(slot)?names[(size_t)slot]:juce::String{};
    }
    juce::File getFile(int slot) const {
        return valid(slot)?locations[(size_t)slot]:juce::File{};
    }
    void prepare(double sr,int block) {
        for(int i=0;i<slotCount;++i) {
            if(auto* instance=get(i)) {
                instance->releaseResources();
                instance->prepareToPlay(std::max(sr,8000.),std::max(block,64));
                latencies[(size_t)i].store(juce::jlimit(0,maxTotalLatency,
                    instance->getLatencySamples()),std::memory_order_release);
            }
        }
    }
    void release() {
        for(int i=0;i<slotCount;++i)
            if(auto* instance=get(i)) instance->releaseResources();
    }
    juce::MemoryBlock getState(int slot) const {
        juce::MemoryBlock state;
        if(auto* plugin=get(slot))plugin->getStateInformation(state);
        return state;
    }
    void setState(int slot,const juce::MemoryBlock& state) {
        if(auto* plugin=get(slot))plugin->setStateInformation(state.getData(),(int)state.getSize());
    }
private:
    static bool valid(int slot) noexcept {return slot>=0 && slot<slotCount;}
    juce::AudioPluginFormatManager formats;
    std::array<std::atomic<juce::AudioPluginInstance*>,slotCount> active{};
    std::array<std::atomic<int>,slotCount> latencies{};
    HostedTransport transport; // outlives all owned plug-ins at destruction
    std::array<std::vector<std::unique_ptr<juce::AudioPluginInstance>>,slotCount> owned;
    std::array<juce::String,slotCount> names,identifiers;
    std::array<juce::File,slotCount> locations;
};
} // namespace skellpreset::host
