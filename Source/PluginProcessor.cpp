#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "chain/EffectRegistry.h"
#include "chain/AlchemistNativePreset.h"
#include <cmath>

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
}

SkellPresetAudioProcessor::SkellPresetAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  state(*this,nullptr,"PARAMETERS",createParameterLayout()) {
    std::uint64_t initialOrder=0;
    for(int i=0;i<slotCount;i++){
        initialOrder|=(std::uint64_t)i << (4*i);
        slotPeaks[(size_t)i].store(0);
        auto& r=rawSlots[(size_t)i];const auto prefix=slotPrefix(i);
        r.type=state.getRawParameterValue(prefix+"type");
        r.extension=state.getRawParameterValue(prefix+"fxExt");
        r.extension2=state.getRawParameterValue(prefix+"fxExt2");
        r.extension3=state.getRawParameterValue(prefix+"fxExt3");
        r.extension4=state.getRawParameterValue(prefix+"fxExt4");
        r.extension5=state.getRawParameterValue(prefix+"fxExt5");
        r.extension6=state.getRawParameterValue(prefix+"fxExt6");
        r.on=state.getRawParameterValue(prefix+"on");
        r.wet=state.getRawParameterValue(prefix+"wet");
        r.eqLowCut=state.getRawParameterValue(prefix+"eqLowCut");
        r.eqHighCut=state.getRawParameterValue(prefix+"eqHighCut");
        for(int k=0;k<16;k++)r.p[(size_t)k]=state.getRawParameterValue(prefix+"p"+juce::String(k+1));
        for(int k=0;k<8;++k)r.q[(size_t)k]=state.getRawParameterValue(prefix+"eqQ"+juce::String(k+1));
    }
    packedOrder.store(initialOrder);
    rawMic.on=state.getRawParameterValue("micPreOn");
    rawMic.input=state.getRawParameterValue("micPreInput");
    rawMic.target=state.getRawParameterValue("micPreTarget");
    rawMic.amount=state.getRawParameterValue("micPreAmount");
    rawMic.trim=state.getRawParameterValue("micPreTrim");
    rawGlobal.mode=state.getRawParameterValue("mode");
    rawGlobal.input=state.getRawParameterValue("input");
    rawGlobal.mix=state.getRawParameterValue("mix");
    rawGlobal.output=state.getRawParameterValue("output");
    rawGlobal.gate=state.getRawParameterValue("globalGate");
    rawGlobal.threshold=state.getRawParameterValue("gateThreshold");
    rawGlobal.delta=state.getRawParameterValue("delta");
    for(int k=0;k<4;k++)rawGlobal.macros[(size_t)k]=state.getRawParameterValue("macro"+juce::String(k+1));
    // Factory initialization is separate from state restoration; no DSP is allocated here.
    // New instances show the sole visible factory sound. Restored DAW
    // projects retain all previous program IDs and their saved state.
    loadFactoryPreset(skellpreset::alchemist::factoryIndex);
}

