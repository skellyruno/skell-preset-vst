#include "PluginProcessor.h"
#include "chain/VocalStarterChains.h"
#include "chain/LiveMatchOptimizer.h"
#include "PluginEditor.h"
#include "chain/EffectRegistry.h"
#include "chain/AlchemistNativePreset.h"
#include "chain/NativeSoundMatchTrials.h"
#include "chain/RackRealtimePolicy.h"
#include <charconv>
#include <cmath>
#include <string>
#include <system_error>

using namespace skellpreset;
using namespace skellpreset::dsp;
namespace {
const juce::StringArray types=[] {
    juce::StringArray result;
    for(const auto& effect:skellpreset::registry::effects) result.add(effect.name);
    return result;
}();
static_assert(skellpreset::registry::count()==SkellPresetAudioProcessor::NumberOfModules);
juce::String slotPrefix(int i) {return "s"+juce::String(i+1)+"_";}

// Host state is external input. JUCE's normalisable-range clamp relies on
// comparisons, so NaN can survive and poison APVTS atomics. Repair values
// before replaceState(): malformed/non-finite values use the declared
// parameter default, finite values clamp, and discrete controls snap legally.
void sanitizeParameterStateValues(
    juce::ValueTree& loaded,
    juce::AudioProcessorValueTreeState& parameters) {
    for (int childIndex = 0; childIndex < loaded.getNumChildren(); ++childIndex) {
        auto child = loaded.getChild(childIndex);
        if (!child.hasProperty("id") || !child.hasProperty("value"))
            continue;

        auto* parameter = parameters.getParameter(child.getProperty("id").toString());
        if (parameter == nullptr)
            continue;

        const auto& range = parameter->getNormalisableRange();
        float fallback = range.convertFrom0to1(parameter->getDefaultValue());
        if (!std::isfinite(fallback))
            fallback = range.start;
        fallback = juce::jlimit(range.start, range.end, fallback);
        if (parameter->isDiscrete())
            fallback = range.snapToLegalValue(fallback);
        if (!std::isfinite(fallback))
            fallback = range.start;

        const auto text = child.getProperty("value").toString().trim().toStdString();
        double parsed = 0.0;
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), parsed, std::chars_format::general);
        const bool valid = !text.empty() && result.ec == std::errc{}
            && result.ptr == text.data() + text.size() && std::isfinite(parsed);

        float restored = fallback;
        if (valid) {
            const float bounded = juce::jlimit(
                range.start, range.end, static_cast<float>(parsed));
            const float legal = parameter->isDiscrete()
                ? range.snapToLegalValue(bounded) : bounded;
            if (std::isfinite(legal))
                restored = legal;
        }
        child.setProperty("value", restored, nullptr);
    }
}
}

SkellPresetAudioProcessor::SkellPresetAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                .withInput("External Sidechain",juce::AudioChannelSet::stereo(),false)
                                .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  state(*this,nullptr,"PARAMETERS",createParameterLayout()) {
    std::uint64_t initialOrder=0;
    for(int i=0;i<slotCount;i++){
        initialOrder|=(std::uint64_t)i << (4*i);
        slotPeaks[(size_t)i].store(0);
        slotGainReductionDb[(size_t)i].store(0);
        slotDuckReductionDb[(size_t)i].store(0);
        slotDeessReductionDb[(size_t)i].store(0);
        for(auto& meter:slotEqReductionDb[(size_t)i])meter.store(0.f,std::memory_order_relaxed);
        for(auto& value:suiteReturnRms[(size_t)i])value.store(0);
        auto& r=rawSlots[(size_t)i];const auto prefix=slotPrefix(i);
        r.type=state.getRawParameterValue(prefix+"type");
        r.extension=state.getRawParameterValue(prefix+"fxExt");
        r.extension2=state.getRawParameterValue(prefix+"fxExt2");
        r.extension3=state.getRawParameterValue(prefix+"fxExt3");
        r.extension4=state.getRawParameterValue(prefix+"fxExt4");
        r.extension5=state.getRawParameterValue(prefix+"fxExt5");
        r.extension6=state.getRawParameterValue(prefix+"fxExt6");
        r.extension7=state.getRawParameterValue(prefix+"fxExt7");
        r.extension8=state.getRawParameterValue(prefix+"fxExt8");
        r.extension9=state.getRawParameterValue(prefix+"fxExt9");
        r.extension10=state.getRawParameterValue(prefix+"fxExt10");
        r.extension11=state.getRawParameterValue(prefix+"fxExt11");
        r.extension12=state.getRawParameterValue(prefix+"fxExt12");
        r.on=state.getRawParameterValue(prefix+"on");
        r.wet=state.getRawParameterValue(prefix+"wet");
        r.duckOn=state.getRawParameterValue(prefix+"duckOn");
        r.rightFirst=state.getRawParameterValue(prefix+"rightFirst");
        r.dubDuckOn=state.getRawParameterValue(prefix+"dubDuckOn");
        r.duckSource=state.getRawParameterValue(prefix+"duckSource");
        r.parallelReturn=state.getRawParameterValue(prefix+"parallelReturn");
        r.duckExternal=state.getRawParameterValue(prefix+"duckExternal");
        r.duckEffect=state.getRawParameterValue(prefix+"duckEffect");
        r.duckDepth=state.getRawParameterValue(prefix+"duckDepth");
        r.duckThreshold=state.getRawParameterValue(prefix+"duckThreshold");
        r.duckRelease=state.getRawParameterValue(prefix+"duckRelease");
        r.duckAttack=state.getRawParameterValue(prefix+"duckAttack");
        r.limitAttack=state.getRawParameterValue(prefix+"limitAttack");
        r.limitRelease=state.getRawParameterValue(prefix+"limitRelease");
        r.eqLowCut=state.getRawParameterValue(prefix+"eqLowCut");
        r.eqHighCut=state.getRawParameterValue(prefix+"eqHighCut");
        for(int k=0;k<16;k++)r.p[(size_t)k]=state.getRawParameterValue(prefix+"p"+juce::String(k+1));
        for(int k=0;k<8;++k){
            r.q[(size_t)k]=state.getRawParameterValue(prefix+"eqQ"+juce::String(k+1));
            r.studioEqKind[(size_t)k]=state.getRawParameterValue(prefix+"studioEqKind"+juce::String(k+1));
            r.studioEqOn[(size_t)k]=state.getRawParameterValue(prefix+"studioEqOn"+juce::String(k+1));
            r.studioEqDynamic[(size_t)k]=state.getRawParameterValue(prefix+"studioEqDynamic"+juce::String(k+1));
            r.studioEqThreshold[(size_t)k]=state.getRawParameterValue(prefix+"studioEqThreshold"+juce::String(k+1));
            r.studioEqAttack[(size_t)k]=state.getRawParameterValue(prefix+"studioEqAttack"+juce::String(k+1));
            r.studioEqRelease[(size_t)k]=state.getRawParameterValue(prefix+"studioEqRelease"+juce::String(k+1));
            r.studioEqSlope[(size_t)k]=state.getRawParameterValue(prefix+"studioEqSlope"+juce::String(k+1));
            r.studioEqExtraShape[(size_t)k]=state.getRawParameterValue(prefix+"studioEqExtraShape"+juce::String(k+1));
        }
    }
    packedOrder.store(initialOrder);
    rawMic.on=state.getRawParameterValue("micPreOn");
    rawMic.input=state.getRawParameterValue("micPreInput");
    rawMic.target=state.getRawParameterValue("micPreTarget");
    rawMic.amount=state.getRawParameterValue("micPreAmount");
    rawMic.trim=state.getRawParameterValue("micPreTrim");
    rawMic.gateOn=state.getRawParameterValue("micGateOn");
    rawMic.gateThreshold=state.getRawParameterValue("micGateThreshold");
    rawMic.gateRelease=state.getRawParameterValue("micGateRelease");
    rawMic.gateHold=state.getRawParameterValue("micGateHold");
    rawMic.gateAttack=state.getRawParameterValue("micGateAttackMs");
    rawMic.gateHysteresis=state.getRawParameterValue("micGateHysteresisDb");
    rawMic.gateDepth=state.getRawParameterValue("micGateDepth");
    rawMic.tubeOn=state.getRawParameterValue("micTubeOn");
    rawMic.tubeGain=state.getRawParameterValue("micTubeGain");
    rawMic.tubeDrive=state.getRawParameterValue("micTubeDrive");
    rawMic.tubeAir=state.getRawParameterValue("micTubeAir");
    rawMic.tubeTrim=state.getRawParameterValue("micTubeTrim");
    rawGlobal.mode=state.getRawParameterValue("mode");
    rawGlobal.input=state.getRawParameterValue("input");
    rawGlobal.mix=state.getRawParameterValue("mix");
    rawGlobal.output=state.getRawParameterValue("output");
    rawGlobal.gate=state.getRawParameterValue("globalGate");
    rawGlobal.threshold=state.getRawParameterValue("gateThreshold");
    rawGlobal.delta=state.getRawParameterValue("delta");
    rawGlobal.stereoSeparation=state.getRawParameterValue("stereoSeparation");
    for(int k=0;k<4;k++)rawGlobal.macros[(size_t)k]=state.getRawParameterValue("macro"+juce::String(k+1));
    addProgramIdentityListeners();
    // New instances start with a clean, latency-free rack. APVTS slot type
    // and bypass defaults are already Empty/OFF (all 16 slots).
    selectedProgram=-2;
    emptyRackIdentity.store(true,std::memory_order_release);
    name="Empty Rack";
    state.state.setProperty("presetName",name,nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout SkellPresetAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto fl=[&](juce::String id,juce::String label,float low,float high,float initial,float step=0.001f){
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},label,juce::NormalisableRange<float>(low,high,step),initial));
    };
    auto percent=[&](juce::String id,juce::String label,float low,float high,float initial,float step=0.001f){
        // Host-side string formatting only: preserve numerical range, value,
        // ID and automation normalization. Native host displays 0..100%.
        const auto factory=juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([low,high](float value,int){
                return juce::String(juce::roundToInt(100.f*
                    (value-low)/(high-low)))+"%";
            })
            .withValueFromStringFunction([low,high](const juce::String& text){
                return low+(high-low)*juce::jlimit(0.f,100.f,
                    text.getFloatValue())*.01f;
            }); // getText() already includes '%'; avoid duplicate host suffixes
        p.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id,1},label,juce::NormalisableRange<float>(low,high,step),initial,factory));
    };
    auto bl=[&](juce::String id,juce::String label,bool initial){
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},label,initial));
    };
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Mode",juce::StringArray{"Simple","Advanced","Vocoding","Bypass"},1));
    percent("input","Input",-24,24,0);percent("mix","Mix",0,1,1);
    percent("output","Output",-24,24,0);
    bl("autoGain","Auto gain (reserved)",false);bl("delta","Delta",false);
    bl("adapt","Adapt (reserved)",false);fl("adaptAmount","Adapt amount (reserved)",0,1,0);
    bl("globalGate","Legacy global gate (inactive; use rack Gate)",false);fl("gateThreshold","Gate threshold",-80,0,-50);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"laneMode",1},"Lane mode (reserved)",juce::StringArray{"Series","Parallel"},0));
    for(int k=1;k<=8;k++) percent("macro"+juce::String(k),"Macro "+juce::String(k),0,1,.5f);
    // Preserve the original 0..22 choice count (and host normalization).
    auto legacyTypes=types;
    while(legacyTypes.size()>RingModFX+1)
        legacyTypes.remove(legacyTypes.size()-1);
    for(int s=1;s<=slotCount;s++) {
        const auto id="s"+juce::String(s)+"_";
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id+"type",1},"Slot "+juce::String(s)+" type",legacyTypes,0));
        bl(id+"on","Slot "+juce::String(s)+" active",false);
        percent(id+"wet","Slot "+juce::String(s)+" wet",0,1,1);
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id+"lane",1},"Slot "+juce::String(s)+" lane (reserved)",juce::StringArray{"A","B","C"},0));
        for(int k=1;k<=16;k++)fl(id+"p"+juce::String(k),"Slot "+juce::String(s)+" Parameter "+juce::String(k),0,1,.5f);
    }
    for(auto lane: {"A","B","C"}) {
        const auto pre="lane"+juce::String(lane);
        fl(pre+"_level",pre+" level (reserved)",-24,12,0);
        fl(pre+"_pan",pre+" pan (reserved)",-1,1,0);
        bl(pre+"_mute",pre+" mute (reserved)",false);
    }
    // Append, don't insert, new host parameters so every existing parameter
    // keeps its original position as well as its stable VST3 ID.
    for(int s=1;s<=slotCount;++s) {
        const auto id="s"+juce::String(s)+"_fxExt";
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id,1},"Slot "+juce::String(s)+" extra effect",
            juce::StringArray{"None","Vocal Rider","Ducking Ping-Pong","Vocal Vibrato"},0));
    }
    // Keep the earlier 4-choice extension unchanged for projects saved
    // with the previous beta. A second extension carries the advanced suite.
    for(int s=1;s<=slotCount;++s) {
        const auto id="s"+juce::String(s)+"_fxExt2";
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id,1},"Slot "+juce::String(s)+" advanced effect",
            juce::StringArray{"None","8-Band EQ","FET + Opto","Harshness Tamer","Adaptive Noise Control"},0));
    }
    // Independent Q is appended after both type-extension generations;
    // existing projects and automation indices are never renormalized.
    for(int s=1;s<=slotCount;++s)
        for(int band=1;band<=8;++band)
            fl("s"+juce::String(s)+"_eqQ"+juce::String(band),
               "Slot "+juce::String(s)+" EQ Band "+juce::String(band)+" Q",
               0,1,.5f);
    // Another appended choice avoids renormalizing any published type ID
    // or the advanced suite introduced in this development branch.
    for(int s=1;s<=slotCount;++s)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(s)+"_fxExt3",1},
            "Slot "+juce::String(s)+" studio pitch effect",
            juce::StringArray{"None","Studio Pitch + Formant"},0));
    // New IDs append after every published parameter. Zero = OFF, so all
    // imported projects and prior factory banks retain their exact sound.
    for(int slot=1;slot<=slotCount;++slot) {
        const auto prefix="s"+juce::String(slot)+"_";
        fl(prefix+"eqLowCut","Slot "+juce::String(slot)+" Parametric EQ low cut",0,1,0);
        fl(prefix+"eqHighCut","Slot "+juce::String(slot)+" Parametric EQ high cut",0,1,0);
    }
    // Append a new generation; never expand fxExt3 choices because changing
    // choice normalization would corrupt old Studio Pitch automation.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt4",1},
            "Slot "+juce::String(slot)+" Alchemist matching effect",
            juce::StringArray{"None","Alchemist Match Finish"},0));
    // New generation is essential: extending fxExt4 would renormalize the
    // published Alchemist choice and change existing FL Studio sessions.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt5",1},
            "Slot "+juce::String(slot)+" Mic Modeling effect",
            juce::StringArray{"None","Mic Modeling"},0));
    // All five front-end parameters are APPENDED after fxExt5. Old slot,
    // Alchemist and FL Studio automation IDs keep their original positions.
    bl("micPreOn","Mic Modeling front-end active",false);
    p.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"micPreInput",1},"Recorded microphone type",
        juce::StringArray{"Neutral","Condenser","Dynamic","Ribbon"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"micPreTarget",1},"Simulated microphone character",
        juce::StringArray{"Flat","Warm","Focus","Air"},0));
    fl("micPreAmount","Mic Modeling front-end amount",0,1,1);
    fl("micPreTrim","Mic Modeling front-end trim",-0.f,1.f,.5f);
    // New generation is append-only. Never renormalize fxExt5 or earlier IDs.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt6",1},
            "Slot "+juce::String(slot)+" Skelldizer effect",
            juce::StringArray{"None","Skelldizer"},0));
    // Append this new choice generation only after all existing host parameters.
    // No legacy choice's normalization, position, or saved ID is modified.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt7",1},
            "Slot "+juce::String(slot)+" Studio native DSP effect",
            juce::StringArray{"None","Studio Parametric EQ (Plain)","Studio Limiter Hybrid",
                              "Studio Compressor","Studio Fast Dist"},0));
    // Appended after all established host parameters: no old choice
    // normalization or automation ID changes.
    for(int slot=1;slot<=slotCount;++slot){
        const auto pre="s"+juce::String(slot)+"_";
        const int initialKinds[]{3,0,0,0,0,0,4,0};
        for(int band=1;band<=8;++band){
            const auto suffix=juce::String(band);
            p.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{pre+"studioEqKind"+suffix,1},
                "Slot "+juce::String(slot)+" Studio EQ B"+suffix+" type",
                juce::StringArray{"Peak","High Pass","Low Pass","Low Shelf","High Shelf"},
                initialKinds[band-1]));
            bl(pre+"studioEqOn"+suffix,
               "Slot "+juce::String(slot)+" Studio EQ B"+suffix+" enabled",
               band<=7);
        }
    }
    // Mic Noise Gate is independent of the original mic model and starts OFF.
    // Append all host IDs after the existing Studio EQ and slot parameters.
    bl("micGateOn","Mic input noise gate active",false);
    fl("micGateThreshold","Mic input gate threshold", -80.f,-10.f,-45.f);
    fl("micGateRelease","Mic input gate release (ms)",20.f,1000.f,160.f);
    fl("micGateHold","Mic input gate hold (ms)",0.f,250.f,55.f);
    // New independent effect selector: published fxExt7 normalization stays unchanged.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt8",1},
            "Slot "+juce::String(slot)+" Studio delay effect",
            juce::StringArray{"None","Studio Delay"},0));
    // New independent choice generation. Never extend or renormalize fxExt8.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt9",1},
            "Slot "+juce::String(slot)+" shared Adlib effects",
            juce::StringArray{"None","Adlib CHOP","Adlib MOTION",
                              "Adlib FILTER","Adlib SATURATE"},0));
    // New parameter IDs are strictly appended after every existing FX choice.
    // OFF by default keeps factory/legacy projects unchanged.
    for(int slot=1;slot<=slotCount;++slot) {
        const auto prefix="s"+juce::String(slot)+"_";
        bl(prefix+"duckOn","Slot "+juce::String(slot)+" vocal sidechain",false);
        fl(prefix+"duckDepth","Slot "+juce::String(slot)+" duck depth",0.f,1.f,.65f);
        fl(prefix+"duckThreshold","Slot "+juce::String(slot)+" duck threshold dBFS",
           -60.f,-12.f,-32.f);
        fl(prefix+"duckRelease","Slot "+juce::String(slot)+" duck release ms",
           50.f,1500.f,320.f);
    }
    // Append after all previous parameters, including 16x four sidechain
    // controls, so existing FL Studio automation indices cannot shift.
    for(int slot=1;slot<=slotCount;++slot) {
        const auto prefix="s"+juce::String(slot)+"_";
        fl(prefix+"duckAttack","Slot "+juce::String(slot)+" ducking attack ms",
           2.f,80.f,12.f);
    }
    // All three new controls append after every existing host ID.
    fl("micGateAttackMs","Mic gate attack ms",.5f,25.f,4.f);
    fl("micGateHysteresisDb","Mic gate hysteresis dB",0.f,12.f,3.f);
    fl("micGateDepth","Mic gate reduction",0.f,1.f,1.f);
    // Native output-stage stereo separation. Appended last so every earlier
    // host ID keeps its published index/normalization. The raw range is the
    // supplied -96..+96 (negative widens, positive merges to mono, 0 neutral).
    // Integer steps keep host automation and the UI readout on the same grid.
    p.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"stereoSeparation",1},
        "Stereo separation",
        juce::NormalisableRange<float>(-96.f,96.f,1.f),0.f));
    // NEW, appended option. Never add to fxExt9: its host choice range is
    // published and changing its normalization would break saved DAW states.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt10",1},
            "Slot "+juce::String(slot)+" Vocal Feel opto effect",
            juce::StringArray{"None","Vocal Feel Opto"},0));
    // Full five-parallel-chain suite. New generation avoids renormalizing
    // the separate Opto fxExt10 or any earlier project/automation choice.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt11",1},
            "Slot "+juce::String(slot)+" Vocal Feel Suite effect",
            juce::StringArray{"None","Vocal Feel Suite"},0));
    // Append timing controls after every published parameter. Defaults reproduce
    // the historical instantaneous detector and 70ms release exactly.
    for(int slot=1;slot<=slotCount;++slot) {
        const auto pre="s"+juce::String(slot)+"_";
        fl(pre+"limitAttack","Slot "+juce::String(slot)+" limiter attack ms",0.f,3.f,0.f);
        fl(pre+"limitRelease","Slot "+juce::String(slot)+" limiter release ms",5.f,500.f,70.f);
    }
    // Append only: preserve existing automation indices and mode normalization.
    for(int slot=1;slot<=slotCount;++slot) {
        const auto prefix="s"+juce::String(slot)+"_";
        bl(prefix+"rightFirst","Slot "+juce::String(slot)+" right-first ping pong",false);
        bl(prefix+"dubDuckOn","Slot "+juce::String(slot)+" dub sidechain",false);
    }
    for(int slot=1;slot<=slotCount;++slot) {
        const auto prefix="s"+juce::String(slot)+"_";
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{prefix+"duckSource",1},
            "Slot "+juce::String(slot)+" duck source",juce::StringArray{"Post MIC","Pre MIC","Slot input"},0));
        bl(prefix+"parallelReturn","Slot "+juce::String(slot)+" parallel return",false);
    }
    // Independent MIC preamp: append IDs, never repurpose micPre* (mic model).
    bl("micTubeOn","Mic Preamp active",false);
    fl("micTubeGain","Mic Preamp input dB",-12.f,12.f,0.f);
    fl("micTubeDrive","Mic Preamp tube drive",0.f,1.f,0.f);
    fl("micTubeAir","Mic Preamp air dB",0.f,6.f,0.f);
    fl("micTubeTrim","Mic Preamp output dB",-12.f,12.f,0.f);
    // Append, do not extend the existing three-choice parameter: its normalized automation must remain stable.
    for(int slot=1;slot<=slotCount;++slot)
        bl("s"+juce::String(slot)+"_duckExternal","Slot "+juce::String(slot)+" external sidechain key",false);
    for(int slot=1;slot<=slotCount;++slot)
        bl("s"+juce::String(slot)+"_duckEffect","Slot "+juce::String(slot)+" effect contribution sidechain",false);
    // Append a new independent type flag: all earlier host indices and choice ranges remain unchanged.
    for(int slot=1;slot<=slotCount;++slot)
        p.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"s"+juce::String(slot)+"_fxExt12",1},
            "Slot "+juce::String(slot)+" phase rotation effect",
            juce::StringArray{"None","Phase Rotation"},0));
    for(int slot=1;slot<=slotCount;++slot)for(int band=1;band<=8;++band)
        fl("s"+juce::String(slot)+"_studioEqDynamic"+juce::String(band),"Slot "+juce::String(slot)+" EQ band "+juce::String(band)+" dynamic depth",0.f,1.f,0.f);
    for(int slot=1;slot<=slotCount;++slot)for(int band=1;band<=8;++band){
        const auto id="s"+juce::String(slot)+"_studioEq",suffix=juce::String(band);
        fl(id+"Threshold"+suffix,"Slot "+juce::String(slot)+" EQ band "+suffix+" threshold",-80.f,0.f,-24.f);
        fl(id+"Attack"+suffix,"Slot "+juce::String(slot)+" EQ band "+suffix+" attack",0.5f,200.f,12.f);
        fl(id+"Release"+suffix,"Slot "+juce::String(slot)+" EQ band "+suffix+" release",5.f,2000.f,110.f);
        fl(id+"Slope"+suffix,"Slot "+juce::String(slot)+" EQ band "+suffix+" slope",0.f,2.f,0.f,1.f);
    }
    for(int slot=1;slot<=slotCount;++slot)for(int band=1;band<=8;++band)
        fl("s"+juce::String(slot)+"_studioEqExtraShape"+juce::String(band),"Slot "+juce::String(slot)+" EQ band "+juce::String(band)+" extra shape",0.f,2.f,0.f,1.f);
    return p;
}
void SkellPresetAudioProcessor::EngineSlot::prepare(double sampleRate) {
    phaseRotation.prepare(sampleRate);
    peak.prepare(sampleRate);highpass.prepare(sampleRate);comp.prepare(sampleRate);gate.prepare(sampleRate);
    deesser.prepare(sampleRate);
    radio.prepare(sampleRate);crusher.prepare(sampleRate);tremolo.prepare(sampleRate);
    width.prepare(sampleRate);threeBand.prepare(sampleRate);
    voxRoom.prepare(sampleRate);voxComp.prepare(sampleRate);
    chorus.prepare(sampleRate);flanger.prepare(sampleRate);ring.prepare(sampleRate);
    adlibFx.prepare(sampleRate);
    studioEq.prepare(sampleRate);studioComp.prepare(sampleRate);
    studioLimiterComp.prepare(sampleRate);studioGate.prepare(sampleRate);
    studioDist.prepare(sampleRate);
    rider.prepare(sampleRate);duckingDelay.prepare(sampleRate);vibrato.prepare(sampleRate);
    eightBand.prepare(sampleRate);serialDynamics.prepare(sampleRate);matchFinish.prepare(sampleRate);
    micModel.prepare(sampleRate);skelldizer.prepare(sampleRate);
    harshness.prepare(sampleRate);noiseControl.prepare(sampleRate);
    studioPitch.prepare(sampleRate);
    pitchDry.prepare(skellpreset::dsp::SpectralPitchFormant::latencySamples);
    delay.prepare(sampleRate);studioDelay.prepare(sampleRate);
    vocalFeel.prepare(sampleRate);
    vocalSuite.prepare(sampleRate);
    reverb[0].prepare(sampleRate);
    doubler.prepare(sampleRate);limiter.prepare(sampleRate,2);
    limiterDry.prepare(limiter.latencySamples());
    hostDry.prepare(skellpreset::host::HostedVST3::maxTotalLatency);
    phrase.prepare(sampleRate);phrase.setThreshold(.018f);
    const auto length=(size_t)(sampleRate*1.25)+4;
    for(auto& line:throwHistory)line.assign(length,0.f);
    throwWrite=throwRead=0;throwRemaining=0;phrasePreviouslyActive=false;
    manualPreviouslyActive=false;throwFilterL=throwFilterR=0;throwFilterPole=.6f;
}
void SkellPresetAudioProcessor::EngineSlot::processThrow(float l,float r,float& outL,float& outR,float amount,float feedback,float tone,float trigger,double sampleRate) noexcept {
    (void)amount;(void)feedback;(void)tone;
    if(throwHistory[0].empty()){outL=l;outR=r;return;}
    const auto cap=throwHistory[0].size();
    throwHistory[0][throwWrite]=l;throwHistory[1][throwWrite]=r;
    phrase.processSample(std::max(std::abs(l),std::abs(r)));
    const bool manual=trigger>.65f;
    const bool active=phrase.active();
    if((phrasePreviouslyActive && !active) || (!manualPreviouslyActive && manual)) {
        // Re-read ~240ms preceding the phrase-end gap into the feedback delay.
        throwRemaining=std::max(1,(int)(sampleRate*.24));
        const size_t lookback=(size_t)(sampleRate*.42)+(size_t)throwRemaining;
        throwRead=(throwWrite+cap-(lookback%cap))%cap;
    }
    phrasePreviouslyActive=active;manualPreviouslyActive=manual;
    float sourceL=0,sourceR=0;
    if(throwRemaining>0){
        sourceL=throwHistory[0][throwRead];sourceR=throwHistory[1][throwRead];
        throwRead=(throwRead+1)%cap;--throwRemaining;
    }
    const auto echo=delay.process(sourceL,sourceR);
    throwFilterL=throwFilterPole*throwFilterL+(1.f-throwFilterPole)*echo[0];
    throwFilterR=throwFilterPole*throwFilterR+(1.f-throwFilterPole)*echo[1];
    outL=l+throwFilterL;outR=r+throwFilterR;
    throwWrite=(throwWrite+1)%cap;
}
void SkellPresetAudioProcessor::updateHostLatency() {
    // Only slots on the audible chain contribute to host PDC. An invisible
    // expanded slot must never add reported latency.
    const int latency=limiterLookahead.load(std::memory_order_acquire);
    const int count=activeSlotCount();
    const auto order=packedOrder.load(std::memory_order_acquire);
    int total=0;
    for(int position=0;position<count;++position) {
        const int id=(int)((order >> (4*position)) & 0x0fU);
        if(id<0 || id>=slotCount || !rawSlots[(size_t)id].type)continue;
        const int type=slotType(id);
        total+=skellpreset::realtime::stageLatency(
            (type==StudioLimiterFX?Limiter:type),Limiter,ExternalVST3,StudioPitchFormantFX,latency,
            type==ExternalVST3?hosted.getLatency(id):0,
            skellpreset::dsp::SpectralPitchFormant::latencySamples);
    }
    if(total!=currentReportedLatency){currentReportedLatency=total;setLatencySamples(total);}
}

