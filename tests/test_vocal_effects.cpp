#include "../Source/chain/VocalEffects.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "These tests need assertions even in Release"
#endif
using namespace skellpreset::dsp;
static bool finite(const std::array<float,2>& x) {
    return std::isfinite(x[0])&&std::isfinite(x[1]);
}
static void run(double sr) {
    BroadcastRadio radio;RetroCrusher crusher;VocalTremolo trem;
    VocalWidth width;ThreeBandDynamics multiband;
    radio.prepare(sr);crusher.prepare(sr);trem.prepare(sr);
    width.prepare(sr);multiband.prepare(sr);
    radio.set(280,3500,.5f,.2f);
    crusher.set(4,5000,.3f,0.f);
    trem.set(4,.0f,.7f,.5f);
    width.set(1.f,180.f,0.f,0.f);
    multiband.set(0.f,0.f,0.f,.2f);
    double unityTrem=0.,unityWidth=0.,unityMultiband=0.;
    // Allow compressor's 12ms parameter smoothing to settle.
    for(int n=0;n<(int)(sr*.32);++n) {
        const float left=.25f*std::sin((float)(2*pi*171.*n/sr));
        const float right=.12f*std::sin((float)(2*pi*503.*n/sr));
        const auto t=trem.process(left,right),w=width.process(left,right);
        const auto m=multiband.process(left,right);
        const auto rb=radio.process(left,right),cr=crusher.process(left,right);
        assert(finite(t)&&finite(w)&&finite(m)&&finite(rb)&&finite(cr));
        if(n>(int)(sr*.22)){
            unityTrem=std::max(unityTrem,(double)std::abs(t[0]-left));
            unityWidth=std::max(unityWidth,(double)std::abs(w[0]-left));
            unityMultiband=std::max(unityMultiband,(double)std::abs(m[0]-left));
        }
    }
    assert(unityTrem<1.e-5 && unityWidth<1.e-5);
    assert(unityMultiband<.001);
    // Width zero is exactly mono, independent of previous side state.
    width.reset();width.set(0.f,160.f,0.f,.0f);
    const auto mono=width.process(.7f,-.2f);
    assert(std::abs(mono[0]-.25f)<1.e-5f && std::abs(mono[1]-.25f)<1.e-5f);
    // Crusher at the lowest bit rate must hold samples and not produce NaNs.
    crusher.reset();crusher.set(3,(float)sr/8.f,0.f,0.f);
    const auto first=crusher.process(.7f,-.7f);
    for(int n=0;n<6;++n) {
        const auto held=crusher.process(0.f,0.f);
        assert(held==first);
    }
    // Multiband should change a loud signal with compression enabled.
    multiband.reset();multiband.set(1.f,1.f,1.f,.1f);
    double lateEnergy=0.,earlyEnergy=0.;
    for(int n=0;n<(int)(sr*.6);++n) {
        const auto out=multiband.process(.85f,.85f);
        assert(finite(out));
        if(n>(int)(sr*.4))lateEnergy+=out[0];
        if(n<(int)(sr*.05))earlyEnergy+=out[0];
    }
    assert(lateEnergy/(sr*.2)<.85*.9);
    for(float bad:{std::numeric_limits<float>::quiet_NaN(),
                   std::numeric_limits<float>::infinity()}) {
        assert(finite(radio.process(bad,bad)));
        assert(finite(crusher.process(bad,bad)));
        assert(finite(trem.process(bad,bad)));
        assert(finite(width.process(bad,bad)));
        assert(finite(multiband.process(bad,bad)));
    }
    radio.reset();crusher.reset();trem.reset();width.reset();multiband.reset();
    for(int i=0;i<(int)(sr*.15);++i){
        assert(finite(radio.process(0.f,0.f)));
        assert(finite(crusher.process(0.f,0.f)));
        assert(finite(trem.process(0.f,0.f)));
        assert(finite(width.process(0.f,0.f)));
        assert(finite(multiband.process(0.f,0.f)));
    }
}
int main() {
    for(double sr:{8000.,44100.,48000.,96000.}) run(sr);
    std::cout<<"PASS native zero-lookahead vocal effects at 44.1/48/96 kHz\n";
}
