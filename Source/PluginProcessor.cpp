#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using namespace skellpreset;
using namespace skellpreset::dsp;
namespace {
const juce::StringArray types{"Empty","EQ","Compressor","Limiter","Gate","Saturator","Reverb","Delay","Exciter","Adlib Throw","Adlib Doubler"};
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
        r.on=state.getRawParameterValue(prefix+"on");
        r.wet=state.getRawParameterValue(prefix+"wet");
        for(int k=0;k<16;k++)r.p[(size_t)k]=state.getRawParameterValue(prefix+"p"+juce::String(k+1));
    }
    packedOrder.store(initialOrder);
    rawGlobal.mode=state.getRawParameterValue("mode");
    rawGlobal.input=state.getRawParameterValue("input");
    rawGlobal.mix=state.getRawParameterValue("mix");
    rawGlobal.output=state.getRawParameterValue("output");
    rawGlobal.gate=state.getRawParameterValue("globalGate");
    rawGlobal.threshold=state.getRawParameterValue("gateThreshold");
    rawGlobal.delta=state.getRawParameterValue("delta");
    for(int k=0;k<4;k++)rawGlobal.macros[(size_t)k]=state.getRawParameterValue("macro"+juce::String(k+1));
    // Factory initialization is separate from state restoration; no DSP is allocated here.
    loadFactoryPreset(0);
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
    for(int s=1;s<=slotCount;s++) {
        const auto id="s"+juce::String(s)+"_";
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id+"type",1},"Slot "+juce::String(s)+" type",types,0));
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
    return p;
}
void SkellPresetAudioProcessor::EngineSlot::prepare(double sampleRate) {
    peak.reset();highpass.reset();comp.prepare(sampleRate);gate.prepare(sampleRate);
    delay.prepare(sampleRate);reverb[0].prepare(sampleRate);
    doubler.prepare(sampleRate);limiter.prepare(sampleRate,2);
    limiterDry.prepare(limiter.latencySamples());
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
    const int latency=limiterLookahead.load(std::memory_order_acquire);
    int total=0;
    for(const auto& slot:rawSlots)
        if(slot.type && (int)slot.type->load(std::memory_order_relaxed)==Limiter)total+=latency;
    if(total!=currentReportedLatency){currentReportedLatency=total;setLatencySamples(total);}
}

void SkellPresetAudioProcessor::prepareToPlay(double sampleRate,int) {
    rate=sampleRate;for(auto& e:engines)e.prepare(rate);
    const auto lookahead=engines[0].limiter.latencySamples();
    limiterLookahead.store(lookahead,std::memory_order_release);
    dryPdc.prepare(slotCount*lookahead);bypassPdc.prepare(slotCount*lookahead);
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
void SkellPresetAudioProcessor::releaseResources() {}
bool SkellPresetAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto in=layout.getMainInputChannelSet(),out=layout.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono() && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo()))
        || (in==juce::AudioChannelSet::stereo() && out==juce::AudioChannelSet::stereo());
}
float SkellPresetAudioProcessor::value(const juce::String& id) const noexcept {
    if(auto* v=state.getRawParameterValue(id))return v->load();return 0;
}
void SkellPresetAudioProcessor::setReal(const juce::String& id,float v) {
    if(auto* p=state.getParameter(id))p->setValueNotifyingHost(p->convertTo0to1(v));
}
float SkellPresetAudioProcessor::getSlotParam(int slot,int param) const noexcept {
    return value(slotPrefix(slot)+"p"+juce::String(param+1));
}
void SkellPresetAudioProcessor::setSlotParam(int slot,int param,float v) {setReal(slotPrefix(slot)+"p"+juce::String(param+1),juce::jlimit(0.f,1.f,v));}
void SkellPresetAudioProcessor::setSlotOn(int slot,bool on) {setReal(slotPrefix(slot)+"on",on?1.f:0.f);}
void SkellPresetAudioProcessor::setSlotType(int slot,int type) {setReal(slotPrefix(slot)+"type",juce::jlimit(0,NumberOfModules-1,type));updateHostLatency();}
juce::String SkellPresetAudioProcessor::moduleName(int type) const {return types[juce::jlimit(0,types.size()-1,type)];}

void SkellPresetAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const auto ch=juce::jmin(2,buffer.getNumChannels()),size=buffer.getNumSamples();
    const bool monoSource=getTotalNumInputChannels()==1;
    if(ch<1 || size<=0)return;
    auto* left=buffer.getWritePointer(0);
    auto* right=ch==2?buffer.getWritePointer(1):nullptr;
    const int mode=juce::jlimit(0,3,(int)rawGlobal.mode->load());
    inputSmooth.setTargetValue(dbToGain(rawGlobal.input->load()));
    outputSmooth.setTargetValue(dbToGain(rawGlobal.output->load()));
    mixSmooth.setTargetValue(juce::jlimit(0.f,1.f,rawGlobal.mix->load()));
    bypassSmooth.setTargetValue(mode==3?0.f:1.f);
    const bool delta=rawGlobal.delta->load()>.5f;
    const float macroTone=rawGlobal.macros[0]->load(),macroPunch=rawGlobal.macros[1]->load(),
                macroSpace=rawGlobal.macros[2]->load(),macroGrit=rawGlobal.macros[3]->load();
    // Read atomics once each block. Fixed slot objects avoid realtime allocations.
    for(int s=0;s<slotCount;++s){
        auto& c=controlCache[(size_t)s];const auto& raw=rawSlots[(size_t)s];
        c.type=juce::jlimit(0,NumberOfModules-1,(int)raw.type->load());
        c.on=raw.on->load()>.5f;c.wet=juce::jlimit(0.f,1.f,raw.wet->load());
        for(int k=0;k<16;k++)c.p[(size_t)k]=raw.p[(size_t)k]->load();
        auto& e=engines[(size_t)s];
        if(c.type==Empty)continue;
        e.peak.setPeak(rate,40.*std::pow(300.,c.p[0]),(c.p[1]-.5)*24.+8.*(macroTone-.5f),.3+7.*c.p[2]);
        e.highpass.setHighPass(rate,20.+c.p[3]*250.);
        e.comp.set(-48.f+c.p[0]*42.f-12.f*(macroPunch-.5f),1.f+c.p[1]*11.f,2.f+c.p[2]*98.f,20.f+c.p[3]*480.f,(c.p[4]-.5f)*24.f);
        e.gate.threshold=-75.f+c.p[0]*70.f;e.gate.release=40.f+c.p[1]*600.f;
        e.delay.configure((int)(rate*(c.type==AdlibThrow ? (.16+.48*c.p[0]) : (.055+.85*c.p[0]))),c.type==AdlibThrow ? (.2f+.68f*c.p[1]) : (c.p[1]*.8f));
        e.reverb[0].setParameters(.15f+6.f*c.p[0],.12f+c.p[1]*.8f,1.f);
        if(c.type==Limiter)e.limiter.setCeilingDb(gainToDb(.12f+c.p[0]*.87f));
        if(c.type==AdlibThrow)e.throwFilterPole=std::exp(-2.f*(float)skellpreset::dsp::pi*(750.f+c.p[2]*11500.f)/(float)rate);
    }
    for(int s=0;s<slotCount;++s){
        const auto& c=controlCache[(size_t)s];
        const float fx=(c.type==Reverb||c.type==Delay)?(.5f+macroSpace):1.f;
        slotWetSmooth[(size_t)s].setTargetValue(juce::jlimit(0.f,1.f,c.wet*fx)*(c.on?1.f:0.f));
    }
    std::array<int,slotCount> sequence{};
    const auto packed=packedOrder.load(std::memory_order_acquire);
    for(int i=0;i<slotCount;i++)sequence[(size_t)i]=(int)((packed>>(4*i))&0x0fU);
    std::array<float,slotCount> meter{};
    const auto lookahead=limiterLookahead.load(std::memory_order_acquire);
    int blockLatency=0;
    for(int s=0;s<slotCount;++s)if(controlCache[(size_t)s].type==Limiter)blockLatency+=lookahead;
    float inPeak=0,outPeak=0;
    for(int n=0;n<size;n++) {
        float l=std::isfinite(left[n])?left[n]:0.f;
        float r=ch==2&&!monoSource?(std::isfinite(right[n])?right[n]:0.f):l;
        const float originalL=l,originalR=r;
        const float inputGain=inputSmooth.getNextValue(),outputGain=outputSmooth.getNextValue();
        const float mix=mixSmooth.getNextValue(),activeBlend=bypassSmooth.getNextValue();
        inPeak=std::max(inPeak,std::max(std::abs(l),std::abs(r)));
        l*=inputGain;r*=inputGain;
        const auto delayedOriginal=bypassPdc.process(originalL,originalR,blockLatency);
        const auto delayedDry=dryPdc.process(l,r,blockLatency);
        const float dryL=delayedDry[0],dryR=delayedDry[1];
        {
            for(int ix=0;ix<slotCount;ix++) {
                const int s=sequence[(size_t)ix];if(s<0||s>=slotCount)continue;
                const auto& c=controlCache[(size_t)s];
                if(c.type==Empty)continue;
                const float slotWet=slotWetSmooth[(size_t)s].getNextValue();
                if(c.type!=Limiter && slotWet<.000001f && slotWetSmooth[(size_t)s].getTargetValue()==0.f)continue;
                auto& e=engines[(size_t)s];float wL=l,wR=r;
                switch(c.type) {
                    case EQ:
                        wL=e.peak.process(e.highpass.process(l,0),0);
                        wR=e.peak.process(e.highpass.process(r,1),1);break;
                    case Compressor: {
                        auto gain=e.comp.gain(std::max(std::abs(l),std::abs(r)));
                        wL=l*gain;wR=r*gain;break;}
                    case Limiter: {
                        const float drive=dbToGain(c.p[1]*18.f);
                        const float in[2]{l*drive,r*drive};float out[2]{};
                        e.limiter.processFrame(in,out,2);
                        wL=out[0];wR=out[1];break;}
                    case Gate: {
                        float gain=e.gate.processGain(std::max(std::abs(l),std::abs(r)));
                        wL=l*gain;wR=r*gain;break;}
                    case Saturator: {
                        float drive=dbToGain(c.p[0]*24.f+10.f*(macroGrit-.5f)),blend=(.5f+c.p[1]);
                        wL=std::tanh(l*drive*blend)/std::max(.5f,drive*.6f);
                        wR=std::tanh(r*drive*blend)/std::max(.5f,drive*.6f);break;}
                    case Exciter: {
                        const float drive=1.f+10.f*c.p[0];
                        wL=(std::tanh(l*drive)-std::tanh(l*drive*.55f))*0.4f+l;
                        wR=(std::tanh(r*drive)-std::tanh(r*drive*.55f))*0.4f+r;break;}
                    case Delay: {auto d=e.delay.process(l,r);wL=d[0];wR=d[1];break;}
                    case Reverb: {auto d=e.reverb[0].processFrame(l,r);wL=d[0];wR=d[1];break;}
                    case AdlibThrow: {e.processThrow(l,r,wL,wR,c.p[0],c.p[1],c.p[2],c.p[3],rate);break;}
                    case AdlibDoubler: {auto d=e.doubler.process(l,r,c.p[0],c.p[1],c.p[2],c.p[3]);wL=d[0];wR=d[1];break;}
                    default: break;
                }
                // Lookahead limiter wet and bypass both carry its declared sample delay.
                if(c.type==Limiter){auto aligned=e.limiterDry.process(l,r,lookahead);l=aligned[0];r=aligned[1];}
                l=l+(wL-l)*slotWet;r=r+(wR-r)*slotWet;
                meter[(size_t)s]=std::max(meter[(size_t)s],std::max(std::abs(l),std::abs(r)));
            }
        }
        const float resultL=dryL+(l-dryL)*mix;
        const float resultR=dryR+(r-dryR)*mix;
        const float processedL=(delta?(resultL-dryL):resultL)*outputGain;
        const float processedR=(delta?(resultR-dryR):resultR)*outputGain;
        left[n]=delayedOriginal[0]+(processedL-delayedOriginal[0])*activeBlend;
        if(right)right[n]=delayedOriginal[1]+(processedR-delayedOriginal[1])*activeBlend;
        outPeak=std::max(outPeak,std::max(std::abs(left[n]),right?std::abs(right[n]):0.f));
    }
    inputPeak.store(inPeak);outputPeak.store(outPeak);
    for(int s=0;s<slotCount;s++)slotPeaks[(size_t)s].store(meter[(size_t)s]);
}

