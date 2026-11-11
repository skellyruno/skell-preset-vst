#include "../Source/chain/ModulationFX.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "Modulation DSP tests require enabled assertions"
#endif
using namespace skellpreset::dsp;
static bool finite(const std::array<float,2>& s){return std::isfinite(s[0])&&std::isfinite(s[1]);}
static void run(double sr) {
    VocalChorus chorus;VocalFlanger flange;VocalRingMod ring;
    chorus.prepare(sr);flange.prepare(sr);ring.prepare(sr);
    chorus.set(.9f,6.f,4.f,.7f);flange.set(.4f,3.8f,.35f,.5f);
    ring.set(120.f,.83f,.7f,.5f);
    const int n=(int)(sr*.3);
    double chorusEnergy=0.,flangeEnergy=0.,ringEnergy=0.,stereoDiff=0.;
    for(int i=0;i<n;++i){
        const float x=.4f*std::sin((float)(i*6.283185307179586*260./sr));
        const auto a=chorus.process(x,x);const auto b=flange.process(x,x);
        const auto c=ring.process(x,x);
        assert(finite(a)&&finite(b)&&finite(c));
        chorusEnergy+=std::abs(a[0]);flangeEnergy+=std::abs(b[0]);ringEnergy+=std::abs(c[0]);
        stereoDiff+=std::abs(b[0]-b[1]);
    }
    assert(chorusEnergy>1.&&flangeEnergy>1.&&ringEnergy>1.);
    assert(stereoDiff>.001);
    chorus.reset();flange.reset();ring.reset();
    for(int i=0;i<(int)(sr*.12);++i){
        assert(chorus.process(0.f,0.f)==(std::array<float,2>{0.f,0.f}));
        assert(flange.process(0.f,0.f)==(std::array<float,2>{0.f,0.f}));
        assert(ring.process(0.f,0.f)==(std::array<float,2>{0.f,0.f}));
    }
    ring.set(82.f,0.f,.8f,.3f);
    for(int i=0;i<128;++i) {
        auto y=ring.process(.35f,-.22f);
        assert(y[0]==.35f && y[1]==-.22f);
    }
    float bad=std::numeric_limits<float>::infinity();
    assert(finite(chorus.process(bad,bad)));
    assert(finite(flange.process(bad,bad)));
    assert(finite(ring.process(bad,bad)));
    for(int i=0;i<50000;++i){
        if(i%17==0)flange.set(.25f+(i%13),10.f,(i%2)?.85f:-.85f,(i%3)*.5f);
        const auto y=flange.process(i%167==0?3.f:0.f,0.f);
        assert(finite(y) && std::abs(y[0])<=8.f && std::abs(y[1])<=8.f);
    }
}
int main(){for(double sr:{8000.,44100.,48000.,96000.})run(sr);
 std::cout<<"PASS chorus, flanger, ring mod: 8k/44.1k/48k/96k, silence, stereo, automation, finite\n";}
