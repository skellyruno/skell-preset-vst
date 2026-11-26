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
#include "NativeVocalPreview.h"
#include "NativeVocalAudition.h"
#include "../Source/chain/VocalStarterChains.h"
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
       || plugin->getProgramName(skellpreset::factoryIndexForRecipe(43))!="Alchemist"
       || plugin->getProgramName(skellpreset::factoryIndexForRecipe(44))!="Deuce D/Max (Native Trial)"
       || plugin->getProgramName(skellpreset::factoryIndexForRecipe(46))!="Deuce"
       || plugin->getProgramName(skellpreset::factoryIndexForRecipe(54))!="Alchemist Serial Dynamics Blend (Trial)")
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
    // Real-editor De-Esser lifecycle regression: select its actual panel,
    // automate spectral settings and process finite audio with GUI alive.
    // This checks the observed crash path more directly than the portable
    // split-band test alone; it does not substitute for FL Studio testing.
    editor->configureForScreenshot(0,1,false);
    plugin->clearRackToEmpty();
    plugin->setSlotType(0,SkellPresetAudioProcessor::DeEsser);
    plugin->setSlotOn(0,true);
    editor->selectRackSlotForScreenshot(0,0);
    auto deesserLayout=editor->getScreenshotLayoutIssues();
    report(deesserLayout,"deesser-editor");
    if(!deesserLayout.isEmpty())return 55;
    for(int block=0;block<80;++block) {
        if(block%8==0) {
            plugin->setSlotParam(0,0,block%16==0?.06f:.94f);
            plugin->setSlotParam(0,1,block%16==0?.15f:.85f);
            plugin->setSlotParam(0,2,block%16==0?.65f:.25f);
            plugin->setSlotParam(0,3,block%16==0?.05f:.95f);
        }
        juce::AudioBuffer<float> samples(2,256);
        for(int n=0;n<256;++n) {
            const auto phase=2.*juce::MathConstants<double>::pi
                *(block*256+n)/48000.;
            const float sample=(float)(.32*std::sin(7200.*phase)
                                       +.10*std::sin(240.*phase));
            samples.setSample(0,n,sample);
            samples.setSample(1,n,sample*.8f);
        }
        juce::MidiBuffer events;
        plugin->processBlock(samples,events);
        for(int channel=0;channel<2;++channel)
            for(int n=0;n<256;++n)
                if(!std::isfinite(samples.getSample(channel,n)))return 56;
    }
    if(plugin->getLatencySamples()!=0)return 57;
    editor->showHomeForScreenshot();
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

// Exercise all five opt-in Studio effects in the actual JUCE processor;
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

