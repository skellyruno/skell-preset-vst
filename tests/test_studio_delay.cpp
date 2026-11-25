#include "../Source/chain/StudioDelayDSP.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <limits>
#include <vector>

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

    // Reading the two wet taps must not alter any audio sample or feedback.
    {
        skellpreset::studio::StereoDelay plain,observed;
        plain.prepare(48000.);observed.prepare(48000.);
        auto controls=defaults();controls[0]=0.f;controls[1]=0.f;controls[2]=.3f;
        controls[3]=.2f;controls[9]=0.f;controls[10]=.5f;controls[11]=1.f;
        plain.set(controls,120.);observed.set(controls,120.);
        const auto times=observed.tapBaseMs();
        if(std::abs(times[0]-25.f)>.01f || std::abs(times[1]-35.f)>.01f)return 16;
        for(int n=0;n<2500;++n){
            const float l=n==0?.5f:0.f,r=n==0?.25f:0.f;
            std::array<float,2> tap{};
            const auto a=plain.process(l,r,.5f),b=observed.process(l,r,.5f,&tap);
            if(a!=b)return 17;
            const float expectedL=n==1200?.125f:0.f,expectedR=n==1680?.0625f:0.f;
            if(std::abs(tap[0]-expectedL)>1.e-5f || std::abs(tap[1]-expectedR)>1.e-5f)return 18;
        }
    }
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
    p[11]=0.f; // Isolate the wet echo: the dry impulse at t=0 is not an early repeat.
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
    // Ten targeted compatibility/protection checks, no old presets or
    // automation parameters are changed. Each stress engine is self-owned.
    for(const double badRate:{0.,-44100.,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity()}) {
        skellpreset::studio::StereoDelay guarded;
        guarded.prepare(badRate);
        auto bad=defaults();
        guarded.set(bad,120.);
        for(int n=0;n<1024;++n) {
            const auto y=guarded.process(n==0?.3f:0.f,-.3f);
            if(!require(std::isfinite(y[0]) && std::isfinite(y[1]),
                        "invalid host sample rate does not poison delay state"))
                return 10;
        }
    }
    // Repeated identical callback updates must not restart time ramps or
    // alter samples versus the original single-set processing.
    for(double rate:{44100.,48000.,96000.}) {
        skellpreset::studio::StereoDelay once,everyBlock;
        once.prepare(rate);everyBlock.prepare(rate);
        auto controls=defaults();
        controls[1]=.81f;controls[2]=.56f;controls[7]=.55f;
        controls[8]=.41f;controls[9]=.2f;controls[15]=.33f;
        controls[12]=1.f;controls[13]=.42f;
        once.set(controls,123.);everyBlock.set(controls,123.);
        for(int n=0;n<(int)(rate*.6);++n) {
            if(n%128==0)everyBlock.set(controls,123.);
            const float x=.24f*std::sin((float)(n*.013));
            const auto a=once.process(x,-.71f*x);
            const auto b=everyBlock.process(x,-.71f*x);
            if(!require(a==b,"identical settings do not alter delay output"))
                return 11;
        }
        // Tempo changes invalidate the cache; repeated updates do not.
        controls[12]=1.f;controls[13]=.8f;
        once.set(controls,64.);
        everyBlock.set(controls,64.);
        for(int n=0;n<4096;++n) {
            if(n%128==0)everyBlock.set(controls,64.);
            const auto a=once.process(n==0?1.f:0.f,0.f);
            const auto b=everyBlock.process(n==0?1.f:0.f,0.f);
            if(!require(a==b,"tempo update preserves sample-exact parity"))
                return 12;
        }
        // Reset must invalidate the previous cache, so the first echo
        // immediately honors the newly set tempo/time without stale state.
        once.reset();everyBlock.reset();
        controls[0]=0.f;controls[12]=0.f;
        controls[1]=0.f;controls[7]=0.f;controls[9]=0.f;
        controls[10]=1.f;controls[11]=0.f;controls[15]=0.f;
        once.set(controls,120.);everyBlock.set(controls,120.);
        const int initial=(int)std::round(rate*.025);
        bool audible=false;
        for(int n=0;n<initial+3;++n) {
            const auto a=once.process(n==0?1.f:0.f,0.f);
            const auto b=everyBlock.process(n==0?1.f:0.f,0.f);
            if(!require(a==b,"reset restores parity"))return 13;
            if(n>=initial-1 && a[0]>.1f)audible=true;
        }
        if(!require(audible,"reset honors first programmed delay time"))
            return 14;
    }
    // Nonfinite or out-of-range automation and BPM never produce NaN audio.
    {
        skellpreset::studio::StereoDelay hostile;
        hostile.prepare(48000.);
        auto controls=defaults();
        controls[0]=std::numeric_limits<float>::quiet_NaN();
        controls[1]=std::numeric_limits<float>::infinity();
        controls[7]=-9.f;controls[9]=5.f;
        hostile.set(controls,std::numeric_limits<double>::quiet_NaN());
        for(int n=0;n<48000;++n) {
            if(n%256==0)hostile.set(controls,std::numeric_limits<double>::infinity());
            const auto a=hostile.process(n==0?1.f:0.f,0.f,
                n%9==0?std::numeric_limits<float>::quiet_NaN():.5f);
            if(!require(std::isfinite(a[0])&&std::isfinite(a[1]) &&
                        std::abs(a[0])<=8.f&&std::abs(a[1])<=8.f,
                        "hostile automation remains finite"))return 15;
        }
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
    // Repeatable 48kHz CPU/memory diagnostics, preallocated instances.
    // These are CI runner measurements, not an FL Studio CPU forecast.
    for(const int instances:{8,16}) {
        std::vector<std::unique_ptr<skellpreset::studio::StereoDelay>> fx;
        fx.reserve((size_t)instances);
        for(int k=0;k<instances;++k) {
            auto engine=std::make_unique<skellpreset::studio::StereoDelay>();
            engine->prepare(48000.);
            auto setup=defaults();setup[9]=0.f;setup[7]=0.f;
            engine->set(setup,120.);
            fx.push_back(std::move(engine));
        }
        volatile float checksum=0.f;
        const auto begin=std::chrono::steady_clock::now();
        for(int n=0;n<48000;++n)for(auto& e:fx) {
            const auto y=e->process(.04f,-.06f);
            checksum=checksum+y[0]*.00001f+y[1]*.00001f;
        }
        const double seconds=std::chrono::duration<double>(
            std::chrono::steady_clock::now()-begin).count();
        const double memoryMB=(double)instances*2.*
            (std::ceil(48000.*6.2)+8.+std::ceil(48000.*.013)+2.)*4.
            /1048576.;
        std::printf("BENCH: Studio Delay DSP-only @ 48kHz (no JUCE/DAW callback): %d instances, one second each, %.3f ms wall/%.2f ms per instance, estimated %.2f MiB delay storage, checksum %.6f\n",
            instances,seconds*1000.,seconds*1000./instances,memoryMB,
            (double)checksum);
        if(!std::isfinite(checksum))return 9;
    }
    return 0;
}
