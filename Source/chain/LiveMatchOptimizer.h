#pragma once
#include "LiveVocalMatch.h"
#include "StudioNativeDSP.h"

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
    return {(r.threshold+48.f)/42.f,(r.ratio-1.f)/11.f,10.f/98.f,100.f/480.f,.5f+r.makeup/24.f};
}
class NativeCandidate {
public:
    NativeCandidate(double rate,const Recipe& r):recipe(r) {
        const EqControls c(r);eq.prepare(rate);eq.set(c.p,c.q,c.kind,c.on);
        gate.prepare(rate);gate.threshold=-65.f;gate.release=160.f;gate.holdMs=55.f;gate.attackMs=4.f;gate.hysteresisDb=3.f;
        const auto p=compControls(r);comp.set(-48.f+p[0]*42.f,1.f+p[1]*11.f,2.f+p[2]*98.f,20.f+p[3]*480.f,(p[4]-.5f)*24.f);comp.prepare(rate);
        deess.prepare(rate);deess.set(3500.f+.35f*8000.f,-53.f+.65f*39.f,2.f+.15f*20.f,35.f+.3f*245.f);
        out=dsp::dbToGain(r.output);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float g=1.f+.55f*(gate.processGain(std::max(std::abs(l),std::abs(r)))-1.f);
        auto v=eq.process(l*g,r*g);
        const float c=comp.gain(std::max(std::abs(v[0]),std::abs(v[1])));v[0]*=c;v[1]*=c;
        if(recipe.deess)v=deess.process(v[0],v[1]);
        return {v[0]*out,v[1]*out};
    }
private:
    Recipe recipe;studio::ParametricEQ eq;dsp::Gate gate;dsp::Compressor comp;dsp::SplitDeEsser deess;float out=1.f;
};
struct Segment {size_t begin=0,end=0;};
inline Segment selectSegment(const Audio& a,size_t lo,size_t hi) noexcept {
    // Fixed disjoint source halves. Select the window with most audible
    // 20 ms frames, not the loudest peak. No resampling or spectral shortcut.
    const size_t length=std::min(hi-lo,(size_t)(a.rate*1.5)),step=std::max<size_t>(1,(size_t)(a.rate*.25)),window=std::max<size_t>(1,(size_t)(a.rate*.02));
    Segment best{lo,lo+length};size_t bestActive=0;
    for(size_t start=lo;start+length<=hi;start+=step){
        size_t active=0;
        for(size_t i=start;i+window<=start+length;i+=window){
            double energy=0.;for(size_t j=i;j<i+window;++j){const float l=a.stereo[j*2],r=a.stereo[j*2+1];energy+=.5*((double)l*l+(double)r*r);}
            if(energy/window>=1.e-5)++active;
        }
        if(active>bestActive){bestActive=active;best={start,start+length};}
    }
    return best;
}
inline Profile measure(const Audio& a,Segment s,const Recipe* r,const std::atomic<bool>* cancel=nullptr) noexcept {
    Analyzer analyzer;analyzer.prepare(a.rate);
    // Warm up the exact native kernels before measuring the chosen phrase.
    Recipe neutral;NativeCandidate chain(a.rate,r?*r:neutral);
    const size_t warm=(size_t)(a.rate*.2),start=s.begin>warm?s.begin-warm:0;
    for(size_t i=start;i<s.end;++i){
        if((i&2047u)==0&&cancel&&cancel->load(std::memory_order_relaxed))return {};
        float l=a.stereo[i*2],rr=a.stereo[i*2+1];
        if(!std::isfinite(l)||!std::isfinite(rr))return {};
        if(r){const auto v=chain.process(l,rr);l=v[0];rr=v[1];}
        if(i>=s.begin)analyzer.push(&l,&rr,1);
    }
    return analyzer.result();
}
inline bool usableSegment(const Profile& p) noexcept {return p.active>=.65f&&p.rmsDb>=-50.f&&p.peak>0.f&&p.clipped<.005f;}
struct Target {float low,high,crest,level;};
inline Target targetFor(const Profile& dry,const Profile* ref) noexcept {
    return {ref?std::clamp(ref->lowDb,dry.lowDb-5.f,dry.lowDb+5.f):std::clamp(-8.f,dry.lowDb-4.f,dry.lowDb+3.f),
            ref?std::clamp(ref->highDb,dry.highDb-5.f,dry.highDb+5.f):std::clamp(-15.f,dry.highDb-3.f,dry.highDb+4.f),
            ref?std::clamp(ref->crest,dry.crest-5.f,dry.crest+2.f):std::min(dry.crest,12.f),
            std::clamp(ref?ref->rmsDb:-20.f,dry.rmsDb-5.f,std::min(-9.f,dry.rmsDb+4.f))};
}
inline float loss(const Profile& p,const Target& t) noexcept {
    if(!usableSegment(p)||p.peak>=.999f)return 1.e6f;
    // Weighted feature deviation in dB, not a perceptual similarity percent.
    return .32f*std::abs(p.lowDb-t.low)+.32f*std::abs(p.highDb-t.high)+.20f*std::abs(p.crest-t.crest)+.16f*std::abs(p.rmsDb-t.level)+4.f*std::max(0.f,p.peak-.89f);
}
struct Optimization {
    Recipe recipe{};
    float initial=1.e6f,final=1.e6f,holdoutInitial=1.e6f,holdoutFinal=1.e6f,dryScore=1.e6f,dryHoldout=1.e6f;
    int tested=0,accepted=0;bool valid=false;
};
inline Optimization optimize(const Audio& a,const Profile& dry,const Profile* ref=nullptr,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr) noexcept {
    Optimization o;
    if(!dry.valid()||(ref&&!comparable(dry,*ref))||!std::isfinite(a.rate)||a.rate<8000.||a.rate>192000.||a.stereo.size()%2||a.frames()<a.rate*5.9||a.frames()>a.rate*20.+1)return o;
    const auto train=selectSegment(a,0,a.frames()/2),hold=selectSegment(a,a.frames()/2,a.frames());
    const auto d1=measure(a,train,nullptr,cancel),d2=measure(a,hold,nullptr,cancel);
    if(!usableSegment(d1)||!usableSegment(d2))return o;
    const auto t1=targetFor(d1,ref),t2=targetFor(d2,ref);
    o.dryScore=loss(d1,t1);o.dryHoldout=loss(d2,t2);
    o.recipe=fit(dry,ref);
    auto evaluate=[&](const Recipe& r){++o.tested;if(progress)progress->store(o.tested,std::memory_order_relaxed);return loss(measure(a,train,&r,cancel),t1);};
    auto validate=[&](const Recipe& r){return loss(measure(a,hold,&r,cancel),t2);};
    o.initial=o.final=evaluate(o.recipe);o.holdoutInitial=o.holdoutFinal=validate(o.recipe);
    // Only train selects parameters. Holdout is evaluated once, after search.
    // Two coordinate passes; bounded finite candidate count.
    for(float scale:{1.f,.45f}){
        for(int dim=0;dim<6;++dim){
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
    if(cancel&&cancel->load(std::memory_order_relaxed))return {};
    o.holdoutFinal=validate(o.recipe);
    // Reject a trained winner that regresses on unseen material or cannot
    // improve on dry input. Keep the starter only if it independently wins.
    if(o.holdoutFinal>o.holdoutInitial+.025f){
        o.recipe=fit(dry,ref);o.final=o.initial;o.holdoutFinal=o.holdoutInitial;o.accepted=0;
    }
    o.valid=o.final<1.e5f&&o.holdoutFinal<1.e5f&&o.final+.025f<o.dryScore&&o.holdoutFinal+.025f<o.dryHoldout;
    return o;
}
}
