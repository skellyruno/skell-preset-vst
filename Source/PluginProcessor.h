#pragma once
#include <JuceHeader.h>
#include <SkellCore/Core.h>
#include <SkellCore/AdlibFX.h>
#include "chain/BasicDSP.h"
#include "chain/LiveAudioScope.h"
#include "chain/ClipRecorder.h"
#include "chain/VocalEffects.h"
#include "chain/ModulationFX.h"
#include "chain/StudioChorusDSP.h"
#include "chain/StudioNativeDSP.h"
#include "chain/StudioDelayDSP.h"
#include "chain/WetDuckerDSP.h"
#include "chain/ImportedVocalFX.h"
#include "chain/AdvancedVocalDSP.h"
#include "chain/AlchemistMatchDSP.h"
#include "chain/MicModelDSP.h"
#include "chain/SkelldizerDSP.h"
#include "chain/SpectralPitchFormant.h"
#include "chain/VoxRackFX.h"
#include "chain/FactoryPresets.h"
#include "chain/PresetPackage.h"
#include "host/HostedVST3.h"
#include <array>
#include <atomic>
#include <cstdint>

class SkellPresetAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::AudioProcessorValueTreeState::Listener {
public:
    enum ModuleType { Empty, EQ, Compressor, Limiter, Gate, Saturator, Reverb, Delay, Exciter, AdlibThrow, AdlibDoubler, DeEsser, ExternalVST3, Radio, RetroCrush, Tremolo, StereoWidth, ThreeBandDynamics, VoxRackRoom, VoxRackSmoothComp, VocalChorusFX, VocalFlangerFX, RingModFX, VocalRiderFX, DuckingPingPongFX, VocalVibratoFX, EightBandToneFX, SerialDynamicsFX, DynamicHarshnessFX, AdaptiveNoiseFX, StudioPitchFormantFX, AlchemistMatchFinishFX, MicModelFX, SkelldizerFX, StudioEQFX, StudioLimiterFX, StudioCompressorFX, StudioFastDistFX, StudioDelayFX, AdlibChop, AdlibMotion, AdlibFilter, AdlibSaturate, NumberOfModules };
    static constexpr int slotCount=16;
    static bool supportsWetSidechain(int type) noexcept {
        return type==Reverb || type==VoxRackRoom || type==Delay ||
               type==StudioDelayFX || type==AdlibThrow ||
               type==DuckingPingPongFX;
    }
    static constexpr int defaultSlotCount=10;
    static constexpr int emptyRackProgramIndex=static_cast<int>(skellpreset::factoryPresets.size());
    static constexpr int customRackProgramIndex=emptyRackProgramIndex+1;
    SkellPresetAudioProcessor();
    ~SkellPresetAudioProcessor() override;
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
    int getNumPrograms() override {return customRackProgramIndex+1;}
    int getCurrentProgram() override {
        if(selectedProgram>=0)return selectedProgram;
        return selectedProgram==-2 && emptyRackIdentity.load(std::memory_order_acquire)
            ? emptyRackProgramIndex : customRackProgramIndex;
    }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState state;
    void loadFactoryPreset(int index);
    void clearRackToEmpty();
    // Message-thread only, validated native .skellpreset ZIP packages.
    juce::Result exportPreset(const juce::File& file) const;
    juce::Result importPreset(const juce::File& file);
    void reorderSlot(int from,int to);
    // Remove an extra rack row without renumbering any APVTS slot identity.
    // The first ten physical rack positions remain for legacy-state safety.
    bool removeRackSlot(int visualPosition);
    // Message-thread operation: expands the real processing rack, not just a placeholder.
    // Returns the new physical slot ID, or -1 when all sixteen slots are visible.
    int appendRackSlot();
    int activeSlotCount() const noexcept {return activeSlots.load(std::memory_order_acquire);}
    int slotIdAt(int position) const noexcept {return (int)((packedOrder.load(std::memory_order_acquire) >> (4*juce::jlimit(0,slotCount-1,position)))&0x0fU);}
    juce::String presetName() const {
        if(selectedProgram>=0 && selectedProgram<(int)skellpreset::factoryPresets.size()) {
            const juce::String factoryName(skellpreset::factoryPresets[(size_t)selectedProgram].name);
            if(factoryName=="Alchemist (Native Preview)")return "Alchemist";
            if(factoryName=="Deuce v2.5 (Native Trial)")return "Deuce";
        }
        return selectedProgram==-2 && !emptyRackIdentity.load(std::memory_order_acquire)
            ? juce::String("Custom Rack") : name;
    }
    // Negative identities are not factory presets; the host program index is
    // mapped separately to Empty Rack or Custom Rack without shifting factories.
    int loadedFactoryPresetIndex() const noexcept {return selectedProgram>=0?selectedProgram:-1;}
    juce::String moduleName(int type) const;
    void setSlotType(int slot,int type);
    // Decodes the backward-compatible legacy type + optional extended effect.
    int slotType(int slot) const noexcept;
    float getSlotParam(int slot,int param) const noexcept;
    void setSlotParam(int slot,int param,float normalised);
    void setSlotOn(int slot,bool on);
    juce::Result loadHostedVST3(int slot,const juce::File& path);
    void openHostedVST3Editor(int slot);
    juce::String hostedVST3Name(int slot) const {return hosted.getName(slot);}
    bool hasHostedVST3(int slot) const noexcept {return hosted.get(slot)!=nullptr;}
    // Message-thread-only borrowed pointer for optional inline JUCE editor.
    // The host retains plugin instances until processor destruction.
    juce::AudioPluginInstance* hostedVST3InstanceForUI(int slot) const noexcept {
        return hosted.get(slot);
    }
    int hostedVST3LatencySamples(int slot) const noexcept {return hosted.getLatency(slot);}
    void refreshHostedLatency();
    double preparedAudioRate() const noexcept {
        return displayedRate.load(std::memory_order_acquire);
    }
    template<std::size_t N> void getLiveWaveform(std::array<float,N>& samples) const noexcept {
        liveScope.snapshot(samples);
    }
    // Audio producer progress is lock-free; the editor can avoid repainting
    // an unchanged waveform while FL Studio's transport is idle.
    unsigned liveScopeSampleCount() const noexcept {return liveScope.samplesCaptured();}
    // UI-triggered WAV capture of the processed host channel; never opens a
    // microphone independently or overwrites DAW input/recording settings.
    juce::Result startCapture(const juce::File& path);
    void stopCapture();
    bool isCapturing() const noexcept {return capture.isRecording();}
    double capturedSeconds() const noexcept {return capture.secondsWritten();}
    std::uint64_t droppedCaptureSamples() const noexcept {return capture.droppedFrames();}
    float getInputPeak() const noexcept {return inputPeak.load();}
    float getOutputPeak() const noexcept {return outputPeak.load();}
    float getSlotPeak(int slot) const noexcept {return slotPeaks[(size_t)juce::jlimit(0,slotCount-1,slot)].load();}
private:
    struct EngineSlot {
        skellpreset::dsp::SmoothedBiquad peak, highpass;
        skellpreset::dsp::Compressor comp;
        skellpreset::dsp::Gate gate;
        skellpreset::dsp::SplitDeEsser deesser;
        skellpreset::dsp::BroadcastRadio radio;
        skellpreset::dsp::RetroCrusher crusher;
        skellpreset::dsp::VocalTremolo tremolo;
        skellpreset::dsp::VocalWidth width;
        skellpreset::dsp::ThreeBandDynamics threeBand;
        skellpreset::dsp::VoxRackRoom voxRoom;
        skellpreset::dsp::VoxRackSmoothCompressor voxComp;
        skellpreset::studio::Chorus chorus;
        skellpreset::studio::ParametricEQ studioEq;
        skellpreset::studio::Compressor studioComp,studioLimiterComp;
        skellpreset::studio::Gate studioGate;
        skellpreset::studio::FastDist studioDist;
        skellpreset::dsp::VocalFlanger flanger;
        skellpreset::dsp::VocalRingMod ring;
        skellcore::adlibfx::Processor adlibFx;
        skellpreset::dsp::VocalRider rider;
        skellpreset::dsp::DuckingPingPongDelay duckingDelay;
        skellpreset::dsp::VocalVibrato vibrato;
        skellpreset::dsp::EightBandTone eightBand;
        skellpreset::dsp::SerialVocalDynamics serialDynamics;
        skellpreset::dsp::AlchemistMatchFinish matchFinish;
        skellpreset::dsp::MicModelTone micModel;
        skellpreset::dsp::Skelldizer skelldizer;
        skellpreset::dsp::DynamicHarshness harshness;
        skellpreset::dsp::AdaptiveNoiseControl noiseControl;
        skellpreset::dsp::SpectralPitchFormant studioPitch;
        skellpreset::dsp::StereoSampleDelay pitchDry;
        skellpreset::dsp::Delay delay;
        skellpreset::studio::StereoDelay studioDelay;
        skellpreset::dsp::VoiceWetDucker voiceDuck;
        skellpreset::dsp::AdlibDoubler doubler;
        skellpreset::dsp::StereoSampleDelay limiterDry,hostDry;
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
    skellpreset::host::HostedVST3 hosted;
    juce::AudioBuffer<float> rackWork,hostWork,dryWork,bypassWork,sidechainWork;
    int hostChunkCapacity=512;
    struct Controls {
        int type=0;bool on=false;float wet=1;float eqLowCut=0,eqHighCut=0;
        bool duckOn=false;
        float duckDepth=.65f,duckThreshold=-32.f,duckRelease=320.f;
        std::array<float,16> p{};std::array<float,8> q{};
        std::array<int,8> studioEqKinds{{3,0,0,0,0,0,4,0}};
        std::array<bool,8> studioEqEnabled{{true,true,true,true,true,true,true,false}};
    };
    std::array<Controls,slotCount> controlCache;
    std::atomic<std::uint64_t> packedOrder{0};
    std::atomic<int> activeSlots{defaultSlotCount};
    struct RawSlot {
        std::atomic<float>* type=nullptr;std::atomic<float>* extension=nullptr;
        std::atomic<float>* extension2=nullptr;
        std::atomic<float>* extension3=nullptr;
        std::atomic<float>* extension4=nullptr;
        std::atomic<float>* extension5=nullptr;
        std::atomic<float>* extension6=nullptr;
        std::atomic<float>* extension7=nullptr;
        std::atomic<float>* extension8=nullptr;
        std::atomic<float>* extension9=nullptr;
        std::atomic<float>* on=nullptr;
        std::atomic<float>* wet=nullptr;
        std::atomic<float>* eqLowCut=nullptr,*eqHighCut=nullptr;
        std::atomic<float>* duckOn=nullptr,*duckDepth=nullptr;
        std::atomic<float>* duckThreshold=nullptr,*duckRelease=nullptr;
        std::array<std::atomic<float>*,16> p{};
        std::array<std::atomic<float>*,8> q{};
        std::array<std::atomic<float>*,8> studioEqKind{};
        std::array<std::atomic<float>*,8> studioEqOn{};
    };
    struct RawMic {
        std::atomic<float>* on=nullptr;
        std::atomic<float>* input=nullptr;
        std::atomic<float>* target=nullptr;
        std::atomic<float>* amount=nullptr;
        std::atomic<float>* trim=nullptr;
        std::atomic<float>* gateOn=nullptr;
        std::atomic<float>* gateThreshold=nullptr;
        std::atomic<float>* gateRelease=nullptr;
        std::atomic<float>* gateHold=nullptr;
    } rawMic;
    skellpreset::dsp::MicModelTone micFrontEnd;
    skellpreset::dsp::Gate micInputGate;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear> micFrontWet,micGateWet;
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
    skellpreset::visual::LiveAudioScope liveScope;
    skellpreset::capture::ClipRecorder capture;
    double rate=48000;
    std::atomic<double> displayedRate{48000.}; // message-thread PDC readout
    int selectedProgram=-2; // -2 empty rack, -1 imported/edited custom chain, >=0 factory
    std::atomic<bool> emptyRackIdentity{true};
    std::atomic<bool> suppressProgramIdentityUpdates{false};
    juce::StringArray programIdentityListenerIds;
    juce::String name="Empty Rack";
    skellpreset::dsp::StereoSampleDelay dryPdc,bypassPdc;
    std::atomic<int> limiterLookahead{0};
    int currentReportedLatency=0;
    void updateHostLatency();
    void handleAsyncUpdate() override;
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear> inputSmooth,outputSmooth,mixSmooth,bypassSmooth;
    // Continuous per-slot wet/bypass fades; all memory is owned by processor.
    std::array<juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear>,slotCount> slotWetSmooth;
    void setReal(const juce::String& id,float value);
    void addProgramIdentityListeners();
    void removeProgramIdentityListeners() noexcept;
    void parameterChanged(const juce::String&,float) override;
    float value(const juce::String& id) const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellPresetAudioProcessor)
};
