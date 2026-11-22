#include "../Source/chain/StudioDelayDSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
static std::array<float,16> defaults() {
    return {.49f,.32f,.30f,0.f,0.f,.85f,.1f,0.f,.12f,0.f,.48f,1.f,0.f,.5f,.15f,.1f};
}
int main() {
    skellpreset::studio::StereoDelay d;
    d.prepare(48000.);
    auto p=defaults();
    d.set(p,120.);
    float peak=0.f;
    bool echo=false;
    for(int i=0;i<96000;++i) {
        const auto sample=d.process(i==0?1.f:0.f,0.f);
        assert(std::isfinite(sample[0])&&std::isfinite(sample[1]));
        if(i>1000 && std::abs(sample[0])>1.e-5f)echo=true;
        peak=std::max(peak,std::abs(sample[0]));
    }
    assert(echo&&peak>1.e-3f&&peak<=8.f);
    // Stress stereo feedback, diffusion, modulation and tempo changes.
    d.reset();
    p[2]=.76f;p[1]=.95f;p[7]=1.f;p[9]=.8f;p[15]=.85f;
    p[12]=1.f;p[13]=0.f;d.set(p,40.);
    for(int i=0;i<96000;++i){
        const auto y=d.process(i<1000?.9f:0.f,i<1000?.4f:0.f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        assert(std::abs(y[0])<=8.f&&std::abs(y[1])<=8.f);
    }
    // Four beats at 40 BPM must be six seconds, not silently capped
    // to the free-running time range.
    d.reset();
    p=defaults();p[12]=1.f;p[13]=0.f;p[1]=0.f;p[2]=.30f;
    d.set(p,40.);
    bool longEcho=false;
    for(int i=0;i<340000;++i) {
        const auto y=d.process(i==0?1.f:0.f,0.f);
        if(i>=285000 && i<=295000 && std::abs(y[0])>1.e-5f)
            longEcho=true;
    }
    assert(longEcho);
    d.set(p,260.);
    for(int i=0;i<48000;++i) {
        const auto y=d.process(.15f,-.15f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
    }
    std::cout<<"Studio Delay DSP smoke passed\n";
}
