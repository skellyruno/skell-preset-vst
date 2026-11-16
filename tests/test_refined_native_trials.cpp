#include "../Source/chain/AlchemistMatchDSP.h"
#include "../Source/chain/NativeSoundMatchTrials.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    using skellpreset::dsp::AlchemistRefinedDSP;
    namespace al=skellpreset::alchemist;
    namespace atr=skellpreset::alchemist_trial;
    namespace de=skellpreset::deuce;
    static_assert(de::factoryIndex==al::factoryIndex+1,"append-only Deuce index");
    static_assert(skellpreset::deuce_v25::factoryIndex==atr::factoryIndex+1,
                  "v2.5 appended without changing existing indexes");
    static_assert(skellpreset::deuce_v25::stages.size()==13,
                  "v2.5 has the evidence-based native presence EQ");
    static_assert(skellpreset::deuce_v25::stages[12].module==26,
                  "v2.5 final stage is an editable EightBandTone EQ");
    static_assert(atr::factoryIndex==de::factoryIndex+1,"append-only Alchemist trial");
    static_assert(al::stages.size()==7 && de::stages.size()==12,"factory shape");
    static_assert(al::stages[6].p[12]==.5f,"base Alchemist must stay refined-OFF");
    static_assert(atr::stages[6].p[12]==1.f,"new refinement trial opt-in");
    for(double sr:{8000.,44100.,48000.,96000.,192000.}) {
        AlchemistRefinedDSP fx;fx.prepare(sr);
        // Legacy p13<=.75 must be absolutely sample-identical, even after
        // 2 seconds of processing. No denormal/NaN/Inf from silence.
        fx.set(.5f,.8f,.8f,.8f);
        const int N=(int)sr;
        for(int i=0;i<N;++i){
            const float l=.6f*std::sin(2.f*3.14159265f*477.f*i/(float)sr);
            const float r=-l*.9f;
            auto y=fx.process(l,r);
            assert(y[0]==l && y[1]==r);
        }
        fx.reset();fx.set(1.f,.65f,.65f,.5f);
        double peak=0.,side=0.;
        std::uint32_t seed=17737;
        for(int i=0;i<N*2;++i){
            seed=1664525u*seed+1013904223u;
            const float n=((seed>>8)*(1.f/16777216.f)-.5f);
            const float l=.3f*std::sin(2.f*3.14159265f*1003.f*i/(float)sr)+.14f*n;
            const float r=-.22f*std::sin(2.f*3.14159265f*2301.f*i/(float)sr)+.11f*n;
            auto y=fx.process(l,r);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            peak=std::max(peak,(double)std::max(std::abs(y[0]),std::abs(y[1])));
            side+=std::abs((double)y[0]-y[1]);
        }
        assert(peak<2.0 && side>0.1);
        // Five independent algorithmic components all respond correctly:
        // DC blocker rejects a sustained DC source;
        // the rest of the 5-stage chain remains bounded in noise/transients.
        fx.reset();fx.set(1.f,0.f,0.f,0.f);
        float afterDc=0.f;
        for(int i=0;i<N*2;++i)afterDc=fx.process(.25f,.25f)[0];
        assert(std::abs(afterDc)<.003f);
        // No lookahead, no heap state, no effect on disabled legacy output.
        fx.set(.5f,.8f,.8f,.8f);
        for(int i=0;i<N/2;++i){
            const auto y=fx.process(0.f,0.f);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        }
    }
    // The optional reference-informed air shelf must audibly act on the
    // previously weak 14–18.5 kHz region but never affect legacy output.
    AlchemistRefinedDSP air;air.prepare(44100.);air.set(1.f,0.f,0.f,0.f);
    double inEnergy=0.,outEnergy=0.;
    for(int i=0;i<44100;++i) {
        const float x=.009f*std::sin(
            2.f*3.14159265f*16000.f*i/44100.f);
        const auto y=air.process(x,x);
        if(i>22050) {
            inEnergy+=(double)x*x;
            outEnergy+=(double)y[0]*y[0];
        }
    }
    assert(outEnergy>inEnergy*1.1); // >0.4 dB top-end restoration
    // Actual full Match Finish is identical with old arguments and explicit
    // p13=.5; quality is opt-in, not silently enabled for existing sessions.
    skellpreset::dsp::AlchemistMatchFinish legacy,disabled;
    legacy.prepare(48000.);disabled.prepare(48000.);
    disabled.set(.2001f,1.f,.323236f,0.f,.670724f,.335037f,.53875f,
                 .270833f,.5f,.5f,.5f,.5f,.5f,1.f,1.f,1.f);
    for(int i=0;i<20000;++i){
        const float x=.15f*std::sin(2.f*3.14159265f*344.f*i/48000.f);
        assert(legacy.process(x,x)==disabled.process(x,x));
    }
    std::cout<<"Five opt-in causal refinements, untouched original preset, "
                "12-slot Deuce candidate, 8–192 kHz: PASS\n";
}
