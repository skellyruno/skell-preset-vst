#pragma once
#include <JuceHeader.h>
#include <SkellCore/Core.h>
#include "chain/BasicDSP.h"
#include "chain/FactoryPresets.h"
#include "chain/PresetPackage.h"
#include <array>
#include <atomic>
#include <cstdint>

class SkellPresetAudioProcessor final : public juce::AudioProcessor {
public:
    enum ModuleType { Empty, EQ, Compressor, Limiter, Gate, Saturator, Reverb, Delay, Exciter, AdlibThrow, AdlibDoubler, NumberOfModules };
    static constexpr int slotCount=10;
    SkellPresetAudioProcessor();
    ~SkellPresetAudioProcessor() override = default;
    void prepareToPlay(double sampleRate,int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "Skell-Preset";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 8.0;}
    int getNumPrograms() override {return (int)skellpreset::factoryPresets.size();}
    int getCurrentProgram() override {return juce::jmax(0,selectedProgram);}
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState state;
    void loadFactoryPreset(int index);
    // Message-thread only, validated native .skellpreset ZIP packages.
    juce::Result exportPreset(const juce::File& file) const;
    juce::Result importPreset(const juce::File& file);
    void reorderSlot(int from,int to);
    int slotIdAt(int position) const noexcept {return (int)((packedOrder.load(std::memory_order_acquire) >> (4*juce::jlimit(0,9,position)))&0x0fU);}
    juce::String presetName() const {return name;}
    // -1 denotes an imported/custom chain; getCurrentProgram() is clamped for
    // host API compatibility and must not drive the browser's ACTIVE indicator.
    int loadedFactoryPresetIndex() const noexcept {return selectedProgram;}
    juce::String moduleName(int type) const;
    void setSlotType(int slot,int type);
    float getSlotParam(int slot,int param) const noexcept;
    void setSlotParam(int slot,int param,float normalised);
    void setSlotOn(int slot,bool on);
    float getInputPeak() const noexcept {return inputPeak.load();}
    float getOutputPeak() const noexcept {return outputPeak.load();}
    float getSlotPeak(int slot) const noexcept {return slotPeaks[(size_t)juce::jlimit(0,9,slot)].load();}
private:
    struct EngineSlot {
        skellpreset::dsp::SmoothedBiquad peak, highpass;
        skellpreset::dsp::Compressor comp;
        skellpreset::dsp::Gate gate;
        skellpreset::dsp::Delay delay;
        skellpreset::dsp::AdlibDoubler doubler;
        skellpreset::dsp::StereoSampleDelay limiterDry;
        skellcore::TruePeakLimiter limiter;
        skellcore::PhraseTracker phrase;
        std::array<std::vector<float>,2> throwHistory;
        size_t throwWrite=0,throwRead=0;
        int throwRemaining=0;
        bool phrasePreviouslyActive=false,manualPreviouslyActive=false;
        float throwFilterL=0,throwFilterR=0,throwFilterPole=.6f;
        void processThrow(float l,float r,float& outL,float& outR,float amount,float feedback,float tone,float trigger,double sampleRate) noexcept;
        std::array<skellcore::FDNReverb,1> reverb;
        void prepare(double sampleRate);
    };
    std::array<EngineSlot,slotCount> engines;
    struct Controls {int type=0;bool on=false;float wet=1;std::array<float,16> p{};};
    std::array<Controls,slotCount> controlCache;
    std::atomic<std::uint64_t> packedOrder{0};
    struct RawSlot {
        std::atomic<float>* type=nullptr;std::atomic<float>* on=nullptr;
        std::atomic<float>* wet=nullptr;
        std::array<std::atomic<float>*,16> p{};
    };
    struct RawGlobal {
        std::atomic<float>* mode=nullptr;std::atomic<float>* input=nullptr;
        std::atomic<float>* mix=nullptr;std::atomic<float>* output=nullptr;
        std::atomic<float>* gate=nullptr;std::atomic<float>* threshold=nullptr;
        std::atomic<float>* delta=nullptr;
        std::array<std::atomic<float>*,4> macros{};
    } rawGlobal;
    std::array<RawSlot,slotCount> rawSlots;
    std::array<std::atomic<float>,slotCount> slotPeaks;
    std::atomic<float> inputPeak{0},outputPeak{0};
    double rate=48000;
    int selectedProgram=0;
    juce::String name="Clean Lead";
    skellpreset::dsp::StereoSampleDelay dryPdc,bypassPdc;
    std::atomic<int> limiterLookahead{0};
    int currentReportedLatency=0;
    void updateHostLatency();
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear> inputSmooth,outputSmooth,mixSmooth,bypassSmooth;
    // Continuous per-slot wet/bypass fades; all memory is owned by processor.
    std::array<juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear>,slotCount> slotWetSmooth;
    void setReal(const juce::String& id,float value);
    float value(const juce::String& id) const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellPresetAudioProcessor)
};