void SkellPresetAudioProcessor::loadFactoryPreset(int index) {
    if(index<0||index>=(int)factoryPresets.size())return;
    selectedProgram=index;const auto& preset=factoryPresets[(size_t)index];name=preset.name;
    for(int s=0;s<slotCount;s++){
        setSlotType(s,Empty);setSlotOn(s,false);setReal(slotPrefix(s)+"wet",1);
        for(int k=0;k<16;k++)setSlotParam(s,k,.5f);
        
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
    setReal("mode",1);setReal("mix",1);setReal("input",0);setReal("output",0);
    setReal("macro1",.5f);setReal("macro2",.5f);setReal("macro3",.5f);setReal("macro4",.5f);
    state.state.setProperty("presetName",name,nullptr);
    state.state.setProperty("presetBank",preset.bank,nullptr);
    updateHostLatency();
}
juce::Result SkellPresetAudioProcessor::exportPreset(const juce::File& file) const {
    skellpreset::package::Document doc;
    doc.name=name;
    doc.bank=state.state.getProperty("presetBank","My Presets").toString();
    doc.mode=juce::jlimit(0,3,(int)value("mode"));
    doc.inputDb=value("input");doc.mix=value("mix");doc.outputDb=value("output");
    for(int i=0;i<8;++i)doc.macros[(size_t)i]=value("macro"+juce::String(i+1));
    for(int position=0;position<slotCount;++position){
        auto& dst=doc.slots[(size_t)position];
        dst.slotId=slotIdAt(position);
        const auto prefix=slotPrefix(dst.slotId);
        dst.type=juce::jlimit(0,NumberOfModules-1,(int)value(prefix+"type"));
        dst.enabled=value(prefix+"on")>.5f;
        dst.wet=value(prefix+"wet");
        for(int k=0;k<16;++k)dst.params[(size_t)k]=getSlotParam(dst.slotId,k);
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
    }
    std::uint64_t next=0;
    for(int position=0;position<slotCount;++position){
        const auto& slot=doc.slots[(size_t)position];
        next|=(std::uint64_t)slot.slotId << (4*position);
        setSlotOn(slot.slotId,slot.enabled);
    }
    packedOrder.store(next,std::memory_order_release);
    setReal("input",doc.inputDb);setReal("mix",doc.mix);setReal("output",doc.outputDb);
    setReal("mode",(float)doc.mode);
    for(int i=0;i<8;++i)setReal("macro"+juce::String(i+1),doc.macros[(size_t)i]);
    name=doc.name;
    selectedProgram=-1; // Imported sound is not a factory preset.
    state.state.setProperty("presetName",name,nullptr);
    state.state.setProperty("presetBank",doc.bank,nullptr);
    updateHostLatency();
    return juce::Result::ok();
}

void SkellPresetAudioProcessor::reorderSlot(int from,int to) {
    if(from<0||from>=slotCount||to<0||to>=slotCount||from==to)return;
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
    snapshot.setProperty("presetName",name,nullptr);
    snapshot.setProperty("program",selectedProgram,nullptr);
    if(auto xml=snapshot.createXml())copyXmlToBinary(*xml,dest);
}
void SkellPresetAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) {
        auto loaded=juce::ValueTree::fromXml(*xml);
        if(!loaded.isValid()||loaded.getType()!=state.state.getType())return;
        const auto sequence=loaded.getProperty("slotOrder").toString();
        juce::StringArray items;items.addTokens(sequence,",","");
        if(items.size()==slotCount){
            std::array<bool,slotCount> seen{};std::array<int,slotCount> proposed{};bool valid=true;
            for(int i=0;i<slotCount;i++){int v=items[i].getIntValue();if(v<0||v>=slotCount||seen[(size_t)v]){valid=false;break;}seen[(size_t)v]=true;proposed[(size_t)i]=v;}
            if(valid){std::uint64_t next=0;for(int i=0;i<slotCount;i++)next|=(std::uint64_t)proposed[(size_t)i]<<(4*i);packedOrder.store(next,std::memory_order_release);}
        }
        name=loaded.getProperty("presetName","Custom").toString();
        selectedProgram=juce::jlimit(-1,(int)factoryPresets.size()-1,(int)loaded.getProperty("program",0));
        state.replaceState(loaded);
        updateHostLatency();
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new SkellPresetAudioProcessor();}
