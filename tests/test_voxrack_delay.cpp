#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "VoxRack delay test must run in Release"
#endif
using skellpreset::dsp::Delay;

static void exercise(double rate) {
    Delay dark,bright;
    dark.prepare(rate,.25f);bright.prepare(rate,.25f);
    constexpr int delaySamples=64;
    dark.configure(delaySamples,.7f,500.f);
    bright.configure(delaySamples,.7f,16000.f);
    // Let the smoothed tone coefficients settle before the impulse.
    for(int n=0;n<(int)(rate*.15);++n){
        dark.process(0.f,0.f);bright.process(0.f,0.f);
    }
    double darkRepeat=0.,brightRepeat=0.,dryRepeat=0.;
    for(int n=0;n<delaySamples*4;++n) {
        const float sample=n==0?1.f:0.f;
        const auto a=dark.process(sample,sample*.5f);
        const auto b=bright.process(sample,sample*.5f);
        assert(std::isfinite(a[0])&&std::isfinite(b[0]));
        if(n==delaySamples){assert(a[0]>.99f&&b[0]>.99f);}
        if(n==delaySamples*2) {
            darkRepeat=std::abs(a[0]);
            brightRepeat=std::abs(b[0]);
        }
        dryRepeat+=std::abs(a[0]-a[1]*2.f);
    }
    // VoxRack feedback low-pass should darken successive repeats, while
    // first-tap timing and left/right separation remain unchanged.
    assert(brightRepeat>darkRepeat*1.6);
    assert(dryRepeat<.005); // stereo tracking must remain independent
    for(float hostile:{std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::max()}) {
        (void)dark.process(hostile,hostile);
    }
    for(int n=0;n<(int)(rate*.2);++n) {
        const auto y=dark.process(0.f,0.f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        assert(std::abs(y[0])<=8.001f);
    }
}

int main() {
    for(double sr:{44100.,48000.,96000.})exercise(sr);
    std::cout<<"PASS VoxRack-inspired filtered feedback 44.1/48/96k, stereo and finite output\n";
}
