// Diagnostic-only Alchemist WAV renderer. Compiles and runs the SAME
// SkellPresetAudioProcessor::processBlock and factory chain as the VST3;
// this is not an external VST3-host or FL Studio session validation.
// Does not package, inspect or transmit the owner's WAV files.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/chain/NativeSoundMatchTrials.h"
#include "../Source/chain/ABMetrics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
constexpr int blockSize = 512;

int process(juce::AudioBuffer<float>& block, SkellPresetAudioProcessor& plugin,
            double& peak, double& energy, std::int64_t& count) {
    juce::MidiBuffer midi;
    plugin.processBlock(block, midi);
    for (int ch = 0; ch < block.getNumChannels(); ++ch)
        for (int n = 0; n < block.getNumSamples(); ++n) {
            const float v = block.getSample(ch,n);
            if (!std::isfinite(v)) {
                std::fprintf(stderr, "ERROR: non-finite native Alchemist sample\n");
                return 1;
            }
            peak = std::max(peak, static_cast<double>(std::abs(v)));
            energy += static_cast<double>(v)*v;
            ++count;
        }
    return 0;
}

// Offline 4-phase / 16-tap interpolated-peak estimate, matching the
// SkellCore Standard limiter's detector kernel for post-render verification.
double estimateFourPhasePeak(const std::vector<float>& samples) {
    constexpr int taps=16, phases=4;
    constexpr double pi=3.1415926535897932384626433832795;
    std::array<std::array<double,taps>,phases> kernel{};
    for(int phase=0;phase<phases;++phase) {
        double sum=0.;
        for(int j=0;j<taps;++j) {
            const double offset=j-(taps-1)*.5+double(phase)/phases;
            const double sinc=std::abs(offset)<1.e-10?1.:
                std::sin(pi*offset)/(pi*offset);
            const double window=.5-.5*std::cos(2.*pi*j/(taps-1));
            kernel[(size_t)phase][(size_t)j]=sinc*window;
            sum+=sinc*window;
        }
        for(auto& value:kernel[(size_t)phase])value/=sum;
    }
    double peak=0.;
    for(size_t i=0;i<samples.size();++i) {
        peak=std::max(peak,std::abs((double)samples[i]));
        if(i+1<(size_t)taps)continue;
        for(int phase=0;phase<phases;++phase) {
            double interpolated=0.;
            for(int j=0;j<taps;++j)
                interpolated+=kernel[(size_t)phase][(size_t)j]*
                    samples[i-(size_t)j];
            peak=std::max(peak,std::abs(interpolated));
        }
    }
    return peak;
}

int nativeStageCountForFactory(int factory) {
    if(factory==skellpreset::alchemist::factoryIndex)
        return (int)skellpreset::alchemist::stages.size();
    if(factory==skellpreset::deuce::factoryIndex)
        return (int)skellpreset::deuce::stages.size();
    if(factory==skellpreset::alchemist_trial::factoryIndex)
        return (int)skellpreset::alchemist_trial::stages.size();
    if(factory==skellpreset::deuce_v25::factoryIndex)
        return (int)skellpreset::deuce_v25::stages.size();
    if(factory==skellpreset::alchemist_peak_guard_trial::factoryIndex)
        return (int)skellpreset::alchemist_peak_guard_trial::stages.size();
    if(factory==skellpreset::alchemist_tone_match_trial::factoryIndex)
        return (int)skellpreset::alchemist_tone_match_trial::stages.size();
    if(factory==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex)
        return (int)skellpreset::alchemist_lighter_dynamics_trial::stages.size();
    if(factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex)
        return (int)skellpreset::alchemist_fast_compressor_blend_trial::stages.size();
    if(factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex)
        return (int)skellpreset::alchemist_smooth_compressor_blend_trial::stages.size();
    if(factory==skellpreset::alchemist_serial_dynamics_trial::factoryIndex)
        return (int)skellpreset::alchemist_serial_dynamics_trial::stages.size();
    if(factory==skellpreset::alchemist_low_end_contour_trial::factoryIndex)
        return (int)skellpreset::alchemist_low_end_contour_trial::stages.size();
    if(factory==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex)
        return (int)skellpreset::alchemist_serial_dynamics_blend_trial::stages.size();
    return 0;
}

bool setProcessorParameter(SkellPresetAudioProcessor& processor,
                           const juce::String& id,float value) {
    auto* parameter=processor.state.getParameter(id);
    if(!parameter)return false;
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    return true;
}

// Deliberately dirty every serialized physical slot before a factory recall.
// This catches stale values that remain hidden until a user expands the rack
// or inserts Studio EQ into a formerly inactive row.
bool dirtySerializedSlotControls(SkellPresetAudioProcessor& processor) {
    for(int slot=0;slot<SkellPresetAudioProcessor::slotCount;++slot) {
        const auto prefix="s"+juce::String(slot+1)+"_";
        if(!setProcessorParameter(processor,prefix+"type",
                                  (float)SkellPresetAudioProcessor::EQ) ||
           !setProcessorParameter(processor,prefix+"on",1.f) ||
           !setProcessorParameter(processor,prefix+"wet",.25f) ||
           !setProcessorParameter(processor,prefix+"eqLowCut",.75f) ||
           !setProcessorParameter(processor,prefix+"eqHighCut",.85f))
            return false;
        for(int k=1;k<=16;++k)
            if(!setProcessorParameter(processor,prefix+"p"+juce::String(k),
                                      .17f+.01f*(float)(k-1)))return false;
        for(int band=1;band<=8;++band) {
            const auto suffix=juce::String(band);
            if(!setProcessorParameter(processor,prefix+"eqQ"+suffix,
                                      .13f+.02f*(float)band) ||
               !setProcessorParameter(processor,prefix+"studioEqKind"+suffix,2.f) ||
               !setProcessorParameter(processor,prefix+"studioEqOn"+suffix,0.f))
                return false;
        }
    }
    return true;
}

int checkNativeFactorySlotDefaults(SkellPresetAudioProcessor& processor,
                                   int stageCount) {
    if(stageCount<=0 || stageCount>SkellPresetAudioProcessor::slotCount)return 249;
    for(int slot=0;slot<SkellPresetAudioProcessor::slotCount;++slot) {
        const auto prefix="s"+juce::String(slot+1)+"_";
        const auto* low=processor.state.getRawParameterValue(prefix+"eqLowCut");
        const auto* high=processor.state.getRawParameterValue(prefix+"eqHighCut");
        if(!low || !high || std::abs(low->load())>1.e-6f ||
           std::abs(high->load())>1.e-6f)return 250;
        for(int band=1;band<=8;++band) {
            const auto suffix=juce::String(band);
            const auto* kind=processor.state.getRawParameterValue(
                prefix+"studioEqKind"+suffix);
            const auto* enabled=processor.state.getRawParameterValue(
                prefix+"studioEqOn"+suffix);
            const float expectedKind=(float)(band==1?3:(band==7?4:0));
            const float expectedEnabled=band<=7?1.f:0.f;
            if(!kind || !enabled ||
               std::abs(kind->load()-expectedKind)>.001f ||
               std::abs(enabled->load()-expectedEnabled)>.001f)return 251;
        }
        if(slot>=stageCount) {
            const auto* enabled=processor.state.getRawParameterValue(prefix+"on");
            const auto* wet=processor.state.getRawParameterValue(prefix+"wet");
            if(processor.slotType(slot)!=SkellPresetAudioProcessor::Empty ||
               !enabled || enabled->load()>.001f ||
               !wet || std::abs(wet->load()-1.f)>.001f)return 252;
            for(int k=0;k<16;++k)
                if(std::abs(processor.getSlotParam(slot,k)-.5f)>.001f)return 253;
            for(int band=1;band<=8;++band) {
                const auto* q=processor.state.getRawParameterValue(
                    prefix+"eqQ"+juce::String(band));
                if(!q || std::abs(q->load()-.5f)>.001f)return 254;
            }
        }
    }
    return 0;
}