juce::Result SkellPresetAudioProcessor::startCapture(const juce::File& file) {
    return capture.start(file,preparedAudioRate());
}
void SkellPresetAudioProcessor::stopCapture() {
    capture.stop();
}

void SkellPresetAudioProcessor::prepareToPlay(double sampleRate,int samplesPerBlock) {
    // A sample-rate / host-device change invalidates any ongoing WAV header.
    // Finalize the previous take on the non-realtime preparation path.
    capture.stop();
    liveScope.reset();
    liveSpectrumInput.reset();liveSpectrumOutput.reset();slotVisual.invalidate();
    rate=(std::isfinite(sampleRate)&&sampleRate>=8000.)?sampleRate:48000.;
    displayedRate.store(rate,std::memory_order_release);
    slotVisual.prepare(rate);
    hostChunkCapacity=juce::jlimit(64,8192,juce::jmax(512,samplesPerBlock));
    rackWork.setSize(2,hostChunkCapacity,false,true);
    sidechainWork.setSize(2,hostChunkCapacity,false,true);
    rawDetectorWork.setSize(2,hostChunkCapacity,false,true);
    parallelWork.setSize(2,hostChunkCapacity,false,true);
    for(auto& scope:slotInputScope)scope.reset();
    for(auto& scope:slotOutputScope)scope.reset();
    hostWork.setSize(2,hostChunkCapacity,false,true);
    dryWork.setSize(2,hostChunkCapacity,false,true);
    for(auto& e:engines) {
        e.prepare(rate);
        e.voiceDuck.prepare(rate);
    }
    for(auto& meter:slotGainReductionDb)meter.store(0.f,std::memory_order_relaxed);
    for(auto& meter:slotDuckReductionDb)meter.store(0.f,std::memory_order_relaxed);
    for(auto& meter:slotDeessReductionDb)meter.store(0.f,std::memory_order_relaxed);
    for(auto& row:slotEqReductionDb)for(auto& meter:row)meter.store(0.f,std::memory_order_relaxed);
    for(auto& channel:suiteReturnRms)
        for(auto& meter:channel)meter.store(0.f,std::memory_order_relaxed);
    micFrontEnd.prepare(rate);
    micTube.prepare(rate,rawMic.tubeOn->load()>.5f,rawMic.tubeGain->load(),rawMic.tubeDrive->load(),rawMic.tubeAir->load(),rawMic.tubeTrim->load());
    micInputGate.prepare(rate);
    // Output-stage separation is prepared with the processor's own sample rate
    // so its 10 ms glide is always the exact host rate, then snapped to the
    // current parameter without waiting for the first block.
    stereoSeparation.prepare(rate);
    if(auto* raw=rawGlobal.stereoSeparation)
        stereoSeparation.setRaw(juce::roundToInt(raw->load()));
    stereoSeparation.reset(); // restored position must apply from the first sample
    micFrontWet.reset(rate,.020);
    micFrontWet.setCurrentAndTargetValue(rawMic.on->load()>.5f?1.f:0.f);
    micGateWet.reset(rate,.020);
    micGateWet.setCurrentAndTargetValue(rawMic.gateOn->load()>.5f?1.f:0.f);
    hosted.prepare(rate,hostChunkCapacity);
    const auto lookahead=engines[0].limiter.latencySamples();
    limiterLookahead.store(lookahead,std::memory_order_release);
    const int pdcCapacity=slotCount*(lookahead+skellpreset::dsp::SpectralPitchFormant::latencySamples)+skellpreset::host::HostedVST3::maxTotalLatency;
    dryPdc.prepare(pdcCapacity);
    parallelPdc.prepare(pdcCapacity);
    preMicHistory.prepare(pdcCapacity,hostChunkCapacity);
    postMicHistory.prepare(pdcCapacity,hostChunkCapacity);
    externalKeyHistory.prepare(pdcCapacity,hostChunkCapacity);
    updateHostLatency();
    for(auto* smooth:{&inputSmooth,&outputSmooth,&mixSmooth,&bypassSmooth})smooth->reset(rate,.020);
    inputSmooth.setCurrentAndTargetValue(dbToGain(rawGlobal.input->load()));
    outputSmooth.setCurrentAndTargetValue(dbToGain(rawGlobal.output->load()));
    mixSmooth.setCurrentAndTargetValue(rawGlobal.mix->load());
    bypassSmooth.setCurrentAndTargetValue((int)rawGlobal.mode->load()==3?0.f:1.f);
    for(int s=0;s<slotCount;++s){
        auto& blend=slotWetSmooth[(size_t)s];
        blend.reset(rate,.020);
        const auto& raw=rawSlots[(size_t)s];
        blend.setCurrentAndTargetValue(raw.on->load()>.5f ? raw.wet->load():0.f);
    }

}
void SkellPresetAudioProcessor::releaseResources() { hosted.release(); }
bool SkellPresetAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto in=layout.getMainInputChannelSet(),out=layout.getMainOutputChannelSet();
    if(layout.inputBuses.size()>1) {
        const auto aux=layout.getChannelSet(true,1);
        if(!aux.isDisabled() && aux!=juce::AudioChannelSet::mono() && aux!=juce::AudioChannelSet::stereo())return false;
    }
    return (in==juce::AudioChannelSet::mono() && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo()))
        || (in==juce::AudioChannelSet::stereo() && out==juce::AudioChannelSet::stereo());
}
float SkellPresetAudioProcessor::value(const juce::String& id) const noexcept {
    if(auto* v=state.getRawParameterValue(id))
        return v->load();
    return 0;
}
void SkellPresetAudioProcessor::setReal(const juce::String& id,float v) {
    if(id!="mode" && selectedProgram==-2 && emptyRackIdentity.exchange(false,std::memory_order_acq_rel)) {
        name="Custom Rack";
        state.state.setProperty("presetName",name,nullptr);
    }
    if(auto* p=state.getParameter(id))p->setValueNotifyingHost(p->convertTo0to1(v));
}
float SkellPresetAudioProcessor::getSlotParam(int slot,int param) const noexcept {
    return value(slotPrefix(slot)+"p"+juce::String(param+1));
}
void SkellPresetAudioProcessor::setSlotParam(int slot,int param,float v) {setReal(slotPrefix(slot)+"p"+juce::String(param+1),juce::jlimit(0.f,1.f,v));}
void SkellPresetAudioProcessor::setSlotOn(int slot,bool on) {setReal(slotPrefix(slot)+"on",on?1.f:0.f);}
int SkellPresetAudioProcessor::slotType(int slot) const noexcept {
    if(slot<0 || slot>=slotCount)return Empty;
    const auto& raw=rawSlots[(size_t)slot];
    const int oldType=raw.type?juce::jlimit(0,(int)RingModFX,(int)raw.type->load()):Empty;
    const int extra=raw.extension?juce::jlimit(0,3,(int)raw.extension->load()):0;
    const int advanced=raw.extension2?juce::jlimit(0,4,(int)raw.extension2->load()):0;
    const int studio=raw.extension3?juce::jlimit(0,1,(int)raw.extension3->load()):0;
    const int match=raw.extension4?juce::jlimit(0,1,(int)raw.extension4->load()):0;
    const int mic=raw.extension5?juce::jlimit(0,1,(int)raw.extension5->load()):0;
    const int dizer=raw.extension6?juce::jlimit(0,1,(int)raw.extension6->load()):0;
    const int studioNative=raw.extension7?juce::jlimit(0,4,(int)raw.extension7->load()):0;
    const int studioDelay=raw.extension8?juce::jlimit(0,1,(int)raw.extension8->load()):0;
    const int adlib=raw.extension9?juce::jlimit(0,4,(int)raw.extension9->load()):0;
    const int vocalFeel=raw.extension10?juce::jlimit(0,1,(int)raw.extension10->load()):0;
    const int vocalSuite=raw.extension11?juce::jlimit(0,1,(int)raw.extension11->load()):0;
    if(oldType!=Empty)return oldType;
    if(raw.extension12 && raw.extension12->load()>.5f)return PhaseRotationFX;
    if(vocalSuite>0)return VocalFeelSuiteFX;
    if(vocalFeel>0)return VocalFeelOptoFX;
    if(adlib>0)return (int)StudioDelayFX+adlib;
    if(studioDelay>0)return (int)StudioFastDistFX+studioDelay;
    if(studioNative>0)return (int)SkelldizerFX+studioNative;
    if(dizer>0)return (int)MicModelFX+dizer;
    if(mic>0)return (int)AlchemistMatchFinishFX+mic;
    if(match>0)return (int)StudioPitchFormantFX+match;
    if(studio>0)return (int)AdaptiveNoiseFX+studio;
    if(advanced>0)return (int)VocalVibratoFX+advanced;
    return extra>0 ? (int)RingModFX+extra : (int)Empty;
}
void SkellPresetAudioProcessor::setSlotType(int slot,int type) {
    if(slot<0 || slot>=slotCount)return;
    const auto id=slotPrefix(slot);
    const int chosen=juce::jlimit(0,NumberOfModules-1,type);
    if(chosen!=slotType(slot)) {
        setReal(id+"rightFirst",0.f);
        setReal(id+"dubDuckOn",0.f);
        setReal(id+"duckSource",0.f);setReal(id+"parallelReturn",0.f);setReal(id+"duckExternal",0.f);setReal(id+"duckEffect",0.f);
    }
    const bool newMic=chosen==MicModelFX && slotType(slot)!=MicModelFX;
    const bool newDizer=chosen==SkelldizerFX && slotType(slot)!=SkelldizerFX;
    const bool newStudio=chosen>SkelldizerFX && slotType(slot)!=chosen;
    if(chosen!=PhaseRotationFX)setReal(id+"fxExt12",0.f);
    if(chosen!=VocalFeelSuiteFX)setReal(id+"fxExt11",0.f);
    if(chosen!=VocalFeelOptoFX)setReal(id+"fxExt10",0.f);
    if(chosen<=StudioDelayFX)setReal(id+"fxExt9",0.f);
    if(chosen<=StudioFastDistFX)setReal(id+"fxExt8",0.f);
    if(chosen<=SkelldizerFX)setReal(id+"fxExt7",0.f);
    if(chosen<=MicModelFX)setReal(id+"fxExt6",0.f);
    if(chosen==PhaseRotationFX) {
        setReal(id+"type",(float)Empty);
        for(const auto* choice:{"fxExt","fxExt2","fxExt3","fxExt4","fxExt5","fxExt6","fxExt7","fxExt8","fxExt9","fxExt10","fxExt11"})
            setReal(id+choice,0.f);
        setReal(id+"fxExt12",1.f);
    } else if(chosen==VocalFeelSuiteFX) {
        setReal(id+"type",(float)Empty);
        for(const auto* choice:{"fxExt","fxExt2","fxExt3","fxExt4","fxExt5",
                                "fxExt6","fxExt7","fxExt8","fxExt9","fxExt10"})
            setReal(id+choice,0.f);
        setReal(id+"fxExt11",1.f);
    } else if(chosen==VocalFeelOptoFX) {
        // Stable legacy type/extension parameters are untouched in meaning.
        setReal(id+"type",(float)Empty);
        for(const auto* choice:{"fxExt","fxExt2","fxExt3","fxExt4","fxExt5",
                                "fxExt6","fxExt7","fxExt8","fxExt9"})
            setReal(id+choice,0.f);
        setReal(id+"fxExt10",1.f);
    } else if(chosen>StudioDelayFX) {
        setReal(id+"type",(float)Empty);
        for(const auto* choice:{"fxExt","fxExt2","fxExt3","fxExt4",
                                "fxExt5","fxExt6","fxExt7","fxExt8"})
            setReal(id+choice,0.f);
        setReal(id+"fxExt9",(float)(chosen-StudioDelayFX));
    } else if(chosen>StudioFastDistFX) {
        setReal(id+"type",(float)Empty);
        for(const auto* choice:{"fxExt","fxExt2","fxExt3","fxExt4","fxExt5","fxExt6","fxExt7"})
            setReal(id+choice,0.f);
        setReal(id+"fxExt8",(float)(chosen-StudioFastDistFX));
    } else if(chosen>SkelldizerFX) {
        setReal(id+"type",(float)Empty);
        for(const auto* choice:{"fxExt","fxExt2","fxExt3","fxExt4","fxExt5","fxExt6"})
            setReal(id+choice,0.f);
        setReal(id+"fxExt7",(float)(chosen-SkelldizerFX));
    } else if(chosen>MicModelFX) {
        setReal(id+"type",(float)Empty);
        setReal(id+"fxExt",0.f);setReal(id+"fxExt2",0.f);
        setReal(id+"fxExt3",0.f);setReal(id+"fxExt4",0.f);
        setReal(id+"fxExt5",0.f);
        setReal(id+"fxExt6",(float)(chosen-MicModelFX));
    } else if(chosen>AlchemistMatchFinishFX) {
        setReal(id+"type",(float)Empty);
        setReal(id+"fxExt",0.f);setReal(id+"fxExt2",0.f);
        setReal(id+"fxExt3",0.f);setReal(id+"fxExt4",0.f);
        setReal(id+"fxExt5",(float)(chosen-AlchemistMatchFinishFX));
    } else if(chosen>StudioPitchFormantFX) {
        setReal(id+"fxExt5",0.f);
        setReal(id+"type",(float)Empty);
        setReal(id+"fxExt",0.f);setReal(id+"fxExt2",0.f);
        setReal(id+"fxExt3",0.f);
        setReal(id+"fxExt4",(float)(chosen-StudioPitchFormantFX));
    } else if(chosen>AdaptiveNoiseFX) {
        setReal(id+"fxExt5",0.f);
        setReal(id+"type",(float)Empty);
        setReal(id+"fxExt",0.f);setReal(id+"fxExt2",0.f);
        setReal(id+"fxExt3",(float)(chosen-AdaptiveNoiseFX));
        setReal(id+"fxExt4",0.f);
    } else if(chosen>VocalVibratoFX) {
        setReal(id+"fxExt5",0.f);
        setReal(id+"type",(float)Empty);
        setReal(id+"fxExt",0.f);setReal(id+"fxExt3",0.f);
        setReal(id+"fxExt2",(float)(chosen-VocalVibratoFX));
        setReal(id+"fxExt4",0.f);
    } else if(chosen>RingModFX) {
        setReal(id+"fxExt5",0.f);
        setReal(id+"type",(float)Empty);
        setReal(id+"fxExt2",0.f);setReal(id+"fxExt3",0.f);
        setReal(id+"fxExt",(float)(chosen-RingModFX));
        setReal(id+"fxExt4",0.f);
    } else {
        setReal(id+"fxExt5",0.f);
        setReal(id+"fxExt3",0.f);setReal(id+"fxExt2",0.f);
        setReal(id+"fxExt",0.f);setReal(id+"fxExt4",0.f);
        setReal(id+"type",(float)chosen);
    }
    if(newDizer) {
        setSlotParam(slot,0,.35f); // default Amount
        setSlotParam(slot,1,0.f); // Punch
        setSlotParam(slot,2,.5f); // 0 dB output trim
        setSlotParam(slot,3,1.f); // auto-match input level
    }
    if(newMic) {
        // First selection starts with a neutral input/target profile and
        // zero output trim; package loading subsequently restores saved p1–p4.
        setSlotParam(slot,0,0.f);
        setSlotParam(slot,1,0.f);
        setSlotParam(slot,2,1.f);
        setSlotParam(slot,3,.5f);
    }
    if(newStudio) {
        // First selection only; imported preset parameters restore afterward.
        for(int k=0;k<16;++k)setSlotParam(slot,k,.5f);
        if(chosen==PhaseRotationFX) {
            setSlotParam(slot,0,.5495f); // approximately 1 kHz
            setSlotParam(slot,1,.300f);  // approximately 0.707 Q
            setSlotParam(slot,2,0.f);    // normal polarity
            setSlotParam(slot,3,.5f);    // 0 dB trim
        }
        if(chosen==VocalFeelSuiteFX) {
            for(int k=0;k<16;++k)
                setSlotParam(slot,k,skellpreset::dsp::vocalFeelNeutral[(size_t)k]);
        }
        if(chosen==VocalFeelOptoFX) {
            setSlotParam(slot,0,.45f); // opto strength
            setSlotParam(slot,1,0.f);  // FLAT colour: no saturation by default
            setSlotParam(slot,2,.5f);  // MALE 2 detector
            setSlotParam(slot,3,1.f);  // 100% processing (rack wet still independent)
        }
        if(chosen>=AdlibChop && chosen<=AdlibSaturate) {
            // First selection only; imported presets override these defaults.
            setSlotParam(slot,0,.65f);
            setSlotParam(slot,1,.50f);
            setSlotParam(slot,2,.50f);
            setSlotParam(slot,3,1.f);
        }
        if(chosen==StudioEQFX) {
            // Separate nodes on the x-axis instead of stacking 7 identical
            // green dots at 632 Hz when selecting a fresh EQ.
            const float frequencies[]{.12f,.27f,.39f,.50f,.61f,.72f,.86f,.94f};
            for(int k=0;k<8;++k){
                setSlotParam(slot,k*2,frequencies[k]);
                setReal(id+"studioEqOn"+juce::String(k+1),k<7?1.f:0.f);
                setReal(id+"studioEqDynamic"+juce::String(k+1),0.f);
                setReal(id+"studioEqThreshold"+juce::String(k+1),-24.f);
                setReal(id+"studioEqAttack"+juce::String(k+1),12.f);
                setReal(id+"studioEqRelease"+juce::String(k+1),110.f);
                setReal(id+"studioEqSlope"+juce::String(k+1),0.f);
                setReal(id+"studioEqExtraShape"+juce::String(k+1),0.f);
                setReal(id+"studioEqKind"+juce::String(k+1),
                        (float)(k==0?3:(k==6?4:0)));
            }
        }
        if(chosen==StudioLimiterFX) {
            setSlotParam(slot,0,.89f); // approximately -1 dBFS final ceiling
            setSlotParam(slot,1,0.f);  // no added drive
            setSlotParam(slot,2,1.f);  // optional compressor off
            setSlotParam(slot,3,0.f);
            setSlotParam(slot,4,0.f);  // optional gate off
            setSlotParam(slot,8,0.f);
        } else if(chosen==StudioDelayFX) {
            constexpr float defaults[16]{.49f,.32f,.30f,0.f,0.f,.85f,
                                         .10f,0.f,.12f,0.f,.48f,1.f,
                                         0.f,.5f,.15f,.10f};
            for(int k=0;k<16;++k)setSlotParam(slot,k,defaults[k]);
        } else if(chosen==StudioFastDistFX) {
            setSlotParam(slot,1,1.f);setSlotParam(slot,2,0.f);
            setSlotParam(slot,3,1.f);setSlotParam(slot,4,1.f);
        }
    }
    updateHostLatency();
}
juce::String SkellPresetAudioProcessor::moduleName(int type) const {return types[juce::jlimit(0,types.size()-1,type)];}
juce::String SkellPresetAudioProcessor::moduleDisplayName(int type) const {return skellpreset::registry::displayName(type);}
// Reports which loaded, enabled effects a Simple macro actually modulates.
// Mirrors the exact switch in processBlock: Tone feeds the classic EQ band
// gain, Punch the classic Compressor threshold, Space the Reverb/Delay return
// and Grit the Saturator drive (plus the classic Limiter drive). Anything the
// macro cannot reach is deliberately absent so the Simple view can mark the
// dial as inactive instead of implying control that does not exist.
juce::StringArray SkellPresetAudioProcessor::macroTargets(int macroIndex) const {
    juce::StringArray targets;
    for(int slot=0;slot<activeSlotCount();++slot) {
        const int id=slotIdAt(slot);
        if(id<0 || id>=slotCount)continue;
        const auto& raw=rawSlots[(size_t)id];
        if(raw.on && raw.on->load()<=.5f)continue;   // bypassed slots are inert
        const int type=slotType(id);
        if(type==Empty)continue;
        bool matches=false;
        switch(macroIndex) {
            case 0: matches=(type==EQ); break;
            case 1: matches=(type==Compressor); break;
            case 2: matches=(type==Reverb || type==Delay); break;
            case 3: matches=(type==Saturator || type==Limiter); break;
            default: break;
        }
        if(matches)targets.add(moduleName(type));
    }
    return targets;
}
juce::Result SkellPresetAudioProcessor::loadHostedVST3(int slot,const juce::File& path,const juce::String& identifier,
                                                     const juce::PluginDescription* scanned) {
    if(slot<0 || slot>=slotCount)return juce::Result::fail("Invalid host slot");
    const auto result=hosted.load(slot,path,rate,hostChunkCapacity,identifier,scanned);
    if(result.failed())return result;
    hostedEditorWindows[(size_t)slot].reset(); // close only after successful replacement
    const auto key=juce::String(slot+1);
    state.state.setProperty("hostPath"+key,path.getFullPathName(),nullptr);
    state.state.setProperty("hostClass"+key,hosted.getIdentifier(slot),nullptr);
    state.state.removeProperty("hostState"+key,nullptr);
    setSlotType(slot,ExternalVST3);
    setSlotOn(slot,true);
    updateHostLatency();
    return juce::Result::ok();
}
namespace {
class HostedEditorWindow final : public juce::DialogWindow {
public:
    HostedEditorWindow(const juce::String& title,juce::AudioProcessorEditor* editor)
        :juce::DialogWindow(title,juce::Colours::black,true) {
        setUsingNativeTitleBar(true);
        setResizable(true,false);
        setContentOwned(editor,true);
        centreWithSize(getWidth(),getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override {
        setVisible(false);
        clearContentComponent();
    }
};
}
void SkellPresetAudioProcessor::openHostedVST3Editor(int slot) {
    if(slot<0 || slot>=slotCount)return;
    auto* plugin=hosted.get(slot);
    if(!plugin)return;
    if(auto* existing=plugin->getActiveEditor()) {
        if(auto* window=hostedEditorWindows[(size_t)slot].get())window->toFront(true);
        else existing->toFront(true);
        return;
    }
    if(!plugin->hasEditor())return;
    if(auto* editor=plugin->createEditorIfNeeded())
        hostedEditorWindows[(size_t)slot]=std::make_unique<HostedEditorWindow>(
            "SKELL  /  "+hosted.getName(slot),editor);
}
void SkellPresetAudioProcessor::closeHostedVST3Editors() {
    for(auto& window:hostedEditorWindows)window.reset();
}
void SkellPresetAudioProcessor::refreshHostedLatency() {
    // Called by the editor timer, never from the audio callback.
    hosted.pollLatencies();
    updateHostLatency();
}
void SkellPresetAudioProcessor::handleAsyncUpdate() {
    // DAW state restoration never scans/loads third-party code in an audio
    // callback. The AsyncUpdater dispatches this on JUCE's message thread.
    for(int s=0;s<slotCount;++s) {
        const auto key=juce::String(s+1);
        const auto path=state.state.getProperty("hostPath"+key).toString();
        const auto* type=rawSlots[(size_t)s].type;
        const auto identifier=state.state.getProperty("hostClass"+key).toString();
        const auto payload=state.state.getProperty("hostState"+key).toString();
        if(path.isEmpty() || !type || (int)type->load()!=ExternalVST3){
            hostedEditorWindows[(size_t)s].reset();hosted.detach(s);continue;
        }
        if(!hosted.get(s) || hosted.getFile(s).getFullPathName()!=path ||
           (identifier.isNotEmpty() && hosted.getIdentifier(s)!=identifier)) {
            // Restore directly: unlike interactive replacement, do not change
            // the saved on/off control or clear its serialized state.
            const auto result=hosted.load(s,juce::File(path),rate,hostChunkCapacity,identifier);
            hostedEditorWindows[(size_t)s].reset();
            if(result.failed()){hosted.detach(s);continue;}
        }
        if(payload.isNotEmpty() && payload.length()<3*1024*1024) {
            juce::MemoryBlock bytes;
            if(bytes.fromBase64Encoding(payload))hosted.setState(s,bytes);
        }
    }
    updateHostLatency();
}


void SkellPresetAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels=juce::jmin(getMainBusNumOutputChannels(),buffer.getNumChannels());
    const int total=buffer.getNumSamples();
    if(channels<1 || total<=0)return;
    skellpreset::host::HostedVST3::AudioReadScope hostedBlock(hosted);
    const int visualSlot=slotVisual.beginBlock();
    const bool monoSource=getMainBusNumInputChannels()==1;
    // Capture the actual main dry input before input trim, MIC, gate or rack.
    if(getMainBusNumInputChannels()>0)liveMatchCapture.push(buffer.getReadPointer(0),monoSource||buffer.getNumChannels()<2?nullptr:buffer.getReadPointer(1),total,rate);
    const int auxChannels=getBusCount(true)>1?getChannelCountOfBus(true,1):0;
    const int auxOffset=auxChannels>0?getChannelIndexInProcessBlockBuffer(true,1,0):0;
    const bool auxValid=auxChannels>0 && auxOffset>=0 && auxOffset+auxChannels<=buffer.getNumChannels();
    const float* keyL=auxValid?buffer.getReadPointer(auxOffset):nullptr;
    const float* keyR=auxValid?buffer.getReadPointer(auxOffset+(auxChannels>1?1:0)):nullptr;
    // Query the host once per block, never inside the sample loop. The new
    // ducking delay uses a deterministic 120-BPM fallback when stopped/unknown.
    juce::Optional<juce::AudioPlayHead::PositionInfo> hostPosition;
    if(auto* playhead=getPlayHead())
        hostPosition=playhead->getPosition();
    double hostBpm=120.;
    if(hostPosition)
        if(const auto bpm=hostPosition->getBpm())
            if(std::isfinite(*bpm) && *bpm>=40. && *bpm<=260.)hostBpm=*bpm;
    const int mode=juce::jlimit(0,3,(int)rawGlobal.mode->load());
    inputSmooth.setTargetValue(dbToGain(rawGlobal.input->load()));
    outputSmooth.setTargetValue(dbToGain(rawGlobal.output->load()));
    mixSmooth.setTargetValue(juce::jlimit(0.f,1.f,rawGlobal.mix->load()));
    bypassSmooth.setTargetValue(mode==3?0.f:1.f);
    // The dedicated MIC panel runs before rack slot 1, not as another
    // inserted effect. Read all automation once per host callback.
    micFrontEnd.set(juce::jlimit(0.f,3.f,rawMic.input->load())/3.f,
                    juce::jlimit(0.f,3.f,rawMic.target->load())/3.f,
                    juce::jlimit(0.f,1.f,rawMic.amount->load()),
                    juce::jlimit(0.f,1.f,rawMic.trim->load()));
    micFrontWet.setTargetValue(rawMic.on->load()>.5f?1.f:0.f);
    micTube.set(rawMic.tubeOn->load()>.5f,rawMic.tubeGain->load(),rawMic.tubeDrive->load(),rawMic.tubeAir->load(),rawMic.tubeTrim->load());
    micInputGate.threshold=juce::jlimit(-80.f,-10.f,rawMic.gateThreshold->load());
    micInputGate.release=juce::jlimit(20.f,1000.f,rawMic.gateRelease->load());
    micInputGate.holdMs=juce::jlimit(0.f,250.f,rawMic.gateHold->load());
    micInputGate.hysteresisDb=juce::jlimit(0.f,12.f,rawMic.gateHysteresis->load());
    micInputGate.attackMs=juce::jlimit(.5f,25.f,rawMic.gateAttack->load());
    const float micGateDepth=juce::jlimit(0.f,1.f,rawMic.gateDepth->load());
    micGateWet.setTargetValue(rawMic.gateOn->load()>.5f?1.f:0.f);
    const bool delta=rawGlobal.delta->load()>.5f;
    const float macroTone=rawGlobal.macros[0]->load();
    const float macroPunch=rawGlobal.macros[1]->load();
    const float macroSpace=rawGlobal.macros[2]->load();
    const float macroGrit=rawGlobal.macros[3]->load();
    // One consistent rack topology snapshot for the entire callback. Only
    // ACTIVE rack slots need coefficient updates, which can involve pow/exp.
    // Inactive expanded slots are configured on their first active callback;
    // no preset parameters, automation IDs or engine states are discarded.
    const int chainCount=activeSlotCount();
    const auto packed=packedOrder.load(std::memory_order_acquire);
    const std::uint32_t activeMask=skellpreset::realtime::activeSlotMask(packed,chainCount,slotCount);
    // Native algorithms share one preallocated engine per physical slot;
    // no allocation, file access or locks in this path.
    for(int s=0;s<slotCount;++s) {
        if((activeMask&(std::uint32_t{1}<<s))==0) {
            controlCache[(size_t)s].type=Empty;
            continue;
        }
        auto& c=controlCache[(size_t)s];
        const auto& raw=rawSlots[(size_t)s];
        const int previousType=c.type;
        c.type=slotType(s);
        c.on=raw.on->load()>.5f;
        c.wet=juce::jlimit(0.f,1.f,raw.wet->load());
        // Old inert duckOn values must not activate legacy dub states.
        c.duckOn=supportsEffectSidechain(c.type) && (c.type==AdlibDoubler?raw.dubDuckOn:supportsWetSidechain(c.type)?raw.duckOn:raw.duckEffect)->load()>.5f;
        c.rightFirst=raw.rightFirst->load()>.5f;
        c.duckSource=juce::jlimit(0,2,(int)raw.duckSource->load());
        c.duckExternal=raw.duckExternal->load()>.5f;
        c.parallelReturn=supportsParallelReturn(c.type) && raw.parallelReturn->load()>.5f;
        c.duckDepth=raw.duckDepth->load();
        c.duckThreshold=raw.duckThreshold->load();
        c.duckRelease=raw.duckRelease->load();
        c.duckAttack=raw.duckAttack->load();
        c.limitAttack=raw.limitAttack->load();c.limitRelease=raw.limitRelease->load();

        c.eqLowCut=raw.eqLowCut->load();
        c.eqHighCut=raw.eqHighCut->load();
        for(int k=0;k<16;++k)c.p[(size_t)k]=raw.p[(size_t)k]->load();
        for(int k=0;k<8;++k)c.q[(size_t)k]=raw.q[(size_t)k]->load();
        if(c.type==StudioEQFX)for(int k=0;k<8;++k){
            c.studioEqKinds[(size_t)k]=juce::jlimit(0,4,
                (int)raw.studioEqKind[(size_t)k]->load());
            c.studioEqEnabled[(size_t)k]=raw.studioEqOn[(size_t)k]->load()>.5f;
            c.studioEqDynamic[(size_t)k]=raw.studioEqDynamic[(size_t)k]->load();
            c.studioEqThreshold[(size_t)k]=raw.studioEqThreshold[(size_t)k]->load();
            c.studioEqAttack[(size_t)k]=raw.studioEqAttack[(size_t)k]->load();
            c.studioEqRelease[(size_t)k]=raw.studioEqRelease[(size_t)k]->load();
            c.studioEqSlope[(size_t)k]=raw.studioEqSlope[(size_t)k]->load();
            const int extra=juce::jlimit(0,2,juce::roundToInt(raw.studioEqExtraShape[(size_t)k]->load()));
            if(extra>0)c.studioEqKinds[(size_t)k]=4+extra;
        }
        auto& e=engines[(size_t)s];
        if(previousType!=c.type) {
            if(c.type==PhaseRotationFX || previousType==PhaseRotationFX)e.phaseRotation.reset();
            if(supportsEffectSidechain(previousType) || supportsEffectSidechain(c.type))e.voiceDuck.prepare(rate);
            slotInputScope[(size_t)s].reset();slotOutputScope[(size_t)s].reset();
            if(s==visualSlot)slotVisual.invalidate();
        }
        if(c.type==Limiter || c.type==StudioLimiterFX)e.limiter.setTiming(c.limitAttack,c.limitRelease);
        if(supportsWetSidechain(c.type) || (supportsEffectSidechain(c.type) && (c.duckOn || e.voiceDuck.currentGain()<1.f)))
            e.voiceDuck.set(c.duckOn,c.duckDepth,c.duckThreshold,
                            c.duckRelease,c.duckAttack);
        switch(c.type) {
            case EQ:
                e.peak.setPeak(rate,40.*std::pow(300.,c.p[0]),
                    (c.p[1]-.5)*24.+8.*(macroTone-.5f),.3+7.*c.p[2]);
                e.highpass.setHighPass(rate,20.+c.p[3]*250.);
                break;
            case Compressor:
                e.comp.set(-48.f+c.p[0]*42.f-12.f*(macroPunch-.5f),
                    1.f+c.p[1]*11.f,2.f+c.p[2]*98.f,20.f+c.p[3]*480.f,
                    (c.p[4]-.5f)*24.f);
                break;
            case Gate:
                e.gate.threshold=-75.f+c.p[0]*70.f;
                e.gate.release=40.f+c.p[1]*600.f;
                e.gate.holdMs=c.p[2]*120.f;
                e.gate.hysteresisDb=c.p[3]*8.f;
                break;
            case DeEsser:
                e.deesser.set(3500.f+c.p[0]*8000.f,-53.f+c.p[1]*39.f,
                    2.f+c.p[2]*20.f,35.f+c.p[3]*245.f);
                break;
            case Radio:
                e.radio.set(100.f+1400.f*c.p[0],1800.f+10000.f*c.p[1],
                            c.p[2],c.p[3]);
                break;
            case RetroCrush:
                e.crusher.set(3.f+13.f*c.p[0],
                    100.f*std::pow((float)(rate/100.),c.p[1]),c.p[2],c.p[3]);
                break;
            case Tremolo:
                e.tremolo.set(.15f+14.f*c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case StereoWidth:
                e.width.set(2.f*c.p[0],60.f+340.f*c.p[1],
                            2.f*c.p[2]-1.f,c.p[3]);
                break;
            case ThreeBandDynamics:
                e.threeBand.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case VoxRackRoom:
                e.voxRoom.set(c.p[0],c.p[1],200.f*c.p[2],20.f+980.f*c.p[3]);
                break;
            case VoxRackSmoothComp:
                e.voxComp.set(-55.f+53.f*c.p[0],1.f+11.f*c.p[1],
                              18.f*c.p[2],40.f+410.f*c.p[3]);
                break;
            case VocalChorusFX:
                e.chorus.set(.08f+3.9f*c.p[0],1.f+10.f*c.p[1],
                             2.f+3.f*c.p[2],c.p[3]);
                break;
            case VocalFlangerFX:
                e.flanger.set(.05f+4.95f*c.p[0],.3f+9.0f*c.p[1],
                              (c.p[2]*2.f-1.f)*.82f,c.p[3]);
                break;
            case RingModFX:
                e.ring.set(15.f*std::pow(70.f,c.p[0]),
                            c.p[1],c.p[2],c.p[3]);
                break;
            case VocalRiderFX:
                e.rider.set(c.p[0],-26.f+14.f*c.p[1],3.f+12.f*c.p[2],c.p[3]);
                break;
            case DuckingPingPongFX:
                e.duckingDelay.set(hostBpm,c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case VocalVibratoFX:
                e.vibrato.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case PhaseRotationFX:
                e.phaseRotation.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case StudioEQFX:
                e.studioEq.set(c.p,c.q,c.studioEqKinds,c.studioEqEnabled,c.studioEqSlope);
                e.studioEq.setDynamics(c.studioEqDynamic,c.studioEqThreshold,c.studioEqAttack,c.studioEqRelease);
                break;
            case StudioCompressorFX:
                e.studioComp.set(c.p[0],c.p[1],c.p[2],c.p[3],c.p[4]);
                break;
            case StudioLimiterFX:
                e.studioLimiterComp.set(c.p[2],c.p[3],c.p[6],c.p[5],c.p[7]);
                e.studioGate.set(c.p[4],c.p[8],c.p[5]);
                e.limiter.setCeilingDb(gainToDb(.12f+c.p[0]*.87f));
                break;
            case StudioFastDistFX:
                e.studioDist.set(c.p[0],c.p[1],c.p[2],c.p[3],c.p[4]);
                break;
            case EightBandToneFX:
                e.eightBand.set(rate,c.p,c.q);
                e.eightBand.setCuts(rate,c.eqLowCut,c.eqHighCut);
                break;
            case SerialDynamicsFX:
                e.serialDynamics.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case AlchemistMatchFinishFX:
                e.matchFinish.set(c.p[0],c.p[1],c.p[2],c.p[3],
                                  c.p[4],c.p[5],c.p[6],c.p[7],
                                  c.p[8],c.p[9],c.p[10],c.p[11],
                                  c.p[12],c.p[13],c.p[14],c.p[15]);
                break;
            case MicModelFX:
                e.micModel.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case SkelldizerFX:
                e.skelldizer.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case DynamicHarshnessFX:
                e.harshness.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case AdaptiveNoiseFX:
                e.noiseControl.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case StudioPitchFormantFX:
                e.studioPitch.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case StudioDelayFX:
                e.studioDelay.set(c.p,hostBpm,c.rightFirst);
                break;
            case VocalFeelOptoFX:
                e.vocalFeel.set(c.p[0],c.p[1],c.p[2],c.p[3]);
                break;
            case VocalFeelSuiteFX:
                e.vocalSuite.set(c.p);
                e.vocalSuite.beginBlock();
                break;
            case Delay:
                e.delay.configure((int)(rate*(.055+.85*c.p[0])),c.p[1]*.8f,
                                  500.f+17500.f*c.p[2]);
                break;
            case AdlibThrow:
                e.delay.configure((int)(rate*(.16+.48*c.p[0])),.2f+.68f*c.p[1],
                                  500.f+17500.f*c.p[2]);
                e.throwFilterPole=std::exp(-2.f*(float)skellpreset::dsp::pi*
                    (750.f+c.p[2]*11500.f)/(float)rate);
                break;
            case Reverb:
                e.reverb[0].setParameters(.15f+6.f*c.p[0],.12f+c.p[1]*.8f,1.f);
                break;
            case Limiter:
                e.limiter.setCeilingDb(gainToDb(.12f+c.p[0]*.87f));
                break;
            default: break;
        }
        const float fx=(c.type==Reverb || c.type==Delay)
            ?(.5f+macroSpace):1.f;
        slotWetSmooth[(size_t)s].setTargetValue(juce::jlimit(0.f,1.f,c.wet*fx)
            *(c.on?1.f:0.f));
    }
    std::array<int,slotCount> sequence{};
    for(int i=0;i<slotCount;++i)
        sequence[(size_t)i]=(int)((packed>>(4*i)) & 0x0fU);
    const int lookahead=limiterLookahead.load(std::memory_order_acquire);
    int blockLatency=0;
    std::array<int,slotCount> precedingLatency{};
    for(int position=0;position<chainCount;++position) {
        const int id=sequence[(size_t)position];
        if(id<0 || id>=slotCount)continue;
        const auto type=controlCache[(size_t)id].type;
        precedingLatency[(size_t)id]=blockLatency;
        blockLatency+=skellpreset::realtime::stageLatency(
            (type==StudioLimiterFX?Limiter:type),Limiter,ExternalVST3,StudioPitchFormantFX,lookahead,
            type==ExternalVST3?hostedBlock.getLatency(id):0,
            skellpreset::dsp::SpectralPitchFormant::latencySamples);
    }
    blockLatency=juce::jlimit(0,slotCount*(lookahead+skellpreset::dsp::SpectralPitchFormant::latencySamples)+
        skellpreset::host::HostedVST3::maxTotalLatency,blockLatency);
    std::array<float,slotCount> meter{},grMeter{},minimumGain,duckMinimum;
    minimumGain.fill(1.f);duckMinimum.fill(1.f);
    float inPeak=0.f,outPeak=0.f;
    const int limit=juce::jmax(64,hostChunkCapacity);
    for(int offset=0;offset<total;offset+=limit) {
        const int frames=juce::jmin(limit,total-offset);
        // Forward host transport to nested VST3s without retaining the DAW's
        // callback-scoped playhead pointer. Account for our internal chunks.
        hosted.updateTransport(hostPosition,offset,rate);
        // setSize(...,avoidReallocating=true) reuses prepareToPlay's fixed buffers.
        rackWork.setSize(2,frames,false,false,true);
        sidechainWork.setSize(2,frames,false,false,true);
        rawDetectorWork.setSize(2,frames,false,false,true);
        parallelWork.setSize(2,frames,false,false,true);parallelWork.clear();
        dryWork.setSize(2,frames,false,false,true);
        auto* inL=buffer.getWritePointer(0)+offset;
        auto* inR=channels==2?buffer.getWritePointer(1)+offset:nullptr;
        for(int n=0;n<frames;++n) {
            const float l=std::isfinite(inL[n])?inL[n]:0.f;
            const float r=inR&&!monoSource?(std::isfinite(inR[n])?inR[n]:0.f):l;
            const float inputGain=inputSmooth.getNextValue();
            const float preL=l*inputGain,preR=r*inputGain;
            rawDetectorWork.setSample(0,n,preL);rawDetectorWork.setSample(1,n,preR);
            inPeak=std::max(inPeak,std::max(std::abs(preL),std::abs(preR)));
            const float gateWet=micGateWet.getNextValue();
            float frontL=preL,frontR=preR;
            // Stereo-linked microphone gate runs before mic coloration.
            // OFF is bit-identical to the former path, with no added PDC.
            if(gateWet>1.e-6f) {
                const float gateGain=micInputGate.processGain(
                    std::max(std::abs(preL),std::abs(preR)));
                const float applied=1.f+gateWet*micGateDepth*(gateGain-1.f);
                frontL*=applied;frontR*=applied;
            }
            const float micWet=micFrontWet.getNextValue();
            if(micWet>1.e-6f) {
                const auto colored=micFrontEnd.process(frontL,frontR);
                frontL+=(colored[0]-frontL)*micWet;
                frontR+=(colored[1]-frontR)*micWet;
            }
            // Gate -> Mic Mod -> Mic Preamp -> post-MIC detector / rack.
            micTube.process(frontL,frontR);
            // Real pre-rack input spectrum; no fabricated energy curves.
            // Global Mix and power bypass share the same latency-aligned
            // original input; both formerly used identical independent delay
            // lines, fed the same samples and delay on every callback.
            const auto dry=dryPdc.process(l,r,blockLatency);
            rackWork.setSample(0,n,frontL);rackWork.setSample(1,n,frontR);
            sidechainWork.setSample(0,n,frontL);
            sidechainWork.setSample(1,n,frontR);
            preMicHistory.push(preL,preR);
            postMicHistory.push(frontL,frontR);
            // Read before writing outputs: mono main + stereo output can alias auxiliary channel 0.
            externalKeyHistory.push(keyL?keyL[offset+n]:0.f,keyR?keyR[offset+n]:0.f);
            dryWork.setSample(0,n,dry[0]);dryWork.setSample(1,n,dry[1]);
        }
        // Capture the exact post-MIC signal before any rack effect mutates it.
        liveSpectrumInput.pushBlock(rackWork.getReadPointer(0),rackWork.getReadPointer(1),frames);
        // The rack is processed one block per stage. Hosted VST3 effects
        // receive proper blocks instead of one-sample callbacks.
        for(int pos=0;pos<chainCount;++pos) {
            const int s=sequence[(size_t)pos];
            if(s<0 || s>=slotCount)continue;
            const auto& c=controlCache[(size_t)s];
            if(c.type==Empty)continue;
            auto& e=engines[(size_t)s];
            const auto& source=c.parallelReturn?sidechainWork:rackWork;
            slotInputScope[(size_t)s].push(source.getReadPointer(0),source.getReadPointer(1),frames);
            const bool captureVisual=s==visualSlot;
            if(captureVisual)slotVisual.captureInput(source.getReadPointer(0),source.getReadPointer(1),frames,c.type==StudioPitchFormantFX,rate);
            if(c.type==ExternalVST3) {
                auto* plugin=hostedBlock.get(s);
                const int latency=hostedBlock.getLatency(s);
                if(plugin) {
                    hostWork.setSize(2,frames,false,false,true);
                    hostWork.copyFrom(0,0,rackWork,0,0,frames);
                    hostWork.copyFrom(1,0,rackWork,1,0,frames);
                    juce::MidiBuffer midi;
                    plugin->processBlock(hostWork,midi);
                }
                for(int n=0;n<frames;++n) {
                    const float l=rackWork.getSample(0,n);
                    const float r=rackWork.getSample(1,n);
                    const auto aligned=e.hostDry.process(l,r,latency);
                    const float w=slotWetSmooth[(size_t)s].getNextValue();
                    const float wetL=plugin?hostWork.getSample(0,n):aligned[0];
                    const float wetR=plugin?hostWork.getSample(1,n):aligned[1];
                    const float resultL=aligned[0]+(std::isfinite(wetL)?wetL-aligned[0]:0.f)*w;
                    const float resultR=aligned[1]+(std::isfinite(wetR)?wetR-aligned[1]:0.f)*w;
                    rackWork.setSample(0,n,resultL);
                    rackWork.setSample(1,n,resultR);
                    meter[(size_t)s]=std::max(meter[(size_t)s],
                        std::max(std::abs(resultL),std::abs(resultR)));
                }
                slotOutputScope[(size_t)s].push(rackWork.getReadPointer(0),rackWork.getReadPointer(1),frames);
                if(captureVisual)slotVisual.captureOutput(rackWork.getReadPointer(0),rackWork.getReadPointer(1),frames);
                continue;
            }
            const bool keepTail=c.type==Limiter || c.type==StudioLimiterFX || c.type==Delay ||
                c.type==StudioDelayFX ||
                c.type==Reverb || c.type==VoxRackRoom ||
                c.type==VocalChorusFX || c.type==VocalFlangerFX ||
                c.type==AdlibThrow || c.type==AdlibDoubler ||
                c.type==DuckingPingPongFX || c.type==VocalVibratoFX ||
                c.type==StudioPitchFormantFX;
            if(!keepTail && slotWetSmooth[(size_t)s].getCurrentValue()<.000001f
                && slotWetSmooth[(size_t)s].getTargetValue()==0.f){
                slotOutputScope[(size_t)s].push(source.getReadPointer(0),source.getReadPointer(1),frames);
                if(captureVisual)slotVisual.captureOutput(source.getReadPointer(0),source.getReadPointer(1),frames);
                continue;
            }
            // These controls are constant for the callback. In particular,
            // dbToGain() involves pow(): never call it on every audio sample.
            const float limiterDrive=(c.type==Limiter || c.type==StudioLimiterFX)
                ?dbToGain(c.p[1]*18.f):1.f;
            const float saturatorDrive=c.type==Saturator
                ?dbToGain(c.p[0]*24.f+10.f*(macroGrit-.5f)):1.f;
            if(c.parallelReturn)hostWork.setSize(2,frames,false,false,true);
            const float saturatorBlend=.5f+c.p[1];
            const float saturatorDenom=std::max(.5f,saturatorDrive*.6f);
            const float exciterDrive=1.f+10.f*c.p[0];
            const bool wetSidechain=supportsWetSidechain(c.type);
            const bool wetDetector=wetSidechain && (c.duckOn || e.voiceDuck.currentGain()<.9995f);
            // Inactive wet keys need no history reads. Preserve the ducker
            // unity/envelope reset and continue processing the full effect tail.
            if(wetSidechain && !wetDetector)e.voiceDuck.processGain(0.f,0.f);
            const bool genericKey=supportsEffectSidechain(c.type) && !supportsWetSidechain(c.type) && (c.duckOn || e.voiceDuck.currentGain()<1.f);
            const int ownLatency=c.type==Limiter || c.type==StudioLimiterFX?lookahead:
                c.type==StudioPitchFormantFX?skellpreset::dsp::SpectralPitchFormant::latencySamples:0;
            const bool observeWet=captureVisual && (c.type==Delay || c.type==StudioDelayFX || c.type==VoxRackRoom || c.type==Reverb);
            double wetEnergyL=0.,wetEnergyR=0.;
            std::array<float,2> observedEcho{};
            for(int n=0;n<frames;++n) {
                float l=source.getSample(0,n);
                float r=source.getSample(1,n);
                float wetL=l,wetR=r;
                float duckGain=1.f;
                if(wetDetector) {
                    // Serial keys follow the latency of all preceding slots.
                    // Parallel returns start at the post-MIC timeline; slot
                    // input is already aligned and must not be delayed again.
                    const int keyDelay=c.parallelReturn?0:precedingLatency[(size_t)s];
                    const auto& detector=c.duckSource==1?rawDetectorWork:c.duckSource==2?source:sidechainWork;
                    const auto key=c.duckExternal?externalKeyHistory.at(frames,n,keyDelay):
                        (keyDelay>0 && c.duckSource!=2)?
                        (c.duckSource==1?preMicHistory:postMicHistory).at(frames,n,keyDelay):
                        std::array<float,2>{detector.getSample(0,n),detector.getSample(1,n)};
                    duckGain=e.voiceDuck.processGain(key[0],key[1]);
                }
                if(c.on && c.duckOn && c.wet>0.f)
                    duckMinimum[(size_t)s]=std::min(duckMinimum[(size_t)s],duckGain);
                switch(c.type) {
                    case EQ: {
                        const auto high=e.highpass.process(l,r);
                        const auto y=e.peak.process(high[0],high[1]);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Compressor: {
                        const float g=e.comp.gain(std::max(std::abs(l),std::abs(r)));
                        grMeter[(size_t)s]=std::max(grMeter[(size_t)s],e.comp.lastReductionDb);
                        if(captureVisual)visualDynamics[(size_t)n]=e.comp.lastReductionDb;
                        wetL=l*g;wetR=r*g;break;
                    }
                    case StudioLimiterFX: {
                        const auto& wetSmoother=slotWetSmooth[(size_t)s];
                        if(!skellpreset::realtime::needsLimiterWetRender(
                               wetSmoother.getCurrentValue(),wetSmoother.getTargetValue()))
                            break;
                        const auto compressed=e.studioLimiterComp.process(l*limiterDrive,r*limiterDrive);
                        const float gateGain=e.studioGate.processGain(compressed[0],compressed[1]);
                        const float input[2]{compressed[0]*gateGain,compressed[1]*gateGain};
                        float output[2]{};
                        // Final SkellCore true-peak stage is never bypassed on wet audio.
                        e.limiter.processFrame(input,output,2);
                        minimumGain[(size_t)s]=std::min(minimumGain[(size_t)s],e.limiter.appliedGain());
                        wetL=output[0];wetR=output[1];break;
                    }
                    case StudioCompressorFX: {
                        const auto y=e.studioComp.process(l,r);
                        const float reduction=e.studioComp.gainReductionDb();
                        grMeter[(size_t)s]=std::max(grMeter[(size_t)s],reduction);
                        if(captureVisual)visualDynamics[(size_t)n]=reduction;
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VocalFeelOptoFX: {
                        const auto y=e.vocalFeel.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VocalFeelSuiteFX: {
                        const auto y=e.vocalSuite.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case PhaseRotationFX: {
                        const auto y=e.phaseRotation.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case StudioEQFX: {
                        const auto y=e.studioEq.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case StudioFastDistFX: {
                        const auto y=e.studioDist.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Limiter: {
                        // Preserve historical limiter PDC, but don't spend
                        // detector CPU when the wet fade is completely dry.
                        const auto& wetSmoother=slotWetSmooth[(size_t)s];
                        if(!skellpreset::realtime::needsLimiterWetRender(
                               wetSmoother.getCurrentValue(),wetSmoother.getTargetValue()))
                            break;
                        const float input[2]{l*limiterDrive,r*limiterDrive};
                        float output[2]{};
                        e.limiter.processFrame(input,output,2);
                        minimumGain[(size_t)s]=std::min(minimumGain[(size_t)s],e.limiter.appliedGain());
                        wetL=output[0];wetR=output[1];break;
                    }
                    case Gate: {
                        const float g=e.gate.processGain(std::max(std::abs(l),std::abs(r)));
                        if(captureVisual)visualDynamics[(size_t)n]=g;
                        minimumGain[(size_t)s]=std::min(minimumGain[(size_t)s],g);
                        wetL=l*g;wetR=r*g;break;
                    }
                    case DeEsser: {
                        const auto y=e.deesser.process(l,r);
                        minimumGain[(size_t)s]=std::min(minimumGain[(size_t)s],(float)e.deesser.reductionGain);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Radio: {
                        const auto y=e.radio.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case RetroCrush: {
                        const auto y=e.crusher.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Tremolo: {
                        const auto y=e.tremolo.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case StereoWidth: {
                        const auto y=e.width.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case ThreeBandDynamics: {
                        const auto y=e.threeBand.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VoxRackRoom: {
                        const auto y=e.voxRoom.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VoxRackSmoothComp: {
                        const auto y=e.voxComp.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VocalChorusFX: {
                        const auto y=e.chorus.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VocalFlangerFX: {
                        const auto y=e.flanger.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case RingModFX: {
                        const auto y=e.ring.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case VocalRiderFX: {
                        const auto y=e.rider.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case DuckingPingPongFX: {
                        // This engine returns wet echoes only; the rack's
                        // existing per-slot wet control adds the aligned dry.
                        const auto y=e.duckingDelay.process(l,r);
                        wetL=l+y[0];wetR=r+y[1];break;
                    }
                    case VocalVibratoFX: {
                        const auto y=e.vibrato.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case EightBandToneFX: {
                        const auto y=e.eightBand.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case SerialDynamicsFX: {
                        const auto y=e.serialDynamics.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case AlchemistMatchFinishFX: {
                        const auto y=e.matchFinish.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case MicModelFX: {
                        const auto y=e.micModel.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case SkelldizerFX: {
                        const auto y=e.skelldizer.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case DynamicHarshnessFX: {
                        const auto y=e.harshness.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case AdaptiveNoiseFX: {
                        const auto y=e.noiseControl.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case StudioPitchFormantFX: {
                        const auto y=e.studioPitch.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case AdlibChop:case AdlibMotion:case AdlibFilter:case AdlibSaturate: {
                        const auto effectType=static_cast<skellcore::adlibfx::Type>(
                            c.type-AdlibChop+1);
                        const skellcore::adlibfx::Parameters params{
                            effectType,c.p[0],c.p[1],c.p[2],c.p[3],(float)hostBpm};
                        const auto y=e.adlibFx.process({l,r},params);
                        wetL=y.left;wetR=y.right;break;
                    }
                    case Saturator: {
                        wetL=skellpreset::visual::saturatorCore(l,saturatorDrive,saturatorBlend,saturatorDenom);
                        wetR=skellpreset::visual::saturatorCore(r,saturatorDrive,saturatorBlend,saturatorDenom);
                        break;
                    }
                    case Exciter: {
                        wetL=(std::tanh(l*exciterDrive)-std::tanh(l*exciterDrive*.55f))*.4f+l;
                        wetR=(std::tanh(r*exciterDrive)-std::tanh(r*exciterDrive*.55f))*.4f+r;
                        break;
                    }
                    case StudioDelayFX: {
                        // Studio Delay owns an independently adjustable dry
                        // level; only attenuate its internal echo return.
                        const auto y=e.studioDelay.process(l,r,duckGain,observeWet?&observedEcho:nullptr);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Delay: {
                        const auto y=e.delay.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Reverb: {
                        const auto y=e.reverb[0].processFrame(l,r);
                        if(observeWet)observedEcho=y; // FDN is configured fully wet
                        wetL=y[0];wetR=y[1];break;
                    }
                    case AdlibThrow:
                        e.processThrow(l,r,wetL,wetR,c.p[0],c.p[1],c.p[2],c.p[3],rate);
                        break;
                    case AdlibDoubler: {
                        const auto y=e.doubler.process(l,r,c.p[0],c.p[1],c.p[2],c.p[3]);
                        wetL=y[0];wetR=y[1];break;
                    }
                    default:break;
                }
                if(c.type==AdlibDoubler || c.type==Delay || c.type==VoxRackRoom) {
                    wetL=skellpreset::dsp::duckDubReturn(wetL,duckGain);
                    wetR=skellpreset::dsp::duckDubReturn(wetR,duckGain);
                } else if(wetSidechain && duckGain<1.f && c.type!=StudioDelayFX) {
                    // Treat other native effect paths as delta-from-dry.
                    // The live vocal remains unaffected by the wet return duck.
                    wetL=l+(wetL-l)*duckGain;
                    wetR=r+(wetR-r)*duckGain;
                }
                // Sample-align a bypassed limiter as it still contributes PDC.
                if(c.type==Limiter || c.type==StudioLimiterFX) {
                    const auto delayed=e.limiterDry.process(l,r,lookahead);
                    l=delayed[0];r=delayed[1];
                } else if(c.type==StudioPitchFormantFX) {
                    // The spectral engine is delayed 768 samples; its local
                    // dry path must match before applying the rack wet knob.
                    const auto aligned=e.pitchDry.process(
                        l,r,skellpreset::dsp::SpectralPitchFormant::latencySamples);
                    l=aligned[0];r=aligned[1];
                }
                // Generic effects duck the latency-aligned wet contribution.
                if(genericKey) {
                    const int delay=precedingLatency[(size_t)s]+ownLatency;
                    const auto key=c.duckExternal?externalKeyHistory.at(frames,n,delay):
                        c.duckSource==2?std::array<float,2>{l,r}:
                        (c.duckSource==1?preMicHistory:postMicHistory).at(frames,n,delay);
                    duckGain=e.voiceDuck.processGain(key[0],key[1]);
                    if(c.on && c.duckOn && c.wet>0.f)
                        duckMinimum[(size_t)s]=std::min(duckMinimum[(size_t)s],duckGain);
                }
                const float slotWet=slotWetSmooth[(size_t)s].getNextValue();
                const float wet=genericKey?slotWet*duckGain:slotWet;
                if(observeWet){
                    // Classic FDN wet audio is attenuated by the actual key and rack mix.
                    // Capture the isolated return, not the dry replacement from ducking.
                    const double a=(c.type==Reverb?observedEcho[0]*duckGain:c.type==StudioDelayFX?observedEcho[0]:wetL)*wet;
                    const double b=(c.type==Reverb?observedEcho[1]*duckGain:c.type==StudioDelayFX?observedEcho[1]:wetR)*wet;
                    wetEnergyL+=a*a;wetEnergyR+=b*b;
                    visualWetL[(size_t)n]=static_cast<float>(a);visualWetR[(size_t)n]=static_cast<float>(b);
                }
                const float processedL=c.parallelReturn?wetL*wet:l+(wetL-l)*wet;
                const float processedR=c.parallelReturn?wetR*wet:r+(wetR-r)*wet;
                if(c.parallelReturn) {
                    parallelWork.addSample(0,n,processedL);parallelWork.addSample(1,n,processedR);
                    // Reuse host scratch for this individual return's trace.
                    hostWork.setSample(0,n,processedL);hostWork.setSample(1,n,processedR);
                }else {
                    rackWork.setSample(0,n,processedL);rackWork.setSample(1,n,processedR);
                }
                meter[(size_t)s]=std::max(meter[(size_t)s],
                    std::max(std::abs(processedL),std::abs(processedR)));
            }
            const auto& measuredOutput=c.parallelReturn?hostWork:rackWork;
            slotOutputScope[(size_t)s].push(measuredOutput.getReadPointer(0),measuredOutput.getReadPointer(1),frames);
            if(captureVisual){
                if(observeWet){
                    std::array<float,2> times{};
                    if(c.type==StudioDelayFX)times=e.studioDelay.tapBaseMs();
                    else if(c.type==Delay)times.fill((float)(1000.*e.delay.samples/rate));
                    slotVisual.setReturn((float)std::sqrt(wetEnergyL/frames),(float)std::sqrt(wetEnergyR/frames),times[0],times[1]);
                    slotVisual.captureReturn(visualWetL.data(),visualWetR.data(),frames);
                }
                skellcore::telemetry::SideInfo dynamics;
                if(c.type==Compressor || c.type==StudioCompressorFX)dynamics.grDbPerSample=visualDynamics.data();
                if(c.type==Gate)dynamics.gatePerSample=visualDynamics.data();
                slotVisual.captureOutput(measuredOutput.getReadPointer(0),measuredOutput.getReadPointer(1),frames,dynamics);
            }
            if(c.type==VocalFeelOptoFX && c.on)
                grMeter[(size_t)s]=std::max(grMeter[(size_t)s],
                                           e.vocalFeel.gainReductionDb());
        }
        // Final output stage. Stereo separation runs after the complete vocal
        // chain (including stereo modulation, delay and reverb) and after the
        // dry/wet blend, so it shapes the finished voice rather than one rack
        // slot. It precedes the master output gain and the power/bypass blend,
        // and therefore also precedes the metering and clip capture below, so
        // any level it adds is visible rather than hidden behind the fader.
        // The matrix is pure gain: no delay, no latency, no added side signal.
        if(auto* raw=rawGlobal.stereoSeparation)
            stereoSeparation.setRaw(juce::roundToInt(raw->load()));
        for(int n=0;n<frames;++n) {
            const float mix=mixSmooth.getNextValue();
            const float dryL=dryWork.getSample(0,n);
            const float dryR=dryWork.getSample(1,n);
            // Independent returns are fed from the same post-MIC source.
            // Align their sum with the complete serial rack before global mix.
            const auto returns=parallelPdc.process(parallelWork.getSample(0,n),parallelWork.getSample(1,n),blockLatency);
            const float wetL=rackWork.getSample(0,n)+returns[0];
            const float wetR=rackWork.getSample(1,n)+returns[1];
            // The wet scratch becomes the mixed scratch: it is read here and
            // never needed again, so no extra per-block allocation is added.
            rackWork.setSample(0,n,mix>=1.f?wetL:mix<=0.f?dryL:dryL+(wetL-dryL)*mix);
            rackWork.setSample(1,n,mix>=1.f?wetR:mix<=0.f?dryR:dryR+(wetR-dryR)*mix);
        }
        stereoSeparation.process(rackWork.getWritePointer(0),
                                 rackWork.getWritePointer(1),frames);
        for(int n=0;n<frames;++n) {
            const float dryL=dryWork.getSample(0,n);
            const float dryR=dryWork.getSample(1,n);
            const float mixedL=rackWork.getSample(0,n);
            const float mixedR=rackWork.getSample(1,n);
            const float outGain=outputSmooth.getNextValue();
            const float blend=bypassSmooth.getNextValue();
            const float finalL=(delta?mixedL-dryL:mixedL)*outGain;
            const float finalR=(delta?mixedR-dryR:mixedR)*outGain;
            // Exact endpoints avoid channel-dependent cancellation/rounding
            // when the wet signal has already collapsed to true mono.
            const float outputL=blend>=1.f?finalL:blend<=0.f?dryL:
                dryL+(finalL-dryL)*blend;
            const float outputR=blend>=1.f?finalR:blend<=0.f?dryR:
                dryR+(finalR-dryR)*blend;
            inL[n]=std::isfinite(outputL)?outputL:0.f;
            if(inR)inR[n]=std::isfinite(outputR)?outputR:0.f;
            outPeak=std::max(outPeak,std::max(std::abs(inL[n]),
                inR?std::abs(inR[n]):0.f));
        }
        // Read only the processed output chunk here; the pointers above
        // are scoped to this host block and remain valid during capture.
        liveSpectrumOutput.pushBlock(inL,inR,frames);
        liveScope.push(inL,inR,frames);
        capture.push(inL,inR,frames);
    }
    inputPeak.store(inPeak,std::memory_order_relaxed);
    outputPeak.store(outPeak,std::memory_order_relaxed);
    for(int s=0;s<slotCount;++s) {
        slotPeaks[(size_t)s].store(meter[(size_t)s],std::memory_order_relaxed);
        slotDuckReductionDb[(size_t)s].store(std::max(0.f,-gainToDb(duckMinimum[(size_t)s])),std::memory_order_relaxed);
        const auto& eq=controlCache[(size_t)s];
        const bool showEq=eq.type==StudioEQFX&&eq.on&&eq.wet>0.f;
        for(int band=0;band<8;++band)slotEqReductionDb[(size_t)s][(size_t)band].store(showEq?engines[(size_t)s].studioEq.reductionDb((size_t)band):0.f,std::memory_order_relaxed);
        const bool suite=controlCache[(size_t)s].type==VocalFeelSuiteFX;
        const auto& dsp=engines[(size_t)s].vocalSuite;
        const float measured=std::max(grMeter[(size_t)s],-gainToDb(minimumGain[(size_t)s]));
        slotGainReductionDb[(size_t)s].store(suite?dsp.compressorReductionDb():
                                            measured,std::memory_order_relaxed);
        slotDeessReductionDb[(size_t)s].store(suite?dsp.deesserReductionDb():0.f,
                                              std::memory_order_relaxed);
        for(int chain=0;chain<5;++chain)
            suiteReturnRms[(size_t)s][(size_t)chain].store(
                suite?dsp.chainRms(chain):0.f,std::memory_order_relaxed);
    }
}

int SkellPresetAudioProcessor::appendRackSlot() {
    const int count=activeSlotCount();
    if(count>=slotCount)return -1;
    activeSlots.store(count+1,std::memory_order_release);
    state.state.setProperty("rackSlots",count+1,nullptr);
    updateHostLatency();
    return slotIdAt(count);
}

void SkellPresetAudioProcessor::clearRackToEmpty() {
    suppressProgramIdentityUpdates.store(true,std::memory_order_release);
    emptyRackIdentity.store(false,std::memory_order_release);
    selectedProgram=-1;
    for(auto* parameter:getParameters())
        if(parameter)parameter->setValueNotifyingHost(parameter->getDefaultValue());
    activeSlots.store(defaultSlotCount,std::memory_order_release);
    state.state.setProperty("rackSlots",defaultSlotCount,nullptr);
    std::uint64_t identityOrder=0;
    for(int position=0;position<slotCount;++position)
        identityOrder|=static_cast<std::uint64_t>(position) << (4*position);
    packedOrder.store(identityOrder,std::memory_order_release);
    name="Empty Rack";
    selectedProgram=-2;
    emptyRackIdentity.store(true,std::memory_order_release);
    suppressProgramIdentityUpdates.store(false,std::memory_order_release);
    state.state.setProperty("presetName",name,nullptr);
    state.state.setProperty("presetBank","My Presets",nullptr);
    updateHostLatency();
}

void SkellPresetAudioProcessor::loadFactoryPreset(int index) {
    if(index<0||index>=(int)factoryPresets.size())return;
    emptyRackIdentity.store(false,std::memory_order_release);
    selectedProgram=index;const auto& preset=factoryPresets[(size_t)index];name=preset.name;
    const int recipe=recipeForFactoryIndex(index);
    activeSlots.store(defaultSlotCount,std::memory_order_release);
    state.state.setProperty("rackSlots",defaultSlotCount,nullptr);
    for(int s=0;s<slotCount;s++){
        setSlotType(s,Empty);setSlotOn(s,false);setReal(slotPrefix(s)+"wet",1);
        for(int k=0;k<16;k++)setSlotParam(s,k,.5f);
        for(int band=1;band<=8;++band){
            setReal(slotPrefix(s)+"eqQ"+juce::String(band),.5f);
            setReal(slotPrefix(s)+"studioEqKind"+juce::String(band),
                    (float)(band==1?3:(band==7?4:0)));
            setReal(slotPrefix(s)+"studioEqOn"+juce::String(band),band<=7?1.f:0.f);
            setReal(slotPrefix(s)+"studioEqDynamic"+juce::String(band),0.f);
                setReal(slotPrefix(s)+"studioEqThreshold"+juce::String(band),-24.f);
                setReal(slotPrefix(s)+"studioEqAttack"+juce::String(band),12.f);
                setReal(slotPrefix(s)+"studioEqRelease"+juce::String(band),110.f);
                setReal(slotPrefix(s)+"studioEqSlope"+juce::String(band),0.f);
                setReal(slotPrefix(s)+"studioEqExtraShape"+juce::String(band),0.f);
        }
        setReal(slotPrefix(s)+"eqLowCut",0.f);
        setReal(slotPrefix(s)+"eqHighCut",0.f);
        setReal(slotPrefix(s)+"duckOn",0.f);
        setReal(slotPrefix(s)+"rightFirst",0.f);
        setReal(slotPrefix(s)+"dubDuckOn",0.f);
        setReal(slotPrefix(s)+"duckSource",0.f);setReal(slotPrefix(s)+"parallelReturn",0.f);setReal(slotPrefix(s)+"duckExternal",0.f);setReal(slotPrefix(s)+"duckEffect",0.f);
        setReal(slotPrefix(s)+"duckDepth",.65f);
        setReal(slotPrefix(s)+"duckThreshold",-32.f);
        setReal(slotPrefix(s)+"duckRelease",320.f);
        setReal(slotPrefix(s)+"duckAttack",12.f);
        setReal(slotPrefix(s)+"limitAttack",0.f);setReal(slotPrefix(s)+"limitRelease",70.f);
        
    }
    std::uint64_t seqReset=0;
    for(int i=0;i<slotCount;i++)seqReset|=(std::uint64_t)i << (4*i);
    packedOrder.store(seqReset,std::memory_order_release);
    const int typesToLoad[]={EQ,Compressor,Gate,Saturator,Delay,Reverb,Limiter};
    for(int s=0;s<7;s++){setSlotType(s,typesToLoad[s]);setSlotOn(s,true);}
    setSlotParam(0,0,.45f+preset.tone*.35f); setSlotParam(0,1,.27f+preset.tone*.46f);
    setSlotParam(0,2,.35f);setSlotParam(0,3,.14f);
    setSlotParam(1,0,.65f-preset.punch*.30f);setSlotParam(1,1,.16f+preset.punch*.40f);
    setSlotParam(1,2,.18f);setSlotParam(1,3,.34f);setSlotParam(1,4,.53f);
    setSlotParam(2,0,.22f);setReal("s3_wet",.3f);
    setSlotParam(3,0,.15f+preset.grit*.7f);setSlotParam(3,1,.4f);setReal("s4_wet",preset.grit*.52f);
    setSlotParam(4,0,.35f);setSlotParam(4,1,.31f);setReal("s5_wet",preset.space*.30f);
    setSlotParam(5,0,.24f+preset.space*.5f);setSlotParam(5,1,.65f);setReal("s6_wet",preset.space*.35f);
    setSlotParam(6,0,.89f);setSlotParam(6,1,.05f);setReal("s7_wet",.65f);
    if(recipe==24 || recipe==25) {
        // Original v1 factory IDs remain stable. Add de-essing before the
        // compressor in the new Vocal Polish and zero-PDC Live Lead chains.
        setSlotType(9,DeEsser);setSlotOn(9,true);
        setSlotParam(9,0,.39f);setSlotParam(9,1,.52f);
        setSlotParam(9,2,.48f);setSlotParam(9,3,.36f);
        setReal("s10_wet",.88f);
        // Positions 0 EQ, 1 De-Esser, then previous modules.
        std::uint64_t order=0;
        const int ids[]{0,9,1,2,3,4,5,6,7,8,10,11,12,13,14,15};
        for(int pos=0;pos<slotCount;++pos)
            order|=(std::uint64_t)ids[pos] << (4*pos);
        packedOrder.store(order,std::memory_order_release);
        if(recipe==25) {
            // Recording/monitoring preset: only the 3ms SkellCore lookahead
            // limiter is omitted, while EQ, compression, gate and de-essing
            // remain in the original zero-PDC processing path.
            setSlotOn(6,false);
            setSlotType(6,Empty);
        }
    }
    if(juce::String(preset.bank)=="Adlibs") {
        // Actual phrase throws and decorrelated doubling in the Adlibs bank.
        setSlotType(4,AdlibThrow);setSlotOn(4,true);
        setSlotParam(4,0,.28f+preset.space*.40f);
        setSlotParam(4,1,.18f+preset.space*.65f);
        setReal("s5_wet",.38f+preset.space*.42f);
        setSlotType(7,AdlibDoubler);setSlotOn(7,true);
        setSlotParam(7,0,.42f+preset.punch*.45f);
        setSlotParam(7,1,.35f+preset.space*.35f);
        setSlotParam(7,2,.32f);
        setReal("s8_wet",.38f+preset.space*.45f);
    }
    if(recipe==27) {
        setSlotType(3,RetroCrush);setSlotOn(3,true);
        setSlotParam(3,0,.28f);setSlotParam(3,1,.42f);
        setSlotParam(3,2,.34f);setSlotParam(3,3,0.f);
        setReal("s4_wet",.7f);
        setSlotType(7,Tremolo);setSlotOn(7,true);
        setSlotParam(7,0,.44f);setSlotParam(7,1,.58f);
        setSlotParam(7,2,.35f);setSlotParam(7,3,.15f);
        setReal("s8_wet",.55f);
    } else if(recipe==28) {
        setSlotType(1,ThreeBandDynamics);setSlotOn(1,true);
        setSlotParam(1,0,.38f);setSlotParam(1,1,.52f);
        setSlotParam(1,2,.45f);setSlotParam(1,3,.45f);
        setSlotType(3,StereoWidth);setSlotOn(3,true);
        setSlotParam(3,0,.61f);setSlotParam(3,1,.35f);
        setSlotParam(3,2,.5f);setSlotParam(3,3,.5f);
        setReal("s4_wet",.62f);
    }
    if(recipe==29) {
        setSlotType(1,VoxRackSmoothComp);setSlotOn(1,true);
        setSlotParam(1,0,.56f);
        setSlotParam(1,1,.30f);
        setSlotParam(1,2,.42f);
        setSlotParam(1,3,.32f);
        setSlotType(5,VoxRackRoom);setSlotOn(5,true);
        setSlotParam(5,0,.38f);
        setSlotParam(5,1,.53f);
        setSlotParam(5,2,.07f);
        setSlotParam(5,3,.16f);
        setReal("s6_wet",.20f);
    }
    if(recipe==34) {
        // Rider before the compressor: leveled phrases, no added PDC.
        setSlotType(8,VocalRiderFX);setSlotOn(8,true);
        setSlotParam(8,0,.71f);setSlotParam(8,1,.54f);
        setSlotParam(8,2,.68f);setSlotParam(8,3,.16f);
        setReal("s9_wet",1.f);
        std::uint64_t order=0;
        const int ids[]{0,8,1,2,3,4,5,6,7,9,10,11,12,13,14,15};
        for(int position=0;position<slotCount;++position)
            order|=(std::uint64_t)ids[position] << (4*position);
        packedOrder.store(order,std::memory_order_release);
    }
    if(recipe>=37 && recipe<=40){
        // A new processor placed before the main compressor (or replaces it)
        // without changing the 16 physical slot IDs or old factory programs.
        const bool dualComp=recipe==38;
        const int physical=dualComp?1:8;
        const int chosen=recipe==37?EightBandToneFX:
            recipe==38?SerialDynamicsFX:
            recipe==39?DynamicHarshnessFX:AdaptiveNoiseFX;
        setSlotType(physical,chosen);
        setSlotOn(physical,true);
        setReal(slotPrefix(physical)+"wet",dualComp?.92f:1.f);
        if(recipe==37) {
            // Airy articulation with restrained low-mid cleanup.
            const float gains[8]{-.04f,-.06f,-.03f,-.01f,.04f,.055f,.08f,.05f};
            for(int k=0;k<8;++k) {
                setSlotParam(8,k*2,.5f);
                setSlotParam(8,k*2+1,.5f+gains[k]);
            }
        } else if(recipe==38){
            setSlotParam(1,0,.58f);setSlotParam(1,1,.47f);
            setSlotParam(1,2,.28f);setSlotParam(1,3,.12f);
        } else if(recipe==39){
            setSlotParam(8,0,.59f);setSlotParam(8,1,.58f);
            setSlotParam(8,2,.44f);setSlotParam(8,3,.34f);
        } else {
            setSlotParam(8,0,.43f);setSlotParam(8,1,.40f);
            setSlotParam(8,2,.11f);setSlotParam(8,3,.60f);
        }
        if(!dualComp){
            std::uint64_t order=0;
            const int ids[]{8,0,1,2,3,4,5,6,7,9,10,11,12,13,14,15};
            for(int position=0;position<slotCount;++position)
                order|=(std::uint64_t)ids[position] << (4*position);
            packedOrder.store(order,std::memory_order_release);
        }
    }
    if(recipe==42){
        // 322 DAW has six built-in Web Audio processors but no supplied
        // named factory preset library. This is an original, opt-in Skell
        // chain using our existing, more capable native DSP; do not import
        // browser AudioNodes or replace the original Simple processor.
        setSlotType(0,EQ);setSlotOn(0,true);
        setSlotType(1,Compressor);setSlotOn(1,true);
        setSlotType(2,Empty);setSlotOn(2,false); // no gate in 322 Six
        setSlotType(3,Saturator);setSlotOn(3,true);
        setSlotType(4,Delay);setSlotOn(4,true);
        setSlotType(5,Reverb);setSlotOn(5,true);
        setSlotType(6,Empty);setSlotOn(6,false); // no limiter in 322 Six
        setSlotType(7,VocalChorusFX);setSlotOn(7,true);
        setSlotParam(0,0,.48f);setSlotParam(0,1,.50f);
        setSlotParam(1,0,.55f);setSlotParam(1,1,.27f);
        setSlotParam(1,2,.10f);setSlotParam(1,3,.38f);
        setSlotParam(3,0,.20f);setReal("s4_wet",.25f);
        setSlotParam(4,0,.22f);setSlotParam(4,1,.35f);setReal("s5_wet",.31f);
        setSlotParam(5,0,.32f);setReal("s6_wet",.23f);
        setSlotParam(7,0,.14f);setSlotParam(7,1,.43f);
        setSlotParam(7,2,.28f);setSlotParam(7,3,.55f);
        setReal("s8_wet",.36f);
        // Show the six active native modules consecutively at the top
        // while retaining each historic physical slot/automation ID.
        std::uint64_t compactOrder=0;
        const int ids[]{0,1,3,4,5,7,2,6,8,9,10,11,12,13,14,15};
        for(int position=0;position<slotCount;++position)
            compactOrder|=(std::uint64_t)ids[position] << (4*position);
        packedOrder.store(compactOrder,std::memory_order_release);
    }
    // Preserve factory indices. Append-only native sound-match trials
    // share the same serialized 16-slot APVTS structure and cannot overwrite
    // the protected factory index 43 or any previously saved FL Studio state.
    auto loadNativeStages=[this](const auto& nativeStages) {
        for(int physical=0;physical<slotCount;++physical) {
            // Native factory recalls must have the same clean physical-slot
            // baseline as the legacy factory bank. Hidden/inactive rows are
            // still serialized and can become visible when the rack expands.
            setSlotOn(physical,false);
            setSlotType(physical,Empty);
            const auto prefix=slotPrefix(physical);
            setReal(prefix+"wet",1.f);
            for(int k=0;k<16;++k)setSlotParam(physical,k,.5f);
            for(int band=1;band<=8;++band) {
                const auto suffix=juce::String(band);
                setReal(prefix+"eqQ"+suffix,.5f);
                setReal(prefix+"studioEqKind"+suffix,
                        (float)(band==1?3:(band==7?4:0)));
                setReal(prefix+"studioEqOn"+suffix,band<=7?1.f:0.f);
                setReal(prefix+"studioEqDynamic"+suffix,0.f);
                setReal(prefix+"studioEqThreshold"+suffix,-24.f);
                setReal(prefix+"studioEqAttack"+suffix,12.f);
                setReal(prefix+"studioEqRelease"+suffix,110.f);
                setReal(prefix+"studioEqSlope"+suffix,0.f);
                setReal(prefix+"studioEqExtraShape"+suffix,0.f);
            }
            setReal(prefix+"eqLowCut",0.f);
            setReal(prefix+"eqHighCut",0.f);
        }
        for(int position=0;position<(int)nativeStages.size();++position) {
            const auto& stage=nativeStages[(size_t)position];
            setSlotType(position,stage.module);
            setSlotOn(position,true);
            setReal(slotPrefix(position)+"wet",stage.wet);
            for(int k=0;k<16;++k)setSlotParam(position,k,stage.p[(size_t)k]);
            for(int k=0;k<8;++k)
                setReal(slotPrefix(position)+"eqQ"+juce::String(k+1),
                        stage.q[(size_t)k]);
        }
        // A 12-slot Deuce study must actually be scrollable/accessible;
        // historical ten-slot presets retain ten visible physical rows.
        const int count=juce::jlimit(defaultSlotCount,slotCount,
                                    (int)nativeStages.size());
        activeSlots.store(count,std::memory_order_release);
        state.state.setProperty("rackSlots",count,nullptr);
        std::uint64_t identityOrder=0;
        for(int position=0;position<slotCount;++position)
            identityOrder|=(std::uint64_t)position << (4*position);
        packedOrder.store(identityOrder,std::memory_order_release);
    };
    if(index==skellpreset::alchemist::factoryIndex)
        loadNativeStages(skellpreset::alchemist::stages);
    else if(index==skellpreset::deuce::factoryIndex)
        loadNativeStages(skellpreset::deuce::stages);
    else if(index==skellpreset::alchemist_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_trial::stages);
    else if(index==skellpreset::deuce_v25::factoryIndex)
        loadNativeStages(skellpreset::deuce_v25::stages);
    else if(index==skellpreset::alchemist_peak_guard_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_peak_guard_trial::stages);
    else if(index==skellpreset::alchemist_tone_match_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_tone_match_trial::stages);
    else if(index==skellpreset::alchemist_lighter_dynamics_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_lighter_dynamics_trial::stages);
    else if(index==skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_fast_compressor_blend_trial::stages);
    else if(index==skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_smooth_compressor_blend_trial::stages);
    else if(index==skellpreset::alchemist_serial_dynamics_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_serial_dynamics_trial::stages);
    else if(index==skellpreset::alchemist_low_end_contour_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_low_end_contour_trial::stages);
    else if(index==skellpreset::alchemist_serial_dynamics_blend_trial::factoryIndex)
        loadNativeStages(skellpreset::alchemist_serial_dynamics_blend_trial::stages);
    setReal("mode",1);setReal("mix",1);setReal("input",0);setReal("output",0);
    // Every factory program restores the output stage to verified neutral.
    // No factory carries an invented separation value.
    setReal("stereoSeparation",0);
    // Calibrated against protected 15s OFFLINE v2.5 (~-20.0 LUFS,
    // -4.03 dBTP), not the ~6.8dB louder original FL recording chain.
    // This is factory-specific; existing user and Alchemist programs
    // retain their prior master gain and automation identities.
    if(index==skellpreset::deuce_v25::factoryIndex)
        setReal("output",5.17f);
    setReal("micPreOn",0);setReal("micPreInput",0);
    setReal("micPreTarget",0);setReal("micPreAmount",1);setReal("micPreTrim",.5f);
    // A fresh factory selection opts into the independent front-end gate.
    // Empty Rack and restored legacy/older saved states remain gate OFF:
    // their state is recalled directly, without selecting a new factory.
    setReal("micGateOn",1.f);setReal("micGateThreshold",-45.f);
    setReal("micGateRelease",160.f);setReal("micGateHold",55.f);
    setReal("micGateAttackMs",4.f);setReal("micGateHysteresisDb",3.f);
    setReal("micGateDepth",1.f);
    setReal("micTubeOn",0.f);setReal("micTubeGain",0.f);setReal("micTubeDrive",0.f);
    setReal("micTubeAir",0.f);setReal("micTubeTrim",0.f);
    // Factory switches must reset all serialized macro values, including the
    // currently hidden compatibility slots 5..8, so imported state cannot leak.
    for(int i=1;i<=8;++i)setReal("macro"+juce::String(i),.5f);
    state.state.setProperty("presetName",name,nullptr);
    state.state.setProperty("presetBank",preset.bank,nullptr);
    updateHostLatency();
}
juce::Result SkellPresetAudioProcessor::makePresetDocument(
    skellpreset::package::Document& doc) const {
    doc={};
    doc.name=name;
    doc.activeSlotCount=activeSlotCount();
    doc.bank=state.state.getProperty("presetBank","My Presets").toString();
    doc.mode=juce::jlimit(0,3,(int)value("mode"));
    doc.inputDb=value("input");doc.mix=value("mix");doc.outputDb=value("output");
    doc.micPreOn=value("micPreOn")>.5f;
    doc.micPreInput=juce::jlimit(0,3,(int)value("micPreInput"));
    doc.micPreTarget=juce::jlimit(0,3,(int)value("micPreTarget"));
    doc.micPreAmount=juce::jlimit(0.f,1.f,value("micPreAmount"));
    doc.micPreTrim=juce::jlimit(0.f,1.f,value("micPreTrim"));
    doc.micGateOn=value("micGateOn")>.5f;
    doc.micGateThreshold=value("micGateThreshold");
    doc.micGateRelease=value("micGateRelease");
    doc.micGateHold=value("micGateHold");
    doc.micGateAttackMs=value("micGateAttackMs");
    doc.micGateHysteresisDb=value("micGateHysteresisDb");
    doc.micGateDepth=value("micGateDepth");
    doc.micTubeOn=value("micTubeOn")>.5f;
    doc.micTubeGain=value("micTubeGain");doc.micTubeDrive=value("micTubeDrive");
    doc.micTubeAir=value("micTubeAir");doc.micTubeTrim=value("micTubeTrim");
    // Stored as a strict integer in the supplied raw range.
    doc.stereoSeparation=juce::jlimit(-96,96,
        juce::roundToInt(value("stereoSeparation")));
    for(int i=0;i<8;++i)doc.macros[(size_t)i]=value("macro"+juce::String(i+1));
    for(int position=0;position<slotCount;++position){
        auto& dst=doc.slots[(size_t)position];
        dst.slotId=slotIdAt(position);
        const auto prefix=slotPrefix(dst.slotId);
        dst.type=slotType(dst.slotId);
        if(dst.type==ExternalVST3)
            return juce::Result::fail("External VST3 preset export is not yet portable. Use FL Studio project save until .skellpreset v3.");
        dst.enabled=value(prefix+"on")>.5f;
        dst.wet=value(prefix+"wet");
        dst.duckOn=value(prefix+"duckOn")>.5f;
        dst.rightFirst=value(prefix+"rightFirst")>.5f;
        dst.dubDuckOn=value(prefix+"dubDuckOn")>.5f;
        dst.duckSource=juce::jlimit(0,2,(int)value(prefix+"duckSource"));
        dst.parallelReturn=value(prefix+"parallelReturn")>.5f;
        dst.duckExternal=value(prefix+"duckExternal")>.5f;
        dst.duckEffect=value(prefix+"duckEffect")>.5f;
        dst.duckDepth=value(prefix+"duckDepth");
        dst.duckThreshold=value(prefix+"duckThreshold");
        dst.duckRelease=value(prefix+"duckRelease");
        dst.duckAttack=value(prefix+"duckAttack");
        dst.limitAttack=value(prefix+"limitAttack");dst.limitRelease=value(prefix+"limitRelease");
        for(int k=0;k<16;++k)dst.params[(size_t)k]=getSlotParam(dst.slotId,k);
        for(int band=0;band<8;++band){
            const auto n=juce::String(band+1);
            dst.eqQ[(size_t)band]=value(prefix+"eqQ"+n);
            dst.studioEqKind[(size_t)band]=juce::jlimit(0,4,(int)value(prefix+"studioEqKind"+n));
            dst.studioEqOn[(size_t)band]=value(prefix+"studioEqOn"+n)>.5f;
            dst.studioEqDynamic[(size_t)band]=value(prefix+"studioEqDynamic"+n);
            dst.studioEqThreshold[(size_t)band]=value(prefix+"studioEqThreshold"+n);
            dst.studioEqAttack[(size_t)band]=value(prefix+"studioEqAttack"+n);
            dst.studioEqRelease[(size_t)band]=value(prefix+"studioEqRelease"+n);
            dst.studioEqSlope[(size_t)band]=value(prefix+"studioEqSlope"+n);
            dst.studioEqExtraShape[(size_t)band]=value(prefix+"studioEqExtraShape"+n);
        }
        dst.eqLowCut=value(prefix+"eqLowCut");
        dst.eqHighCut=value(prefix+"eqHighCut");
    }
    return juce::Result::ok();
}
juce::Result SkellPresetAudioProcessor::exportPreset(const juce::File& file) const {
    skellpreset::package::Document doc;
    if(const auto status=makePresetDocument(doc);status.failed())return status;
    // The caller may be on the message thread; ZIP compression and the
    // atomic file replace are the only work left here.
    return skellpreset::package::write(file,doc);
}
juce::Result SkellPresetAudioProcessor::importPreset(const juce::File& file) {
    skellpreset::package::Document doc;
    if(const auto result=skellpreset::package::read(file,doc);result.failed())return result;
    return applyValidatedPresetDocument(doc);
}
juce::Result SkellPresetAudioProcessor::applyLiveMatch(const skellpreset::liveMatch::Profile& dry,const skellpreset::liveMatch::Profile* reference,const skellpreset::liveMatch::Recipe* optimized) {
    using namespace skellpreset::liveMatch;
    if(!dry.valid())return juce::Result::fail("Need more clean vocal audio.");
    if(reference&&(!reference->valid()||(!optimized&&!comparable(dry,*reference))))return juce::Result::fail("Choose a clearer, different isolated reference vocal.");
    for(int s=0;s<slotCount;++s)if(slotType(s)==ExternalVST3)return juce::Result::fail("Save and remove hosted effects before replacing the rack.");
    const auto r=optimized?*optimized:fit(dry,reference);
    skellpreset::package::Document doc;
    doc.name=reference?"Match / Reference Vocal":"Match / My Vocal";doc.bank="My Presets";
    doc.mode=(int)state.getRawParameterValue("mode")->load();
    if(doc.mode==3)return juce::Result::fail("Turn on the rack before applying Match.");
    doc.micGateOn=true;doc.micGateThreshold=-65.f;doc.micGateDepth=.55f;
    auto& eq=doc.slots[0];eq.type=StudioEQFX;eq.enabled=true;eq.studioEqOn.fill(false);
    const EqControls settings(r);eq.params=settings.p;eq.eqQ=settings.q;eq.studioEqKind=settings.kind;eq.studioEqOn=settings.on;
    auto& comp=doc.slots[1];comp.type=Compressor;comp.enabled=true;
    const auto cp=compControls(r);std::copy(cp.begin(),cp.end(),comp.params.begin());
    if(r.deess){auto& ds=doc.slots[2];ds.type=DeEsser;ds.enabled=true;ds.params[0]=.35f;ds.params[1]=.65f;ds.params[2]=.15f;ds.params[3]=.3f;}
    int next=3;
    auto add=[&](int type,const EffectChoice& choice){if(choice.wet<=0.f)return;auto& slot=doc.slots[(size_t)next++];slot.type=type;slot.enabled=true;slot.wet=choice.wet;std::copy(choice.p.begin(),choice.p.end(),slot.params.begin());};
    add(VocalRiderFX,r.rider);add(Saturator,r.saturation);add(DynamicHarshnessFX,r.harshness);
    if(r.modulation==1)add(VocalChorusFX,r.mod);else if(r.modulation==2)add(VocalFlangerFX,r.mod);
    add(Delay,r.delay);add(VoxRackRoom,r.room);
    doc.outputDb=r.output;
    setReal("delta",0.f); // normal audition, never inherit delta-only output
    return applyValidatedPresetDocument(doc);
}
juce::Result SkellPresetAudioProcessor::loadVocalStarter(int profile,int variant) {
    if(profile<0 || profile>=skellpreset::starter::count || variant<0 || variant>skellpreset::starter::variations)
        return juce::Result::fail("Invalid vocal starter or variation");
    auto doc=skellpreset::starter::make<SkellPresetAudioProcessor>(profile,variant);
    const int mode=(int)state.getRawParameterValue("mode")->load();
    if(mode==0 || mode==3)doc.mode=mode;
    return applyValidatedPresetDocument(doc);
}
juce::Result SkellPresetAudioProcessor::applyValidatedPresetDocument(
    const skellpreset::package::Document& doc) {
    // The entire document is validated before touching live parameters.
    for(const auto& slot:doc.slots){
        const auto prefix=slotPrefix(slot.slotId);
        setSlotOn(slot.slotId,false);
        setSlotType(slot.slotId,slot.type);
        setReal(prefix+"wet",slot.wet);
        setReal(prefix+"duckOn",slot.duckOn?1.f:0.f);
        setReal(prefix+"rightFirst",slot.rightFirst?1.f:0.f);
        setReal(prefix+"dubDuckOn",slot.dubDuckOn?1.f:0.f);
        setReal(prefix+"duckSource",(float)slot.duckSource);
        setReal(prefix+"parallelReturn",slot.parallelReturn?1.f:0.f);
        setReal(prefix+"duckExternal",slot.duckExternal?1.f:0.f);
        setReal(prefix+"duckEffect",slot.duckEffect?1.f:0.f);
        setReal(prefix+"duckDepth",slot.duckDepth);
        setReal(prefix+"duckThreshold",slot.duckThreshold);
        setReal(prefix+"duckRelease",slot.duckRelease);
        setReal(prefix+"duckAttack",slot.duckAttack);
        setReal(prefix+"limitAttack",slot.limitAttack);setReal(prefix+"limitRelease",slot.limitRelease);
        for(int k=0;k<16;++k)setSlotParam(slot.slotId,k,slot.params[(size_t)k]);
        for(int band=0;band<8;++band){
            const auto n=juce::String(band+1);
            setReal(prefix+"eqQ"+n,slot.eqQ[(size_t)band]);
            setReal(prefix+"studioEqKind"+n,(float)slot.studioEqKind[(size_t)band]);
            setReal(prefix+"studioEqOn"+n,slot.studioEqOn[(size_t)band]?1.f:0.f);
            setReal(prefix+"studioEqDynamic"+n,slot.studioEqDynamic[(size_t)band]);
            setReal(prefix+"studioEqThreshold"+n,slot.studioEqThreshold[(size_t)band]);
            setReal(prefix+"studioEqAttack"+n,slot.studioEqAttack[(size_t)band]);
            setReal(prefix+"studioEqRelease"+n,slot.studioEqRelease[(size_t)band]);
            setReal(prefix+"studioEqSlope"+n,slot.studioEqSlope[(size_t)band]);
            setReal(prefix+"studioEqExtraShape"+n,slot.studioEqExtraShape[(size_t)band]);
        }
        setReal(prefix+"eqLowCut",slot.eqLowCut);
        setReal(prefix+"eqHighCut",slot.eqHighCut);
    }
    std::uint64_t next=0;
    for(int position=0;position<slotCount;++position){
        const auto& slot=doc.slots[(size_t)position];
        next|=(std::uint64_t)slot.slotId << (4*position);
        setSlotOn(slot.slotId,slot.enabled);
    }
    packedOrder.store(next,std::memory_order_release);
    activeSlots.store(doc.activeSlotCount,std::memory_order_release);
    state.state.setProperty("rackSlots",doc.activeSlotCount,nullptr);
    setReal("input",doc.inputDb);setReal("mix",doc.mix);setReal("output",doc.outputDb);
    setReal("mode",(float)doc.mode);
    setReal("micPreOn",doc.micPreOn?1.f:0.f);
    setReal("micPreInput",(float)doc.micPreInput);
    setReal("micPreTarget",(float)doc.micPreTarget);
    setReal("micPreAmount",doc.micPreAmount);
    setReal("micPreTrim",doc.micPreTrim);
    // The full document is validated before any live parameter changes.
    // Legacy packages default to gate OFF; modern packages preserve settings.
    setReal("micGateOn",doc.micGateOn?1.f:0.f);
    setReal("micGateThreshold",doc.micGateThreshold);
    setReal("micGateRelease",doc.micGateRelease);
    setReal("micGateHold",doc.micGateHold);
    setReal("micGateAttackMs",doc.micGateAttackMs);
    setReal("micGateHysteresisDb",doc.micGateHysteresisDb);
    setReal("micGateDepth",doc.micGateDepth);
    setReal("micTubeOn",doc.micTubeOn?1.f:0.f);setReal("micTubeGain",doc.micTubeGain);
    setReal("micTubeDrive",doc.micTubeDrive);setReal("micTubeAir",doc.micTubeAir);setReal("micTubeTrim",doc.micTubeTrim);
    // Pre-R6 packages have no separation field; the document default is 0,
    // so every existing preset loads verified neutral rather than inheriting
    // whatever the previous program left on the control.
    setReal("stereoSeparation",(float)doc.stereoSeparation);
    for(int i=0;i<8;++i)setReal("macro"+juce::String(i+1),doc.macros[(size_t)i]);
    name=doc.name;
    emptyRackIdentity.store(false,std::memory_order_release);
    selectedProgram=-1; // Imported sound is not a factory preset.
    state.state.setProperty("presetName",name,nullptr);
    state.state.setProperty("presetBank",doc.bank,nullptr);
    updateHostLatency();
    return juce::Result::ok();
}

void SkellPresetAudioProcessor::reorderSlot(int from,int to) {
    const int count=activeSlotCount();
    if(from<0||from>=count||to<0||to>=count||from==to)return;
    std::array<int,slotCount> seq{};
    const auto packed=packedOrder.load(std::memory_order_acquire);
    for(int i=0;i<slotCount;i++)seq[(size_t)i]=(int)((packed>>(4*i))&0x0fU);
    const auto moved=seq[(size_t)from];
    if(from<to)for(int i=from;i<to;i++)seq[(size_t)i]=seq[(size_t)(i+1)];
    else for(int i=from;i>to;i--)seq[(size_t)i]=seq[(size_t)(i-1)];
    seq[(size_t)to]=moved;
    std::uint64_t next=0;
    for(int i=0;i<slotCount;i++)next|=(std::uint64_t)seq[(size_t)i] << (4*i);
    packedOrder.store(next,std::memory_order_release);
}
bool SkellPresetAudioProcessor::removeRackSlot(int visualPosition) {
    // Deleting an extra row must NOT shift the APVTS physical IDs or destroy
    // hosted processors on the audio thread. Move its stable ID into the
    // inactive tail, clear its type/power and atomically reduce active count.
    const int count=activeSlotCount();
    if(count<=defaultSlotCount || visualPosition<0 || visualPosition>=count)
        return false;
    const int physical=slotIdAt(visualPosition);
    reorderSlot(visualPosition,count-1);
    setSlotOn(physical,false);
    setSlotType(physical,Empty);
    activeSlots.store(count-1,std::memory_order_release);
    state.state.setProperty("rackSlots",count-1,nullptr);
    updateHostLatency();
    return true;
}
void SkellPresetAudioProcessor::setCurrentProgram(int index) {
    if(index==emptyRackProgramIndex) {clearRackToEmpty();return;}
    if(index==customRackProgramIndex)return;
    loadFactoryPreset(index);
}
const juce::String SkellPresetAudioProcessor::getProgramName(int index) {
    if(index==emptyRackProgramIndex)return "Empty Rack";
    if(index==customRackProgramIndex)return "Custom Rack";
    if(index==skellpreset::alchemist::factoryIndex)return "Alchemist";
    if(index==skellpreset::deuce_v25::factoryIndex)return "Deuce";
    return index>=0&&index<(int)factoryPresets.size()
        ? factoryPresets[(size_t)index].name : juce::String{};
}
void SkellPresetAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=state.copyState();
    snapshot.setProperty("effectPresentation",skellcore::ui::toString(presentation()),nullptr);
    juce::String sequence;
    for(int i=0;i<slotCount;i++)sequence+=juce::String(slotIdAt(i))+(i==slotCount-1?"":",");
    snapshot.setProperty("slotOrder",sequence,nullptr);
    snapshot.setProperty("rackSlots",activeSlotCount(),nullptr);
    snapshot.setProperty("presetName",presetName(),nullptr);
    const int savedProgram=selectedProgram==-2 &&
        !emptyRackIdentity.load(std::memory_order_acquire) ? -1 : selectedProgram;
    snapshot.setProperty("program",savedProgram,nullptr);
    for(int s=0;s<slotCount;++s) {
        const auto index=juce::String(s+1);
        if(slotType(s)!=ExternalVST3){
            snapshot.removeProperty("hostPath"+index,nullptr);
            snapshot.removeProperty("hostClass"+index,nullptr);
            snapshot.removeProperty("hostState"+index,nullptr);
            continue;
        }
        const auto file=hosted.getFile(s);
        // Keep saved metadata when the exact plugin is missing.
        if(file.getFullPathName().isEmpty())continue;
        snapshot.setProperty("hostPath"+index,file.getFullPathName(),nullptr);
        snapshot.setProperty("hostClass"+index,hosted.getIdentifier(s),nullptr);
        snapshot.removeProperty("hostState"+index,nullptr);
        auto data=hosted.getState(s);
        if(data.getSize()>0 && data.getSize()<2*1024*1024)
            snapshot.setProperty("hostState"+index,data.toBase64Encoding(),nullptr);
    }
    if(auto xml=snapshot.createXml())copyXmlToBinary(*xml,dest);
}
void SkellPresetAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) {
        auto loaded=juce::ValueTree::fromXml(*xml);
        if(!loaded.isValid()||loaded.getType()!=state.state.getType())return;
        const auto sequence=loaded.getProperty("slotOrder").toString();
        juce::StringArray items;items.addTokens(sequence,",","");
        // Stage 8 states had ten positions; append six default identities when
        // reading them. A malformed order leaves the current permutation intact.
        // JUCE addTokens omits empty tokens and getIntValue silently maps
        // malformed text to zero. Only an exact decimal permutation can
        // reorder an already-open FL Studio session. Preserve the existing
        // rack order if commas, signs or nonnumeric tokens are malformed.
        const bool exactFields=(sequence.length()-
            sequence.removeCharacters(",").length())==items.size()-1;
        if(exactFields && (items.size()==defaultSlotCount || items.size()==slotCount)) {
            std::array<bool,slotCount> seen{};
            std::array<int,slotCount> proposed{};
            bool valid=true;
            const int oldCount=items.size();
            for(int i=0;i<oldCount;++i) {
                const auto token=items[i].trim();
                if(token.isEmpty() || token.length()>2 ||
                    !token.containsOnly("0123456789")) {
                    valid=false;break;
                }
                const int v=token.getIntValue();
                if(v<0 || v>=oldCount || seen[(size_t)v]){valid=false;break;}
                seen[(size_t)v]=true;proposed[(size_t)i]=v;
            }
            if(valid){
                for(int i=oldCount;i<slotCount;++i)proposed[(size_t)i]=i;
                std::uint64_t next=0;
                for(int i=0;i<slotCount;++i)
                    next|=(std::uint64_t)proposed[(size_t)i] << (4*i);
                packedOrder.store(next,std::memory_order_release);
            }
        }
        const int restoredCount=juce::jlimit(defaultSlotCount,slotCount,
            (int)loaded.getProperty("rackSlots",defaultSlotCount));
        activeSlots.store(restoredCount,std::memory_order_release);
        name=loaded.getProperty("presetName","Custom").toString();
        // Legacy/custom host states may not contain a trustworthy factory ID.
        // Never silently turn a missing or malformed identity into factory 0 or
        // clamp an invalid positive ID onto the last factory preset.
        selectedProgram=-1;
        if(loaded.hasProperty("program")) {
            const auto storedProgram=loaded.getProperty("program");
            bool validProgram=false;
            int parsedProgram=-1;
            if(storedProgram.isInt() || storedProgram.isInt64() || storedProgram.isDouble()) {
                const double numeric=static_cast<double>(storedProgram);
                if(std::isfinite(numeric) && numeric==std::floor(numeric)
                    && numeric>=-2.0 && numeric<(double)factoryPresets.size()) {
                    parsedProgram=(int)numeric;
                    validProgram=true;
                }
            } else if(storedProgram.isString()) {
                // JUCE XML can surface an integer attribute as text. Accept
                // only a canonical decimal token; getIntValue alone accepts
                // malformed strings such as "oops" as zero.
                const auto token=storedProgram.toString().trim();
                if(token.isNotEmpty() && token.length()<=3
                    && token.containsOnly("0123456789-")) {
                    const int numeric=token.getIntValue();
                    if(juce::String(numeric)==token && numeric>=-2
                        && numeric<(int)factoryPresets.size()) {
                        parsedProgram=numeric;
                        validProgram=true;
                    }
                }
            }
            if(validProgram)selectedProgram=parsedProgram;
        }
        // Restore missing phase-type flags to OFF before replacing state.
        for(int slot=1;slot<=slotCount;++slot) {
            const auto id="s"+juce::String(slot)+"_fxExt12";
            bool present=false;
            for(int i=0;i<loaded.getNumChildren();++i)
                if(loaded.getChild(i).getProperty("id").toString()==id){present=true;break;}
            if(!present){juce::ValueTree node(juce::Identifier("PARAM"));node.setProperty("id",id,nullptr);node.setProperty("value",0.f,nullptr);loaded.appendChild(node,nullptr);}
        }
        // Old host snapshots may lack this generation; avoid retaining a
        // previous Suite insert when recalling an older project.
        for(int slot=1;slot<=slotCount;++slot) {
            const auto id="s"+juce::String(slot)+"_fxExt11";
            bool present=false;
            for(int i=0;i<loaded.getNumChildren();++i)
                if(loaded.getChild(i).getProperty("id").toString()==id) {
                    present=true;break;
                }
            if(!present) {
                juce::ValueTree child(juce::Identifier("PARAM"));
                child.setProperty("id",id,nullptr);
                child.setProperty("value",0.f,nullptr);
                loaded.appendChild(child,nullptr);
            }
        }
        // Old FL Studio snapshots have no fxExt10. Explicitly supply OFF to
        // avoid inheriting a previously active opto instance when recalling a
        // legacy project on an already-open processor.
        for(int slot=1;slot<=slotCount;++slot) {
            const auto id="s"+juce::String(slot)+"_fxExt10";
            bool present=false;
            for(int i=0;i<loaded.getNumChildren();++i)
                if(loaded.getChild(i).getProperty("id").toString()==id) {
                    present=true;break;
                }
            if(!present) {
                juce::ValueTree child(juce::Identifier("PARAM"));
                child.setProperty("id",id,nullptr);
                child.setProperty("value",0.f,nullptr);
                loaded.appendChild(child,nullptr);
            }
        }
        // Pre-gate FL Studio host snapshots have no micGateOn child. If an
        // older state replaces an active factory gate, explicitly restore OFF
        // rather than letting the current atomics leak into the old project.
        // This addition affects only state recall, never fresh factory loads.
        bool serializedMicGate=false;
        for(int child=0;child<loaded.getNumChildren();++child) {
            if(loaded.getChild(child).getProperty("id").toString()=="micGateOn") {
                serializedMicGate=true;
                break;
            }
        }
        if(!serializedMicGate) {
            juce::ValueTree absentMicGate(juce::Identifier("PARAM"));
            absentMicGate.setProperty("id","micGateOn",nullptr);
            absentMicGate.setProperty("value",0.f,nullptr);
            loaded.appendChild(absentMicGate,nullptr);
        }
        // Legacy project snapshots omit new controls: fill defaults instead
        // of inheriting settings from a previously loaded modern preset.
        struct GateLegacyDefault {const char* id;float initial;};
        constexpr GateLegacyDefault defaults[]{
            {"micGateAttackMs",4.f},{"micGateHysteresisDb",3.f},{"micGateDepth",1.f},
            // Pre-R6 snapshots predate the output-stage separation control.
            {"stereoSeparation",0.f},
            {"micTubeOn",0.f},{"micTubeGain",0.f},{"micTubeDrive",0.f},{"micTubeAir",0.f},{"micTubeTrim",0.f}
        };
        for(const auto& def:defaults) {
            bool present=false;
            for(int i=0;i<loaded.getNumChildren();++i)
                if(loaded.getChild(i).getProperty("id").toString()==def.id) {
                    present=true;break;
                }
            if(!present) {
                juce::ValueTree node(juce::Identifier("PARAM"));
                node.setProperty("id",def.id,nullptr);
                node.setProperty("value",def.initial,nullptr);
                loaded.appendChild(node,nullptr);
            }
        }
        for(int slot=1;slot<=slotCount;++slot)for(int timing=0;timing<2;++timing) {
            const auto id="s"+juce::String(slot)+(timing==0?"_limitAttack":"_limitRelease");
            bool present=false;
            for(int i=0;i<loaded.getNumChildren();++i)
                present=present || loaded.getChild(i).getProperty("id").toString()==id;
            if(!present) {
                juce::ValueTree node(juce::Identifier("PARAM"));
                node.setProperty("id",id,nullptr);
                node.setProperty("value",timing==0?0.f:70.f,nullptr);
                loaded.appendChild(node,nullptr);
            }
        }
        // Old states must not inherit a current opt-in send.
        for(int slot=1;slot<=slotCount;++slot)
            for(const auto* suffix:{"rightFirst","dubDuckOn","duckSource","parallelReturn","duckExternal","duckEffect"}) {
                const auto id="s"+juce::String(slot)+"_"+suffix;
                bool present=false;
                for(int i=0;i<loaded.getNumChildren();++i)
                    present=present || loaded.getChild(i).getProperty("id").toString()==id;
                if(!present) {
                    juce::ValueTree node(juce::Identifier("PARAM"));
                    node.setProperty("id",id,nullptr);
                    node.setProperty("value",0.f,nullptr);
                    loaded.appendChild(node,nullptr);
                }
            }
        // Projects predating dynamic EQ must always recall the original static sound.
        juce::StringArray existingEqIds;for(int i=0;i<loaded.getNumChildren();++i)existingEqIds.add(loaded.getChild(i).getProperty("id").toString());
        const char* eqFields[]{"Dynamic","Threshold","Attack","Release","Slope","ExtraShape"};
        const float eqLegacyDefaults[]{0.f,-24.f,12.f,110.f,0.f,0.f};
        for(int slot=1;slot<=slotCount;++slot)for(int band=1;band<=8;++band)for(int field=0;field<6;++field){
            const auto id="s"+juce::String(slot)+"_studioEq"+eqFields[field]+juce::String(band);
            if(!existingEqIds.contains(id)){juce::ValueTree node(juce::Identifier("PARAM"));node.setProperty("id",id,nullptr);node.setProperty("value",eqLegacyDefaults[field],nullptr);loaded.appendChild(node,nullptr);}
        }
        // Repair untrusted host XML before JUCE copies it into APVTS atomics.
        sanitizeParameterStateValues(loaded, state);
        setPresentation(skellcore::ui::presentationFromString(loaded.getProperty("effectPresentation").toString().toStdString()));
        loaded.removeProperty("effectPresentation",nullptr); // UI state, not a parameter
        state.replaceState(loaded);
        bool slotsEmpty=true;
        for(int slot=0;slot<slotCount;++slot) {
            const auto* enabled=state.getRawParameterValue(slotPrefix(slot)+"on");
            if(slotType(slot)!=Empty || (enabled && enabled->load()>0.5f)) {
                slotsEmpty=false;
                break;
            }
        }
        if(selectedProgram==-1 && name=="Empty Rack" && slotsEmpty)
            selectedProgram=-2;
        if(selectedProgram==-2 && !slotsEmpty)
            selectedProgram=-1;
        emptyRackIdentity.store(selectedProgram==-2,std::memory_order_release);
        updateHostLatency();
        // Don't instantiate external code in setStateInformation(): hosts may
        // call this from threads that cannot safely load a plugin DLL.
        bool restore=false;
        for(int s=0;s<slotCount;++s)
            if(loaded.hasProperty("hostPath"+juce::String(s+1))) {restore=true;break;}
        if(restore)triggerAsyncUpdate();
    }
}
void SkellPresetAudioProcessor::addProgramIdentityListeners() {
    auto add=[this](const juce::String& id) {
        if(state.getParameter(id)==nullptr)return;
        state.addParameterListener(id,this);
        programIdentityListenerIds.add(id);
    };
    for(const auto* id:{"input","mix","output","autoGain","delta",
                        "adapt","adaptAmount","globalGate","gateThreshold","laneMode",
                        "micPreOn","micPreInput","micPreTarget","micPreAmount","micPreTrim",
                        "micGateOn","micGateThreshold","micGateRelease","micGateHold",
                        "micGateAttackMs","micGateHysteresisDb","micGateDepth",
                        "stereoSeparation","micTubeOn","micTubeGain","micTubeDrive","micTubeAir","micTubeTrim"})
        add(id);
    for(int macro=1;macro<=8;++macro)add("macro"+juce::String(macro));
    for(const auto* lane:{"A","B","C"}) {
        const auto prefix="lane"+juce::String(lane);
        add(prefix+"_level");add(prefix+"_pan");add(prefix+"_mute");
    }
    for(int slot=0;slot<slotCount;++slot) {
        const auto prefix=slotPrefix(slot);
        for(const auto* suffix:{"type","on","wet","lane","fxExt","fxExt2",
                                "fxExt3","fxExt4","fxExt5","fxExt6","fxExt7",
                                "fxExt8","fxExt9","fxExt10","fxExt11","fxExt12","eqLowCut","eqHighCut",
                                "rightFirst","dubDuckOn","duckSource","parallelReturn","duckExternal","duckEffect"})add(prefix+suffix);
        for(int parameter=1;parameter<=16;++parameter)
            add(prefix+"p"+juce::String(parameter));
        for(int band=1;band<=8;++band) {
            const auto suffix=juce::String(band);
            add(prefix+"eqQ"+suffix);
            add(prefix+"studioEqKind"+suffix);
            add(prefix+"studioEqOn"+suffix);
            add(prefix+"studioEqDynamic"+suffix);
            add(prefix+"studioEqThreshold"+suffix);
            add(prefix+"studioEqAttack"+suffix);
            add(prefix+"studioEqRelease"+suffix);
            add(prefix+"studioEqSlope"+suffix);
            add(prefix+"studioEqExtraShape"+suffix);
        }
    }
}
void SkellPresetAudioProcessor::removeProgramIdentityListeners() noexcept {
    for(const auto& id:programIdentityListenerIds)
        state.removeParameterListener(id,this);
    programIdentityListenerIds.clear();
}
void SkellPresetAudioProcessor::parameterChanged(const juce::String&,float) {
    if(!suppressProgramIdentityUpdates.load(std::memory_order_acquire))
        emptyRackIdentity.store(false,std::memory_order_release);
}
SkellPresetAudioProcessor::~SkellPresetAudioProcessor() {
    removeProgramIdentityListeners();
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new SkellPresetAudioProcessor();}
