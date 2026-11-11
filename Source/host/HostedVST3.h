#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// In-process VST3 hosting proof of integration, Windows only.
// All scanning, creation, state IO, UI and destruction run off the audio thread.
// Published instances are never destroyed until the outer processor stops,
// avoiding use-after-free during host parameter/slot changes. Replacements per
// slot are capped to prevent unbounded memory retention.
namespace skellpreset::host {
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

    juce::Result load(int slot,const juce::File& path,double sr,int block) {
        if(!valid(slot))return juce::Result::fail("Invalid rack slot");
       #if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
        if(!path.exists() || !path.hasFileExtension(".vst3"))
            return juce::Result::fail("Choose an installed .vst3 bundle");
        if(owned[(size_t)slot].size()>=8)
            return juce::Result::fail("Restart the DAW session before replacing this insert again");
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
        juce::String error;
        auto instance=formats.createPluginInstance(*chosen,std::max(sr,8000.),
                                                   std::max(block,64),error);
        if(!instance)return juce::Result::fail(error.isNotEmpty()?error:"Could not instantiate VST3");
        if(instance->getName().containsIgnoreCase("Skell-Preset"))
            return juce::Result::fail("Recursive hosting of Skell-Preset is disabled");
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        if(!instance->setBusesLayout(stereo))
            return juce::Result::fail("Hosted VST3 must support stereo in/out");
        instance->setPlayHead(nullptr); // no dangling pointer into the outer processor
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
        names[(size_t)slot]=chosen->descriptiveName;
        locations[(size_t)slot]=path;
        latencies[(size_t)slot].store(latency,std::memory_order_release);
        active[(size_t)slot].store(pointer,std::memory_order_release);
        return juce::Result::ok();
       #else
        (void)path;(void)sr;(void)block;
        return juce::Result::fail("External VST3 hosting is available only in Windows builds");
       #endif
    }
    juce::AudioPluginInstance* get(int slot) const noexcept {
        return valid(slot)?active[(size_t)slot].load(std::memory_order_acquire):nullptr;
    }
    int getLatency(int slot) const noexcept {
        return valid(slot)?latencies[(size_t)slot].load(std::memory_order_acquire):0;
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
    std::array<std::vector<std::unique_ptr<juce::AudioPluginInstance>>,slotCount> owned;
    std::array<juce::String,slotCount> names;
    std::array<juce::File,slotCount> locations;
};
} // namespace skellpreset::host
