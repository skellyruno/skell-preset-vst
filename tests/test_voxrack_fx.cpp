#include "../Source/chain/VoxRackFX.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "Assertions must stay enabled in VoxRack DSP tests"
#endif
using namespace skellpreset::dsp;
static void run(double rate) {
    VoxRackRoom room;
    VoxRackSmoothCompressor comp;
    room.prepare(rate);comp.prepare(rate);
    room.set(.65f,.45f,18.f,170.f);
    comp.set(-30.f,5.f,8.f,130.f);
    const int frames=(int)(rate*.65);
    double leftEnergy=0.,rightEnergy=0.,diff=0.;
    double compressed=0.,uncompressed=0.;
    for(int n=0;n<frames;++n) {
        const float input=n==0?1.f:0.f;
        const auto wet=room.process(input,input);
        assert(std::isfinite(wet[0])&&std::isfinite(wet[1]));
        assert(std::abs(wet[0])<=8.001&&std::abs(wet[1])<=8.001);
        leftEnergy+=std::abs(wet[0]);rightEnergy+=std::abs(wet[1]);
        diff+=std::abs(wet[0]-wet[1]);
        const float sine=.8f*std::sin((float)(2*pi*171.*n/rate));
        const auto output=comp.process(sine,sine);
        assert(std::isfinite(output[0])&&std::isfinite(output[1]));
        if(n>frames/2) {compressed+=std::abs(output[0]);uncompressed+=std::abs(sine);}
    }
    assert(leftEnergy>0.0001&&rightEnergy>0.0001);
    assert(diff>.0001); // decorrelated stereo Freeverb network
    assert(compressed<uncompressed*.65);
    // Unity ratio is sample-transparent when the smoothing has settled.
    comp.reset();comp.set(-40.f,1.f,0.f,100.f);
    for(int n=0;n<(int)(rate*.35);++n) {
        const auto out=comp.process(.45f,-.25f);
        if(n>(int)(rate*.27)) {
            assert(std::abs(out[0]-.45f)<.001);
            assert(std::abs(out[1]+.25f)<.001);
        }
    }
    for(float invalid:{std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
        auto a=comp.process(invalid,invalid);
        auto b=room.process(invalid,invalid);
        assert(std::isfinite(a[0])&&std::isfinite(a[1]));
        assert(std::isfinite(b[0])&&std::isfinite(b[1]));
    }
    room.set(.28f,.6f,135.f,420.f);
    for(int n=0;n<(int)(rate*.2);++n) {
        const auto y=room.process(0.f,0.f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
    }
    room.reset();comp.reset();
    double silent=0.;
    for(int n=0;n<(int)(rate*.18);++n) {
        const auto y=room.process(0.f,0.f);
        silent+=std::abs(y[0])+std::abs(y[1]);
    }
    assert(silent<1.e-6);
}
int main() {
    for(double rate:{44100.,48000.,96000.})run(rate);
    std::cout<<"PASS authorized VoxRack stereo Freeverb and smooth-knee compressor\n";
}