// Newly added instances must start without an effect or hidden lookahead.
// A restored FL Studio state must still recall its former preset/latency.
int blankSmoke() {
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    if(plugin->loadedFactoryPresetIndex()!=-1
       || plugin->presetName()!="Empty Rack"
       || plugin->activeSlotCount()!=SkellPresetAudioProcessor::defaultSlotCount
       || plugin->getNumPrograms()!=SkellPresetAudioProcessor::customRackProgramIndex+1
       || plugin->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex
       || plugin->getProgramName(SkellPresetAudioProcessor::emptyRackProgramIndex)!="Empty Rack"
       || plugin->getProgramName(SkellPresetAudioProcessor::customRackProgramIndex)!="Custom Rack"
       || plugin->getProgramName(43)!="Alchemist"
       || plugin->getProgramName(44)!="Deuce D/Max (Native Trial)"
       || plugin->getProgramName(46)!="Deuce"
       || plugin->getProgramName(54)!="Alchemist Serial Dynamics Blend (Trial)")
        return 31;
    for(int s=0;s<SkellPresetAudioProcessor::slotCount;++s) {
        const auto* active=plugin->state.getRawParameterValue(
            "s"+juce::String(s+1)+"_on");
        if(plugin->slotType(s)!=SkellPresetAudioProcessor::Empty
           || !active || active->load()>.5f)return 32;
    }
    plugin->prepareToPlay(48000.,128);
    if(plugin->getLatencySamples()!=0)return 33;
    juce::AudioBuffer<float> signal(2,128);
    juce::MidiBuffer midi;
    for(int block=0;block<8;++block) {
        for(int i=0;i<128;++i) {
            const float x=.10f*std::sin((float)(.017*(block*128+i)));
            signal.setSample(0,i,x);
            signal.setSample(1,i,-.83f*x);
        }
        juce::AudioBuffer<float> original;
        original.makeCopyOf(signal);
        plugin->processBlock(signal,midi);
        for(int ch=0;ch<2;++ch)
            for(int i=0;i<128;++i)
                if(std::abs(signal.getSample(ch,i)-original.getSample(ch,i))>1.e-6f)
                    return 34;
    }
    // Switching from a custom/imported macro state to any factory preset must
    // clear all eight serialized macro slots. The last four are currently hidden
    // from the UI/audio mixer, but remain in APVTS and .skellpreset state.
    for(int k=0;k<8;++k) {
        auto* parameter=plugin->state.getParameter("macro"+juce::String(k+1));
        if(!parameter)return 46;
        parameter->setValueNotifyingHost(.13f+.09f*(float)k);
    }
    if(!dirtySerializedSlotControls(*plugin))return 48;
    plugin->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(const int resetResult=checkNativeFactorySlotDefaults(
           *plugin,(int)skellpreset::alchemist::stages.size());resetResult!=0)
        return resetResult;
    for(int k=0;k<8;++k) {
        auto* macro=plugin->state.getRawParameterValue("macro"+juce::String(k+1));
        if(!macro || std::abs(macro->load()-.5f)>1.e-6f)return 47;
    }

    // The native Alchemist factory must export/import the clean hidden rows,
    // all eight Q values, and Studio EQ shape/enable/low/high-cut state.
    {
        const auto packageFile=juce::File::getSpecialLocation(
            juce::File::tempDirectory).getNonexistentChildFile(
                "skell-native-stage-neutral",".skellpreset",false);
        if(plugin->exportPreset(packageFile).failed())return 55;
        skellpreset::package::Document document;
        if(skellpreset::package::read(packageFile,document).failed() ||
           document.activeSlotCount!=SkellPresetAudioProcessor::defaultSlotCount)
            return 56;
        for(int slot=(int)skellpreset::alchemist::stages.size();
            slot<SkellPresetAudioProcessor::slotCount;++slot) {
            const auto& saved=document.slots[(size_t)slot];
            if(saved.slotId!=slot || saved.type!=SkellPresetAudioProcessor::Empty ||
               saved.enabled || std::abs(saved.wet-1.f)>.001f ||
               std::abs(saved.eqLowCut)>.001f || std::abs(saved.eqHighCut)>.001f)
                return 57;
            for(float value:saved.params)if(std::abs(value-.5f)>.001f)return 58;
            for(float value:saved.eqQ)if(std::abs(value-.5f)>.001f)return 59;
            for(int band=0;band<8;++band) {
                const int expectedKind=band==0?3:(band==6?4:0);
                const bool expectedOn=band<7;
                if(saved.studioEqKind[(size_t)band]!=expectedKind ||
                   saved.studioEqOn[(size_t)band]!=expectedOn)return 60;
            }
        }
        for(int band=0;band<8;++band) {
            const float expected=std::round(
                skellpreset::alchemist::stages[1].q[(size_t)band]*1000.f)/1000.f;
            if(std::abs(document.slots[1].eqQ[(size_t)band]-expected)>.001f)
                return 61;
        }

        auto imported=std::make_unique<SkellPresetAudioProcessor>();
        if(imported->importPreset(packageFile).failed() ||
           imported->loadedFactoryPresetIndex()!=-1 ||
           checkNativeFactorySlotDefaults(*imported,
               (int)skellpreset::alchemist::stages.size())!=0)
            return 62;
        imported->releaseResources();
        packageFile.deleteFile();
    }
    // Host-state save/reload must retain the same neutral hidden slots.
    {
        juce::MemoryBlock nativeState;
        plugin->getStateInformation(nativeState);
        auto nativeRestored=std::make_unique<SkellPresetAudioProcessor>();
        nativeRestored->setStateInformation(nativeState.getData(),
                                             (int)nativeState.getSize());
        nativeRestored->prepareToPlay(48000.,128);
        if(nativeRestored->loadedFactoryPresetIndex()!=
               skellpreset::alchemist::factoryIndex ||
           checkNativeFactorySlotDefaults(*nativeRestored,
               (int)skellpreset::alchemist::stages.size())!=0)
            return 63;
        nativeRestored->releaseResources();
    }

    // Test actual state save/restore, not just loading a factory preset.
    plugin->loadFactoryPreset(0); // published legacy limiter chain
    if(plugin->slotType(6)!=SkellPresetAudioProcessor::Limiter)return 35;
    juce::MemoryBlock save;
    plugin->getStateInformation(save);
    auto restored=std::make_unique<SkellPresetAudioProcessor>();
    if(restored->loadedFactoryPresetIndex()!=-1)return 36;
    restored->setStateInformation(save.getData(),(int)save.getSize());
    restored->prepareToPlay(48000.,128);
    if(restored->loadedFactoryPresetIndex()!=0
        || restored->slotType(6)!=SkellPresetAudioProcessor::Limiter
        || restored->getLatencySamples()<=0)return 37;

    // A pre-identity host state must not be mislabelled as factory 0.
    // XML string values are accepted only when they are canonical integers;
    // malformed and out-of-range values remain custom/unselected.
    auto savedXml=juce::AudioProcessor::getXmlFromBinary(
        save.getData(),(int)save.getSize());
    if(!savedXml)return 38;
    const auto savedTree=juce::ValueTree::fromXml(*savedXml);
    if(!savedTree.isValid())return 39;
    const auto checkProgramIdentity=[&](bool includeProgram,
                                        const juce::var& programValue,
                                        int expected)->bool {
        auto document=savedTree.createCopy();
        if(includeProgram)document.setProperty("program",programValue,nullptr);
        else document.removeProperty("program",nullptr);
        auto xml=document.createXml();
        if(!xml)return false;
        juce::MemoryBlock encoded;
        juce::AudioProcessor::copyXmlToBinary(*xml,encoded);
        auto candidate=std::make_unique<SkellPresetAudioProcessor>();
        candidate->setStateInformation(encoded.getData(),(int)encoded.getSize());
        const int expectedHost=expected>=0?expected:SkellPresetAudioProcessor::customRackProgramIndex;
        return candidate->loadedFactoryPresetIndex()==expected
            && candidate->getCurrentProgram()==expectedHost;
    };
    if(!checkProgramIdentity(false,juce::var(),-1))return 40;
    if(!checkProgramIdentity(true,juce::var("not-a-number"),-1))return 41;
    if(!checkProgramIdentity(true,juce::var(99),-1))return 42;
    if(!checkProgramIdentity(true,juce::var("0"),0))return 43;
    if(!checkProgramIdentity(true,juce::var(-1),-1))return 44;
    if(!checkProgramIdentity(true,juce::var(0.5),-1))return 45;
    if(!checkProgramIdentity(true,juce::var(-2),-1))return 49;

    auto modeOnly=std::make_unique<SkellPresetAudioProcessor>();
    auto* modeParameter=modeOnly->state.getParameter("mode");
    if(!modeParameter)return 72;
    modeParameter->setValueNotifyingHost(modeParameter->convertTo0to1(0.f));
    if(modeOnly->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex)
        return 73;

    auto custom=std::make_unique<SkellPresetAudioProcessor>();
    auto* macro=custom->state.getParameter("macro1");
    if(!macro)return 64;
    macro->setValueNotifyingHost(.73f);
    if(custom->getCurrentProgram()!=SkellPresetAudioProcessor::customRackProgramIndex
       || custom->presetName()!="Custom Rack")return 65;
    juce::MemoryBlock customState;
    custom->getStateInformation(customState);
    auto customRestored=std::make_unique<SkellPresetAudioProcessor>();
    customRestored->setStateInformation(customState.getData(),(int)customState.getSize());
    if(customRestored->getCurrentProgram()!=SkellPresetAudioProcessor::customRackProgramIndex
       || customRestored->loadedFactoryPresetIndex()!=-1)return 66;

    auto legacyEmpty=std::make_unique<SkellPresetAudioProcessor>();
    juce::MemoryBlock legacyEmptyState;
    legacyEmpty->getStateInformation(legacyEmptyState);
    auto legacyXml=juce::AudioProcessor::getXmlFromBinary(
        legacyEmptyState.getData(),(int)legacyEmptyState.getSize());
    if(!legacyXml)return 67;
    auto legacyTree=juce::ValueTree::fromXml(*legacyXml);
    legacyTree.setProperty("program",-1,nullptr);
    legacyTree.setProperty("presetName","Empty Rack",nullptr);
    auto legacyEncodedXml=legacyTree.createXml();
    if(!legacyEncodedXml)return 68;
    juce::MemoryBlock legacyEncoded;
    juce::AudioProcessor::copyXmlToBinary(*legacyEncodedXml,legacyEncoded);
    auto legacyEmptyRestored=std::make_unique<SkellPresetAudioProcessor>();
    legacyEmptyRestored->setStateInformation(legacyEncoded.getData(),(int)legacyEncoded.getSize());
    if(legacyEmptyRestored->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex
       || legacyEmptyRestored->presetName()!="Empty Rack")return 69;

    customRestored->setCurrentProgram(SkellPresetAudioProcessor::emptyRackProgramIndex);
    if(customRestored->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex
       || customRestored->presetName()!="Empty Rack")return 70;
    for(auto* parameter:customRestored->getParameters())
        if(parameter && std::abs(parameter->getValue()-parameter->getDefaultValue())>1.e-5f)
            return 71;

    // Host XML is untrusted. Verify NaN/Inf/malformed values are repaired
    // before APVTS atomics update; finite outliers clamp and discrete controls
    // snap legally while valid in-range automation retains its value.
    {
        const auto fail=[](int code,const char* reason) {
            std::fprintf(stderr,"FAIL: host-state sanitizer %s (%d)\\n",reason,code);
            return code;
        };
        auto corruptSource=std::make_unique<SkellPresetAudioProcessor>();
        corruptSource->loadFactoryPreset(0);
        juce::MemoryBlock baseline;
        corruptSource->getStateInformation(baseline);
        auto corruptXml=juce::AudioProcessor::getXmlFromBinary(
            baseline.getData(),(int)baseline.getSize());
        if(!corruptXml)return fail(160,"baseline XML missing");
        auto corruptTree=juce::ValueTree::fromXml(*corruptXml);
        if(!corruptTree.isValid())return fail(161,"baseline ValueTree invalid");
        const auto setParameterText=[&](const juce::String& id,
                                        const juce::String& value)->bool {
            for(int i=0;i<corruptTree.getNumChildren();++i) {
                auto child=corruptTree.getChild(i);
                if(child.getProperty("id").toString()==id) {
                    child.setProperty("value",value,nullptr);
                    return true;
                }
            }
            return false;
        };
        if(!setParameterText("input","nan") ||
           !setParameterText("mix","inf") ||
           !setParameterText("output","-inf") ||
           !setParameterText("macro1","not-a-number") ||
           !setParameterText("macro2","0.371") ||
           !setParameterText("s1_wet","999") ||
           !setParameterText("s1_p1","-999") ||
           !setParameterText("s1_p2","0.371") ||
           !setParameterText("s1_eqQ1","nan") ||
           !setParameterText("s1_eqQ2","0.371") ||
           !setParameterText("mode","2.4") ||
           !setParameterText("s1_type","999") ||
           !setParameterText("s2_type","not-a-number"))
            return fail(162,"test parameter ID missing");
        auto corruptedXml=corruptTree.createXml();
        if(!corruptedXml)return fail(163,"corrupt XML missing");
        juce::MemoryBlock corruptedState;
        juce::AudioProcessor::copyXmlToBinary(*corruptedXml,corruptedState);

        auto sanitized=std::make_unique<SkellPresetAudioProcessor>();
        sanitized->setStateInformation(corruptedState.getData(),
                                        (int)corruptedState.getSize());
        const auto expectedDefault=[](SkellPresetAudioProcessor& processor,
                                      const juce::String& id)->float {
            auto* parameter=processor.state.getParameter(id);
            if(!parameter)return std::numeric_limits<float>::quiet_NaN();
            const auto& range=parameter->getNormalisableRange();
            float expected=range.convertFrom0to1(parameter->getDefaultValue());
            if(!std::isfinite(expected))expected=range.start;
            expected=juce::jlimit(range.start,range.end,expected);
            if(parameter->isDiscrete())expected=range.snapToLegalValue(expected);
            return expected;
        };
        const auto matchesDefault=[&](SkellPresetAudioProcessor& processor,
                                      const juce::String& id)->bool {
            const auto* raw=processor.state.getRawParameterValue(id);
            const float expected=expectedDefault(processor,id);
            return raw && std::isfinite(expected) && std::isfinite(raw->load()) &&
                   std::abs(raw->load()-expected)<1.e-4f;
        };
        const auto matchesLegal=[](SkellPresetAudioProcessor& processor,
                                   const juce::String& id,float requested)->bool {
            auto* parameter=processor.state.getParameter(id);
            const auto* raw=processor.state.getRawParameterValue(id);
            if(!parameter||!raw)return false;
            const auto& range=parameter->getNormalisableRange();
            const float bounded=juce::jlimit(range.start,range.end,requested);
            const float expected=parameter->isDiscrete()
                ? range.snapToLegalValue(bounded) : bounded;
            return std::isfinite(raw->load()) &&
                   std::abs(raw->load()-expected)<1.e-4f;
        };
        const auto matchesValue=[](SkellPresetAudioProcessor& processor,
                                   const juce::String& id,float expected)->bool {
            const auto* raw=processor.state.getRawParameterValue(id);
            return raw && std::isfinite(raw->load()) &&
                   std::abs(raw->load()-expected)<1.e-4f;
        };
        const auto allFiniteAndInRange=[](SkellPresetAudioProcessor& processor)->bool {
            for(int i=0;i<processor.state.state.getNumChildren();++i) {
                const auto child=processor.state.state.getChild(i);
                const auto id=child.getProperty("id").toString();
                auto* parameter=processor.state.getParameter(id);
                const auto* raw=processor.state.getRawParameterValue(id);
                if(!parameter||!raw)continue;
                const auto& range=parameter->getNormalisableRange();
                const float value=raw->load();
                if(!std::isfinite(value)||value<range.start-1.e-4f||
                   value>range.end+1.e-4f)return false;
            }
            return true;
        };

        if(sanitized->loadedFactoryPresetIndex()!=0 ||
           sanitized->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
           sanitized->activeSlotCount()!=SkellPresetAudioProcessor::defaultSlotCount)
            return fail(164,"factory identity/rack state changed");
        if(!matchesDefault(*sanitized,"input") ||
           !matchesDefault(*sanitized,"mix") ||
           !matchesDefault(*sanitized,"output") ||
           !matchesDefault(*sanitized,"macro1") ||
           !matchesDefault(*sanitized,"s1_eqQ1") ||
           !matchesDefault(*sanitized,"s2_type"))
            return fail(165,"nonfinite or malformed values did not use parameter defaults");
        if(!matchesLegal(*sanitized,"s1_wet",999.f) ||
           !matchesLegal(*sanitized,"s1_p1",-999.f) ||
           !matchesLegal(*sanitized,"mode",2.4f) ||
           !matchesLegal(*sanitized,"s1_type",999.f) ||
           !matchesValue(*sanitized,"macro2",.371f) ||
           !matchesValue(*sanitized,"s1_p2",.371f) ||
           !matchesValue(*sanitized,"s1_eqQ2",.371f) ||
           !allFiniteAndInRange(*sanitized))
            return fail(166,"clamp, discrete snapping or valid automation mismatch");

        juce::MemoryBlock repairedState;
        sanitized->getStateInformation(repairedState);
        auto roundtrip=std::make_unique<SkellPresetAudioProcessor>();
        roundtrip->setStateInformation(repairedState.getData(),
                                        (int)repairedState.getSize());
        if(!allFiniteAndInRange(*roundtrip) ||
           !matchesDefault(*roundtrip,"input") ||
           !matchesDefault(*roundtrip,"mix") ||
           !matchesDefault(*roundtrip,"output") ||
           !matchesDefault(*roundtrip,"macro1") ||
           !matchesDefault(*roundtrip,"s1_eqQ1") ||
           !matchesValue(*roundtrip,"s1_p2",.371f) ||
           !matchesValue(*roundtrip,"s1_eqQ2",.371f))
            return fail(167,"repaired save/reload mismatch");

        roundtrip->prepareToPlay(44100.,blockSize);
        juce::AudioBuffer<float> audio(2,blockSize);
        juce::MidiBuffer midi;
        for(int n=0;n<blockSize;++n) {
            const float sample=.10f*std::sin(
                (float)(2.*juce::MathConstants<double>::pi*211.*n/44100.));
            audio.setSample(0,n,sample);
            audio.setSample(1,n,-.83f*sample);
        }
        roundtrip->processBlock(audio,midi);
        float peak=0.f;
        for(int channel=0;channel<2;++channel)
            for(int n=0;n<blockSize;++n) {
                const float sample=audio.getSample(channel,n);
                if(!std::isfinite(sample))return fail(168,"nonfinite DSP output");
                peak=std::max(peak,std::abs(sample));
            }
        if(peak>8.f)return fail(169,"excessive DSP peak");
        roundtrip->releaseResources();
        std::puts("PASS: corrupt host parameters default/clamp; legal automation and DSP remain finite");
    }
    restored->releaseResources();
    plugin->releaseResources();
    std::puts("PASS: blank rack 0-PDC identity and legacy FL Studio preset-state restore");
    return 0;
}