juce::AudioProcessorValueTreeState::ParameterLayout SkellPresetAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto fl=[&](juce::String id,juce::String label,float low,float high,float initial,float step=0.001f){
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},label,juce::NormalisableRange<float>(low,high,step),initial));
    };
    auto bl=[&](juce::String id,juce::String label,bool initial){
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},label,initial));
    };
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Mode",juce::StringArray{"Simple","Advanced","Vocoding","Bypass"},1));
    fl("input","Input",-24,24,0);fl("mix","Mix",0,1,1);
    fl("output","Output",-24,24,0);
    bl("autoGain","Auto gain (reserved)",false);bl("delta","Delta",false);
    bl("adapt","Adapt (reserved)",false);fl("adaptAmount","Adapt amount (reserved)",0,1,0);
    bl("globalGate","Legacy global gate (inactive; use rack Gate)",false);fl("gateThreshold","Gate threshold",-80,0,-50);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"laneMode",1},"Lane mode (reserved)",juce::StringArray{"Series","Parallel"},0));
    for(int k=1;k<=8;k++) fl("macro"+juce::String(k),"Macro "+juce::String(k),0,1,.5f);
    // Preserve the original 0..22 choice count (and host normalization).
    auto legacyTypes=types;
    while(legacyTypes.size()>RingModFX+1)
        legacyTypes.remove(legacyTypes.size()-1);
    for(int s=1;s<=slotCount;s++) {
        const auto id="s"+juce::String(s)+"_";
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id+"type",1},"Slot "+juce::String(s)+" type",legacyTypes,0));
        bl(id+"on","Slot "+juce::String(s)+" active",false);
        fl(id+"wet","Slot "+juce::String(s)+" wet",0,1,1);
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
    return p;
}
void SkellPresetAudioProcessor::EngineSlot::prepare(double sampleRate) {
    peak.prepare(sampleRate);highpass.prepare(sampleRate);comp.prepare(sampleRate);gate.prepare(sampleRate);
    deesser.prepare(sampleRate);
    radio.prepare(sampleRate);crusher.prepare(sampleRate);tremolo.prepare(sampleRate);
    width.prepare(sampleRate);threeBand.prepare(sampleRate);
    voxRoom.prepare(sampleRate);voxComp.prepare(sampleRate);
    chorus.prepare(sampleRate);flanger.prepare(sampleRate);ring.prepare(sampleRate);
    rider.prepare(sampleRate);duckingDelay.prepare(sampleRate);vibrato.prepare(sampleRate);
    eightBand.prepare(sampleRate);serialDynamics.prepare(sampleRate);matchFinish.prepare(sampleRate);
    micModel.prepare(sampleRate);skelldizer.prepare(sampleRate);
    harshness.prepare(sampleRate);noiseControl.prepare(sampleRate);
    studioPitch.prepare(sampleRate);
    pitchDry.prepare(skellpreset::dsp::SpectralPitchFormant::latencySamples);
    delay.prepare(sampleRate);reverb[0].prepare(sampleRate);
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
        if(type==Limiter) total+=latency;
        else if(type==ExternalVST3) total+=hosted.getLatency(id);
        else if(type==StudioPitchFormantFX)total+=skellpreset::dsp::SpectralPitchFormant::latencySamples;
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
    rate=(std::isfinite(sampleRate)&&sampleRate>=8000.)?sampleRate:48000.;
    displayedRate.store(rate,std::memory_order_release);
    hostChunkCapacity=juce::jlimit(64,8192,juce::jmax(512,samplesPerBlock));
    rackWork.setSize(2,hostChunkCapacity,false,true);
    hostWork.setSize(2,hostChunkCapacity,false,true);
    dryWork.setSize(2,hostChunkCapacity,false,true);
    bypassWork.setSize(2,hostChunkCapacity,false,true);
    for(auto& e:engines)e.prepare(rate);
    micFrontEnd.prepare(rate);
    micFrontWet.reset(rate,.020);
    micFrontWet.setCurrentAndTargetValue(rawMic.on->load()>.5f?1.f:0.f);
    hosted.prepare(rate,hostChunkCapacity);
    const auto lookahead=engines[0].limiter.latencySamples();
    limiterLookahead.store(lookahead,std::memory_order_release);
    const int pdcCapacity=slotCount*(lookahead+skellpreset::dsp::SpectralPitchFormant::latencySamples)+skellpreset::host::HostedVST3::maxTotalLatency;
    dryPdc.prepare(pdcCapacity);
    bypassPdc.prepare(pdcCapacity);
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
    return (in==juce::AudioChannelSet::mono() && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo()))
        || (in==juce::AudioChannelSet::stereo() && out==juce::AudioChannelSet::stereo());
}
float SkellPresetAudioProcessor::value(const juce::String& id) const noexcept {
    if(auto* v=state.getRawParameterValue(id))
        return v->load();
    return 0;
}
void SkellPresetAudioProcessor::setReal(const juce::String& id,float v) {
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
    if(oldType!=Empty)return oldType;
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
    const bool newMic=chosen==MicModelFX && slotType(slot)!=MicModelFX;
    const bool newDizer=chosen==SkelldizerFX && slotType(slot)!=SkelldizerFX;
    if(chosen<=MicModelFX)setReal(id+"fxExt6",0.f);
    if(chosen>MicModelFX) {
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
    updateHostLatency();
}
juce::String SkellPresetAudioProcessor::moduleName(int type) const {return types[juce::jlimit(0,types.size()-1,type)];}
juce::Result SkellPresetAudioProcessor::loadHostedVST3(int slot,const juce::File& path) {
    if(slot<0 || slot>=slotCount)return juce::Result::fail("Invalid host slot");
    const auto result=hosted.load(slot,path,rate,hostChunkCapacity);
    if(result.failed())return result;
    setSlotType(slot,ExternalVST3);
    setSlotOn(slot,true);
    updateHostLatency();
    return juce::Result::ok();
}
void SkellPresetAudioProcessor::openHostedVST3Editor(int slot) {
    auto* plugin=hosted.get(slot);
    if(!plugin)return;
    if(auto* existing=plugin->getActiveEditor()) {
        existing->toFront(true);
        return;
    }
    if(!plugin->hasEditor())return;
    if(auto* editor=plugin->createEditorIfNeeded()) {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle="SKELL  /  "+hosted.getName(slot);
        options.dialogBackgroundColour=juce::Colours::black;
        options.content.setOwned(editor);
        options.resizable=true;
        options.useNativeTitleBar=true;
        options.launchAsync();
    }
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
        if(path.isEmpty() || !type || (int)type->load()!=ExternalVST3)continue;
        if(hosted.getFile(s).getFullPathName()!=path) {
            const auto result=loadHostedVST3(s,juce::File(path));
            if(result.failed())continue; // Missing third-party plugin stays dry/safe.
        }
        const auto payload=state.state.getProperty("hostState"+key).toString();
        if(payload.isNotEmpty() && payload.length()<3*1024*1024) {
            juce::MemoryBlock bytes;
            if(bytes.fromBase64Encoding(payload))hosted.setState(s,bytes);
        }
    }
    updateHostLatency();
}


void SkellPresetAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels=juce::jmin(2,buffer.getNumChannels());
    const int total=buffer.getNumSamples();
    if(channels<1 || total<=0)return;
    const bool monoSource=getTotalNumInputChannels()==1;
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
    const bool delta=rawGlobal.delta->load()>.5f;
    const float macroTone=rawGlobal.macros[0]->load();
    const float macroPunch=rawGlobal.macros[1]->load();
    const float macroSpace=rawGlobal.macros[2]->load();
    const float macroGrit=rawGlobal.macros[3]->load();
    // Read all host automation once per callback. Native algorithms share
    // one preallocated engine per physical slot; no allocation or lock here.
    for(int s=0;s<slotCount;++s) {
        auto& c=controlCache[(size_t)s];
        const auto& raw=rawSlots[(size_t)s];
        c.type=slotType(s);
        c.on=raw.on->load()>.5f;
        c.wet=juce::jlimit(0.f,1.f,raw.wet->load());
        c.eqLowCut=raw.eqLowCut->load();
        c.eqHighCut=raw.eqHighCut->load();
        for(int k=0;k<16;++k)c.p[(size_t)k]=raw.p[(size_t)k]->load();
        for(int k=0;k<8;++k)c.q[(size_t)k]=raw.q[(size_t)k]->load();
        auto& e=engines[(size_t)s];
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
                                  c.p[8],c.p[9],c.p[10],c.p[11]);
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
    const int chainCount=activeSlotCount();
    std::array<int,slotCount> sequence{};
    const auto packed=packedOrder.load(std::memory_order_acquire);
    for(int i=0;i<slotCount;++i)
        sequence[(size_t)i]=(int)((packed>>(4*i)) & 0x0fU);
    const int lookahead=limiterLookahead.load(std::memory_order_acquire);
    int blockLatency=0;
    for(int position=0;position<chainCount;++position) {
        const int id=sequence[(size_t)position];
        if(id<0 || id>=slotCount)continue;
        const auto type=controlCache[(size_t)id].type;
        if(type==Limiter)blockLatency+=lookahead;
        else if(type==ExternalVST3)blockLatency+=hosted.getLatency(id);
        else if(type==StudioPitchFormantFX)blockLatency+=skellpreset::dsp::SpectralPitchFormant::latencySamples;
    }
    blockLatency=juce::jlimit(0,slotCount*(lookahead+skellpreset::dsp::SpectralPitchFormant::latencySamples)+
        skellpreset::host::HostedVST3::maxTotalLatency,blockLatency);
    std::array<float,slotCount> meter{};
    float inPeak=0.f,outPeak=0.f;
    const int limit=juce::jmax(64,hostChunkCapacity);
    for(int offset=0;offset<total;offset+=limit) {
        const int frames=juce::jmin(limit,total-offset);
        // Forward host transport to nested VST3s without retaining the DAW's
        // callback-scoped playhead pointer. Account for our internal chunks.
        hosted.updateTransport(hostPosition,offset,rate);
        // setSize(...,avoidReallocating=true) reuses prepareToPlay's fixed buffers.
        rackWork.setSize(2,frames,false,false,true);
        dryWork.setSize(2,frames,false,false,true);
        bypassWork.setSize(2,frames,false,false,true);
        auto* inL=buffer.getWritePointer(0)+offset;
        auto* inR=channels==2?buffer.getWritePointer(1)+offset:nullptr;
        for(int n=0;n<frames;++n) {
            const float l=std::isfinite(inL[n])?inL[n]:0.f;
            const float r=inR&&!monoSource?(std::isfinite(inR[n])?inR[n]:0.f):l;
            inPeak=std::max(inPeak,std::max(std::abs(l),std::abs(r)));
            const float inputGain=inputSmooth.getNextValue();
            const float preL=l*inputGain,preR=r*inputGain;
            const float micWet=micFrontWet.getNextValue();
            float frontL=preL,frontR=preR;
            // Default OFF should be CPU-neutral for legacy FL Studio racks.
            // Process the input model only while active or during its 20ms
            // bypass crossfade; no allocations or latency are introduced.
            if(micWet>1.e-6f) {
                const auto colored=micFrontEnd.process(preL,preR);
                frontL=preL+(colored[0]-preL)*micWet;
                frontR=preR+(colored[1]-preR)*micWet;
            }
            const auto original=bypassPdc.process(l,r,blockLatency);
            // Front-end tone is upstream of the rack AND its wet/dry blend.
            // Full bypass still returns the original DAW signal unchanged.
            const auto dry=dryPdc.process(frontL,frontR,blockLatency);
            rackWork.setSample(0,n,frontL);rackWork.setSample(1,n,frontR);
            dryWork.setSample(0,n,dry[0]);dryWork.setSample(1,n,dry[1]);
            bypassWork.setSample(0,n,original[0]);
            bypassWork.setSample(1,n,original[1]);
        }
        // The rack is processed one block per stage. Hosted VST3 effects
        // receive proper blocks instead of one-sample callbacks.
        for(int pos=0;pos<chainCount;++pos) {
            const int s=sequence[(size_t)pos];
            if(s<0 || s>=slotCount)continue;
            const auto& c=controlCache[(size_t)s];
            if(c.type==Empty)continue;
            auto& e=engines[(size_t)s];
            if(c.type==ExternalVST3) {
                auto* plugin=hosted.get(s);
                const int latency=hosted.getLatency(s);
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
                continue;
            }
            const bool keepTail=c.type==Limiter || c.type==Delay ||
                c.type==Reverb || c.type==VoxRackRoom ||
                c.type==VocalChorusFX || c.type==VocalFlangerFX ||
                c.type==AdlibThrow || c.type==AdlibDoubler ||
                c.type==DuckingPingPongFX || c.type==VocalVibratoFX ||
                c.type==StudioPitchFormantFX;
            if(!keepTail && slotWetSmooth[(size_t)s].getCurrentValue()<.000001f
                && slotWetSmooth[(size_t)s].getTargetValue()==0.f)continue;
            for(int n=0;n<frames;++n) {
                float l=rackWork.getSample(0,n);
                float r=rackWork.getSample(1,n);
                float wetL=l,wetR=r;
                switch(c.type) {
                    case EQ: {
                        const auto high=e.highpass.process(l,r);
                        const auto y=e.peak.process(high[0],high[1]);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Compressor: {
                        const float g=e.comp.gain(std::max(std::abs(l),std::abs(r)));
                        wetL=l*g;wetR=r*g;break;
                    }
                    case Limiter: {
                        const float drive=dbToGain(c.p[1]*18.f);
                        const float input[2]{l*drive,r*drive};
                        float output[2]{};
                        e.limiter.processFrame(input,output,2);
                        wetL=output[0];wetR=output[1];break;
                    }
                    case Gate: {
                        const float g=e.gate.processGain(std::max(std::abs(l),std::abs(r)));
                        wetL=l*g;wetR=r*g;break;
                    }
                    case DeEsser: {
                        const auto y=e.deesser.process(l,r);
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
                    case Saturator: {
                        const float drive=dbToGain(c.p[0]*24.f+10.f*(macroGrit-.5f));
                        const float blend=.5f+c.p[1];
                        wetL=std::tanh(l*drive*blend)/std::max(.5f,drive*.6f);
                        wetR=std::tanh(r*drive*blend)/std::max(.5f,drive*.6f);
                        break;
                    }
                    case Exciter: {
                        const float drive=1.f+10.f*c.p[0];
                        wetL=(std::tanh(l*drive)-std::tanh(l*drive*.55f))*.4f+l;
                        wetR=(std::tanh(r*drive)-std::tanh(r*drive*.55f))*.4f+r;
                        break;
                    }
                    case Delay: {
                        const auto y=e.delay.process(l,r);
                        wetL=y[0];wetR=y[1];break;
                    }
                    case Reverb: {
                        const auto y=e.reverb[0].processFrame(l,r);
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
                // Sample-align a bypassed limiter as it still contributes PDC.
                if(c.type==Limiter) {
                    const auto delayed=e.limiterDry.process(l,r,lookahead);
                    l=delayed[0];r=delayed[1];
                } else if(c.type==StudioPitchFormantFX) {
                    // The spectral engine is delayed 768 samples; its local
                    // dry path must match before applying the rack wet knob.
                    const auto aligned=e.pitchDry.process(
                        l,r,skellpreset::dsp::SpectralPitchFormant::latencySamples);
                    l=aligned[0];r=aligned[1];
                }
                const float wet=slotWetSmooth[(size_t)s].getNextValue();
                const float processedL=l+(wetL-l)*wet;
                const float processedR=r+(wetR-r)*wet;
                rackWork.setSample(0,n,processedL);
                rackWork.setSample(1,n,processedR);
                meter[(size_t)s]=std::max(meter[(size_t)s],
                    std::max(std::abs(processedL),std::abs(processedR)));
            }
        }
        for(int n=0;n<frames;++n) {
            const float dryL=dryWork.getSample(0,n);
            const float dryR=dryWork.getSample(1,n);
            const float wetL=rackWork.getSample(0,n);
            const float wetR=rackWork.getSample(1,n);
            const float mix=mixSmooth.getNextValue();
            const float outGain=outputSmooth.getNextValue();
            const float blend=bypassSmooth.getNextValue();
            const float mixedL=dryL+(wetL-dryL)*mix;
            const float mixedR=dryR+(wetR-dryR)*mix;
            const float finalL=(delta?mixedL-dryL:mixedL)*outGain;
            const float finalR=(delta?mixedR-dryR:mixedR)*outGain;
            const float outputL=bypassWork.getSample(0,n)+
                (finalL-bypassWork.getSample(0,n))*blend;
            const float outputR=bypassWork.getSample(1,n)+
                (finalR-bypassWork.getSample(1,n))*blend;
            inL[n]=std::isfinite(outputL)?outputL:0.f;
            if(inR)inR[n]=std::isfinite(outputR)?outputR:0.f;
            outPeak=std::max(outPeak,std::max(std::abs(inL[n]),
                inR?std::abs(inR[n]):0.f));
        }
        // Read only the processed output chunk here; the pointers above
        // are scoped to this host block and remain valid during capture.
        liveScope.push(inL,inR,frames);
        capture.push(inL,inR,frames);
    }
    inputPeak.store(inPeak,std::memory_order_relaxed);
    outputPeak.store(outPeak,std::memory_order_relaxed);
    for(int s=0;s<slotCount;++s)
        slotPeaks[(size_t)s].store(meter[(size_t)s],std::memory_order_relaxed);
}

int SkellPresetAudioProcessor::appendRackSlot() {
    const int count=activeSlotCount();
    if(count>=slotCount)return -1;
    activeSlots.store(count+1,std::memory_order_release);
    state.state.setProperty("rackSlots",count+1,nullptr);
    updateHostLatency();
    return slotIdAt(count);
}
void SkellPresetAudioProcessor::loadFactoryPreset(int index) {
    if(index<0||index>=(int)factoryPresets.size())return;
    selectedProgram=index;const auto& preset=factoryPresets[(size_t)index];name=preset.name;
    activeSlots.store(defaultSlotCount,std::memory_order_release);
    state.state.setProperty("rackSlots",defaultSlotCount,nullptr);
    for(int s=0;s<slotCount;s++){
        setSlotType(s,Empty);setSlotOn(s,false);setReal(slotPrefix(s)+"wet",1);
        for(int k=0;k<16;k++)setSlotParam(s,k,.5f);
        for(int band=1;band<=8;++band)setReal(slotPrefix(s)+"eqQ"+juce::String(band),.5f);
        setReal(slotPrefix(s)+"eqLowCut",0.f);
        setReal(slotPrefix(s)+"eqHighCut",0.f);
        
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
    if(index==24 || index==25) {
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
        if(index==25) {
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
    if(index==26) {
        // A VoxRack-toned phrase throw plus an independently authored
        // radio insert; no native VST3 host latency is added.
        setSlotType(3,Radio);setSlotOn(3,true);
        setSlotParam(3,0,.26f);setSlotParam(3,1,.38f);
        setSlotParam(3,2,.52f);setSlotParam(3,3,.28f);
        setReal("s4_wet",.72f);
        setSlotParam(4,2,.37f);
    } else if(index==27) {
        setSlotType(3,RetroCrush);setSlotOn(3,true);
        setSlotParam(3,0,.28f);setSlotParam(3,1,.42f);
        setSlotParam(3,2,.34f);setSlotParam(3,3,0.f);
        setReal("s4_wet",.7f);
        setSlotType(7,Tremolo);setSlotOn(7,true);
        setSlotParam(7,0,.44f);setSlotParam(7,1,.58f);
        setSlotParam(7,2,.35f);setSlotParam(7,3,.15f);
        setReal("s8_wet",.55f);
    } else if(index==28) {
        setSlotType(1,ThreeBandDynamics);setSlotOn(1,true);
        setSlotParam(1,0,.38f);setSlotParam(1,1,.52f);
        setSlotParam(1,2,.45f);setSlotParam(1,3,.45f);
        setSlotType(3,StereoWidth);setSlotOn(3,true);
        setSlotParam(3,0,.61f);setSlotParam(3,1,.35f);
        setSlotParam(3,2,.5f);setSlotParam(3,3,.5f);
        setReal("s4_wet",.62f);
    }
    if(index==29 || index==30) {
        setSlotType(1,VoxRackSmoothComp);setSlotOn(1,true);
        setSlotParam(1,0,index==29?.56f:.39f);
        setSlotParam(1,1,index==29?.30f:.56f);
        setSlotParam(1,2,index==29?.42f:.56f);
        setSlotParam(1,3,.32f);
        setSlotType(5,VoxRackRoom);setSlotOn(5,true);
        setSlotParam(5,0,index==29?.38f:.73f);
        setSlotParam(5,1,index==29?.53f:.37f);
        setSlotParam(5,2,index==29?.07f:.18f);
        setSlotParam(5,3,.16f);
        setReal("s6_wet",index==29?.20f:.45f);
    }
    if(index>=31 && index<=33) {
        const int slot=index==31?3:(index==32?7:3);
        setSlotType(slot,index==31?VocalChorusFX:(index==32?VocalFlangerFX:RingModFX));
        setSlotOn(slot,true);
        setSlotParam(slot,0,index==33?.43f:.22f);
        setSlotParam(slot,1,index==33?.81f:.57f);
        setSlotParam(slot,2,index==33?.20f:.52f);
        setSlotParam(slot,3,.62f);
        setReal(slotPrefix(slot)+"wet",index==33?.63f:.49f);
    }
    if(index==34) {
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
    } else if(index==35) {
        // Ducking stereo echo replaces the ordinary phrase throw in this bank.
        setSlotType(4,DuckingPingPongFX);setSlotOn(4,true);
        setSlotParam(4,0,.62f);setSlotParam(4,1,.52f);
        setSlotParam(4,2,.79f);setSlotParam(4,3,.55f);
        setReal("s5_wet",.44f);
    } else if(index==36) {
        setSlotType(8,VocalVibratoFX);setSlotOn(8,true);
        setSlotParam(8,0,.48f);setSlotParam(8,1,.56f);
        setSlotParam(8,2,.62f);setSlotParam(8,3,.34f);
        setReal("s9_wet",.53f);
        std::uint64_t order=0;
        const int ids[]{0,1,2,3,8,4,5,6,7,9,10,11,12,13,14,15};
        for(int position=0;position<slotCount;++position)
            order|=(std::uint64_t)ids[position] << (4*position);
        packedOrder.store(order,std::memory_order_release);
    }
    if(index>=37 && index<=40){
        // A new processor placed before the main compressor (or replaces it)
        // without changing the 16 physical slot IDs or old factory programs.
        const bool dualComp=index==38;
        const int physical=dualComp?1:8;
        const int chosen=index==37?EightBandToneFX:
            index==38?SerialDynamicsFX:
            index==39?DynamicHarshnessFX:AdaptiveNoiseFX;
        setSlotType(physical,chosen);
        setSlotOn(physical,true);
        setReal(slotPrefix(physical)+"wet",dualComp?.92f:1.f);
        if(index==37) {
            // Airy articulation with restrained low-mid cleanup.
            const float gains[8]{-.04f,-.06f,-.03f,-.01f,.04f,.055f,.08f,.05f};
            for(int k=0;k<8;++k) {
                setSlotParam(8,k*2,.5f);
                setSlotParam(8,k*2+1,.5f+gains[k]);
            }
        } else if(index==38){
            setSlotParam(1,0,.58f);setSlotParam(1,1,.47f);
            setSlotParam(1,2,.28f);setSlotParam(1,3,.12f);
        } else if(index==39){
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
    if(index==41) {
        // Intentionally omitted from live vocal defaults: 768-sample STFT PDC.
        setSlotType(8,StudioPitchFormantFX);setSlotOn(8,true);
        setSlotParam(8,0,.72f);setSlotParam(8,1,.42f);
        setSlotParam(8,2,.62f);setSlotParam(8,3,.58f);
        setReal("s9_wet",.48f);
        std::uint64_t order=0;
        const int ids[]{0,1,2,3,8,4,5,6,7,9,10,11,12,13,14,15};
        for(int position=0;position<slotCount;++position)
            order|=(std::uint64_t)ids[position] << (4*position);
        packedOrder.store(order,std::memory_order_release);
    }
    if(index==42){
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
    if(index==skellpreset::alchemist::factoryIndex) {
        // Append-only sound study. The original source chain's plugin types
        // are documented, but proprietary Waves/FL plugin state is NOT ported.
        // Do not insert a default final limiter: measured reference difference
        // was negligible for the supplied sample, and it would add PDC.
        for(int physical=0;physical<slotCount;++physical) {
            setSlotOn(physical,false);
            setSlotType(physical,Empty);
            setReal(slotPrefix(physical)+"wet",1.f);
        }
        for(int position=0;
            position<(int)skellpreset::alchemist::stages.size();++position) {
            const auto& stage=skellpreset::alchemist::stages[(size_t)position];
            setSlotType(position,stage.module);
            setSlotOn(position,true);
            setReal(slotPrefix(position)+"wet",stage.wet);
            for(int k=0;k<16;++k)setSlotParam(position,k,stage.p[(size_t)k]);
            for(int k=0;k<8;++k)
                setReal(slotPrefix(position)+"eqQ"+juce::String(k+1),stage.q[(size_t)k]);
        }
        // Explicit identity ordering prevents inherited legacy factory
        // reorders from silently changing Alchemist's serial stage order.
        std::uint64_t identityOrder=0;
        for(int position=0;position<slotCount;++position)
            identityOrder|=(std::uint64_t)position << (4*position);
        packedOrder.store(identityOrder,std::memory_order_release);
    }
    setReal("mode",1);setReal("mix",1);setReal("input",0);setReal("output",0);
    setReal("micPreOn",0);setReal("micPreInput",0);
    setReal("micPreTarget",0);setReal("micPreAmount",1);setReal("micPreTrim",.5f);
    setReal("macro1",.5f);setReal("macro2",.5f);setReal("macro3",.5f);setReal("macro4",.5f);
    state.state.setProperty("presetName",name,nullptr);
    state.state.setProperty("presetBank",preset.bank,nullptr);
    updateHostLatency();
}
juce::Result SkellPresetAudioProcessor::exportPreset(const juce::File& file) const {
    skellpreset::package::Document doc;
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
        for(int k=0;k<16;++k)dst.params[(size_t)k]=getSlotParam(dst.slotId,k);
        for(int band=0;band<8;++band)dst.eqQ[(size_t)band]=value(prefix+"eqQ"+juce::String(band+1));
        dst.eqLowCut=value(prefix+"eqLowCut");
        dst.eqHighCut=value(prefix+"eqHighCut");
    }
    return skellpreset::package::write(file,doc);
}
juce::Result SkellPresetAudioProcessor::importPreset(const juce::File& file) {
    skellpreset::package::Document doc;
    if(const auto result=skellpreset::package::read(file,doc);result.failed())return result;
    // The entire document is validated before touching live parameters.
    for(const auto& slot:doc.slots){
        const auto prefix=slotPrefix(slot.slotId);
        setSlotOn(slot.slotId,false);
        setSlotType(slot.slotId,slot.type);
        setReal(prefix+"wet",slot.wet);
        for(int k=0;k<16;++k)setSlotParam(slot.slotId,k,slot.params[(size_t)k]);
        for(int band=0;band<8;++band)setReal(prefix+"eqQ"+juce::String(band+1),slot.eqQ[(size_t)band]);
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
    for(int i=0;i<8;++i)setReal("macro"+juce::String(i+1),doc.macros[(size_t)i]);
    name=doc.name;
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
void SkellPresetAudioProcessor::setCurrentProgram(int index) {loadFactoryPreset(index);}
const juce::String SkellPresetAudioProcessor::getProgramName(int index) {
    return index>=0&&index<(int)factoryPresets.size()?factoryPresets[(size_t)index].name:juce::String{};
}
void SkellPresetAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=state.copyState();
    juce::String sequence;
    for(int i=0;i<slotCount;i++)sequence+=juce::String(slotIdAt(i))+(i==slotCount-1?"":",");
    snapshot.setProperty("slotOrder",sequence,nullptr);
    snapshot.setProperty("rackSlots",activeSlotCount(),nullptr);
    snapshot.setProperty("presetName",name,nullptr);
    snapshot.setProperty("program",selectedProgram,nullptr);
    for(int s=0;s<slotCount;++s) {
        const auto index=juce::String(s+1);
        snapshot.removeProperty("hostPath"+index,nullptr);
        snapshot.removeProperty("hostState"+index,nullptr);
        if(slotType(s)!=ExternalVST3)continue;
        const auto file=hosted.getFile(s);
        if(file.getFullPathName().isEmpty())continue;
        snapshot.setProperty("hostPath"+index,file.getFullPathName(),nullptr);
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
        if(items.size()==defaultSlotCount || items.size()==slotCount) {
            std::array<bool,slotCount> seen{};
            std::array<int,slotCount> proposed{};
            bool valid=true;
            const int oldCount=items.size();
            for(int i=0;i<oldCount;++i) {
                const int v=items[i].getIntValue();
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
        selectedProgram=juce::jlimit(-1,(int)factoryPresets.size()-1,(int)loaded.getProperty("program",0));
        state.replaceState(loaded);
        updateHostLatency();
        // Don't instantiate external code in setStateInformation(): hosts may
        // call this from threads that cannot safely load a plugin DLL.
        bool restore=false;
        for(int s=0;s<slotCount;++s)
            if(loaded.hasProperty("hostPath"+juce::String(s+1))) {restore=true;break;}
        if(restore)triggerAsyncUpdate();
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new SkellPresetAudioProcessor();}