int masterSignalSmoke(){
    // Render deterministic, stereo, same-input vectors through the real processor.
    // Exercise host-notified APVTS values after prepare; skip the 20 ms ramp
    // for steady-state comparisons, while separately measuring ramp continuity.
    constexpr int blockSize=256,blocks=28,settle=12;
    constexpr double sr=48000.;
    auto source=[sr](int n,int ch){
        return (ch==0?1.f:.73f)*(.075f*(float)std::sin(2.*juce::MathConstants<double>::pi*713.*n/sr)
               +.022f*(float)std::sin(2.*juce::MathConstants<double>::pi*1741.*n/sr));
    };
    struct Render {std::vector<float> output,original;int latency=0;bool finite=true;};
    auto render=[&](int mode,int factory,float input,float mix,float output){
        Render result;
        auto p=std::make_unique<SkellPresetAudioProcessor>();
        if(factory>=0)p->loadFactoryPreset(factory);else p->clearRackToEmpty();
        if(factory==-2){p->setSlotType(0,SkellPresetAudioProcessor::Limiter);p->setSlotOn(0,true);}
        setProcessorParameter(*p,"mode",(float)mode);
        setProcessorParameter(*p,"micGateOn",0.f);setProcessorParameter(*p,"micPreOn",0.f);
        setProcessorParameter(*p,"delta",0.f);
        setProcessorParameter(*p,"input",0.f);setProcessorParameter(*p,"mix",1.f);setProcessorParameter(*p,"output",0.f);
        p->prepareToPlay(sr,blockSize);
        result.latency=p->getLatencySamples();
        setProcessorParameter(*p,"input",input);setProcessorParameter(*p,"mix",mix);setProcessorParameter(*p,"output",output);
        juce::AudioBuffer<float> audio(2,blockSize);juce::MidiBuffer midi;
        for(int b=0;b<blocks;++b){
            for(int ch=0;ch<2;++ch)for(int n=0;n<blockSize;++n)audio.setSample(ch,n,source(b*blockSize+n,ch));
            p->processBlock(audio,midi);
            if(b<settle)continue;
            for(int n=0;n<blockSize;++n)for(int ch=0;ch<2;++ch){
                const float sample=audio.getSample(ch,n);
                result.finite=result.finite && std::isfinite(sample);
                result.output.push_back(sample);
                result.original.push_back(source(b*blockSize+n-result.latency,ch));
            }
        }
        p->releaseResources();
        return result;
    };
    auto error=[](const std::vector<float>& a,const std::vector<float>& b,double scale=1.){
        if(a.size()!=b.size() || a.empty())return 1.e9;
        double peak=0.;for(size_t i=0;i<a.size();++i)peak=std::max(peak,std::abs((double)a[i]-scale*b[i]));
        return peak;
    };
    auto rms=[](const std::vector<float>& a){
        double sum=0.;for(float x:a)sum+=(double)x*x;
        return a.empty()?0.:std::sqrt(sum/a.size());
    };
    constexpr int factory=skellpreset::alchemist::factoryIndex;
    for(int mode:{0,1}){
        const auto unity=render(mode,-1,0.f,1.f,0.f);
        const auto in=render(mode,-1,6.f,1.f,0.f);
        const auto out=render(mode,-1,0.f,1.f,-6.f);
        const auto dry=render(mode,factory,6.f,0.f,0.f);
        const auto wet=render(mode,factory,6.f,1.f,0.f);
        const auto middle=render(mode,factory,6.f,.5f,0.f);
        auto expected=wet.output;
        for(size_t i=0;i<expected.size();++i)expected[i]=.5f*(wet.output[i]+dry.output[i]);
        const double inDb=20.*std::log10(rms(in.output)/rms(unity.output));
        const double outDb=20.*std::log10(rms(out.output)/rms(unity.output));
        const double dryError=error(dry.output,dry.original),blendError=error(middle.output,expected);
        const double wetDiff=error(wet.output,dry.output);
        std::printf("MASTER_RENDER mode=%d input=%+.3f dB output=%+.3f dB dry_error=%.8g blend_error=%.8g wet_difference=%.8g latency=%d samples\n",
            mode,inDb,outDb,dryError,blendError,wetDiff,wet.latency);
        if(!unity.finite || !in.finite || !out.finite || !dry.finite || !wet.finite || !middle.finite ||
           std::abs(inDb-6.)>.03 || std::abs(outDb+6.)>.03 || dryError>2.e-5 ||
           blendError>2.e-4 || wetDiff<.001 || wet.latency!=dry.latency)return 93;
    }
    for(int mode:{0,1}) {
        const auto dry=render(mode,-2,6.f,0.f,0.f),wet=render(mode,-2,6.f,1.f,0.f),blend=render(mode,-2,6.f,.5f,0.f);
        auto expected=wet.output;for(size_t i=0;i<expected.size();++i)expected[i]=.5f*(dry.output[i]+wet.output[i]);
        const double dryError=error(dry.output,dry.original),mixError=error(blend.output,expected);
        std::printf("MASTER_PDC mode=%d latency=%d dry_error=%.9g blend_error=%.9g\n",mode,dry.latency,dryError,mixError);
        if(!dry.finite || !wet.finite || !blend.finite || dry.latency<=0 || dry.latency!=wet.latency || dryError>2.e-5 || mixError>2.e-4)return 97;
    }
    auto p=std::make_unique<SkellPresetAudioProcessor>();p->clearRackToEmpty();
    setProcessorParameter(*p,"mode",1.f);setProcessorParameter(*p,"input",0.f);
    setProcessorParameter(*p,"mix",1.f);setProcessorParameter(*p,"output",0.f);
    p->prepareToPlay(sr,blockSize);
    juce::AudioBuffer<float> audio(2,blockSize);juce::MidiBuffer midi;
    float previous=.1f;double maxStep=0.;
    setProcessorParameter(*p,"input",6.f);
    for(int b=0;b<8;++b){
        for(int ch=0;ch<2;++ch)for(int n=0;n<blockSize;++n)audio.setSample(ch,n,.1f);
        p->processBlock(audio,midi);
        for(int n=0;n<blockSize;++n){const float sample=audio.getSample(0,n);maxStep=std::max(maxStep,(double)std::abs(sample-previous));previous=sample;}
    }
    std::printf("MASTER_RENDER input automation max sample step=%.8g final=%.8g\n",maxStep,previous);
    if(maxStep>.002 || std::abs(previous-.1f*std::pow(10.f,6.f/20.f))>.001f)return 94;
    const auto ramp=[&](const char* id,float target,float expected){
        setProcessorParameter(*p,id,target);
        double step=0.;bool finite=true;
        for(int b=0;b<8;++b){
            for(int ch=0;ch<2;++ch)for(int n=0;n<blockSize;++n)audio.setSample(ch,n,.1f);
            p->processBlock(audio,midi);
            for(int n=0;n<blockSize;++n){const float sample=audio.getSample(0,n);finite=finite&&std::isfinite(sample);step=std::max(step,(double)std::abs(sample-previous));previous=sample;}
        }
        std::printf("MASTER_RENDER %s automation max sample step=%.8g final=%.8g\n",id,step,previous);
        return finite && step<.002 && std::abs(previous-expected)<.001f;
    };
    if(!ramp("mix",0.f,.1f) || !ramp("output",-6.f,.1f*std::pow(10.f,-6.f/20.f)))return 94;
    p->releaseResources();
    setProcessorParameter(*p,"mix",.37f);setProcessorParameter(*p,"output",-3.f);
    juce::MemoryBlock state;p->getStateInformation(state);
    auto restored=std::make_unique<SkellPresetAudioProcessor>();
    restored->setStateInformation(state.getData(),(int)state.getSize());
    if(std::abs(restored->state.getRawParameterValue("input")->load()-6.f)>.02f ||
       std::abs(restored->state.getRawParameterValue("mix")->load()-.37f)>.002f ||
       std::abs(restored->state.getRawParameterValue("output")->load()+3.f)>.02f)return 95;
    // Round-trip the same user-saved rack via the actual preset archive path.
    const auto personalFile=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("skell-master-qa-"+juce::Uuid().toString()+".skellpreset");
    struct DeleteFixture {juce::File file;~DeleteFixture(){file.deleteFile();}} cleanup{personalFile};
    if(p->exportPreset(personalFile).failed() || restored->importPreset(personalFile).failed())return 96;
    const float expected=.1f*(1.f+.37f*(std::pow(10.f,6.f/20.f)-1.f))*std::pow(10.f,-3.f/20.f);
    for(int mode:{0,1}){
        setProcessorParameter(*restored,"mode",(float)mode);
        restored->prepareToPlay(sr,blockSize);
        double peakError=0.;bool finite=true;
        for(int b=0;b<8;++b){
            for(int ch=0;ch<2;++ch)for(int n=0;n<blockSize;++n)audio.setSample(ch,n,.1f);
            restored->processBlock(audio,midi);
            if(b<4)continue;
            for(int ch=0;ch<2;++ch)for(int n=0;n<blockSize;++n){const float sample=audio.getSample(ch,n);finite=finite&&std::isfinite(sample);peakError=std::max(peakError,(double)std::abs(sample-expected));}
        }
        const int latency=restored->getLatencySamples();restored->releaseResources();
        std::printf("MASTER_RENDER personal mode=%d expected=%.8g peak_error=%.8g latency=%d\n",mode,expected,peakError,latency);
        if(!finite || peakError>2.e-5 || latency!=0)return 96;
    }
    std::puts("PASS: master gain, dry/wet interpolation, automation smoothing, PDC, state and personal preset recall");
    return 0;
}
int limiterStateSmoke() {
    using P=SkellPresetAudioProcessor;
    for(int type:{P::Limiter,P::StudioLimiterFX}) {
        auto p=std::make_unique<P>();p->clearRackToEmpty();p->setSlotType(0,type);p->setSlotOn(0,true);
        p->setSlotParam(0,0,.35f);p->setSlotParam(0,1,.2f);p->setSlotParam(0,2,1.f);
        p->setSlotParam(0,3,0.f);p->setSlotParam(0,4,0.f);
        setProcessorParameter(*p,"s1_limitAttack",1.25f);setProcessorParameter(*p,"s1_limitRelease",227.f);
        juce::MemoryBlock modern;p->getStateInformation(modern);
        auto q=std::make_unique<P>();q->setStateInformation(modern.getData(),(int)modern.getSize());
        const auto valuesMatch=[&](P& target,float attack,float release) {
            return target.slotType(0)==type && std::abs(target.state.getRawParameterValue("s1_limitAttack")->load()-attack)<.001f &&
                std::abs(target.state.getRawParameterValue("s1_limitRelease")->load()-release)<.001f &&
                target.getSlotParam(0,1)==.2f && target.getSlotParam(0,2)==1.f && target.getSlotParam(0,3)==0.f;
        };
        if(!valuesMatch(*q,1.25f,227.f))return 98;
        const auto path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("limiter-"+juce::Uuid().toString()+".skellpreset");
        const bool zip=p->exportPreset(path).wasOk() && q->importPreset(path).wasOk() && valuesMatch(*q,1.25f,227.f);
        path.deleteFile();if(!zip)return 98;
        auto xml=juce::AudioProcessor::getXmlFromBinary(modern.getData(),(int)modern.getSize());
        auto tree=juce::ValueTree::fromXml(*xml);
        for(int i=tree.getNumChildren();--i>=0;) {
            const auto id=tree.getChild(i).getProperty("id").toString();
            if(id.endsWith("_limitAttack") || id.endsWith("_limitRelease"))tree.removeChild(i,nullptr);
        }
        juce::MemoryBlock old;auto oldXml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*oldXml,old);
        q->setStateInformation(old.getData(),(int)old.getSize());if(!valuesMatch(*q,0.f,70.f))return 98;
        auto render=[&](float attack,float release) {
            q->setStateInformation(modern.getData(),(int)modern.getSize());
            setProcessorParameter(*q,"mode",1.f);setProcessorParameter(*q,"micGateOn",0.f);
            setProcessorParameter(*q,"s1_limitAttack",attack);setProcessorParameter(*q,"s1_limitRelease",release);
            q->prepareToPlay(48000.,256);std::vector<float> samples;
            juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
            for(int block=0;block<64;++block) {
                for(int n=0;n<256;++n) {
                    const int pos=block*256+n;const float value=(pos<1200?.9f:.04f)*std::sin(pos*.173f);
                    audio.setSample(0,n,value);audio.setSample(1,n,value*.8f);
                }
                q->processBlock(audio,midi);
                for(int n=0;n<256;++n){const float value=audio.getSample(0,n);if(!std::isfinite(value))return std::vector<float>{};samples.push_back(value);}
            }
            const int latency=q->getLatencySamples();q->releaseResources();if(latency!=144)return std::vector<float>{};
            return samples;
        };
        const auto fast=render(0.f,15.f),slow=render(0.f,400.f),attack=render(2.f,15.f);
        if(fast.empty() || fast.size()!=slow.size() || fast.size()!=attack.size())return 98;
        double attackDelta=0.,releaseDelta=0.;
        for(size_t i=0;i<fast.size();++i){if(i<1600)attackDelta+=std::abs(fast[i]-attack[i]);else releaseDelta+=std::abs(fast[i]-slow[i]);}
        std::printf("LIMITER_RENDER type=%d attack_delta=%.8g release_delta=%.8g PDC=144 host+ZIP+legacy recall PASS\n",type,attackDelta,releaseDelta);
        if(attackDelta<.001 || releaseDelta<.01)return 98;
    }
    return 0;
}
int studioSmoke() {
    {
        using P=SkellPresetAudioProcessor;auto a=std::make_unique<P>();auto b=std::make_unique<P>();
        a->clearRackToEmpty();b->clearRackToEmpty();
        auto set=[&](const char* id,float v){auto* p=a->state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));};
        set("micTubeGain",3.f);set("micTubeDrive",.5f);set("micTubeAir",2.f);set("micTubeTrim",-3.f);
        a->prepareToPlay(48000.,256);b->prepareToPlay(48000.,256);
        juce::AudioBuffer<float> x(2,256),y(2,256);juce::MidiBuffer midi;
        double diff=0.;
        for(int block=0;block<32;++block){
            if(block==8)set("micTubeOn",1.f);
            for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i){const float sample=.35f*std::sin((block*256+i)*.07f);x.setSample(ch,i,sample);y.setSample(ch,i,sample);}
            a->processBlock(x,midi);b->processBlock(y,midi);
            for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i){
                const float l=x.getSample(ch,i),r=y.getSample(ch,i);if(!std::isfinite(l))return 181;
                if(block<8 && l!=r)return 182;
                if(block>=16)diff+=std::abs(l-r);
            }
        }
        if(diff<.01 || a->getLatencySamples()!=b->getLatencySamples())return 183;
        juce::MemoryBlock state;a->getStateInformation(state);b->setStateInformation(state.getData(),(int)state.getSize());
        if(b->state.getRawParameterValue("micTubeOn")->load()<.5f || std::abs(b->state.getRawParameterValue("micTubeDrive")->load()-.5f)>.005f)return 184;
        a->releaseResources();b->releaseResources();
        a->loadFactoryPreset(0);if(a->state.getRawParameterValue("micTubeOn")->load()>.5f)return 185;
        std::puts("PASS: MIC preamp independent opt-in, exact off parity, audible-path difference, host recall and factory reset");
    }
    // All original starter chains must use native effects, retain zero
    // reported latency, round-trip through host state and render finite audio.
    {
        using P=SkellPresetAudioProcessor;
        auto p=std::make_unique<P>();
        for(int profile=0;profile<skellpreset::starter::count;++profile){
            if(p->loadVocalStarter(profile).failed() || p->loadedFactoryPresetIndex()!=-1 ||
               p->slotType(0)!=P::AdaptiveNoiseFX || p->slotType(7)!=P::StudioDelayFX ||
               p->state.getRawParameterValue("micGateOn")->load()<.5f)return 172;
            p->prepareToPlay(48000.,256);
            if(p->getLatencySamples()!=0)return 173;
            juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;double energy=0.;
            for(int block=0;block<100;++block){
                for(int ch=0;ch<2;++ch)for(int n=0;n<256;++n)audio.setSample(ch,n,.2f*std::sin((block*256+n)*.031f));
                p->processBlock(audio,midi);
                for(int n=0;n<256;++n){const float x=audio.getSample(0,n);if(!std::isfinite(x) || std::abs(x)>8.f)return 174;energy+=x*x;}
            }
            if(energy<.001)return 175;
            const auto name=p->presetName();juce::MemoryBlock state;p->getStateInformation(state);
            auto restored=std::make_unique<P>();restored->setStateInformation(state.getData(),(int)state.getSize());
            if(restored->presetName()!=name || restored->slotType(7)!=P::StudioDelayFX ||
                restored->state.getRawParameterValue("s8_duckOn")->load()<.5f)return 176;
            p->releaseResources();
        }
        if(p->loadVocalStarter(-1).wasOk())return 177;
        using namespace skellpreset::starter;
        for(int profile=0;profile<count;++profile)for(int variant=1;variant<=variations;++variant){
            auto a=make<P>(profile,variant),b=make<P>(profile,variant);
            if(a.name!=b.name || a.name==make<P>(profile,0).name || !a.name.startsWith("Skelly "))return 178;
            for(size_t s=0;s<a.slots.size();++s){
                if(a.slots[s].params!=b.slots[s].params || a.slots[s].type==P::StudioPitchFormantFX ||
                   a.slots[s].type==P::ExternalVST3 || a.slots[s].wet<0.f || a.slots[s].wet>1.f)return 179;
                for(float value:a.slots[s].params)if(!std::isfinite(value)||value<0.f||value>1.f)return 179;
            }
        }
        if(p->loadVocalStarter(0,65).wasOk() || p->loadVocalStarter(count-1,64).failed())return 180;
        std::puts("PASS: twenty original starters, 1280 bounded reproducible variants, zero reported latency and host-state recall for base profiles");
    }
    {
        using P=SkellPresetAudioProcessor;
        auto a=std::make_unique<P>();auto b=std::make_unique<P>();
        a->clearRackToEmpty();a->setSlotType(0,P::Compressor);a->setSlotOn(0,true);
        a->setSlotParam(0,0,0.f);a->setSlotParam(0,1,1.f);
        a->setPresentation(P::Presentation::Visual);
        juce::MemoryBlock saved;a->getStateInformation(saved);
        b->setStateInformation(saved.getData(),(int)saved.getSize());
        if(b->presentation()!=P::Presentation::Visual)return 168;
        b->setPresentation(P::Presentation::Clean);
        a->prepareToPlay(48000.,256);b->prepareToPlay(48000.,256);
        a->requestSlotVisual(0);
        juce::AudioBuffer<float> x(2,256),y(2,256);juce::MidiBuffer midi;
        for(int block=0;block<100;++block){
            for(int ch=0;ch<2;++ch)for(int j=0;j<256;++j){const float z=.35f*std::sin((block*256+j)*.03f);x.setSample(ch,j,z);y.setSample(ch,j,z);}
            a->processBlock(x,midi);b->processBlock(y,midi);
            for(int ch=0;ch<2;++ch)for(int j=0;j<256;++j)if(x.getSample(ch,j)!=y.getSample(ch,j))return 169;
        }
        skellpreset::visual::SlotVisualCapture::Frame frame;
        if(!a->getSlotVisual(0,frame) || !frame.sharedValid ||
           !(frame.sharedOutput.flags&skellcore::telemetry::kFlagGrValid) || frame.sharedOutput.grDbMax<=0.f ||
           frame.slowCount==0 || b->getSlotVisual(0,frame))return 170;
        a->requestSlotVisual(-1);if(a->getSlotVisual(0,frame))return 171;
        a->releaseResources();b->releaseResources();
        std::puts("PASS: persistent presentation, shared compressor telemetry, bit-identical audio with tap on/off");
    }
    if(const int limiterResult=limiterStateSmoke())return limiterResult;
    if(const int masterResult=masterSignalSmoke())return masterResult;
    // Host automation of newly appended parameters must change Empty Rack's
    // program identity to Custom Rack, just like every legacy control.
    // This regression exercises the registered APVTS listeners directly.
    {
        auto identity=std::make_unique<SkellPresetAudioProcessor>();
        if(identity->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex)
            return 87;
        auto* gate=identity->state.getParameter("micGateOn");
        if(!gate)return 88;
        gate->setValueNotifyingHost(gate->convertTo0to1(1.f));
        if(identity->getCurrentProgram()!=SkellPresetAudioProcessor::customRackProgramIndex)
            return 89;
        identity->clearRackToEmpty();
        if(identity->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex)
            return 90;
        auto* delay=identity->state.getParameter("s1_fxExt8");
        if(!delay)return 91;
        delay->setValueNotifyingHost(delay->convertTo0to1(1.f));
        if(identity->slotType(0)!=SkellPresetAudioProcessor::StudioDelayFX ||
           identity->getCurrentProgram()!=SkellPresetAudioProcessor::customRackProgramIndex)
            return 93;
        std::puts("PASS: new mic gate and Studio Delay automation preserves Empty/Custom program identity");
    }
    // Selecting any factory opts into the MIC gate without changing factory IDs.
    // Empty Rack and explicitly saved OFF host state must continue to restore OFF.
    {
        auto selected=std::make_unique<SkellPresetAudioProcessor>();
        const auto* gate=selected->state.getRawParameterValue("micGateOn");
        if(!gate || gate->load()>.5f) return 104;
        juce::MemoryBlock savedEmpty;
        selected->getStateInformation(savedEmpty);
        for(int program=0;program<SkellPresetAudioProcessor::emptyRackProgramIndex;++program) {
            selected->loadFactoryPreset(program);
            if(gate->load()<.5f || selected->getCurrentProgram()!=program)
                return 105;
        }
        selected->clearRackToEmpty();
        if(gate->load()>.5f ||
           selected->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex)
            return 106;
        selected->loadFactoryPreset(0);
        selected->setStateInformation(savedEmpty.getData(),(int)savedEmpty.getSize());
        const auto* restored=selected->state.getRawParameterValue("micGateOn");
        if(!restored || restored->load()>.5f ||
           selected->getCurrentProgram()!=SkellPresetAudioProcessor::emptyRackProgramIndex)
            return 107;
        // Emulate an older FL Studio factory snapshot with no micGateOn
        // parameter at all, and restore it over an active new factory gate.
        // The historical factory 43 identity and original OFF sound survive.
        selected->loadFactoryPreset(skellpreset::factoryIndexForRecipe(43));
        auto* gateParameter=selected->state.getParameter("micGateOn");
        if(!gateParameter) return 108;
        gateParameter->setValueNotifyingHost(gateParameter->convertTo0to1(0.f));
        juce::MemoryBlock oldFactory;
        selected->getStateInformation(oldFactory);
        auto oldXml=juce::AudioProcessor::getXmlFromBinary(
            oldFactory.getData(),(int)oldFactory.getSize());
        if(!oldXml) return 109;
        auto oldTree=juce::ValueTree::fromXml(*oldXml);
        std::array<bool,4> missingIds{};
        const juce::StringArray newGateIds{"micGateOn","micGateAttackMs",
            "micGateHysteresisDb","micGateDepth"};
        for(int child=oldTree.getNumChildren()-1;child>=0;--child) {
            const auto id=oldTree.getChild(child).getProperty("id").toString();
            const int which=newGateIds.indexOf(id);
            if(which>=0) {
                oldTree.removeChild(child,nullptr);
                missingIds[(size_t)which]=true;
            }
        }
        for(const auto removed:missingIds)if(!removed)return 110;
        auto missingGateXml=oldTree.createXml();
        if(!missingGateXml) return 111;
        juce::MemoryBlock oldFactoryWithoutGate;
        juce::AudioProcessor::copyXmlToBinary(*missingGateXml,oldFactoryWithoutGate);
        selected->loadFactoryPreset(skellpreset::factoryIndexForRecipe(43)); // New user selection enables the gate.
        if(selected->state.getRawParameterValue("micGateOn")->load()<.5f)
            return 112;
        // Deliberately dirty all added controls to verify missing-state
        // migration does not preserve stale values from a different preset.
        struct Change {const char* id;float value;};
        constexpr Change changed[]{
            {"micGateAttackMs",19.f},{"micGateHysteresisDb",10.f},
            {"micGateDepth",.2f}
        };
        for(const auto& change:changed) {
            auto* param=selected->state.getParameter(change.id);
            if(!param)return 118;
            param->setValueNotifyingHost(param->convertTo0to1(change.value));
        }
        selected->setStateInformation(oldFactoryWithoutGate.getData(),
                                      (int)oldFactoryWithoutGate.getSize());
        const auto* restoredAttack=selected->state.getRawParameterValue("micGateAttackMs");
        const auto* restoredHyst=selected->state.getRawParameterValue("micGateHysteresisDb");
        const auto* restoredDepth=selected->state.getRawParameterValue("micGateDepth");
        if(selected->state.getRawParameterValue("micGateOn")->load()>.5f ||
           selected->getCurrentProgram()!=skellpreset::factoryIndexForRecipe(43) ||
           !restoredAttack || std::abs(restoredAttack->load()-4.f)>.05f ||
           !restoredHyst || std::abs(restoredHyst->load()-3.f)>.05f ||
           !restoredDepth || std::abs(restoredDepth->load()-1.f)>.001f)
            return 113;
        std::puts("PASS: factory gate ON, saved OFF and legacy missing-gate FL host states remain OFF");
    }
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    processor->prepareToPlay(48000.,257);
    const int types[]{
        SkellPresetAudioProcessor::StudioEQFX,
        SkellPresetAudioProcessor::StudioLimiterFX,
        SkellPresetAudioProcessor::StudioCompressorFX,
        SkellPresetAudioProcessor::StudioFastDistFX,
        SkellPresetAudioProcessor::StudioDelayFX,
        SkellPresetAudioProcessor::AdlibChop,
        SkellPresetAudioProcessor::AdlibMotion,
        SkellPresetAudioProcessor::AdlibFilter,
        SkellPresetAudioProcessor::AdlibSaturate
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
        }else if(type>=SkellPresetAudioProcessor::AdlibChop &&
                 type<=SkellPresetAudioProcessor::AdlibSaturate) {
            processor->setSlotParam(0,0,.88f);
            processor->setSlotParam(0,1,.90f);
            processor->setSlotParam(0,2,.22f);
            processor->setSlotParam(0,3,1.f);
        }else if(type==SkellPresetAudioProcessor::StudioDelayFX) {
            processor->setSlotParam(0,0,.32f); // ~100 ms free-running echo
            processor->setSlotParam(0,1,.36f);
            processor->setSlotParam(0,2,.30f); // true stereo
            processor->setSlotParam(0,10,.68f); // wet echo
            processor->setSlotParam(0,11,1.f);  // full dry
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
    // Host state and native preset recall of new opt-in return duck controls.
    // Every previous factory slot, effect ID and wet state stays unchanged.
    {
        processor->setSlotType(0,SkellPresetAudioProcessor::Reverb);
        processor->setSlotOn(0,true);
        auto* duckOn=processor->state.getParameter("s1_duckOn");
        auto* duckDepth=processor->state.getParameter("s1_duckDepth");
        auto* duckRelease=processor->state.getParameter("s1_duckRelease");
        auto* duckAttack=processor->state.getParameter("s1_duckAttack");
        if(!duckOn || !duckDepth || !duckRelease || !duckAttack ||
           processor->getLatencySamples()!=0) return 98;
        duckOn->setValueNotifyingHost(duckOn->convertTo0to1(1.f));
        duckDepth->setValueNotifyingHost(duckDepth->convertTo0to1(.78f));
        duckRelease->setValueNotifyingHost(duckRelease->convertTo0to1(420.f));
        duckAttack->setValueNotifyingHost(duckAttack->convertTo0to1(26.f));
        juce::MemoryBlock packed;
        processor->getStateInformation(packed);
        auto recalled=std::make_unique<SkellPresetAudioProcessor>();
        recalled->setStateInformation(packed.getData(),(int)packed.getSize());
        const auto* on=recalled->state.getRawParameterValue("s1_duckOn");
        const auto* amount=recalled->state.getRawParameterValue("s1_duckDepth");
        const auto* attackSaved=recalled->state.getRawParameterValue("s1_duckAttack");
        if(!on || !amount || !attackSaved || on->load()<.5f ||
           std::abs(amount->load()-.78f)>.02f ||
           std::abs(attackSaved->load()-26.f)>.1f)return 99;
        const auto duckFile=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("skell-wet-sidechain-smoke",".skellpreset",false);
        if(processor->exportPreset(duckFile).failed())return 100;
        auto restored=std::make_unique<SkellPresetAudioProcessor>();
        const auto saved=restored->importPreset(duckFile);
        duckFile.deleteFile();
        const auto* restoredAttack=restored->state.getRawParameterValue("s1_duckAttack");
        if(saved.failed() || !restoredAttack ||
           restored->state.getRawParameterValue("s1_duckOn")->load()<.5f ||
           std::abs(restoredAttack->load()-26.f)>.1f)return 101;
        restored->loadFactoryPreset(0);
        if(restored->state.getRawParameterValue("s1_duckOn")->load()>.5f ||
           std::abs(restored->state.getRawParameterValue("s1_duckAttack")->load()-12.f)>.1f)
            return 102;
        std::puts("PASS: wet sidechain APVTS state, ZIP import/export, OFF factory reset, 0 PDC");
    }
    // Acoustic proof through the real compiled JUCE audio processor:
    // a voiced input must duck the ECHO, not the signal path's dry vocal.
    // Compare identical 400 Hz stereo input with opt-in sidechain OFF/ON.
    {
        const auto runDelay=[&](bool enabled)->std::array<double,2> {
            auto p=std::make_unique<SkellPresetAudioProcessor>();
            p->prepareToPlay(48000.,256);
            p->setSlotType(0,SkellPresetAudioProcessor::StudioDelayFX);
            p->setSlotOn(0,true);
            p->setSlotParam(0,0,0.f); // 25 ms
            p->setSlotParam(0,1,.50f);
            p->setSlotParam(0,2,.30f); // true stereo
            p->setSlotParam(0,4,0.f);
            p->setSlotParam(0,5,.85f);
            p->setSlotParam(0,7,0.f);
            p->setSlotParam(0,9,0.f);
            p->setSlotParam(0,10,1.f); // full echo
            p->setSlotParam(0,11,1.f); // exact unity dry
            p->setSlotParam(0,12,0.f);
            if(auto* amount=p->state.getParameter("s1_duckDepth"))
                amount->setValueNotifyingHost(amount->convertTo0to1(.90f));
            if(auto* on=p->state.getParameter("s1_duckOn"))
                on->setValueNotifyingHost(on->convertTo0to1(enabled?1.f:0.f));
            std::array<double,2> energy{};
            for(int block=0;block<250;++block) {
                juce::AudioBuffer<float> b(2,256);
                for(int n=0;n<256;++n) {
                    const int index=block*256+n;
                    const float voice=index<48000?(float)(
                        .3*std::sin(2.*juce::MathConstants<double>::pi*
                                      400.*index/48000.)):0.f;
                    b.setSample(0,n,voice);
                    b.setSample(1,n,voice);
                }
                juce::MidiBuffer midi;
                p->processBlock(b,midi);
                for(int n=0;n<256;++n) {
                    const int index=block*256+n;
                    const double dry=index<48000?
                        .3*std::sin(2.*juce::MathConstants<double>::pi*
                                     400.*index/48000.):0.;
                    const double echo=(double)b.getSample(0,n)-dry;
                    if(!std::isfinite(echo)) {
                        p->releaseResources();
                        return std::array<double,2>{{-1.,-1.}};
                    }
                    if(index>12000&&index<45000)energy[0]+=echo*echo;
                    if(index>49000&&index<56000)energy[1]+=echo*echo;
                }
            }
            p->releaseResources();
            return energy;
        };
        const auto without=runDelay(false),with=runDelay(true);
        if(!(without[0]>.01 && with[0]>=0. &&
             with[0]<without[0]*.33 && with[1]>=0.)) {
            std::fprintf(stderr,"VOICE SIDECHAIN AUDIO: dry-echo %.7f, ducked-echo %.7f, tails %.7f / %.7f\n",
                         without[0],with[0],without[1],with[1]);
            return 103;
        }
        std::printf("PASS: compiled Studio Delay wet return ducked %.1f dB while dry vocal stays unity; zero new latency\n",
                    10.*std::log10(std::max(1.e-8,with[0]/without[0])));
    }
    // Real compiled processor: independent sends must preserve the dry
    // signal, not feed into each other, and respect serial rack PDC.
    {
        using P=SkellPresetAudioProcessor;
        constexpr int frames=8192,blockSize=256;
        auto renderSends=[=](bool delayOn,bool dubOn,bool latency) {
            auto p=std::make_unique<P>();p->clearRackToEmpty();
            auto set=[&](const juce::String& id,float value) {
                if(auto* param=p->state.getParameter(id))param->setValueNotifyingHost(param->convertTo0to1(value));
            };
            if(delayOn){
                p->setSlotType(0,P::Delay);p->setSlotOn(0,true);
                p->setSlotParam(0,0,0.f);p->setSlotParam(0,1,0.f);
                set("s1_parallelReturn",1.f);set("s1_wet",.5f);
            }
            if(dubOn){
                p->setSlotType(1,P::AdlibDoubler);p->setSlotOn(1,true);
                set("s2_parallelReturn",1.f);set("s2_wet",.5f);
            }
            if(latency){p->setSlotType(2,P::StudioPitchFormantFX);p->setSlotOn(2,false);}
            p->prepareToPlay(48000.,blockSize);
            const int expected=latency?skellpreset::dsp::SpectralPitchFormant::latencySamples:0;
            if(p->getLatencySamples()!=expected)return std::vector<float>{};
            juce::AudioBuffer<float> audio(2,blockSize);juce::MidiBuffer midi;
            for(int i=0;i<24;++i){audio.clear();p->processBlock(audio,midi);}
            std::vector<float> result;result.reserve(frames);
            for(int i=0;i<frames;i+=blockSize){
                audio.clear();
                if(i==0){audio.setSample(0,0,.25f);audio.setSample(1,0,.25f);}
                p->processBlock(audio,midi);
                for(int n=0;n<blockSize;++n)result.push_back(audio.getSample(0,n));
            }
            p->releaseResources();return result;
        };
        const auto dry=renderSends(false,false,false),delay=renderSends(true,false,false),
            dub=renderSends(false,true,false),both=renderSends(true,true,false),
            aligned=renderSends(true,true,true);
        if(dry.size()!=frames || delay.size()!=frames || dub.size()!=frames ||
           both.size()!=frames || aligned.size()!=frames)return 161;
        const int lag=skellpreset::dsp::SpectralPitchFormant::latencySamples;
        if(std::abs(dry[0]-.25f)>1.e-5f || std::abs(both[0]-dry[0])>1.e-5f)return 162;
        double delayEnergy=0.,dubEnergy=0.;
        for(int n=0;n<frames;++n){
            if(!std::isfinite(both[(size_t)n]) || !std::isfinite(aligned[(size_t)n]))return 163;
            const float d=delay[(size_t)n]-dry[(size_t)n],u=dub[(size_t)n]-dry[(size_t)n];
            delayEnergy+=d*d;dubEnergy+=u*u;
            if(std::abs(both[(size_t)n]-(dry[(size_t)n]+d+u))>2.e-5f)return 164;
            const float expected=n>=lag?both[(size_t)(n-lag)]:0.f;
            if(std::abs(aligned[(size_t)n]-expected)>2.e-5f)return 165;
        }
        if(delayEnergy<1.e-6 || dubEnergy<1.e-6)return 166;
        std::printf("PASS: parallel delay/dub independence, dry impulse and %d-sample rack PDC\n",lag);
    }
    {
        auto p=std::make_unique<SkellPresetAudioProcessor>();p->clearRackToEmpty();
        p->setSlotType(0,SkellPresetAudioProcessor::Delay);p->setSlotOn(0,true);
        auto set=[&](const juce::String& id,float v){
            if(auto* param=p->state.getParameter(id))param->setValueNotifyingHost(param->convertTo0to1(v));
        };
        set("micGateOn",1.f);set("micGateThreshold",-10.f);set("micGateDepth",1.f);
        set("s1_duckOn",1.f);set("s1_duckDepth",.9f);set("s1_duckThreshold",-60.f);
        set("s1_duckRelease",50.f);
        p->prepareToPlay(48000.,256);
        auto measure=[&](int source){
            set("s1_duckSource",(float)source);
            juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
            for(int i=0;i<220;++i){
                for(int n=0;n<256;++n){audio.setSample(0,n,.03f);audio.setSample(1,n,.03f);}
                p->processBlock(audio,midi);
            }
            return p->getSlotDuckReductionDb(0);
        };
        const float post=measure(0),pre=measure(1),postAgain=measure(0);
        p->releaseResources();
        if(!std::isfinite(pre) || pre<8.f || post>1.f || postAgain>1.f){
            std::fprintf(stderr,"DETECTOR SOURCE: post %.2f pre %.2f post %.2f dB\n",post,pre,postAgain);
            return 167;
        }
        std::printf("PASS: measured pre/post MIC detector difference %.2f dB\n",pre-post);
    }
    // Exercise the FRONT-END mic gate through the actual JUCE audio callback.
    // It must be opt-in, zero-PDC, stereo linked, recallable, and bypass clean.
    processor->setSlotOn(0,false);
    processor->setSlotType(0,SkellPresetAudioProcessor::Empty);
    auto* micGate=processor->state.getParameter("micGateOn");
    auto* micThreshold=processor->state.getParameter("micGateThreshold");
    if(!micGate || !micThreshold || processor->slotType(0)!=SkellPresetAudioProcessor::Empty)
        return 76;
    micThreshold->setValueNotifyingHost(micThreshold->convertTo0to1(-20.f));
    if(processor->getLatencySamples()!=0)return 77;
    auto gateEnergy=[&](float left,float right) {
        std::array<double,2> energy{};
        for(int block=0;block<100;++block) {
            juce::AudioBuffer<float> audio(2,257);
            for(int n=0;n<257;++n) {
                audio.setSample(0,n,left);
                audio.setSample(1,n,right);
            }
            juce::MidiBuffer midi;
            processor->processBlock(audio,midi);
            if(block>=75)for(int ch=0;ch<2;++ch)
                for(int n=0;n<257;++n) {
                    const double sample=audio.getSample(ch,n);
                    if(!std::isfinite(sample))
                        return std::array<double,2>{-1.,-1.};
                    energy[(size_t)ch]+=sample*sample;
                }
        }
        return energy;
    };
    micGate->setValueNotifyingHost(micGate->convertTo0to1(1.f));
    const auto quietGate=gateEnergy(.004f,.004f);
    // A loud left input must hold the same gate open on a quiet right channel.
    const auto stereoLinked=gateEnergy(.35f,.005f);
    micGate->setValueNotifyingHost(micGate->convertTo0to1(0.f));
    const auto quietBypass=gateEnergy(.004f,.004f);
    if(quietBypass[1]<.1 || quietGate[1]<0. ||
       quietGate[1]>=quietBypass[1]*.05 || stereoLinked[1]<.08) {
        std::fprintf(stderr,"Mic gate audio failed: quiet-on %.6f stereo-linked %.6f bypass %.6f\n",
                     quietGate[1],stereoLinked[1],quietBypass[1]);
        return 78;
    }
    micGate->setValueNotifyingHost(micGate->convertTo0to1(1.f));
    // Prove the newly appended REDUCTION control affects the actual
    // processBlock output. Zero reduction is the same audible dry gate bypass.
    auto* depthControl=processor->state.getParameter("micGateDepth");
    auto* attackControl=processor->state.getParameter("micGateAttackMs");
    auto* hysteresisControl=processor->state.getParameter("micGateHysteresisDb");
    if(!depthControl || !attackControl || !hysteresisControl)return 114;
    depthControl->setValueNotifyingHost(depthControl->convertTo0to1(0.f));
    const auto zeroReduction=gateEnergy(.004f,.004f);
    depthControl->setValueNotifyingHost(depthControl->convertTo0to1(1.f));
    const auto fullReduction=gateEnergy(.004f,.004f);
    if(!(zeroReduction[1]>quietBypass[1]*.95 &&
         fullReduction[1]>=0. && fullReduction[1]<zeroReduction[1]*.05))
        return 115;
    attackControl->setValueNotifyingHost(attackControl->convertTo0to1(11.f));
    hysteresisControl->setValueNotifyingHost(hysteresisControl->convertTo0to1(7.f));
    depthControl->setValueNotifyingHost(depthControl->convertTo0to1(.4f));
    juce::MemoryBlock micState;
    processor->getStateInformation(micState);
    auto recalledMic=std::make_unique<SkellPresetAudioProcessor>();
    recalledMic->setStateInformation(micState.getData(),(int)micState.getSize());
    const auto* gateRestored=recalledMic->state.getRawParameterValue("micGateOn");
    const auto* thresholdRestored=recalledMic->state.getRawParameterValue("micGateThreshold");
    if(!gateRestored || gateRestored->load()<.5f ||
       !thresholdRestored || std::abs(thresholdRestored->load()+20.f)>.05f)
        return 79;
    const auto* savedAttack=recalledMic->state.getRawParameterValue("micGateAttackMs");
    const auto* savedHyst=recalledMic->state.getRawParameterValue("micGateHysteresisDb");
    const auto* savedDepth=recalledMic->state.getRawParameterValue("micGateDepth");
    if(!savedAttack || std::abs(savedAttack->load()-11.f)>.05f ||
       !savedHyst || std::abs(savedHyst->load()-7.f)>.05f ||
       !savedDepth || std::abs(savedDepth->load()-.4f)>.001f)
        return 116;
    std::puts("PASS: input noise gate bypass, stereo link, zero PDC and host-state recall");
    // A new factory selection enables its front-end gate with safe knob
    // defaults; restored project and package states keep saved ON/OFF choices.
    recalledMic->loadFactoryPreset(0);
    const auto gateRelease=recalledMic->state.getRawParameterValue("micGateRelease");
    const auto gateHold=recalledMic->state.getRawParameterValue("micGateHold");
    const auto* attack=recalledMic->state.getRawParameterValue("micGateAttackMs");
    const auto* hysteresis=recalledMic->state.getRawParameterValue("micGateHysteresisDb");
    const auto* depth=recalledMic->state.getRawParameterValue("micGateDepth");
    if(!attack || std::abs(attack->load()-4.f)>.05f ||
       !hysteresis || std::abs(hysteresis->load()-3.f)>.05f ||
       !depth || std::abs(depth->load()-1.f)>.001f ||
       gateRestored->load()<.5f || std::abs(thresholdRestored->load()+45.f)>.05f ||
       !gateRelease || std::abs(gateRelease->load()-160.f)>.05f ||
       !gateHold || std::abs(gateHold->load()-55.f)>.05f)
        return 94;
    // Portable .skellpreset must preserve the new gate, not only host binary
    // state. Previous schema-v2 files without gate fields remain OFF.
    const auto gateFile=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skell-studio-gate-roundtrip",".skellpreset",false);
    if(processor->exportPreset(gateFile).failed())return 95;
    auto importedGate=std::make_unique<SkellPresetAudioProcessor>();
    const auto importResult=importedGate->importPreset(gateFile);
    gateFile.deleteFile();
    if(importResult.failed())return 96;
    const auto* importedOn=importedGate->state.getRawParameterValue("micGateOn");
    const auto* importedThreshold=importedGate->state.getRawParameterValue("micGateThreshold");
    if(!importedOn || importedOn->load()<.5f || !importedThreshold ||
       std::abs(importedThreshold->load()+20.f)>.05f)
        return 97;
    const auto* pkgAttack=importedGate->state.getRawParameterValue("micGateAttackMs");
    const auto* pkgHyst=importedGate->state.getRawParameterValue("micGateHysteresisDb");
    const auto* pkgDepth=importedGate->state.getRawParameterValue("micGateDepth");
    if(!pkgAttack || std::abs(pkgAttack->load()-11.f)>.05f ||
       !pkgHyst || std::abs(pkgHyst->load()-7.f)>.05f ||
       !pkgDepth || std::abs(pkgDepth->load()-.4f)>.001f)
        return 117;
    std::puts("PASS: factory gate ON reset and native package gate roundtrip");
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
        // Recipe IDs survive factory-bank compaction; public program indexes do not.
        constexpr int liveLead=skellpreset::factoryIndexForRecipe(25);
        constexpr int noiseGuard=skellpreset::factoryIndexForRecipe(40);
        constexpr int studioSix=skellpreset::factoryIndexForRecipe(42);
        constexpr int cleanLead=skellpreset::factoryIndexForRecipe(0);
        static_assert(liveLead>=0 && noiseGuard>=0 && studioSix>=0 && cleanLead>=0);
        static_assert(std::string_view(skellpreset::factoryPresets[liveLead].name)=="Skell Live Lead");
        static_assert(std::string_view(skellpreset::factoryPresets[noiseGuard].name)=="Noise Guard");
        static_assert(std::string_view(skellpreset::factoryPresets[studioSix].name)=="Studio Six");
        struct LatencyCase { int preset; bool lookahead; };
        const LatencyCase cases[]{
            {skellpreset::alchemist::factoryIndex,false},
            {skellpreset::alchemist_serial_dynamics_trial::factoryIndex,true},
            {skellpreset::alchemist_low_end_contour_trial::factoryIndex,true},
            {skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex,true},
            {liveLead,false},{noiseGuard,true},{studioSix,false},{cleanLead,true}
        };
        for(const auto& test:cases) {
            auto p=std::make_unique<SkellPresetAudioProcessor>();
            p->loadFactoryPreset(test.preset);
            p->prepareToPlay(sr,64);
            const int expected=test.lookahead?std::max(48,(int)(sr*.003)):0;
            if(p->getLatencySamples()!=expected) {
                std::fprintf(stderr,
                    "FAIL factory %d (%s; recipe %d) / %.0f Hz: reported=%d expected=%d samples\n",
                    test.preset,p->getProgramName(test.preset).toRawUTF8(),
                    skellpreset::recipeForFactoryIndex(test.preset),
                    sr,p->getLatencySamples(),expected);
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
int vocalPreviewSmoke() {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("SkellVocalPreview","",false);
    if(root.createDirectory().failed())return 1;
    struct Cleanup{juce::File dir;~Cleanup(){dir.deleteRecursively();}} cleanup{root};
    const auto dry=root.getChildFile("dry.wav"),wet=root.getChildFile("wet.wav");
    const auto preset=root.getChildFile("latency.skellpreset");
    constexpr int samples=4099;
    juce::AudioBuffer<float> signal(1,samples);signal.clear();signal.setSample(0,0,.25f);
    for(int n=1;n<samples;++n)signal.setSample(0,n,.125f*std::sin(n*.07f));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(dry.createOutputStream());
    if(!stream)return 2;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriter::Options{}
        .withSampleRate(48000.).withNumChannels(1).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriter::Options::SampleFormat::floatingPoint));
    if(!writer || !writer->writeFromAudioSampleBuffer(signal,0,samples))return 3;
    writer.reset();
    auto p=std::make_unique<SkellPresetAudioProcessor>();p->clearRackToEmpty();
    p->setSlotType(0,SkellPresetAudioProcessor::StudioPitchFormantFX);p->setSlotOn(0,false);
    if(p->exportPreset(preset).failed())return 4;
    p.reset();
    if(skellpreset::preview::render(dry,wet,-1,preset).failed())return 5;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wet));
    if(!reader || reader->lengthInSamples!=samples || reader->numChannels!=2)return 6;
    juce::AudioBuffer<float> restored(2,samples);
    if(!reader->read(&restored,0,samples,0,true,true))return 7;
    for(int ch=0;ch<2;++ch)for(int n=0;n<samples;++n)
        if(std::abs(restored.getSample(ch,n)-signal.getSample(0,n))>1.e-5f)return 8;
    reader.reset();
    if(skellpreset::preview::render(dry,wet,-1,preset).wasOk())return 9;
    const auto starter=root.getChildFile("starter.wav");
    if(skellpreset::preview::render(dry,starter,1,{},.05).failed())return 10;
    reader.reset(formats.createReaderFor(starter));
    if(!reader || reader->lengthInSamples!=samples+2400)return 11;
    reader.reset();
    const auto pair=root.getChildFile("pair");
    if(skellpreset::preview::renderMatchedPair(dry,pair,-1,preset,.05).failed())return 12;
    if(!pair.getChildFile("listen.html").existsAsFile()||!pair.getChildFile("processed-raw.wav").existsAsFile())return 13;
    std::unique_ptr<juce::AudioFormatReader> aa(formats.createReaderFor(pair.getChildFile("A-dry.wav"))),
        bb(formats.createReaderFor(pair.getChildFile("B-processed.wav")));
    if(!aa||!bb||aa->lengthInSamples!=samples+2400||bb->lengthInSamples!=aa->lengthInSamples)return 14;
    skellpreset::preview::Metrics am,bm;
    if(skellpreset::preview::measureFile(*aa,samples,am).failed()||
       skellpreset::preview::measureFile(*bb,samples,bm).failed()||
       std::abs(am.rms()-bm.rms())>1.e-6||am.peak>.891252||bm.peak>.891252)return 15;
    if(skellpreset::preview::renderMatchedPair(dry,pair,-1,preset).wasOk())return 16;
    std::puts("PASS: native preview, latency alignment, matched audition WAVs, HTML player, no overwrite and explicit tail");
    return 0;
}
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
    plugin->setCurrentProgram(skellpreset::factoryIndexForRecipe(37)); // legacy host program: Eight-Band Air
    if(plugin->getCurrentProgram()!=skellpreset::factoryIndexForRecipe(37) ||
       plugin->loadedFactoryPresetIndex()!=skellpreset::factoryIndexForRecipe(37) ||
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
    if(restored->getCurrentProgram()!=skellpreset::factoryIndexForRecipe(37) ||
       restored->loadedFactoryPresetIndex()!=skellpreset::factoryIndexForRecipe(37) ||
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

// Output-stage stereo separation exercised through the REAL processor at
// 44.1/48/96/192 kHz: neutral null, mono compatibility, mid/side response,
// automation smoothing, peak safety, block-size consistency, preset recall
// and factory neutrality.
int stereoSeparationSmoke() {
    int failures = 0;
    auto fail = [&failures](const char* what) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    };
    auto setP = [](SkellPresetAudioProcessor& p, const char* id, float real) {
        if (auto* param = p.state.getParameter(id))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
                param->setValueNotifyingHost(
                    ranged->getNormalisableRange().convertTo0to1(real));
    };
    auto valueOf = [](SkellPresetAudioProcessor& p, const char* id) {
        auto* raw = p.state.getRawParameterValue(id);
        return raw ? raw->load() : 0.f;
    };
    auto fill = [](juce::AudioBuffer<float>& b, int index, int chunk, bool mono) {
        for (int n = 0; n < b.getNumSamples(); ++n) {
            const float t = (float)(index * chunk + n) / 48000.f;
            const float l = .22f * std::sin(2.f * juce::MathConstants<float>::pi * 220.f * t)
                          + .11f * std::sin(2.f * juce::MathConstants<float>::pi * 1370.f * t);
            const float r = .19f * std::sin(2.f * juce::MathConstants<float>::pi * 330.f * t)
                          + .07f * std::sin(2.f * juce::MathConstants<float>::pi * 910.f * t);
            b.setSample(0, n, l);
            b.setSample(1, n, mono ? l : r);
        }
    };
    auto peakOf = [](const juce::AudioBuffer<float>& b) {
        double p = 0.;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int n = 0; n < b.getNumSamples(); ++n)
                p = std::max(p, (double)std::abs(b.getSample(ch, n)));
        return p;
    };

    for (double rate : {44100., 48000., 96000., 192000.}) {
        SkellPresetAudioProcessor plugin;
        plugin.prepareToPlay(rate, blockSize);
        // A brand-new instance is the Empty Rack: no slots, no mic stage. That
        // isolates the output stage so a residual cannot be blamed on an
        // effect's own stereo colouring.
        setP(plugin, "mix", 1.f);
        setP(plugin, "input", 0.f);
        setP(plugin, "output", 0.f);
        setP(plugin, "stereoSeparation", 0.f);

        // Neutral null against a second reference pass after leaving neutral.
        std::vector<float> reference;
        for (int block = 0; block < 8; ++block) {
            juce::AudioBuffer<float> b(2, blockSize);
            fill(b, block, blockSize, false);
            juce::MidiBuffer midi;
            plugin.processBlock(b, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < blockSize; ++n)
                    reference.push_back(b.getSample(ch, n));
        }
        setP(plugin, "stereoSeparation", 96.f);
        for (int block = 0; block < 8; ++block) {
            juce::AudioBuffer<float> drain(2, blockSize);
            fill(drain, block, blockSize, false);
            juce::MidiBuffer midi;
            plugin.processBlock(drain, midi);
        }
        setP(plugin, "stereoSeparation", 0.f);
        for (int block = 0; block < 4; ++block) {
            juce::AudioBuffer<float> drain(2, blockSize);
            fill(drain, block, blockSize, false);
            juce::MidiBuffer midi;
            plugin.processBlock(drain, midi);
        }
        size_t cursor = 0;
        double nullDiff = 0.;
        for (int block = 0; block < 8; ++block) {
            juce::AudioBuffer<float> b(2, blockSize);
            fill(b, block, blockSize, false);
            juce::MidiBuffer midi;
            plugin.processBlock(b, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < blockSize; ++n)
                    nullDiff = std::max(nullDiff,
                        (double)std::abs(b.getSample(ch, n) - reference[cursor++]));
        }
        if (nullDiff != 0.) fail("neutral separation is not bit-exactly transparent");

        // Mono compatibility at every position. The processor derives mono
        // operation from the declared bus layout, so this case advertises a
        // genuine mono input rather than a two-channel buffer of equal sides.
        for (int raw : {-96, -40, 0, 55, 96}) {
            SkellPresetAudioProcessor mono;
            mono.setPlayConfigDetails(1, 2, rate, blockSize);
            mono.prepareToPlay(rate, 512);
            
            setP(mono, "mix", 1.f);
            setP(mono, "input", 0.f);
            setP(mono, "output", 0.f);
            setP(mono, "stereoSeparation", (float)raw);
            for (int block = 0; block < 4; ++block) {
                juce::AudioBuffer<float> drain(2, blockSize);
                fill(drain, block, blockSize, true);
                juce::MidiBuffer midi;
                mono.processBlock(drain, midi);
            }
            double side = 0.;
            for (int block = 0; block < 5; ++block) {
                juce::AudioBuffer<float> b(2, blockSize);
                fill(b, block, blockSize, true);
                juce::MidiBuffer midi;
                mono.processBlock(b, midi);
                for (int n = 0; n < blockSize; ++n)
                    side = std::max(side, (double)std::abs(
                        b.getSample(0, n) - b.getSample(1, n)));
            }
            if (side != 0.) fail("mono input gained a side signal");
        }

        // Independent expected transfer for the actual processor. A static
        // hard-panned probe separates width law, gain, and routing errors.
        // No private audio is required for this deterministic regression.
        for (int raw : {-96,-72,-48,-24,0,24,48,72,96}) {
            setP(plugin,"stereoSeparation",static_cast<float>(raw));
            const int settleBlocks=static_cast<int>(std::ceil(rate*.020/blockSize))+1;
            for(int block=0;block<settleBlocks;++block) {
                juce::AudioBuffer<float> drain(2,blockSize);
                fill(drain,block,blockSize,false);
                juce::MidiBuffer midi;
                plugin.processBlock(drain,midi);
            }
            juce::AudioBuffer<float> probe(2,blockSize);
            fill(probe,7,blockSize,false);
            const float travel=static_cast<float>(raw)/96.f;
            const float width=1.f-(travel<=0.f?travel:static_cast<float>((std::pow(5.,static_cast<double>(travel))-1.)*.25));
            juce::AudioBuffer<float> original;
            original.makeCopyOf(probe);
            juce::MidiBuffer midi;
            plugin.processBlock(probe,midi);
            double error=0.;
            bool exactMono=true;
            for(int n=0;n<blockSize;++n) {
                const float mid=.5f*(original.getSample(0,n)+original.getSample(1,n));
                const float side=.5f*(original.getSample(0,n)-original.getSample(1,n))*width;
                error=std::max(error,static_cast<double>(std::abs(probe.getSample(0,n)-(mid+side))));
                error=std::max(error,static_cast<double>(std::abs(probe.getSample(1,n)-(mid-side))));
                if(raw==96 && probe.getSample(0,n)!=probe.getSample(1,n))exactMono=false;
            }
            if(!exactMono)fail("maximum mono retained a side signal");
            if(error>2.e-5)fail("actual processor separation transfer or center gain differs");
        }

        // Peak safety and finiteness.
        for (int raw : {-96, -60, 0, 60, 96}) {
            setP(plugin, "stereoSeparation", (float)raw);
            for (int block = 0; block < 4; ++block) {
                juce::AudioBuffer<float> drain(2, blockSize);
                fill(drain, block, blockSize, false);
                juce::MidiBuffer midi;
                plugin.processBlock(drain, midi);
            }
            double inPeak = 0., outPeak = 0.;
            for (int block = 0; block < 5; ++block) {
                juce::AudioBuffer<float> b(2, blockSize);
                fill(b, block, blockSize, false);
                inPeak = std::max(inPeak, peakOf(b));
                juce::MidiBuffer midi;
                plugin.processBlock(b, midi);
                outPeak = std::max(outPeak, peakOf(b));
            }
            const double bound = (double)skellpreset::dsp::MixerStereoSeparation
                ::peakGainBound(raw) * inPeak + 1.e-5;
            if (outPeak > bound) fail("output exceeded the matrix peak bound");
            if (!std::isfinite(outPeak) || outPeak >= 4.0)
                fail("output was not finite and bounded");
        }

        // Automation smoothing: no sample-to-sample step on a full swing.
        {
            setP(plugin, "stereoSeparation", -96.f);
            for (int block = 0; block < 4; ++block) {
                juce::AudioBuffer<float> drain(2, blockSize);
                fill(drain, block, blockSize, false);
                juce::MidiBuffer midi;
                plugin.processBlock(drain, midi);
            }
            setP(plugin, "stereoSeparation", 96.f);
            juce::AudioBuffer<float> b(2, blockSize);
            fill(b, 20, blockSize, false);
            juce::MidiBuffer midi;
            plugin.processBlock(b, midi);
            double step = 0.;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 1; n < blockSize; ++n)
                    step = std::max(step, (double)std::abs(
                        b.getSample(ch, n) - b.getSample(ch, n - 1)));
            if (step >= .6) fail("automation swing stepped instead of gliding");
        }

        // Block-size consistency of the separation matrix itself. The plugin's
        // 20 ms master smoothing is intentionally block-rate dependent, so
        // verifying the output stage means driving the module directly with an
        // identical absolute sample grid chopped into different block sizes.
        {
            const size_t total = (size_t)(rate * .12);
            std::vector<float> srcL(total), srcR(total);
            for (size_t i = 0; i < total; ++i) {
                const float t = (float)i / (float)rate;
                srcL[i] = .22f * std::sin(2.f * juce::MathConstants<float>::pi * 220.f * t)
                        + .11f * std::sin(2.f * juce::MathConstants<float>::pi * 1370.f * t);
                srcR[i] = .19f * std::sin(2.f * juce::MathConstants<float>::pi * 330.f * t)
                        + .07f * std::sin(2.f * juce::MathConstants<float>::pi * 910.f * t);
            }
            auto renderAt = [&](int chunk) {
                skellpreset::dsp::MixerStereoSeparation sep;
                sep.prepare(rate);
                sep.setRaw(96);
                auto l = srcL, r = srcR;
                for (size_t offset = 0; offset < total; offset += (size_t)chunk) {
                    const int n = (int)std::min((size_t)chunk, total - offset);
                    sep.process(l.data() + offset, r.data() + offset, n);
                }
                return std::make_pair(l, r);
            };
            const auto small = renderAt(127);
            const auto large = renderAt(512);
            double diff = 0.;
            for (size_t i = 0; i < total; ++i) {
                diff = std::max(diff, (double)std::abs(small.first[i] - large.first[i]));
                diff = std::max(diff, (double)std::abs(small.second[i] - large.second[i]));
            }
            if (diff != 0.) fail("block-size change altered the separation result");
        }
    }

    // Preset recall through export/import.
    {
        SkellPresetAudioProcessor plugin;
        plugin.prepareToPlay(48000., blockSize);
        plugin.loadFactoryPreset(0);
        setP(plugin, "stereoSeparation", -61.f);
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("skell-stereo-recall-", ".skellpreset", false);
        if (!plugin.exportPreset(file).wasOk()) fail("export with separation failed");
        setP(plugin, "stereoSeparation", 88.f);
        if (!plugin.importPreset(file).wasOk()) fail("import with separation failed");
        if (std::abs(valueOf(plugin, "stereoSeparation") + 61.f) >= .01f)
            fail("preset recall lost the saved separation");
        file.deleteFile();
    }

    // Every factory program loads at verified neutral.
    {
        SkellPresetAudioProcessor plugin;
        plugin.prepareToPlay(48000., blockSize);
        for (int preset = 0; preset < (int)skellpreset::factoryPresets.size(); ++preset) {
            setP(plugin, "stereoSeparation", 77.f);
            plugin.loadFactoryPreset(preset);
            if (valueOf(plugin, "stereoSeparation") != 0.f)
                fail("factory preset did not reset separation to neutral");
        }
    }

    if (failures == 0)
        std::puts("PASS Stereo separation: 44.1/48/96/192k null, mono, peaks, smoothing, block size, recall, factories");
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    if(argc==2 && juce::String(argv[1])=="--preview-smoke")return vocalPreviewSmoke();
    if(argc>=5 && argc<=7 && (juce::String(argv[1])=="--preview-starter" || juce::String(argv[1])=="--preview-preset" ||
                                juce::String(argv[1])=="--audition-starter" || juce::String(argv[1])=="--audition-preset")){
        const bool starter=juce::String(argv[1]).endsWith("-starter");
        const bool audition=juce::String(argv[1]).startsWith("--audition-");
        const juce::String profile(argv[2]);
        int profileId=0,variant=0;
        if(starter){
            const auto parts=juce::StringArray::fromTokens(profile,":","");
            if(parts.size()<1 || parts.size()>2)return 7;
            if(parts[0].isEmpty() || !parts[0].containsOnly("0123456789") || parts[0].length()>2)return 7;
            profileId=parts[0].getIntValue();
            if(parts.size()==2){if(parts[1].isEmpty() || parts[1].length()>2 || !parts[1].containsOnly("0123456789"))return 7;variant=parts[1].getIntValue();}
            if(profileId<1 || profileId>skellpreset::starter::count || variant>skellpreset::starter::variations)return 7;
        }
        double tail=0.,bpm=120.;
        if(argc>=6){char* end=nullptr;tail=std::strtod(argv[5],&end);if(end==argv[5] || *end!='\0')return 7;}
        if(argc==7){char* end=nullptr;bpm=std::strtod(argv[6],&end);if(end==argv[6] || *end!='\0')return 7;}
        const auto source=juce::File(juce::String::fromUTF8(argv[3])),destination=juce::File(juce::String::fromUTF8(argv[4]));
        const auto package=starter?juce::File{}:juce::File(juce::String::fromUTF8(argv[2]));
        const auto result=audition?skellpreset::preview::renderMatchedPair(source,destination,starter?profileId-1:-1,package,tail,bpm,variant):
            skellpreset::preview::render(source,destination,starter?profileId-1:-1,package,tail,bpm,variant);
        if(result.failed()){std::fprintf(stderr,"%s\n",result.getErrorMessage().toRawUTF8());return 8;}
        return 0;
    }
    if(argc==2 && juce::String(argv[1])=="--smoke")return smoke();
    if(argc==2 && juce::String(argv[1])=="--smoke-blank")return blankSmoke();
    if(argc==2 && juce::String(argv[1])=="--smoke-slot-order")return strictSlotOrderSmoke();
    if(argc==2 && juce::String(argv[1])=="--ui-smoke")return uiSmoke();
    if(argc==2 && juce::String(argv[1])=="--studio-smoke")return studioSmoke();
    if(argc==2 && juce::String(argv[1])=="--master-signal-smoke")return masterSignalSmoke();
    if(argc==2 && juce::String(argv[1])=="--native-trials-smoke")return nativeTrialsSmoke();
    if(argc==2 && juce::String(argv[1])=="--lowend-override-smoke")return lowEndOverrideSmoke();
    if(argc==2 && juce::String(argv[1])=="--percent-smoke")return percentDisplaySmoke();
    if(argc==2 && juce::String(argv[1])=="--stress")return stress();
    if(argc==2 && juce::String(argv[1])=="--smoke-mono")return monoSmoke();
    if(argc==2 && juce::String(argv[1])=="--stereo-separation")return stereoSeparationSmoke();
    if(argc==7 && juce::String(argv[1])=="--factory-override") {
        const int factory=skellpreset::factoryIndexForRecipe(juce::String(argv[2]).getIntValue());
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
                      skellpreset::factoryIndexForRecipe(requested.getIntValue()));
    }
    if(argc!=3) {
        std::fprintf(stderr,"Usage: SkellPresetAlchemistRender --factory 43|44|45|46|47|48|49|50|51|52|53|54 INPUT.wav NEW_OUTPUT.wav\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --factory-override 54 HPF_NORMALIZED BAND70_GAIN_NORMALIZED INPUT.wav NEW_OUTPUT.wav\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --audition-starter PROFILE[:VARIANT] INPUT NEW_FOLDER [TAIL_SECONDS] [BPM]\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --audition-preset PRESET.skellpreset INPUT NEW_FOLDER [TAIL_SECONDS] [BPM]\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --preview-starter PROFILE[:VARIANT] INPUT NEW_OUTPUT.wav [TAIL_SECONDS] [BPM]\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --preview-preset PRESET.skellpreset INPUT NEW_OUTPUT.wav [TAIL_SECONDS] [BPM]\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --lowend-override-smoke\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --compare CANDIDATE.wav REFERENCE.wav\n");
        return 1;
    }
    return render(juce::File(juce::String::fromUTF8(argv[1])),
                  juce::File(juce::String::fromUTF8(argv[2])));
}