// Saved state with corrupt slotOrder must not overwrite current rack order.
// Prior getIntValue() accepted "oops" as slot ID 0; preserve valid legacy 10
// and full 16-position permutations without touching a published parameter.
int strictSlotOrderSmoke(){
    auto owner=std::make_unique<SkellPresetAudioProcessor>();
    juce::MemoryBlock baseline;
    owner->getStateInformation(baseline);
    auto xml=juce::AudioProcessor::getXmlFromBinary(baseline.getData(),(int)baseline.getSize());
    if(!xml)return 81;
    const auto parsed=juce::ValueTree::fromXml(*xml);
    if(!parsed.isValid())return 82;
    const auto verify=[&](const juce::String& order,bool valid)->bool{
        auto document=parsed.createCopy();
        document.setProperty("slotOrder",order,nullptr);
        auto treeXml=document.createXml();
        if(!treeXml)return false;
        juce::MemoryBlock stateBytes;
        juce::AudioProcessor::copyXmlToBinary(*treeXml,stateBytes);
        auto processor=std::make_unique<SkellPresetAudioProcessor>();
        processor->reorderSlot(0,3); // [1,2,3,0,4,...], not the identity.
        if(processor->slotIdAt(0)!=1||processor->slotIdAt(3)!=0)return false;
        processor->setStateInformation(stateBytes.getData(),(int)stateBytes.getSize());
        if(!valid)return processor->slotIdAt(0)==1 &&
                         processor->slotIdAt(1)==2 &&
                         processor->slotIdAt(2)==3 &&
                         processor->slotIdAt(3)==0;
        for(int position=0;position<16;++position)
            if(processor->slotIdAt(position)!=position)return false;
        return true;
    };
    juce::StringArray natural,ten;
    for(int i=0;i<16;++i){
        natural.add(juce::String(i));
        if(i<10)ten.add(juce::String(i));
    }
    juce::StringArray malformed=natural;
    malformed.set(0,"oops"); // used to coerce to valid ID=0
    if(!verify(malformed.joinIntoString(","),false))return 83;
    malformed.set(0,"-0");
    if(!verify(malformed.joinIntoString(","),false))return 84;
    if(!verify(natural.joinIntoString(",")+",",false))return 85;
    if(!verify(","+natural.joinIntoString(","),false))return 86;
    if(!verify(natural.joinIntoString(",,"),false))return 87;
    if(!verify(natural.joinIntoString(","),true))return 88;
    if(!verify(ten.joinIntoString(","),true))return 89;
    std::puts("PASS: strict 10/16 slot-order state recall; malformed order unchanged");
    return 0;
}

// Windows desktop UI interaction/layout smoke; does NOT capture screenshots,
// trigger the screenshot workflow, or open FL Studio. Uses the real editor.
int uiSmoke() {
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->prepareToPlay(48000.,256);
    std::unique_ptr<SkellPresetAudioProcessorEditor> editor(
        static_cast<SkellPresetAudioProcessorEditor*>(plugin->createEditor()));
    if(!editor)return 51;
    auto report=[](const juce::StringArray& issues,const char* stage) {
        for(const auto& issue:issues)
            std::fprintf(stderr,"UI %s: %s\n",stage,issue.toRawUTF8());
    };
    const auto before=editor->getScreenshotLayoutIssues();
    report(before,"initial-layout");
    if(!before.isEmpty())return 52;
    const auto actions=editor->runScreenshotInteractionTests();
    report(actions,"interactions");
    if(!actions.isEmpty())return 53;
    const auto after=editor->getScreenshotLayoutIssues();
    report(after,"final-layout");
    if(!after.isEmpty())return 54;
    editor.reset();
    plugin->releaseResources();
    std::puts("PASS: real JUCE editor geometry, rack wheel, MIC, and picker controls; no screenshot captures");
    return 0;
}

int smoke() {
    // Large rack engines must be allocated on the heap: Windows main thread
    // defaults to a substantially smaller stack than the Linux renderer.
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if (plugin->loadedFactoryPresetIndex() != skellpreset::alchemist::factoryIndex
        || plugin->slotType(6) != SkellPresetAudioProcessor::AlchemistMatchFinishFX
        || plugin->activeSlotCount() < 7) {
        std::fprintf(stderr,"ERROR: native Alchemist factory chain did not load\n");
        return 2;
    }
    plugin->prepareToPlay(44100.,blockSize);
    // This seven-stage factory has no limiter, spectral pitch, or hosted
    // processor; Match Finish itself must not introduce host PDC.
    if(plugin->getLatencySamples()!=0) {
        std::fprintf(stderr,"ERROR: Alchemist unexpectedly reports %d latency samples\\n",
                     plugin->getLatencySamples());
        return 6;
    }
    double peak=0.,energy=0.,difference=0.;
    std::int64_t count=0;
    for (int blockIndex=0;blockIndex<100;++blockIndex) {
        juce::AudioBuffer<float> audio(2,blockSize);
        for (int n=0;n<blockSize;++n)
            for (int ch=0;ch<2;++ch) {
                const double t=(blockIndex*blockSize+n)/44100.;
                audio.setSample(ch,n,static_cast<float>(
                    .075*std::sin(2.*juce::MathConstants<double>::pi*211.*t)
                    +.012*std::sin(2.*juce::MathConstants<double>::pi*4900.*t)));
            }
        const float initial=audio.getSample(0,blockSize/2);
        if(process(audio,*plugin,peak,energy,count)!=0) return 3;
        if(blockIndex>5)
            difference+=std::abs(audio.getSample(0,blockSize/2)-initial);
    }
    plugin->releaseResources();
    if (!std::isfinite(peak) || peak > 8. || energy < 1.e-5 ||
        difference < 1.e-4 || count != 100*blockSize*2) {
        std::fprintf(stderr,"ERROR: native output out of bounds or unchanged\n");
        return 4;
    }
    std::printf("PASS: compiled seven-stage Alchemist native processing; peak %.5f RMS %.5f\n",
                peak,std::sqrt(energy / count));
    return 0;
}

// Load all four append-only Studio effects in the actual JUCE processor;
// measure host-facing PDC and exercise the real Windows rack signal path.
int percentDisplaySmoke(){
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    const auto check=[&](const juce::String& id,float physical,
                         const juce::String& expected){
        auto* parameter=processor->state.getParameter(id);
        if(!parameter)return false;
        const auto shown=parameter->getText(parameter->convertTo0to1(physical),64);
        if(shown!=expected) {
            std::fprintf(stderr,"Host %s displayed '%s', expected '%s'\n",
                         id.toRawUTF8(),shown.toRawUTF8(),expected.toRawUTF8());
            return false;
        }
        return parameter->getLabel().isEmpty(); // no extra '%' appended
    };
    if(!check("input",-24.f,"0%") ||
       !check("input",0.f,"50%") ||
       !check("input",24.f,"100%") ||
       !check("output",0.f,"50%") ||
       !check("mix",1.f,"100%") ||
       !check("macro1",.37f,"37%") ||
       !check("s1_wet",.82f,"82%"))
        return 92;
    std::puts("PASS: host Input/Mix/Output, macros and rack wet map to 0-100% text");
    return 0;
}

int studioSmoke() {
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    processor->prepareToPlay(48000.,257);
    const int types[]{
        SkellPresetAudioProcessor::StudioEQFX,
        SkellPresetAudioProcessor::StudioLimiterFX,
        SkellPresetAudioProcessor::StudioCompressorFX,
        SkellPresetAudioProcessor::StudioFastDistFX
    };
    for(const int type:types){
        processor->setSlotOn(0,false);
        processor->setSlotType(0,type);
        processor->setSlotOn(0,true);
        // Set audible, deterministic non-default parameters.
        if(type==SkellPresetAudioProcessor::StudioEQFX) {
            processor->setSlotParam(0,0,.8f);
            processor->setSlotParam(0,1,.9f);
        }else if(type==SkellPresetAudioProcessor::StudioCompressorFX) {
            processor->setSlotParam(0,0,.35f);
            processor->setSlotParam(0,1,.85f);
        }else if(type==SkellPresetAudioProcessor::StudioFastDistFX) {
            processor->setSlotParam(0,0,.7f);
            processor->setSlotParam(0,3,.6f);
        }else{
            processor->setSlotParam(0,0,.89f);
            processor->setSlotParam(0,1,.1f);
            processor->setSlotParam(0,2,.35f);
            processor->setSlotParam(0,3,.85f);
        }
        const int latency=processor->getLatencySamples();
        if((type==SkellPresetAudioProcessor::StudioLimiterFX &&
            (latency<48||latency>1024)) ||
           (type!=SkellPresetAudioProcessor::StudioLimiterFX && latency!=0)) {
            std::fprintf(stderr,"Studio effect %d PDC invalid: %d\n",type,latency);
            return 72;
        }
        double peak=0.,difference=0.,energy=0.;
        for(int block=0;block<75;++block){
            juce::AudioBuffer<float> audio(2,257);
            for(int n=0;n<257;++n){
                const double t=(block*257+n)/48000.;
                const float input=(float)(.34*std::sin(2.*juce::MathConstants<double>::pi*713.*t)
                                 +.22*std::sin(2.*juce::MathConstants<double>::pi*4300.*t));
                audio.setSample(0,n,input);
                audio.setSample(1,n,input*.73f);
            }
            juce::MidiBuffer midi;
            processor->processBlock(audio,midi);
            for(int ch=0;ch<2;++ch)for(int n=0;n<257;++n){
                const float out=audio.getSample(ch,n);
                if(!std::isfinite(out))return 73;
                peak=std::max(peak,(double)std::abs(out));
                energy+=(double)out*out;
                if(block>20){
                    const double t=(block*257+n)/48000.;
                    const float input=(float)(.34*std::sin(2.*juce::MathConstants<double>::pi*713.*t)
                                         +.22*std::sin(2.*juce::MathConstants<double>::pi*4300.*t));
                    difference+=std::abs((double)out-(double)(ch==0?input:input*.73f));
                }
            }
        }
        // The deliberately extreme +14.4 dB Studio EQ smoke setting can exceed
        // 0 dBFS in floating point; bound it separately from the other Studio
        // effects so a runaway gain regression cannot hide behind the old 8x cap.
        const double peakLimit=type==SkellPresetAudioProcessor::StudioEQFX?3.25:1.25;
        if(peak>peakLimit||energy<.0001||difference<.05){
            std::fprintf(stderr,"Studio effect %d audio validation failed: peak %.5f (limit %.2f), energy %.5f, change %.5f\n",
                         type,peak,peakLimit,energy,difference);
            return 74;
        }
        juce::MemoryBlock state;
        processor->getStateInformation(state);
        auto restored=std::make_unique<SkellPresetAudioProcessor>();
        restored->setStateInformation(state.getData(),(int)state.getSize());
        if(restored->slotType(0)!=type){
            std::fprintf(stderr,"Studio effect %d FL project state recall failed\n",type);
            return 75;
        }
        std::printf("PASS: Studio ID %d, PDC %d, peak %.4f, change %.4f, state recall\n",
                    type,latency,peak,difference);
    }
    processor->releaseResources();
    return 0;
}

