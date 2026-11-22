// Portable test: actual opt-in Vocal Feel Opto stereo DSP; no JUCE dependency.
#include "../Source/chain/VocalFeelOptoDSP.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
using skellpreset::dsp::VocalFeelOpto;
constexpr double pi=3.14159265358979323846;
static std::array<double,3> measure(double sr,float amount,float colour,float voice,float mix,bool stereo=false) {
    VocalFeelOpto op;op.prepare(sr);op.set(amount,colour,voice,mix);
    double in=0.,out=0.,peak=0.;
    for(int i=0;i<(int)sr;++i){
        const float sample=.3f*std::sin(2.*pi*250.*i/sr);
        const auto y=op.process(sample,stereo?-sample:sample);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        if(stereo)assert(std::abs(y[0]+y[1])<.002f);
        if(i>sr*.5){in+=sample*sample;out+=y[0]*y[0];}
        peak=std::max(peak,(double)std::max(std::abs(y[0]),std::abs(y[1])));
    }
    return {std::sqrt(out/in),op.gainReductionDb(),peak};
}
int main(){
    VocalFeelOpto dry;dry.prepare(48000.);
    dry.set(0.f,0.f,.5f,1.f);
    for(int i=0;i<5000;++i){
        const float x=std::sin((float)i*.03f);
        const auto y=dry.process(x,-x);
        assert(y[0]==x&&y[1]==-x);
    }
    dry.set(1.f,1.f,1.f,0.f);
    for(int i=0;i<5000;++i){
        const float x=std::sin((float)i*.03f);
        const auto y=dry.process(x,-x);
        assert(y[0]==x&&y[1]==-x);
    }
    static_assert(VocalFeelOpto::latencySamples()==0);
    for(double rate:{44100.,48000.,96000.}){
        const auto flat=measure(rate,.7f,0.f,.5f,1.f,true);
        const auto warm=measure(rate,.7f,.5f,.5f,1.f);
        const auto tube=measure(rate,.7f,1.f,.5f,1.f);
        const auto parallel=measure(rate,.7f,0.f,.5f,.5f);
        std::printf("OPTO %.0f Hz: flat %.3f, GR %.2f dB; warm %.3f tube %.3f parallel %.3f\n",
                    rate,flat[0],flat[1],warm[0],tube[0],parallel[0]);
        assert(flat[0]<.95&&flat[0]>.08&&flat[1]>1.&&flat[1]<40.);
        assert(parallel[0]>flat[0]&&parallel[0]<1.001);
        assert(std::abs(warm[0]-tube[0])>.0001);
        assert(flat[2]<.8);
    }
    for(float strength:{0.f,.5f,1.f})
        for(float colour:{0.f,.5f,1.f})
            for(float voice:{0.f,.5f,1.f}){
                VocalFeelOpto fx;fx.prepare(48000.);fx.set(strength,colour,voice,1.f);
                for(int i=0;i<48000;++i){
                    const float x=i%100<50?.9f:-.9f;
                    const auto y=fx.process(x,0.f);
                    assert(std::isfinite(y[0])&&std::isfinite(y[1]));
                    assert(std::abs(y[0])<2.f&&std::abs(y[1])<2.f);
                }
                for(int i=0;i<48000;++i){
                    const auto y=fx.process(0.f,0.f);
                    assert(std::isfinite(y[0])&&std::isfinite(y[1]));
                }
            }
    std::array<VocalFeelOpto,10> engines;
    for(auto& fx:engines){fx.prepare(48000.);fx.set(.5f,.5f,.5f,.65f);}
    auto begin=std::chrono::steady_clock::now();float checksum=0.f;
    for(int i=0;i<48000;++i){
        float x=.17f*std::sin((float)i*.07f);
        for(auto& fx:engines)x=fx.process(x,x*.9f)[0];
        checksum+=x;
    }
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    std::printf("PASS: Flat/Warm/Tube, 3 rates, 3 voices, zero latency, finite peaks, parallel & stereo. 10x DSP-only 1s @48k: %.1f ms; sum %.4f\n",ms,checksum);
    return 0;
}
