#include "../Source/chain/StudioDelayDSP.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

static std::array<float,16> defaults() {
    return {.49f,.32f,.30f,0.f,0.f,.85f,.1f,0.f,.12f,0.f,.48f,1.f,0.f,.5f,.15f,.1f};
}
static bool require(bool condition,const char* label) {
    if(!condition)std::fprintf(stderr,"FAIL: Studio Delay %s\n",label);
    return condition;
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
        if(!require(std::isfinite(sample[0])&&std::isfinite(sample[1]),"finite stereo"))
            return 1;
        if(i>1000 && std::abs(sample[0])>1.e-5f)echo=true;
        peak=std::max(peak,std::abs(sample[0]));
    }
    if(!require(echo&&peak>1.e-3f&&peak<=8.f,"basic audible echo"))return 2;

    // First callback after prepare/reset MUST use 25 ms, rather than
    // gliding down from a hidden 250 ms factory-internal delay. No feedback,
    // modulation or dry leakage; sample-accurate first stereo echo.
    d.reset();p=defaults();
    p[0]=0.f;p[1]=0.f;p[2]=.3f;p[9]=0.f;p[10]=1.f;p[11]=0.f;
    p[7]=0.f;p[15]=0.f;
    d.set(p,120.);
    bool early=false,firstEcho=false,wrongChannel=false;
    for(int i=0;i<4000;++i) {
        const auto sample=d.process(i==0?1.f:0.f,0.f);
        if(!std::isfinite(sample[0])||!std::isfinite(sample[1]))return 3;
        if(i<1199 && std::abs(sample[0])>1.e-6f)early=true;
        if(i>=1199 && i<=1201 && sample[0]>.8f)firstEcho=true;
        if(std::abs(sample[1])>1.e-6f)wrongChannel=true;
    }
    if(!require(!early && firstEcho && !wrongChannel,
                "sample-accurate 25 ms first echo / true stereo"))return 4;

    // Stress cross-feedback, saturation, diffusion, modulation and tempo.
    d.reset();p=defaults();
    p[2]=.76f;p[1]=.95f;p[7]=1.f;p[9]=.8f;p[15]=.85f;
    p[12]=1.f;p[13]=0.f;d.set(p,40.);
    for(int i=0;i<96000;++i) {
        const auto y=d.process(i<1000?.9f:0.f,i<1000?.4f:0.f);
        if(!require(std::isfinite(y[0])&&std::isfinite(y[1])&&
                    std::abs(y[0])<=8.f&&std::abs(y[1])<=8.f,
                    "saturated feedback safety"))return 5;
    }

    // Four beats at 40 BPM is six seconds; do not cap at the free time range.
    d.reset();p=defaults();p[12]=1.f;p[13]=0.f;p[1]=0.f;p[2]=.30f;
    d.set(p,40.);
    bool longEcho=false,tooEarly=false;
    for(int i=0;i<340000;++i) {
        const auto y=d.process(i==0?1.f:0.f,0.f);
        if(!std::isfinite(y[0])||!std::isfinite(y[1]))return 6;
        if(i<285000 && std::abs(y[0])>1.e-5f)tooEarly=true;
        if(i>=287999 && i<=288001 && std::abs(y[0])>1.e-5f)longEcho=true;
    }
    if(!require(longEcho&&!tooEarly,"exact six-second beat sync"))return 7;
    d.set(p,260.);
    for(int i=0;i<48000;++i) {
        const auto y=d.process(.15f,-.15f);
        if(!std::isfinite(y[0])||!std::isfinite(y[1]))return 8;
    }
    // Diagnostic CPU cost only (no flaky CI timing threshold).
    // Deliberately exercise non-modulated, non-saturated live default path.
    d.reset();p=defaults();p[9]=0.f;p[7]=0.f;d.set(p,120.);
    float checksum=0.f;
    const auto before=std::chrono::steady_clock::now();
    for(int i=0;i<48000;++i) {
        const auto y=d.process(.05f,-.07f);
        checksum+=y[0]*.00001f+y[1]*.000001f;
    }
    const auto seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-before).count();
    std::printf("PASS: Studio Delay DSP, 25 ms initial echo, six-second beat sync, feedback safety; %.3f ms / 48k stereo samples, checksum %.6f\n",
                seconds*1000.,checksum);
    return 0;
}