// End-to-end native processor workload: not a simulated individual effect.
// Measures serialized callbacks for several plugin instances sharing one audio
// device thread. Hosted CI machine timings are NOT FL Studio CPU results.
int stress() {
    const int instances[]{5,10,20};
    const int buffers[]{64,128,256};
    const double rates[]{44100.,48000.,96000.};
    for(const double sr:rates) {
        // Check actual host-facing processor PDC, not a helper's estimate.
        for(const int preset:{skellpreset::alchemist::factoryIndex,
                                skellpreset::alchemist_serial_dynamics_trial::factoryIndex,
                                skellpreset::alchemist_low_end_contour_trial::factoryIndex,
                                skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex,25,0}) {
            auto p=std::make_unique<SkellPresetAudioProcessor>();
            p->loadFactoryPreset(preset);
            p->prepareToPlay(sr,64);
            const int expected=(preset==0 || preset==skellpreset::alchemist_serial_dynamics_trial::factoryIndex ||
                                preset==skellpreset::alchemist_low_end_contour_trial::factoryIndex ||
                                preset==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex)
                ?std::max(48,(int)(sr*.003)):0;
            if(p->getLatencySamples()!=expected) {
                std::fprintf(stderr,
                    "FAIL factory %d / %.0f Hz: reported=%d expected=%d samples\n",
                    preset,sr,p->getLatencySamples(),expected);
                return 9;
            }
            p->releaseResources();
        }
        for(const int rackPreset:{skellpreset::alchemist::factoryIndex,
                                  skellpreset::alchemist_serial_dynamics_trial::factoryIndex,
                                  skellpreset::alchemist_low_end_contour_trial::factoryIndex,
                                  skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex}) {
          for(const int frames:buffers) {
            for(const int count:instances) {
                // Construct/prepare and allocate audio buffers outside timing.
                std::vector<std::unique_ptr<SkellPresetAudioProcessor>> plugins;
                std::vector<std::unique_ptr<juce::AudioBuffer<float>>> work;
                plugins.reserve((size_t)count);
                work.reserve((size_t)count);
                for(int n=0;n<count;++n) {
                    auto p=std::make_unique<SkellPresetAudioProcessor>();
                    p->loadFactoryPreset(rackPreset);
                    p->prepareToPlay(sr,frames);
                    const int expectedLatency=(rackPreset==skellpreset::alchemist_serial_dynamics_trial::factoryIndex ||
                                               rackPreset==skellpreset::alchemist_low_end_contour_trial::factoryIndex ||
                                               rackPreset==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex)
                        ?std::max(48,(int)(sr*.003)):0;
                    if(p->getLatencySamples()!=expectedLatency)return 10;
                    plugins.push_back(std::move(p));
                    work.push_back(std::make_unique<juce::AudioBuffer<float>>(2,frames));
                }
                juce::AudioBuffer<float> source(2,frames);
                juce::MidiBuffer midi;
                const int blocks=std::max(1,(int)(sr*.20/frames));
                double peak=0.,energy=0.,dspSeconds=0.,worstCallback=0.;
                for(int k=0;k<blocks;++k) {
                    // Generate once per device callback, outside DSP timing.
                    // Source contains vocals, upper harmonics, transients,
                    // silence windows and deliberately invalid input.
                    for(int i=0;i<frames;++i) {
                        const auto n=(std::int64_t)k*frames+i;
                        const double t=(double)n/sr;
                        const float x=(float)(
                            .12*std::sin(2.*juce::MathConstants<double>::pi*207.*t)
                            +.035*std::sin(2.*juce::MathConstants<double>::pi*6100.*t)
                            +((n%1223)==0?.21:0.));
                        const float y=(n%7117)==0
                            ?std::numeric_limits<float>::quiet_NaN()
                            :(n%9001)==0?std::numeric_limits<float>::infinity():x;
                        source.setSample(0,i,y);
                        source.setSample(1,i,((n/500)%3)==0?-y:y);
                    }
                    for(int inst=0;inst<count;++inst) {
                        auto& dst=*work[(size_t)inst];
                        dst.copyFrom(0,0,source,0,0,frames);
                        dst.copyFrom(1,0,source,1,0,frames);
                    }
                    // Measure ONLY processBlock() calls, not test-signal
                    // synthesis, buffer copies, assertions or metrics.
                    const auto callbackStart=std::chrono::steady_clock::now();
                    for(int inst=0;inst<count;++inst)
                        plugins[(size_t)inst]->processBlock(*work[(size_t)inst],midi);
                    const double elapsed=std::chrono::duration<double>(
                        std::chrono::steady_clock::now()-callbackStart).count();
                    dspSeconds+=elapsed;
                    worstCallback=std::max(worstCallback,elapsed);
                    for(int inst=0;inst<count;++inst) {
                        const auto& dst=*work[(size_t)inst];
                        for(int i=0;i<frames;++i) {
                            for(int ch=0;ch<2;++ch) {
                                const double value=dst.getSample(ch,i);
                                if(!std::isfinite(value) || std::abs(value)>32.) {
                                    std::fprintf(stderr,
                                        "FAIL nonfinite/unbounded count=%d rate=%.0f block=%d sample=%d\n",
                                        count,sr,frames,i);
                                    return 11;
                                }
                                peak=std::max(peak,std::abs(value));
                                energy+=value*value;
                            }
                        }
                    }
                }
                const double audioSeconds=(double)(blocks*frames)/sr;
                const double callbackBudget=(double)frames/sr;
                std::printf("REAL-RACK preset=%d instances=%d rate=%.0f block=%d audio_s=%.3f dsp_s=%.3f "
                            "dsp_cpu_vs_realtime=%.3f worst_callback_vs_budget=%.3f "
                            "peak=%.4f RMS=%.4f\n",
                            rackPreset,count,sr,frames,audioSeconds,dspSeconds,
                            dspSeconds/audioSeconds,worstCallback/callbackBudget,peak,
                            std::sqrt(energy/(2.*blocks*frames*count)));
                if(!std::isfinite(energy) || energy<=0. || peak<=0.)return 12;
                for(auto& p:plugins)p->releaseResources();
            }
          }
        }
    }
    std::puts("PASS compiled 5/10/20-instance Alchemist baseline + serial-trial variant finite-output and latency stress");
    return 0;
}

