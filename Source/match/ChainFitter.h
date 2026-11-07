#pragma once
#include "../chain/BasicDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace skellpreset::match {
// The first deterministic optimizer: a RESTRICTED EQ/Comp/Delay model.
// The caller supplies a band centre/Q and envelope attack/release prior.
// It uses precisely the same Biquad, Compressor and Delay classes as the plugin.
// This is not yet the free-form multistage vocal MATCH workflow.
struct ThreeModuleSettings {
    float eqFreq=1200.f,eqGainDb=0.f,eqQ=1.f;
    float thresholdDb=-18.f,ratio=3.f,attackMs=8.f,releaseMs=100.f;
    int delaySamples=2400;
    float feedback=.3f,delayMix=.3f;
};
struct FitReport {
    ThreeModuleSettings settings;
    float score=0, normalizedError=1;
    int evaluations=0;
};
inline std::vector<float> renderKnownChain(const std::vector<float>& dry,double rate,const ThreeModuleSettings& s) {
    dsp::Biquad eq;
    eq.setPeak(rate,s.eqFreq,s.eqGainDb,s.eqQ);
    dsp::Compressor comp;comp.prepare(rate);
    comp.set(s.thresholdDb,s.ratio,s.attackMs,s.releaseMs,0);
    dsp::Delay echo;echo.prepare(rate,1.f);
    echo.configure(s.delaySamples,s.feedback);
    std::vector<float> out(dry.size());
    for(size_t i=0;i<dry.size();++i) {
        const float eqSample=eq.process(dry[i],0);
        const float processed=eqSample*comp.gain(eqSample);
        const auto delay=echo.process(processed,processed);
        out[i]=processed+s.delayMix*delay[0];
    }
    return out;
}
inline float normalizedError(const std::vector<float>& target,const std::vector<float>& actual) {
    double e=0,d=0;
    for(size_t i=0;i<target.size();++i){
        const double z=(double)target[i]-(double)actual[i];e+=z*z;d+=(double)target[i]*target[i];
    }
    return static_cast<float>(std::sqrt(e/std::max(d,1e-15)));
}
// Coordinate descent using a bounded parameter vector. Runs entirely offline.
// Call on a worker thread, never the plugin's audio callback.
inline FitReport fitKnownChain(const std::vector<float>& dry,const std::vector<float>& wet,
                               double rate,ThreeModuleSettings prior,int rounds=7) {
    FitReport r; r.settings=prior;
    if(dry.size()!=wet.size()||dry.size()<512)return r;
    auto evaluate=[&](const ThreeModuleSettings& c)->float {
        ++r.evaluations;
        return normalizedError(wet,renderKnownChain(dry,rate,c));
    };
    float best=evaluate(r.settings);
    // First estimate discrete echo offset from the residual. Sample-exact lag is
    // crucial: a local optimizer cannot climb a comb of wrong-delay minima.
    {
        auto direct=r.settings;direct.delayMix=0.f;
        const auto predicted=renderKnownChain(dry,rate,direct);
        int candidateLag=r.settings.delaySamples;
        double bestCorrelation=-1.;
        const int first=std::max(1,(int)(rate*.08)),last=std::min((int)(rate*.52),(int)dry.size()/2);
        for(int lag=first;lag<=last;lag++){
            double dot=0,a=0,b=0;
            for(size_t i=(size_t)lag;i<dry.size();i++){
                const double residual=(double)wet[i]-predicted[i];
                const double tap=predicted[i-(size_t)lag];
                dot+=residual*tap;a+=residual*residual;b+=tap*tap;
            }
            const auto correlation=dot/std::sqrt(std::max(a*b,1e-20));
            if(correlation>bestCorrelation){bestCorrelation=correlation;candidateLag=lag;}
        }
        auto candidate=r.settings;candidate.delaySamples=candidateLag;
        const auto trial=evaluate(candidate);
        if(trial<best){r.settings=candidate;best=trial;}
    }
    // Staged coarse/fine search around the supplied model prior; per-parameter bounds
    // protect against implausible values and are reduced each round.
    for(int phase=0;phase<rounds;phase++){
        float shrink=std::pow(.55f,(float)phase);
        for(int coordinate: {0,1,2,4,5}) {
            for(int direction:{-2,-1,1,2}) {
                auto candidate=r.settings;
                switch(coordinate){
                    case 0:candidate.eqGainDb=dsp::clamp(candidate.eqGainDb+direction*3.5f*shrink,-15,15);break;
                    case 1:candidate.thresholdDb=dsp::clamp(candidate.thresholdDb+direction*6.f*shrink,-50,0);break;
                    case 2:candidate.ratio=dsp::clamp(candidate.ratio+direction*1.8f*shrink,1,12);break;
                    case 3:candidate.delaySamples=std::clamp(candidate.delaySamples+(int)std::round(direction*rate*.035*shrink),1,(int)(rate*.9));break;
                    case 4:candidate.feedback=dsp::clamp(candidate.feedback+direction*.15f*shrink,0,.85f);break;
                    case 5:candidate.delayMix=dsp::clamp(candidate.delayMix+direction*.18f*shrink,0,.95f);break;
                    default:break;
                }
                const auto err=evaluate(candidate);
                if(err<best){best=err;r.settings=candidate;}
            }
        }
    }
    r.normalizedError=best;r.score=std::clamp(100.f*(1.f-best),0.f,100.f);
    return r;
}
}
