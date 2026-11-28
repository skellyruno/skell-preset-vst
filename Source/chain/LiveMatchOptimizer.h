#pragma once
#include "LiveVocalMatch.h"
#include "StudioNativeDSP.h"
#include "ModulationFX.h"
#include "VoxRackFX.h"
#include "ImportedVocalFX.h"
#include "AdvancedVocalDSP.h"
#include "VisualAnalysis.h"

namespace skellpreset::liveMatch {
// Shared normalized control mapping: the scorer and rack writer use the
// same frequency/gain/Q, compressor and de-esser settings.
struct EqControls {
    std::array<float,16> p{};
    std::array<float,8> q{};
    std::array<int,8> kind{{1,3,4,0,0,0,0,0}};
    std::array<bool,8> on{{true,true,true,false,false,false,false,false}};
    explicit EqControls(const Recipe& r) noexcept {
        p.fill(.5f);q.fill(.5f);
        const auto hz=[](float f){return std::log(f/20.f)/std::log(1000.f);};
        p[0]=hz(75.f);q[0]=.28f;p[2]=hz(250.f);p[3]=.5f+r.low/36.f;p[4]=hz(4500.f);p[5]=.5f+r.high/36.f;
    }
};
inline std::array<float,5> compControls(const Recipe& r) noexcept {
    return {(r.threshold+48.f)/42.f,(r.ratio-1.f)/11.f,(std::clamp(r.attackMs,2.f,100.f)-2.f)/98.f,(std::clamp(r.releaseMs,20.f,500.f)-20.f)/480.f,.5f+r.makeup/24.f};
}
class NativeCandidate {
public:
    NativeCandidate(double rate,const Recipe& r,size_t startFrame=0):recipe(r) {
        const EqControls c(r);eq.prepare(rate);eq.set(c.p,c.q,c.kind,c.on);
        gate.prepare(rate);gate.threshold=-65.f;gate.release=160.f;gate.holdMs=55.f;gate.attackMs=4.f;gate.hysteresisDb=3.f;
        const auto cp=compControls(r);comp.set(-48.f+cp[0]*42.f,1.f+cp[1]*11.f,2.f+cp[2]*98.f,20.f+cp[3]*480.f,(cp[4]-.5f)*24.f);comp.prepare(rate);
        deess.prepare(rate);const auto& ds=r.deessControls;deess.set(3500.f+ds[0]*8000.f,-53.f+ds[1]*39.f,2.f+ds[2]*20.f,35.f+ds[3]*245.f);
        if(r.mod.wet>0.f){const auto& p=r.mod.p;if(r.modulation==1){chorus.prepare(rate);chorus.set(.08f+3.9f*p[0],1.f+10.f*p[1],2.f+3.f*p[2],p[3]);chorus.reset();chorus.phase=std::fmod(startFrame*(double)chorus.rate/rate,1.);}else if(r.modulation==2){flanger.prepare(rate);flanger.set(.05f+4.95f*p[0],.3f+9.f*p[1],(p[2]*2.f-1.f)*.82f,p[3]);flanger.reset();flanger.phase=std::fmod(startFrame*(double)flanger.rate/rate,1.);}}
        if(r.delay.wet>0.f){const auto& p=r.delay.p;delay.prepare(rate);delay.configure((int)(rate*(.055+.85*p[0])),p[1]*.8f,500.f+17500.f*p[2]);delay.reset();}
        if(r.room.wet>0.f){const auto& p=r.room.p;room.prepare(rate);room.set(p[0],p[1],200.f*p[2],20.f+980.f*p[3]);room.reset();}
        if(r.rider.wet>0.f){const auto& controls=r.rider.p;rider.prepare(rate);rider.set(controls[0],-26.f+14.f*controls[1],3.f+12.f*controls[2],controls[3]);}
        if(r.harshness.wet>0.f){const auto& controls=r.harshness.p;harshness.prepare(rate);harshness.set(controls[0],controls[1],controls[2],controls[3]);}
        if(r.saturation.wet>0.f){satDrive=dsp::dbToGain(r.saturation.p[0]*24.f);satBlend=.5f+r.saturation.p[1];satDenom=std::max(.5f,satDrive*.6f);}
        out=dsp::dbToGain(r.output);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float g=1.f+.55f*(gate.processGain(std::max(std::abs(l),std::abs(r)))-1.f);
        auto v=eq.process(l*g,r*g);
        const float c=comp.gain(std::max(std::abs(v[0]),std::abs(v[1])));v[0]*=c;v[1]*=c;
        if(recipe.deess)v=deess.process(v[0],v[1]);
        auto mix=[&](const std::array<float,2>& w,float wet){v[0]+=(w[0]-v[0])*wet;v[1]+=(w[1]-v[1])*wet;};
        if(recipe.rider.wet>0.f)mix(rider.process(v[0],v[1]),recipe.rider.wet);
        if(recipe.saturation.wet>0.f)mix({visual::saturatorCore(v[0],satDrive,satBlend,satDenom),visual::saturatorCore(v[1],satDrive,satBlend,satDenom)},recipe.saturation.wet);
        if(recipe.harshness.wet>0.f)mix(harshness.process(v[0],v[1]),recipe.harshness.wet);
        if(recipe.mod.wet>0.f){if(recipe.modulation==1)mix(chorus.process(v[0],v[1]),recipe.mod.wet);else if(recipe.modulation==2)mix(flanger.process(v[0],v[1]),recipe.mod.wet);}
        if(recipe.delay.wet>0.f)mix(delay.process(v[0],v[1]),recipe.delay.wet);
        if(recipe.room.wet>0.f)mix(room.process(v[0],v[1]),recipe.room.wet);
        return {v[0]*out,v[1]*out};
    }
private:
    dsp::VocalChorus chorus;dsp::VocalFlanger flanger;dsp::Delay delay;dsp::VoxRackRoom room;
    dsp::VocalRider rider;dsp::DynamicHarshness harshness;float satDrive=1.f,satBlend=1.f,satDenom=1.f;
    Recipe recipe;studio::ParametricEQ eq;dsp::Gate gate;dsp::Compressor comp;dsp::SplitDeEsser deess;float out=1.f;
};
struct Segment {size_t begin=0,end=0;};
inline Segment selectSegment(const Audio& a,size_t lo,size_t hi) noexcept {
    // Scan 20 ms RMS windows once; choose a short audible phrase first,
    // then extend it for sparse performances, never cross the split.
    const size_t window=std::max<size_t>(1,(size_t)(a.rate*.02));
    constexpr size_t maxWindows=1001;
    std::array<unsigned,maxWindows+1> prefix{};
    const size_t count=std::min(maxWindows,(hi-lo)/window);
    for(size_t i=0;i<count;++i){double energy=0.;for(size_t j=lo+i*window;j<lo+(i+1)*window;++j){const float l=a.stereo[j*2],r=a.stereo[j*2+1];energy+=.5*((double)l*l+(double)r*r);}prefix[i+1]=prefix[i]+(energy/window>=1.e-5);}
    Segment best{lo,hi};unsigned bestActive=0;
    for(size_t desired:{75u,125u}){
        const size_t n=std::min(count,desired);if(!n)continue;
        for(size_t i=0;i+n<=count;++i){const auto active=prefix[i+n]-prefix[i];if(active>bestActive){bestActive=active;best={lo+i*window,lo+(i+n)*window};}}
        if(bestActive>=38u)break;
    }
    return best;
}
inline Segment selectReferenceSegment(const Audio& a,size_t lo,size_t hi) noexcept {
    const auto phrase=selectSegment(a,lo,hi);const size_t length=std::min(hi-lo,(size_t)(a.rate*3.));
    const size_t center=phrase.begin+(phrase.end-phrase.begin)/2;const size_t begin=std::clamp(center>length/2?center-length/2:0,lo,hi-length);return {begin,begin+length};
}
inline Profile measure(const Audio& a,Segment s,const Recipe* r,const std::atomic<bool>* cancel=nullptr,bool detailed=false) {
    Analyzer analyzer;analyzer.prepare(a.rate,detailed);
    // Warm up the exact native kernels before measuring the chosen phrase.
    Recipe neutral;
    const size_t warm=(size_t)(a.rate*(r&&(r->mod.wet>0||r->delay.wet>0||r->room.wet>0||r->rider.wet>0||r->harshness.wet>0)?1.5:.2)),start=s.begin>warm?s.begin-warm:0;
    NativeCandidate chain(a.rate,r?*r:neutral,start);
    for(size_t i=start;i<s.end;++i){
        if((i&2047u)==0&&cancel&&cancel->load(std::memory_order_relaxed))return {};
        float l=a.stereo[i*2],rr=a.stereo[i*2+1];
        if(!std::isfinite(l)||!std::isfinite(rr))return {};
        if(r){const auto v=chain.process(l,rr);l=v[0];rr=v[1];}
        if(i>=s.begin)analyzer.push(&l,&rr,1);
    }
    return analyzer.result();
}
// Reject corrupt PCM even if it is outside the selected analysis phrases.
inline bool validMatchAudio(const Audio& a,const std::atomic<bool>* cancel) noexcept {
    if(!std::isfinite(a.rate)||a.rate<8000.||a.rate>192000.||a.stereo.size()%2||a.frames()<a.rate*5.9||a.frames()>a.rate*20.+1)return false;
    for(size_t i=0;i<a.stereo.size();++i){if((i&4095u)==0&&cancel&&cancel->load(std::memory_order_relaxed))return false;if(!std::isfinite(a.stereo[i]))return false;}
    return !(cancel&&cancel->load(std::memory_order_relaxed));
}
inline bool usableSegment(const Profile& p) noexcept {return p.finite()&&p.invalid==0.f&&p.active>=.65f&&p.rmsDb>=-50.f&&p.peak>0.f&&p.clipped<.005f;}
struct Target {float low,high,crest,level;};
inline Target targetFor(const Profile& dry,const Profile* ref) noexcept {
    return {ref?std::clamp(ref->lowDb,dry.lowDb-5.f,dry.lowDb+5.f):std::clamp(-8.f,dry.lowDb-4.f,dry.lowDb+3.f),
            ref?std::clamp(ref->highDb,dry.highDb-5.f,dry.highDb+5.f):std::clamp(-15.f,dry.highDb-3.f,dry.highDb+4.f),
            ref?std::clamp(ref->crest,dry.crest-5.f,dry.crest+2.f):std::min(dry.crest,12.f),
            std::clamp(ref?ref->rmsDb:-20.f,std::min(dry.rmsDb-5.f,-9.f),std::min(-9.f,dry.rmsDb+4.f))};
}
inline float loss(const Profile& p,const Target& t) noexcept {
    if(!usableSegment(p)||p.peak>=.999f)return 1.e6f;
    // Weighted feature deviation in dB, not a perceptual similarity percent.
    return .32f*std::abs(p.lowDb-t.low)+.32f*std::abs(p.highDb-t.high)+.20f*std::abs(p.crest-t.crest)+.16f*std::abs(p.rmsDb-t.level)+4.f*std::max(0.f,p.peak-.89f);
}
// Independent acceptance gates: a low aggregate score cannot hide wrong level,
// broad spectral balance or stereo phase/width. Mono floors avoid unstable ratios.
inline float bandError(const Profile& p,const Profile& ref) noexcept {
    if(!p.detailed||!ref.detailed)return 0.f;
    float sum=0;for(size_t i=0;i<p.bands.size();++i)sum+=std::abs(p.bands[i]-ref.bands[i]);return sum/6.f;
}
inline float temporalError(const Profile& p,const Profile& ref) noexcept {
    if(!p.detailed||!ref.detailed)return 0.f;
    float lag=0;for(size_t k=0;k<p.envelopeLag.size();++k)lag+=std::abs(p.envelopeLag[k]-ref.envelopeLag[k]);
    return .10f*std::abs(p.envelopeContrast-ref.envelopeContrast)+.06f*std::abs(p.quietDb-ref.quietDb)+.60f*lag;
}
inline float referenceLoss(const Profile& p,const Target& t,const Profile* ref) noexcept {
    const float base=loss(p,t);if(!ref||base>=1.e5f)return base;
    return base+.35f*bandError(p,*ref)+.12f*std::abs(std::max(-60.f,p.sideDb)-std::max(-60.f,ref->sideDb))+2.f*std::abs(p.correlation-ref->correlation)
        +temporalError(p,*ref);
}
inline bool referencePass(const Profile& p,const Profile& ref) noexcept {
    return usableSegment(p)&&p.peak<.999f&&usableSegment(ref)&&std::abs(p.rmsDb-ref.rmsDb)<=2.f
        &&std::abs(p.lowDb-ref.lowDb)<=4.5f&&std::abs(p.highDb-ref.highDb)<=4.5f
        &&std::abs(p.crest-ref.crest)<=5.f
        &&std::abs(std::max(-60.f,p.sideDb)-std::max(-60.f,ref.sideDb))<=6.f
        &&std::abs(p.correlation-ref.correlation)<=.35f&&bandError(p,ref)<=4.5f
        &&(!p.detailed||!ref.detailed||(std::abs(p.envelopeContrast-ref.envelopeContrast)<=10.f&&temporalError(p,ref)<=3.5f));
}
struct Optimization {
    Recipe recipe{};
    float initial=1.e6f,final=1.e6f,holdoutInitial=1.e6f,holdoutFinal=1.e6f,dryScore=1.e6f,dryHoldout=1.e6f;
    int tested=0,accepted=0;bool valid=false,referenceRejected=false;
};
inline constexpr int plainBudget=25,referenceBudget=132;
inline Optimization optimize(const Audio& a,const Profile& dry,const Profile* ref=nullptr,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr,const Audio* referenceAudio=nullptr) {
    Optimization o;
    if(!dry.valid()||(ref&&!ref->valid())||!validMatchAudio(a,cancel))return o;
    const auto train=ref?selectReferenceSegment(a,0,a.frames()/2):selectSegment(a,0,a.frames()/2),hold=ref?selectReferenceSegment(a,a.frames()/2,a.frames()):selectSegment(a,a.frames()/2,a.frames());
    const auto d1=measure(a,train,nullptr,cancel,ref!=nullptr),d2=measure(a,hold,nullptr,cancel,ref!=nullptr);
    if(!usableSegment(d1)||!usableSegment(d2))return o;
    Profile refTrain,refHold;const Profile* rt=ref;const Profile* rh=ref;
    if(ref&&referenceAudio){const auto& b=*referenceAudio;if(!validMatchAudio(b,cancel))return o;
        refTrain=measure(b,selectReferenceSegment(b,0,b.frames()/2),nullptr,cancel,true);refHold=measure(b,selectReferenceSegment(b,b.frames()/2,b.frames()),nullptr,cancel,true);
        if(!usableSegment(refTrain)||!usableSegment(refHold))return o;
        rt=&refTrain;rh=&refHold;
    }else if(ref&&!comparable(dry,*ref))return o;
    auto t1=targetFor(d1,rt),t2=targetFor(d2,rh);if(rt){t1.level=rt->rmsDb;t2.level=rh->rmsDb;}
    o.dryScore=referenceLoss(d1,t1,rt);o.dryHoldout=referenceLoss(d2,t2,rh);
    o.recipe=fit(dry,ref);
    auto evaluate=[&](Recipe& r){++o.tested;if(progress)progress->store(o.tested,std::memory_order_relaxed);
        if(rt){r.output=0;const auto unscaled=measure(a,train,&r,cancel,true);if(!unscaled.finite()||unscaled.invalid>0||unscaled.active<.65f||unscaled.peak<=0)return 1.e6f;
            // Independent train-only output gain, capped by sample headroom. Holdout never adjusts gain.
            const float ceiling=20.f*std::log10(.94f/std::max(1.e-8f,unscaled.peak));r.output=std::clamp(std::min(rt->rmsDb-unscaled.rmsDb,ceiling),-12.f,12.f);}
        return referenceLoss(measure(a,train,&r,cancel,rt!=nullptr),t1,rt);};
    auto validate=[&](const Recipe& r){return measure(a,hold,&r,cancel,rh!=nullptr);};
    o.initial=o.final=evaluate(o.recipe);
    const auto starter=o.recipe;const auto starterHold=validate(starter);o.holdoutInitial=o.holdoutFinal=referenceLoss(starterHold,t2,rh);
    if(rt){
        // Feature-conditioned native seeds: a near-neutral seed protects
        // references that mostly differ in level; dry tone is a second seed.
        Recipe neutral;neutral.ratio=1.f;neutral.threshold=-9.f;neutral.output=0.f;
        for(auto seed:{neutral,fit(dry)}){const float score=evaluate(seed);if(score+.015f<o.final){o.recipe=seed;o.final=score;++o.accepted;}}
    }
    // Only train selects parameters. Holdout is evaluated once, after search.
    // Two coordinate passes; bounded finite candidate count.
    for(float scale:{1.f,.45f}){
        for(int dim=0;dim<(rt?5:6);++dim){
            for(float sign:{-1.f,1.f}){
                if(cancel&&cancel->load(std::memory_order_relaxed))return {};
                auto r=o.recipe;
                switch(dim){
                    case 0:r.low=std::clamp(r.low+sign*2.f*scale,-6.f,5.f);break;
                    case 1:r.high=std::clamp(r.high+sign*2.f*scale,-5.f,6.f);break;
                    case 2:r.threshold=std::clamp(r.threshold+sign*6.f*scale,-45.f,-9.f);break;
                    case 3:r.ratio=std::clamp(r.ratio+sign*1.f*scale,1.f,5.f);break;
                    case 4:r.makeup=std::clamp(r.makeup+sign*2.f*scale,0.f,5.f);break;
                    case 5:r.output=std::clamp(r.output+sign*3.f*scale,-9.f,3.f);break;
                }
                const float score=evaluate(r);
                if(score+.015f<o.final){o.recipe=r;o.final=score;++o.accepted;}
            }
        }
    }
    if(rt){
        // Train-only structure search: compare single effects and serial combinations
        // using the same existing rack kernels and normalized controls.
        auto complexity=[](const Recipe& r){return .12f*((r.mod.wet>0)+(r.delay.wet>0)+(r.room.wet>0));};
        auto offer=[&](Recipe r){if(o.tested>=referenceBudget|| (cancel&&cancel->load(std::memory_order_relaxed)))return;const float score=evaluate(r);if(score+complexity(r)+.015f<o.final+complexity(o.recipe)){o.recipe=r;o.final=score;++o.accepted;}};
        const auto tone=o.recipe;const auto dryTone=fit(dry);
        for(int type:{1,2})for(float wet:{.30f,.65f,.90f})for(float rate:{.12f,.42f}){auto r=tone;r.modulation=type;r.mod={wet,{{rate,.65f,.5f,1.f}}};offer(r);auto alternative=dryTone;alternative.modulation=type;alternative.mod=r.mod;offer(alternative);}
        const auto modulationWinner=o.recipe;
        for(float time:{.04f,.22f,.50f})for(float wet:{.18f,.42f,.65f}){auto r=tone;r.delay={wet,{{time,.35f,.45f,.5f}}};offer(r);if(modulationWinner.mod.wet>0){r.modulation=modulationWinner.modulation;r.mod=modulationWinner.mod;offer(r);}}
        const auto delayWinner=o.recipe;
        for(float size:{.25f,.60f,.88f})for(float wet:{.20f,.48f,.75f}){auto r=tone;r.room={wet,{{size,.45f,.10f,.16f}}};offer(r);if(delayWinner.mod.wet>0||delayWinner.delay.wet>0){r.modulation=delayWinner.modulation;r.mod=delayWinner.mod;r.delay=delayWinner.delay;offer(r);}}
        for(int fx=0;fx<3;++fx){auto r=o.recipe;(fx==0?r.mod:fx==1?r.delay:r.room).wet=0;offer(r);}
        for(float scale:{1.f,.45f})for(int fx=0;fx<3;++fx)for(int dim=0;dim<3;++dim)for(float sign:{-1.f,1.f}){auto r=o.recipe;auto& e=fx==0?r.mod:fx==1?r.delay:r.room;if(e.wet==0)continue;if(dim==0)e.wet=std::clamp(e.wet+sign*.18f*scale,0.f,.98f);else e.p[dim-1]=std::clamp(e.p[dim-1]+sign*.18f*scale,0.f,1.f);offer(r);}
    }
    if(cancel&&cancel->load(std::memory_order_relaxed))return {};
    auto finalHold=validate(o.recipe);o.holdoutFinal=referenceLoss(finalHold,t2,rh);
    // Reject a trained winner that regresses on unseen material or cannot
    // improve on dry input. Keep the starter only if it independently wins.
    if(o.holdoutFinal>o.holdoutInitial+.025f||o.holdoutFinal+.025f>=o.dryHoldout||
       (rh&&!referencePass(finalHold,*rh))){
        o.recipe=starter;o.final=o.initial;o.holdoutFinal=o.holdoutInitial;o.accepted=0;finalHold=starterHold;
    }
    if(cancel&&cancel->load(std::memory_order_relaxed))return {};
    o.referenceRejected=rh&&!referencePass(finalHold,*rh);
    o.valid=!o.referenceRejected&&o.final<1.e5f&&o.holdoutFinal<1.e5f&&o.final+.025f<o.dryScore&&o.holdoutFinal+.025f<o.dryHoldout;
    return o;
}
}