int render(const juce::File& source, const juce::File& destination,
           int factoryIndex=skellpreset::alchemist::factoryIndex,
           float highpassNormalizedOverride=-1.f,
           float band70GainNormalizedOverride=-1.f) {
    if (!source.existsAsFile() || source == destination) {
        std::fprintf(stderr,"ERROR: source missing or output equals input\n");
        return 2;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (!reader || (reader->numChannels != 1 && reader->numChannels != 2)
        || reader->lengthInSamples < 1
        || reader->sampleRate < 8000. || reader->sampleRate > 192000.) {
        std::fprintf(stderr,"ERROR: source must be mono or stereo WAV (8k-192kHz)\n");
        return 3;
    }
    if(destination.existsAsFile()) {
        std::fprintf(stderr,"ERROR: refusing to overwrite existing output\n");
        return 4;
    }
    std::unique_ptr<juce::FileOutputStream> fileStream(destination.createOutputStream());
    if (!fileStream || !fileStream->openedOk()) {
        std::fprintf(stderr,"ERROR: cannot create destination WAV\n");
        return 5;
    }
    std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriter::Options{}
        .withSampleRate(reader->sampleRate).withNumChannels(2)
        .withBitsPerSample(32)
        // IEEE float WAV preserves >0 dBFS samples for meaningful clipping
        // and intersample-peak diagnostics. PCM32 would silently clip them.
        .withSampleFormat(juce::AudioFormatWriter::Options::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor(stream,options);
    if(!writer) {
        destination.deleteFile();
        std::fprintf(stderr,"ERROR: cannot prepare output WAV writer\n");
        return 6;
    }
    // Large rack engines must be allocated on the heap: Windows main thread
    // defaults to a substantially smaller stack than the Linux renderer.
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    if(factoryIndex!=skellpreset::alchemist::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_trial::factoryIndex &&
       factoryIndex!=skellpreset::deuce::factoryIndex &&
       factoryIndex!=skellpreset::deuce_v25::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_peak_guard_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_tone_match_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_lighter_dynamics_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_serial_dynamics_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_low_end_contour_trial::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex) {
        std::fprintf(stderr,"ERROR: factory selection must be 43 through 54\n");
        return 7;
    }
    plugin->loadFactoryPreset(factoryIndex);
    const bool hasLowEndOverride=highpassNormalizedOverride>=0.f ||
                                 band70GainNormalizedOverride>=0.f;
    if(hasLowEndOverride) {
        if(factoryIndex!=skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex ||
           !std::isfinite(highpassNormalizedOverride) ||
           !std::isfinite(band70GainNormalizedOverride) ||
           highpassNormalizedOverride<0.f || highpassNormalizedOverride>1.f ||
           band70GainNormalizedOverride<0.f || band70GainNormalizedOverride>1.f) {
            std::fprintf(stderr,"ERROR: low-end overrides require Factory 54 and normalized values in [0,1]\n");
            return 7;
        }
        // Diagnostic-only overrides: slot 0 classic EQ HPF and slot 1 70 Hz
        // bell. They never modify a factory definition or saved preset bank.
        plugin->setSlotParam(0,3,highpassNormalizedOverride);
        plugin->setSlotParam(1,1,band70GainNormalizedOverride);
        if(std::abs(plugin->getSlotParam(0,3)-highpassNormalizedOverride)>.0011f ||
           std::abs(plugin->getSlotParam(1,1)-band70GainNormalizedOverride)>.0011f) {
            std::fprintf(stderr,"ERROR: low-end override did not reach APVTS state\n");
            return 7;
        }
    }
    const bool deuceFactory=factoryIndex==skellpreset::deuce::factoryIndex ||
                             factoryIndex==skellpreset::deuce_v25::factoryIndex;
    const bool deuceCountValid=factoryIndex==skellpreset::deuce::factoryIndex
        ?plugin->activeSlotCount()==12
        :plugin->activeSlotCount()==13;
    const bool serialDynamicsFactory=factoryIndex==skellpreset::alchemist_serial_dynamics_trial::factoryIndex ||
                                     factoryIndex==skellpreset::alchemist_low_end_contour_trial::factoryIndex ||
                                     factoryIndex==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex;
    const bool peakGuardFactory=factoryIndex==skellpreset::alchemist_peak_guard_trial::factoryIndex ||
                                 factoryIndex==skellpreset::alchemist_tone_match_trial::factoryIndex ||
                                 factoryIndex==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex ||
                                 factoryIndex==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex ||
                                 factoryIndex==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex;
    const bool alchemistChainValid=serialDynamicsFactory
        ?plugin->slotType(5)==SkellPresetAudioProcessor::AlchemistMatchFinishFX &&
         plugin->slotType(6)==SkellPresetAudioProcessor::Limiter
        :plugin->slotType(6)==SkellPresetAudioProcessor::AlchemistMatchFinishFX &&
         (peakGuardFactory
            ?plugin->slotType(7)==SkellPresetAudioProcessor::Limiter
            :plugin->slotType(7)==SkellPresetAudioProcessor::Empty);
    if(plugin->loadedFactoryPresetIndex()!=factoryIndex ||
       (deuceFactory ? !deuceCountValid : !alchemistChainValid)) {
        std::fprintf(stderr,"ERROR: incorrect native factory rack loaded\n");
        return 7;
    }
    plugin->prepareToPlay(reader->sampleRate,blockSize);
    juce::AudioBuffer<float> audio(2,blockSize);
    double peak=0.,energy=0.;
    std::int64_t total=0;
    bool ok=true;
    for(juce::int64 pos=0;pos<reader->lengthInSamples;pos+=blockSize) {
        const auto count=static_cast<int>(
            std::min(static_cast<juce::int64>(blockSize),
                     reader->lengthInSamples-pos));
        audio.setSize(2,count,false,false,true);
        audio.clear();
        const bool stereoInput=reader->numChannels==2;
        if(!reader->read(&audio,0,count,pos,true,stereoInput)) {ok=false;break;}
        // A mono vocal is represented by identical L/R signals throughout
        // the stereo native chain. Never leave the second channel stale.
        if(!stereoInput)audio.copyFrom(1,0,audio,0,0,count);
        if(process(audio,*plugin,peak,energy,total)!=0) {ok=false;break;}
        if(!writer->writeFromAudioSampleBuffer(audio,0,count)) {ok=false;break;}
    }
    plugin->releaseResources();
    writer.reset(); // finalize RIFF size and flush outside the audio thread
    if(!ok || total<=0) {
        destination.deleteFile();
        std::fprintf(stderr,"ERROR: native render aborted; partial WAV removed\n");
        return 8;
    }
    if(hasLowEndOverride) {
        std::printf("Diagnostic override: HPF %.1f Hz, 70 Hz bell %.2f dB\n",
                    20.f+250.f*highpassNormalizedOverride,
                    30.f*(band70GainNormalizedOverride-.5f));
    }
    std::printf("Rendered native factory %d: %lld samples, %.0f Hz, peak %.5f, RMS %.5f\n",
                factoryIndex,
                static_cast<long long>(total/2),reader->sampleRate,
                peak,std::sqrt(energy/total));
    std::puts("NOTE: this uses the plugin's compiled processBlock, not a hosted VST3. A/B and FL Studio checks are separate.");
    return 0;
}
// Compare the first 15 seconds of sample-aligned same-take stereo (or mono)
// WAVs. No hidden time alignment, resampling, signal modification or LUFS
// claims. Use with Alchemist render (candidate) and FL Studio wet print (ref).
int compareWavs(const juce::File& candidateFile,const juce::File& referenceFile) {
    if(candidateFile.getFileExtension().toLowerCase()!=".wav"
        || referenceFile.getFileExtension().toLowerCase()!=".wav") {
        std::fprintf(stderr,"ERROR: --compare expects two WAV files\n");
        return 23;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> candidate(
        formats.createReaderFor(candidateFile));
    std::unique_ptr<juce::AudioFormatReader> reference(
        formats.createReaderFor(referenceFile));
    if(!candidate || !reference
        || candidate->numChannels<1 || candidate->numChannels>2
        || reference->numChannels<1 || reference->numChannels>2
        || std::abs(candidate->sampleRate-reference->sampleRate)>.001
        || candidate->lengthInSamples<=0 || reference->lengthInSamples<=0) {
        std::fprintf(stderr,
            "ERROR: WAVs need equal sample rates, 1 or 2 channels, and audio\n");
        return 24;
    }
    const auto length=std::min({
        (juce::int64)candidate->lengthInSamples,
        (juce::int64)reference->lengthInSamples,
        (juce::int64)(candidate->sampleRate*15.)});
    juce::AudioBuffer<float> candidateBlock(2,2048),referenceBlock(2,2048);
    skellpreset::analysis::ComparisonAccumulator stats;
    for(juce::int64 pos=0;pos<length;pos+=2048) {
        const int count=(int)std::min((juce::int64)2048,length-pos);
        candidateBlock.clear();
        referenceBlock.clear();
        if(!candidate->read(&candidateBlock,0,count,pos,true,
                            candidate->numChannels==2)
           ||!reference->read(&referenceBlock,0,count,pos,true,
                              reference->numChannels==2)) {
            std::fprintf(stderr,"ERROR: WAV decode failed during A/B comparison\n");
            return 25;
        }
        if(candidate->numChannels==1)
            candidateBlock.copyFrom(1,0,candidateBlock,0,0,count);
        if(reference->numChannels==1)
            referenceBlock.copyFrom(1,0,referenceBlock,0,0,count);
        for(int n=0;n<count;++n)
            for(int ch=0;ch<2;++ch)
                stats.push(candidateBlock.getSample(ch,n),
                           referenceBlock.getSample(ch,n));
    }
    const auto result=stats.result();
    if(!result.hasSignal || result.rejected!=0) {
        std::fprintf(stderr,
            "ERROR: A/B signal silent, nonfinite or otherwise unusable\n");
        return 26;
    }
    std::printf("SKELL A/B first %.3f seconds at %.0f Hz\n",
                (double)length/candidate->sampleRate,candidate->sampleRate);
    std::printf("Candidate minus reference RMS: %+.3f dB\n",
                result.candidateMinusReferenceRmsDb);
    std::printf("Candidate peak: %+.3f dBFS; reference peak: %+.3f dBFS\n",
                result.candidatePeakDbfs,result.referencePeakDbfs);
    std::printf("Sample correlation: %.6f (signed); normalized level-matched "
                "RMS error: %.2f%%\n",
                result.sampleCorrelation,result.levelMatchedNrmse*100.);
    std::puts("NOTE: same performance, start sample and sample rate required.");
    std::puts("NOTE: RMS/correlation are diagnostics, not LUFS or perceptual quality.");
    return 0;
}

// Exercise real mono WAV decoding, native Alchemist processing and stereo WAV
// encoding. File cleanup is automatic even if a regression assertion fails.
int monoSmoke() {
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto input=temp.getNonexistentChildFile("SkellPresetMonoSmoke",".wav",false);
    const auto output=temp.getNonexistentChildFile("SkellPresetMonoSmokeProcessed",".wav",false);
    struct Cleanup {
        juce::File in,out;
        ~Cleanup() {in.deleteFile();out.deleteFile();}
    } cleanup{input,output};

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(input.createOutputStream());
    if(!stream)return 14;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriter::Options{}
        .withSampleRate(48000.).withNumChannels(1).withBitsPerSample(24));
    if(!writer)return 15;
    constexpr int samples=4096;
    juce::AudioBuffer<float> signal(1,samples);
    for(int n=0;n<samples;++n)
        signal.setSample(0,n,.12f*std::sin((float)(
            2.*juce::MathConstants<double>::pi*230.*n/48000.)));
    if(!writer->writeFromAudioSampleBuffer(signal,0,samples))return 16;
    writer.reset(); // Finalize WAV header before reopening input.
    if(render(input,output)!=0)return 17;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));
    if(!reader || reader->numChannels!=2 || reader->lengthInSamples!=samples)
        return 18;
    juce::AudioBuffer<float> rendered(2,samples);
    if(!reader->read(&rendered,0,samples,0,true,true))return 19;
    double energy=0.,stereoDifference=0.;
    for(int n=0;n<samples;++n) {
        const float l=rendered.getSample(0,n),r=rendered.getSample(1,n);
        if(!std::isfinite(l)||!std::isfinite(r))return 20;
        energy+=(double)l*l;
        stereoDifference+=std::abs((double)l-r);
    }
    if(energy<1.e-5 || stereoDifference>1.e-3)return 21;
    if(compareWavs(output,output)!=0)return 22;
    std::puts("PASS: mono WAV processed through full Alchemist rack to finite stereo output");
    return 0;
}

int eightBandAirFactorySmoke() {
    constexpr float gains[8]{-.04f,-.06f,-.03f,-.01f,.04f,.055f,.08f,.05f};
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->setCurrentProgram(37); // legacy host program: Eight-Band Air
    if(plugin->getCurrentProgram()!=37 ||
       plugin->loadedFactoryPresetIndex()!=37 ||
       plugin->activeSlotCount()!=10 ||
       plugin->slotType(8)!=SkellPresetAudioProcessor::EightBandToneFX ||
       plugin->slotIdAt(0)!=8)
        return 251;

    auto hasExpectedEqState=[&](SkellPresetAudioProcessor& candidate) {
        if(candidate.activeSlotCount()!=10 ||
           candidate.slotType(8)!=SkellPresetAudioProcessor::EightBandToneFX ||
           candidate.slotIdAt(0)!=8)
            return false;
        const auto* enabled=candidate.state.getRawParameterValue("s9_on");
        const auto* wet=candidate.state.getRawParameterValue("s9_wet");
        if(!enabled || enabled->load()<.999f || !wet || std::abs(wet->load()-1.f)>.001f)
            return false;
        for(int band=0;band<8;++band) {
            const float expectedGain=.5f+gains[band];
            if(std::abs(candidate.getSlotParam(8,band*2)-.5f)>.001f ||
               std::abs(candidate.getSlotParam(8,band*2+1)-expectedGain)>.001f)
                return false;
            const auto* q=candidate.state.getRawParameterValue(
                "s9_eqQ"+juce::String(band+1));
            if(!q || std::abs(q->load()-.5f)>.001f)
                return false;
        }
        return true;
    };
    if(!hasExpectedEqState(*plugin))return 252;

    plugin->prepareToPlay(48000.,256);
    juce::MidiBuffer midi;
    double peak=0.,energy=0.,difference=0.;
    for(int block=0;block<60;++block) {
        juce::AudioBuffer<float> audio(2,256);
        for(int n=0;n<256;++n) {
            const double t=(block*256+n)/48000.;
            const float input=(float)(.06*std::sin(2.*juce::MathConstants<double>::pi*213.*t)
                                     +.02*std::sin(2.*juce::MathConstants<double>::pi*6593.*t));
            audio.setSample(0,n,input);
            audio.setSample(1,n,input*.87f);
        }
        midi.clear();
        plugin->processBlock(audio,midi);
        for(int ch=0;ch<2;++ch)for(int n=0;n<256;++n) {
            const float out=audio.getSample(ch,n);
            if(!std::isfinite(out))return 253;
            peak=std::max(peak,(double)std::abs(out));
            energy+=(double)out*out;
            if(block>10) {
                const double t=(block*256+n)/48000.;
                const float input=(float)(.06*std::sin(2.*juce::MathConstants<double>::pi*213.*t)
                                         +.02*std::sin(2.*juce::MathConstants<double>::pi*6593.*t));
                difference+=std::abs((double)out-(double)(ch==0?input:input*.87f));
            }
        }
    }
    // This factory curve is only a few dB of tonal shaping on a 0.08-peak
    // test signal; a 0.20 ceiling catches runaway gain while allowing filter
    // settling and float headroom.
    if(peak>.20 || energy<.0001 || difference<.001)
        return 254;

    juce::MemoryBlock hostState;
    plugin->getStateInformation(hostState);
    auto restored=std::make_unique<SkellPresetAudioProcessor>();
    restored->setStateInformation(hostState.getData(),static_cast<int>(hostState.getSize()));
    restored->prepareToPlay(48000.,256);
    if(restored->getCurrentProgram()!=37 ||
       restored->loadedFactoryPresetIndex()!=37 ||
       !hasExpectedEqState(*restored))
        return 255;

    const auto presetFile=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skellpreset-eight-band-air-roundtrip",".skellpreset",false);
    const auto exported=plugin->exportPreset(presetFile);
    if(exported.failed()) {
        presetFile.deleteFile();
        std::fprintf(stderr,"Eight-Band Air .skellpreset export failed: %s\n",
                     exported.getErrorMessage().toRawUTF8());
        return 256;
    }
    auto imported=std::make_unique<SkellPresetAudioProcessor>();
    const auto importedResult=imported->importPreset(presetFile);
    presetFile.deleteFile();
    if(importedResult.failed()) {
        std::fprintf(stderr,"Eight-Band Air .skellpreset import failed: %s\n",
                     importedResult.getErrorMessage().toRawUTF8());
        return 257;
    }
    if(imported->loadedFactoryPresetIndex()!=-1 || !hasExpectedEqState(*imported))
        return 258;

    std::printf("PASS: Eight-Band Air factory 37, host state + .skellpreset EQ recall, peak %.4f, PDC %d\n",
                peak,plugin->getLatencySamples());
    plugin->releaseResources();
    restored->releaseResources();
    imported->releaseResources();
    return 0;
}

int nativeTrialsSmoke() {
    if(const int eqResult=eightBandAirFactorySmoke();eqResult!=0)return eqResult;
    for(int factory:{skellpreset::alchemist::factoryIndex,
                     skellpreset::deuce::factoryIndex,
                     skellpreset::alchemist_trial::factoryIndex,
                     skellpreset::deuce_v25::factoryIndex,
                     skellpreset::alchemist_peak_guard_trial::factoryIndex,
                     skellpreset::alchemist_tone_match_trial::factoryIndex,
                     skellpreset::alchemist_lighter_dynamics_trial::factoryIndex,
                     skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex,
                     skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex,
                     skellpreset::alchemist_serial_dynamics_trial::factoryIndex,
                     skellpreset::alchemist_low_end_contour_trial::factoryIndex,
                     skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex}) {
        auto plugin=std::make_unique<SkellPresetAudioProcessor>();
        const int stageCount=nativeStageCountForFactory(factory);
        if(stageCount<=0 || !dirtySerializedSlotControls(*plugin))return 200;
        // Recall comparison must use the SAME prepare/order for both
        // instances. Preparing the source before loading its program while
        // preparing the restored copy after recall compared different
        // smoothed initial DSP states, not actual serialization fidelity.
        plugin->loadFactoryPreset(factory);
        if(const int resetResult=checkNativeFactorySlotDefaults(*plugin,stageCount);
           resetResult!=0)return resetResult;
        plugin->prepareToPlay(48000.,256);
        const int expectedCount=factory==skellpreset::deuce_v25::factoryIndex?13:
            factory==skellpreset::deuce::factoryIndex?12:10;
        if(plugin->loadedFactoryPresetIndex()!=factory ||
           plugin->activeSlotCount()!=expectedCount)return 201;
        if(factory==skellpreset::deuce::factoryIndex &&
           (plugin->slotType(6)!=SkellPresetAudioProcessor::Compressor ||
            plugin->slotType(7)!=SkellPresetAudioProcessor::DeEsser))
            return 202;
        if(factory==skellpreset::alchemist::factoryIndex &&
           (plugin->slotType(1)!=SkellPresetAudioProcessor::EightBandToneFX ||
            plugin->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            std::abs(plugin->getSlotParam(1,15)-.866667f)>.001f ||
            std::abs(plugin->getSlotParam(6,12)-.5f)>1.e-6f))return 209;
        if(factory==skellpreset::alchemist_trial::factoryIndex &&
           (plugin->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            plugin->getSlotParam(6,12)<.75f ||
            std::abs(plugin->getSlotParam(6,13)-.25f)>.001f ||
            std::abs(plugin->getSlotParam(6,14)-.45f)>.001f ||
            std::abs(plugin->getSlotParam(6,15)-.38f)>.001f))return 203;
        if((factory==skellpreset::alchemist_peak_guard_trial::factoryIndex ||
            factory==skellpreset::alchemist_tone_match_trial::factoryIndex ||
            factory==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex ||
            factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex ||
            factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex) &&
           (plugin->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            plugin->slotType(7)!=SkellPresetAudioProcessor::Limiter ||
            std::abs(plugin->getSlotParam(7,0)-.89f)>.001f ||
            std::abs(plugin->getSlotParam(7,1))>.001f ||
            plugin->getLatencySamples()<48 || plugin->getLatencySamples()>1024))
            return 219;
        if(factory==skellpreset::alchemist_tone_match_trial::factoryIndex &&
           (std::abs(plugin->getSlotParam(1,11)-.478534f)>.001f ||
            std::abs(plugin->getSlotParam(1,13)-.738915f)>.001f ||
            std::abs(plugin->getSlotParam(1,15)-.830000f)>.001f))
            return 227;
        if(factory==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex ||
           factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex ||
           factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex) {
            const auto* fastWet=plugin->state.getRawParameterValue("s3_wet");
            const auto* slowWet=plugin->state.getRawParameterValue("s4_wet");
            const float expectedFast=factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex?.35f:.20f;
            const float expectedSlow=factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex?.70f:.50f;
            if(plugin->slotType(2)!=skellpreset::alchemist_fast_compressor_blend_trial::stages[2].module ||
               plugin->slotType(3)!=skellpreset::alchemist_fast_compressor_blend_trial::stages[3].module ||
               !fastWet || std::abs(fastWet->load()-expectedFast)>.001f ||
               !slowWet || std::abs(slowWet->load()-expectedSlow)>.001f ||
               plugin->getLatencySamples()<48 || plugin->getLatencySamples()>1024)
                return 231;
        }
        if(factory==skellpreset::alchemist_serial_dynamics_trial::factoryIndex &&
           (plugin->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
            plugin->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
            plugin->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            plugin->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
            std::abs(plugin->getSlotParam(2,0)-.428571f)>.001f ||
            std::abs(plugin->getSlotParam(2,1)-.5f)>.001f ||
            std::abs(plugin->getSlotParam(2,2)-.35f)>.001f ||
            std::abs(plugin->getSlotParam(2,3)-.15f)>.001f ||
            plugin->getLatencySamples()<48 || plugin->getLatencySamples()>1024))
            return 235;
        if(factory==skellpreset::alchemist_low_end_contour_trial::factoryIndex &&
           (plugin->slotType(0)!=skellpreset::alchemist_low_end_contour_trial::stages[0].module ||
            plugin->slotType(1)!=skellpreset::alchemist_low_end_contour_trial::stages[1].module ||
            plugin->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
            plugin->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
            plugin->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            plugin->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
            std::abs(plugin->getSlotParam(0,3)-.04f)>.001f ||
            std::abs(plugin->getSlotParam(1,1)-.5f)>.001f ||
            plugin->getLatencySamples()!=144))
            return 241;
        if(factory==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex) {
            const auto* wet=plugin->state.getRawParameterValue("s3_wet");
            if(plugin->slotType(0)!=SkellPresetAudioProcessor::EQ ||
               plugin->slotType(1)!=SkellPresetAudioProcessor::EightBandToneFX ||
               plugin->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
               plugin->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
               plugin->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
               plugin->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
               std::abs(plugin->getSlotParam(0,3)-.04f)>.001f ||
               std::abs(plugin->getSlotParam(1,1)-.5f)>.001f ||
               std::abs(plugin->getSlotParam(2,0)-.5f)>.001f ||
               std::abs(plugin->getSlotParam(2,1)-.5f)>.001f ||
               std::abs(plugin->getSlotParam(2,2)-.35f)>.001f ||
               std::abs(plugin->getSlotParam(2,3)-.15f)>.001f ||
               !wet || std::abs(wet->load()-.45f)>.001f ||
               plugin->getLatencySamples()!=144)
                return 242;
        }
        if(factory==skellpreset::alchemist_serial_dynamics_trial::factoryIndex ||
           factory==skellpreset::alchemist_low_end_contour_trial::factoryIndex ||
           factory==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex) {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio(2,256);
            double energy=0.;
            float peak=0.f;
            const int latencyBefore=plugin->getLatencySamples();
            for(int block=0;block<375;++block) {
                for(int i=0;i<256;++i) {
                    const int sample=block*256+i;
                    const double t=(double)sample/48000.;
                    float x=.28f*(float)std::sin(6.283185307179586*220.*t)
                            +.12f*(float)std::sin(6.283185307179586*3150.*t);
                    if(sample%4800==0)x+=.25f;
                    audio.setSample(0,i,x);
                    audio.setSample(1,i,-.85f*x);
                }
                midi.clear();
                plugin->processBlock(audio,midi);
                for(int i=0;i<256;++i) {
                    const float l=audio.getSample(0,i),r=audio.getSample(1,i);
                    if(!std::isfinite(l)||!std::isfinite(r)||std::abs(l)>=1.f||std::abs(r)>=1.f)
                        return 237;
                    peak=std::max(peak,std::max(std::abs(l),std::abs(r)));
                    energy+=(double)l*l+(double)r*r;
                }
            }
            if(energy<1.e-3||peak>=1.f)return 238;
            // Bypass the new stage once and verify the rest of the rack and PDC stay valid.
            plugin->setSlotOn(2,false);
            for(int block=0;block<4;++block) {
                for(int i=0;i<256;++i) {
                    const double t=(double)(block*256+i)/48000.;
                    const float x=.2f*(float)std::sin(6.283185307179586*440.*t);
                    audio.setSample(0,i,x);audio.setSample(1,i,-.8f*x);
                }
                midi.clear();plugin->processBlock(audio,midi);
                for(int i=0;i<256;++i)
                    if(!std::isfinite(audio.getSample(0,i))||
                       !std::isfinite(audio.getSample(1,i)))return 239;
            }
            plugin->setSlotOn(2,true);
            if(plugin->getLatencySamples()!=latencyBefore)return 240;
        }
        if(factory==skellpreset::deuce_v25::factoryIndex) {
            const auto* eqQ=plugin->state.getRawParameterValue("s13_eqQ7");
            const auto* output=plugin->state.getRawParameterValue("output");
            const auto* wet=plugin->state.getRawParameterValue("s13_wet");
            const auto* enabled=plugin->state.getRawParameterValue("s13_on");
            const float qNormalized=eqQ?eqQ->load():-1.f;
            const float qPhysical=.4f*std::pow(20.f,qNormalized);
            bool baseChainMatches=true;
            for(int stage=0;stage<12;++stage)
                baseChainMatches=baseChainMatches &&
                    plugin->slotType(stage)==skellpreset::deuce::stages[(size_t)stage].module;
            if(!baseChainMatches ||
               plugin->slotType(12)!=SkellPresetAudioProcessor::EightBandToneFX ||
               std::abs(plugin->getSlotParam(12,12)-.906f)>.001f ||
               std::abs(plugin->getSlotParam(12,13)-.525f)>.001f ||
               !eqQ || std::abs(qNormalized-.210f)>.00051f ||
               std::abs(qPhysical-.75f)>.001f ||
               !output || std::abs(output->load()-5.17f)>.001f ||
               !wet || wet->load()<.999f || !enabled || enabled->load()<.999f ||
               plugin->getLatencySamples()!=0) {
                std::fprintf(stderr,
                    "Deuce v2.5 factory mismatch: base=%d type=%d freq=%g gain=%g qNorm=%g q=%g output=%g wet=%g on=%g PDC=%d\\n",
                    baseChainMatches?1:0,plugin->slotType(12),
                    plugin->getSlotParam(12,12),plugin->getSlotParam(12,13),
                    qNormalized,qPhysical,output?output->load():-999.f,
                    wet?wet->load():-1.f,enabled?enabled->load():-1.f,
                    plugin->getLatencySamples());
                return 208;
            }
        }
        juce::MemoryBlock state;
        plugin->getStateInformation(state);
        // Factory 52's preceding smoke intentionally advances its compressor envelopes.
        // Runtime DSP history is not serialized into DAW/plugin state, so reset the
        // source after capturing parameters and before comparing it with a freshly
        // prepared restored instance. This tests preset/state recall, not envelope cloning.
        if(factory==skellpreset::alchemist_serial_dynamics_trial::factoryIndex ||
           factory==skellpreset::alchemist_low_end_contour_trial::factoryIndex ||
           factory==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex)
            plugin->prepareToPlay(48000.,256);
        auto recalled=std::make_unique<SkellPresetAudioProcessor>();
        recalled->setStateInformation(state.getData(),(int)state.getSize());
        recalled->prepareToPlay(48000.,256);
        if(recalled->loadedFactoryPresetIndex()!=factory ||
           recalled->activeSlotCount()!=expectedCount)return 204;
        if(factory==skellpreset::alchemist::factoryIndex &&
           (std::abs(recalled->getSlotParam(1,15)-.866667f)>.001f ||
            std::abs(recalled->getSlotParam(6,12)-.5f)>1.e-6f))return 210;
        if(factory==skellpreset::alchemist_trial::factoryIndex &&
           (recalled->getSlotParam(6,12)<.75f ||
            std::abs(recalled->getSlotParam(6,13)-.25f)>.001f ||
            std::abs(recalled->getSlotParam(6,14)-.45f)>.001f ||
            std::abs(recalled->getSlotParam(6,15)-.38f)>.001f))return 211;
        if((factory==skellpreset::alchemist_peak_guard_trial::factoryIndex ||
            factory==skellpreset::alchemist_tone_match_trial::factoryIndex ||
            factory==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex ||
            factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex ||
            factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex) &&
           (recalled->slotType(7)!=SkellPresetAudioProcessor::Limiter ||
            std::abs(recalled->getSlotParam(7,0)-.89f)>.001f ||
            std::abs(recalled->getSlotParam(7,1))>.001f ||
            recalled->getLatencySamples()!=plugin->getLatencySamples()))return 220;
        if(factory==skellpreset::alchemist_tone_match_trial::factoryIndex &&
           (std::abs(recalled->getSlotParam(1,11)-.478534f)>.001f ||
            std::abs(recalled->getSlotParam(1,13)-.738915f)>.001f ||
            std::abs(recalled->getSlotParam(1,15)-.830000f)>.001f))return 228;
        if(factory==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex ||
           factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex ||
           factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex) {
            const auto* fastWet=recalled->state.getRawParameterValue("s3_wet");
            const auto* slowWet=recalled->state.getRawParameterValue("s4_wet");
            const float expectedFast=factory==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex?.35f:.20f;
            const float expectedSlow=factory==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex?.70f:.50f;
            if(!fastWet || std::abs(fastWet->load()-expectedFast)>.001f ||
               !slowWet || std::abs(slowWet->load()-expectedSlow)>.001f)
                return 232;
        }
        if(factory==skellpreset::alchemist_serial_dynamics_trial::factoryIndex &&
           (recalled->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
            recalled->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
            recalled->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            recalled->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
            std::abs(recalled->getSlotParam(2,0)-.428571f)>.001f ||
            std::abs(recalled->getSlotParam(2,1)-.5f)>.001f ||
            std::abs(recalled->getSlotParam(2,2)-.35f)>.001f ||
            std::abs(recalled->getSlotParam(2,3)-.15f)>.001f ||
            recalled->getLatencySamples()!=plugin->getLatencySamples()))
            return 236;
        if(factory==skellpreset::alchemist_low_end_contour_trial::factoryIndex &&
           (recalled->slotType(0)!=skellpreset::alchemist_low_end_contour_trial::stages[0].module ||
            recalled->slotType(1)!=skellpreset::alchemist_low_end_contour_trial::stages[1].module ||
            recalled->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
            recalled->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
            recalled->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            recalled->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
            std::abs(recalled->getSlotParam(0,3)-.04f)>.001f ||
            std::abs(recalled->getSlotParam(1,1)-.5f)>.001f ||
            recalled->getLatencySamples()!=plugin->getLatencySamples()))
            return 242;
        if(factory==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex) {
            const auto* wet=recalled->state.getRawParameterValue("s3_wet");
            if(recalled->slotType(0)!=SkellPresetAudioProcessor::EQ ||
               recalled->slotType(1)!=SkellPresetAudioProcessor::EightBandToneFX ||
               recalled->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
               recalled->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
               recalled->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
               recalled->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
               std::abs(recalled->getSlotParam(0,3)-.04f)>.001f ||
               std::abs(recalled->getSlotParam(1,1)-.5f)>.001f ||
               std::abs(recalled->getSlotParam(2,0)-.5f)>.001f ||
               std::abs(recalled->getSlotParam(2,1)-.5f)>.001f ||
               std::abs(recalled->getSlotParam(2,2)-.35f)>.001f ||
               std::abs(recalled->getSlotParam(2,3)-.15f)>.001f ||
               !wet || std::abs(wet->load()-.45f)>.001f ||
               recalled->getLatencySamples()!=plugin->getLatencySamples())
                return 243;
        }
        if(factory==skellpreset::deuce_v25::factoryIndex) {
            const auto* eqQ=recalled->state.getRawParameterValue("s13_eqQ7");
            const auto* output=recalled->state.getRawParameterValue("output");
            const float qNormalized=eqQ?eqQ->load():-1.f;
            const float qPhysical=.4f*std::pow(20.f,qNormalized);
            if(!eqQ || std::abs(qNormalized-.210f)>.00051f ||
               std::abs(qPhysical-.75f)>.001f ||
               std::abs(recalled->getSlotParam(12,12)-.906f)>.001f ||
               std::abs(recalled->getSlotParam(12,13)-.525f)>.001f ||
               !output || std::abs(output->load()-5.17f)>.001f ||
               recalled->getLatencySamples()!=0) {
                std::fprintf(stderr,
                    "Deuce v2.5 state roundtrip mismatch: qNorm=%g q=%g output=%g PDC=%d\\n",
                    qNormalized,qPhysical,output?output->load():-999.f,
                    recalled->getLatencySamples());
                return 214;
            }
        }
        double squared=0.;
        for(int block=0;block<60;++block) {
            juce::AudioBuffer<float> signal(2,256),other(2,256);
            for(int n=0;n<256;++n) {
                const float t=(float)(block*256+n)/48000.f;
                constexpr float testPi=3.14159265358979323846f;
                const float l=.06f*std::sin(2.f*testPi*213.f*t)
                   +.02f*std::sin(2.f*testPi*6593.f*t);
                signal.setSample(0,n,l);
                signal.setSample(1,n,.87f*l);
            }
            other.makeCopyOf(signal);
            juce::MidiBuffer midi,otherMidi;
            plugin->processBlock(signal,midi);
            recalled->processBlock(other,otherMidi);
            for(int ch=0;ch<2;++ch)for(int n=0;n<256;++n) {
                const float value=signal.getSample(ch,n);
                const float saved=other.getSample(ch,n);
                if(!std::isfinite(value) || !std::isfinite(saved) ||
                   std::abs(value)>8.f ||
                   std::abs(value-saved)>1.e-3f) {
                    std::fprintf(stderr,
                      "Factory %d recall output mismatch block %d ch %d n %d: %.7f vs %.7f\n",
                      factory,block,ch,n,(double)value,(double)saved);
                    return 205;
                }
                squared+=(double)value*value;
            }
        }
        if(squared<1.e-6)return 206;
    }
    // The peak-guard variant must add PDC only while its final limiter is active.
    auto peakSwitcher=std::make_unique<SkellPresetAudioProcessor>();
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_peak_guard_trial::factoryIndex);
    peakSwitcher->prepareToPlay(48000.,256);
    const int guardedLatency=peakSwitcher->getLatencySamples();
    if(peakSwitcher->slotType(7)!=SkellPresetAudioProcessor::Limiter ||
       guardedLatency!=144)return 221;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(peakSwitcher->slotType(7)!=SkellPresetAudioProcessor::Empty ||
       peakSwitcher->getLatencySamples()!=0 ||
       std::abs(peakSwitcher->getSlotParam(6,12)-.5f)>1.e-6f)return 222;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_peak_guard_trial::factoryIndex);
    if(peakSwitcher->slotType(7)!=SkellPresetAudioProcessor::Limiter ||
       peakSwitcher->getLatencySamples()!=guardedLatency)return 223;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_tone_match_trial::factoryIndex);
    if(peakSwitcher->slotType(7)!=SkellPresetAudioProcessor::Limiter ||
       peakSwitcher->getLatencySamples()!=guardedLatency ||
       std::abs(peakSwitcher->getSlotParam(1,11)-.478534f)>.001f ||
       std::abs(peakSwitcher->getSlotParam(1,13)-.738915f)>.001f ||
       std::abs(peakSwitcher->getSlotParam(1,15)-.830000f)>.001f)return 229;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_lighter_dynamics_trial::factoryIndex);
    const auto* fastWet=peakSwitcher->state.getRawParameterValue("s3_wet");
    const auto* slowWet=peakSwitcher->state.getRawParameterValue("s4_wet");
    if(peakSwitcher->slotType(7)!=SkellPresetAudioProcessor::Limiter ||
       peakSwitcher->getLatencySamples()!=guardedLatency ||
       !fastWet || std::abs(fastWet->load()-.20f)>.001f ||
       !slowWet || std::abs(slowWet->load()-.50f)>.001f)return 233;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_tone_match_trial::factoryIndex);
    fastWet=peakSwitcher->state.getRawParameterValue("s3_wet");
    slowWet=peakSwitcher->state.getRawParameterValue("s4_wet");
    if(!fastWet || std::abs(fastWet->load()-.35f)>.001f ||
       !slowWet || std::abs(slowWet->load()-.70f)>.001f ||
       peakSwitcher->getLatencySamples()!=guardedLatency)return 234;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex);
    fastWet=peakSwitcher->state.getRawParameterValue("s3_wet");
    slowWet=peakSwitcher->state.getRawParameterValue("s4_wet");
    if(!fastWet || std::abs(fastWet->load()-.20f)>.001f ||
       !slowWet || std::abs(slowWet->load()-.70f)>.001f ||
       peakSwitcher->getLatencySamples()!=guardedLatency)return 235;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex);
    fastWet=peakSwitcher->state.getRawParameterValue("s3_wet");
    slowWet=peakSwitcher->state.getRawParameterValue("s4_wet");
    if(!fastWet || std::abs(fastWet->load()-.35f)>.001f ||
       !slowWet || std::abs(slowWet->load()-.50f)>.001f ||
       peakSwitcher->getLatencySamples()!=guardedLatency)return 236;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist_tone_match_trial::factoryIndex);
    fastWet=peakSwitcher->state.getRawParameterValue("s3_wet");
    slowWet=peakSwitcher->state.getRawParameterValue("s4_wet");
    if(!fastWet || std::abs(fastWet->load()-.35f)>.001f ||
       !slowWet || std::abs(slowWet->load()-.70f)>.001f ||
       peakSwitcher->getLatencySamples()!=guardedLatency)return 237;
    peakSwitcher->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(peakSwitcher->slotType(7)!=SkellPresetAudioProcessor::Empty ||
       peakSwitcher->getLatencySamples()!=0)return 230;
    peakSwitcher->releaseResources();
    // The final guard must actually constrain a high-frequency, high-crest
    // signal after the full native chain, not merely load the limiter slot.
    auto peakGuardStress=std::make_unique<SkellPresetAudioProcessor>();
    peakGuardStress->loadFactoryPreset(skellpreset::alchemist_tone_match_trial::factoryIndex);
    peakGuardStress->prepareToPlay(44100.,256);
    if(peakGuardStress->getLatencySamples()!=132)return 224;
    std::vector<float> guardLeft,guardRight;
    guardLeft.reserve(80*256);guardRight.reserve(80*256);
    for(int block=0;block<80;++block) {
        juce::AudioBuffer<float> stress(2,256);
        for(int n=0;n<256;++n) {
            const float t=float(block*256+n)/44100.f;
            const float x=.50f*std::sin(2.f*juce::MathConstants<float>::pi*19000.f*t)
                         +.08f*std::sin(2.f*juce::MathConstants<float>::pi*211.f*t);
            stress.setSample(0,n,x);
            stress.setSample(1,n,-.83f*x);
        }
        juce::MidiBuffer midi;
        peakGuardStress->processBlock(stress,midi);
        for(int n=0;n<256;++n) {
            const float l=stress.getSample(0,n),r=stress.getSample(1,n);
            if(!std::isfinite(l)||!std::isfinite(r))return 225;
            guardLeft.push_back(l);guardRight.push_back(r);
        }
    }
    const double guardedTruePeak=std::max(estimateFourPhasePeak(guardLeft),
                                          estimateFourPhasePeak(guardRight));
    if(!std::isfinite(guardedTruePeak)||guardedTruePeak>.91)return 226;
    peakGuardStress->releaseResources();
    // Independent blend trials retain the same final guard. Exercise both
    // on the same high-crest transient and anti-phase stereo stress vector.
    for(int factory:{skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex,
                     skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex}) {
        auto trialStress=std::make_unique<SkellPresetAudioProcessor>();
        trialStress->loadFactoryPreset(factory);
        trialStress->prepareToPlay(44100.,256);
        if(trialStress->getLatencySamples()!=132)return 238;
        std::vector<float> left,right;
        left.reserve(80*256);right.reserve(80*256);
        for(int block=0;block<80;++block) {
            juce::AudioBuffer<float> stress(2,256);
            for(int n=0;n<256;++n) {
                const float t=float(block*256+n)/44100.f;
                const float x=.50f*std::sin(2.f*juce::MathConstants<float>::pi*19000.f*t)
                             +.08f*std::sin(2.f*juce::MathConstants<float>::pi*211.f*t);
                stress.setSample(0,n,x);
                stress.setSample(1,n,-.83f*x);
            }
            juce::MidiBuffer midi;
            trialStress->processBlock(stress,midi);
            for(int n=0;n<256;++n) {
                const float l=stress.getSample(0,n),r=stress.getSample(1,n);
                if(!std::isfinite(l)||!std::isfinite(r))return 239;
                left.push_back(l);right.push_back(r);
            }
        }
        const double tp=std::max(estimateFourPhasePeak(left),estimateFourPhasePeak(right));
        if(!std::isfinite(tp)||tp>.91)return 240;
        trialStress->releaseResources();
    }
    // Switching between the protected original and opt-in trial must
    // restore every refinement control instead of leaving stale automation.
    auto switcher=std::make_unique<SkellPresetAudioProcessor>();
    switcher->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    switcher->loadFactoryPreset(skellpreset::alchemist_trial::factoryIndex);
    if(switcher->getSlotParam(6,12)<.75f ||
       std::abs(switcher->getSlotParam(6,13)-.25f)>.001f ||
       std::abs(switcher->getSlotParam(6,14)-.45f)>.001f ||
       std::abs(switcher->getSlotParam(6,15)-.38f)>.001f)return 212;
    switcher->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(std::abs(switcher->getSlotParam(6,12)-.5f)>1.e-6f ||
       std::abs(switcher->getSlotParam(6,13)-.5f)>1.e-6f ||
       std::abs(switcher->getSlotParam(6,14)-.5f)>1.e-6f ||
       std::abs(switcher->getSlotParam(6,15)-.5f)>1.e-6f)return 213;
    // Host program changes must also switch into and out of the opt-in serial
    // FET/RMS trial without leaving its module IDs, parameters or 3ms PDC behind.
    // APVTS slot floats use a 0.001 step, so nominal .428571 is stored as .429.
    // Keep the host-state assertion within that declared parameter precision.
    auto serialSwitcher=std::make_unique<SkellPresetAudioProcessor>();
    serialSwitcher->setCurrentProgram(skellpreset::alchemist_serial_dynamics_trial::factoryIndex);
    serialSwitcher->prepareToPlay(48000.,256);
    if(serialSwitcher->getCurrentProgram()!=skellpreset::alchemist_serial_dynamics_trial::factoryIndex ||
       serialSwitcher->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
       serialSwitcher->slotType(3)!=SkellPresetAudioProcessor::SkelldizerFX ||
       serialSwitcher->slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
       serialSwitcher->slotType(6)!=SkellPresetAudioProcessor::Limiter ||
       serialSwitcher->getLatencySamples()!=144 ||
       std::abs(serialSwitcher->getSlotParam(2,0)-.428571f)>.001f ||
       std::abs(serialSwitcher->getSlotParam(2,1)-.5f)>1.e-6f)
        return 241;
    serialSwitcher->setCurrentProgram(skellpreset::alchemist::factoryIndex);
    serialSwitcher->prepareToPlay(48000.,256);
    if(serialSwitcher->getCurrentProgram()!=skellpreset::alchemist::factoryIndex ||
       serialSwitcher->slotType(2)!=SkellPresetAudioProcessor::Compressor ||
       serialSwitcher->slotType(3)!=SkellPresetAudioProcessor::VoxRackSmoothComp ||
       serialSwitcher->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
       serialSwitcher->slotType(7)!=SkellPresetAudioProcessor::Empty ||
       serialSwitcher->getLatencySamples()!=0 ||
       // Slot parameters are stored on the APVTS 0.001 normalized grid; compare
    // the restored factory value within one published parameter step.
    std::abs(serialSwitcher->getSlotParam(2,0)-skellpreset::alchemist::stages[2].p[0])>.001f)
        return 242;
    serialSwitcher->setCurrentProgram(skellpreset::alchemist_serial_dynamics_trial::factoryIndex);
    serialSwitcher->prepareToPlay(48000.,256);
    if(serialSwitcher->slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
       serialSwitcher->getLatencySamples()!=144 ||
       std::abs(serialSwitcher->getSlotParam(2,0)-.428571f)>.001f)
        return 243;
    // Host program changes must restore the v2.5-only output calibration,
    // then clear it when returning to the original Deuce/Alchemist programs.
    auto deuceSwitcher=std::make_unique<SkellPresetAudioProcessor>();
    deuceSwitcher->setCurrentProgram(skellpreset::deuce_v25::factoryIndex);
    auto* switchedOutput=deuceSwitcher->state.getRawParameterValue("output");
    if(deuceSwitcher->getCurrentProgram()!=skellpreset::deuce_v25::factoryIndex ||
       !switchedOutput || std::abs(switchedOutput->load()-5.17f)>.001f)
        return 215;
    deuceSwitcher->setCurrentProgram(skellpreset::deuce::factoryIndex);
    if(deuceSwitcher->getCurrentProgram()!=skellpreset::deuce::factoryIndex ||
       std::abs(switchedOutput->load())>.001f)
        return 216;
    deuceSwitcher->setCurrentProgram(skellpreset::alchemist::factoryIndex);
    if(deuceSwitcher->getCurrentProgram()!=skellpreset::alchemist::factoryIndex ||
       std::abs(switchedOutput->load())>.001f ||
       std::abs(deuceSwitcher->getSlotParam(6,12)-.5f)>1.e-6f)
        return 217;
    deuceSwitcher->setCurrentProgram(skellpreset::deuce_v25::factoryIndex);
    if(deuceSwitcher->getCurrentProgram()!=skellpreset::deuce_v25::factoryIndex ||
       std::abs(switchedOutput->load()-5.17f)>.001f ||
       std::abs(deuceSwitcher->getSlotParam(12,13)-.525f)>.001f)
        return 218;
    std::puts("PASS: original/refined Alchemist + Deuce recall, v2.5 EQ/output, host switching and 0-NaN stereo");
    return 0;
}

int lowEndOverrideSmoke() {
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->loadFactoryPreset(skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex);
    if(plugin->getSlotParam(0,3)<.039f || plugin->getSlotParam(0,3)>.041f ||
       std::abs(plugin->getSlotParam(1,1)-.5f)>.001f)return 250;
    plugin->setSlotParam(0,3,0.f);          // 20 Hz HPF
    plugin->setSlotParam(1,1,.4666667f);    // -1 dB at 70 Hz
    if(std::abs(plugin->getSlotParam(0,3))>.001f ||
       std::abs(plugin->getSlotParam(1,1)-.4666667f)>.0011f)return 251;
    plugin->prepareToPlay(44100.,256);
    if(plugin->getLatencySamples()!=132)return 252;
    juce::AudioBuffer<float> block(2,256);
    juce::MidiBuffer midi;
    double peak=0.;
    for(int b=0;b<96;++b) {
        for(int n=0;n<256;++n) {
            const double t=double(b*256+n)/44100.;
            const float x=.045f*std::sin(2.*juce::MathConstants<double>::pi*31.5*t)
                         +.040f*std::sin(2.*juce::MathConstants<double>::pi*40.*t)
                         +.030f*std::sin(2.*juce::MathConstants<double>::pi*50.*t)
                         +.025f*std::sin(2.*juce::MathConstants<double>::pi*63.*t)
                         +.020f*std::sin(2.*juce::MathConstants<double>::pi*80.*t)
                         +.015f*std::sin(2.*juce::MathConstants<double>::pi*100.*t);
            block.setSample(0,n,x);
            block.setSample(1,n,x);
        }
        plugin->processBlock(block,midi);
        for(int n=0;n<256;++n)for(int ch=0;ch<2;++ch) {
            const float value=block.getSample(ch,n);
            if(!std::isfinite(value))return 253;
            peak=std::max(peak,double(std::abs(value)));
        }
    }
    plugin->releaseResources();
    if(!std::isfinite(peak)||peak>=1.0)return 254;
    plugin->loadFactoryPreset(skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex);
    if(std::abs(plugin->getSlotParam(0,3)-.04f)>.001f ||
       std::abs(plugin->getSlotParam(1,1)-.5f)>.001f)return 255;
    std::puts("PASS Factory 54 diagnostic low-end overrides: APVTS recall, 3ms PDC, finite unclipped output, factory reset");
    return 0;
}

} // namespace

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    if(argc==2 && juce::String(argv[1])=="--smoke")return smoke();
    if(argc==2 && juce::String(argv[1])=="--smoke-blank")return blankSmoke();
    if(argc==2 && juce::String(argv[1])=="--smoke-slot-order")return strictSlotOrderSmoke();
    if(argc==2 && juce::String(argv[1])=="--ui-smoke")return uiSmoke();
    if(argc==2 && juce::String(argv[1])=="--studio-smoke")return studioSmoke();
    if(argc==2 && juce::String(argv[1])=="--native-trials-smoke")return nativeTrialsSmoke();
    if(argc==2 && juce::String(argv[1])=="--lowend-override-smoke")return lowEndOverrideSmoke();
    if(argc==2 && juce::String(argv[1])=="--percent-smoke")return percentDisplaySmoke();
    if(argc==2 && juce::String(argv[1])=="--stress")return stress();
    if(argc==2 && juce::String(argv[1])=="--smoke-mono")return monoSmoke();
    if(argc==7 && juce::String(argv[1])=="--factory-override") {
        const int factory=juce::String(argv[2]).getIntValue();
        const float hpf=std::strtof(argv[3],nullptr);
        const float band70=std::strtof(argv[4],nullptr);
        if(factory!=skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex ||
           !std::isfinite(hpf)||!std::isfinite(band70)||
           hpf<0.f||hpf>1.f||band70<0.f||band70>1.f)return 7;
        return render(juce::File(juce::String::fromUTF8(argv[5])),
                      juce::File(juce::String::fromUTF8(argv[6])),
                      factory,hpf,band70);
    }
    if(argc==4 && juce::String(argv[1])=="--compare")
        return compareWavs(juce::File(juce::String::fromUTF8(argv[2])),
                           juce::File(juce::String::fromUTF8(argv[3])));
    if(argc==5 && juce::String(argv[1])=="--factory") {
        const auto requested=juce::String(argv[2]);
        if(requested!="43" && requested!="44" && requested!="45" && requested!="46" && requested!="47" && requested!="48" && requested!="49" && requested!="50" && requested!="51" && requested!="52" && requested!="53" && requested!="54")return 7;
        return render(juce::File(juce::String::fromUTF8(argv[3])),
                      juce::File(juce::String::fromUTF8(argv[4])),
                      requested.getIntValue());
    }
    if(argc!=3) {
        std::fprintf(stderr,"Usage: SkellPresetAlchemistRender --factory 43|44|45|46|47|48|49|50|51|52|53|54 INPUT.wav NEW_OUTPUT.wav\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --factory-override 54 HPF_NORMALIZED BAND70_GAIN_NORMALIZED INPUT.wav NEW_OUTPUT.wav\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --lowend-override-smoke\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --compare CANDIDATE.wav REFERENCE.wav\n");
        return 1;
    }
    return render(juce::File(juce::String::fromUTF8(argv[1])),
                  juce::File(juce::String::fromUTF8(argv[2])));
}
