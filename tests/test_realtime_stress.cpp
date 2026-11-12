#include "../Source/chain/BasicDSP.h"
#include <cassert>
#if defined(NDEBUG)
#error "DSP stress assertions must remain enabled in release builds"
#endif
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace skellpreset::dsp;

static void testCompressorAutomation() {
    Compressor c;
    c.prepare(48000);
    c.set(-16.f,2.5f,5.f,70.f,0.f);
    for(int i=0;i<24000;++i) (void)c.gain(.2f);
    const float baseline=c.gain(.2f);
    c.set(-16.f,2.5f,5.f,70.f,24.f);
    const float first=c.gain(.2f);
    assert(first>0 && first<baseline*1.04f); // no instant 24dB jump
    for(int i=0;i<24000;++i) (void)c.gain(.2f);
    const float settled=c.gain(.2f);
    assert(std::isfinite(settled) && settled>baseline*10.f);
    assert(std::isfinite(c.gain(std::numeric_limits<float>::quiet_NaN())));
    assert(std::isfinite(c.gain(std::numeric_limits<float>::infinity())));
}

static void stressRate(double sr) {
    SmoothedBiquad eq, highpass;
    eq.prepare(sr); highpass.prepare(sr);
    Compressor comp; comp.prepare(sr);
    Gate gate;gate.prepare(sr);
    Delay delay;delay.prepare(sr);
    AdlibDoubler doubler;doubler.prepare(sr);
    StereoSampleDelay dry;dry.prepare((int)(sr*.04));
    const auto started=std::chrono::steady_clock::now();
    float peak=0, derivative=0,previous=0;
    constexpr int cycles=2;
    for(int n=0;n<(int)(sr*cycles);++n){
        // Deliberately hostile 16-sample parameter automation and rapid changes
        // between block sizes. This also checks 0/small and long buffers.
        if(n%16==0){
            const auto t=(float)(n/16%31)/30.f;
            eq.setPeak(sr,60.*std::pow(180.,t),-12.+24.*t,.4+8.*(1.-t));
            highpass.setHighPass(sr,20.+180.*t);
            comp.set(-38.+30.*t,1.+11.*t,.8f+40.f*t,20.f+150.f*t,6.f*t);
            delay.configure(1+(int)(sr*(.07+.4*t)),.15f+.5f*t);
        }
        const float raw=(n%2511==0)?std::numeric_limits<float>::quiet_NaN():
                        (n%3377==0)?std::numeric_limits<float>::infinity():
                        .11f*std::sin((float)(2.*pi*380.*n/sr));
        const auto cut=highpass.process(raw,raw);
        const auto filtered=eq.process(cut[0],cut[1]);
        const auto gain=comp.gain(std::max(std::abs(filtered[0]),std::abs(filtered[1])));
        const auto gateGain=gate.processGain(std::abs(filtered[0]));
        const float l=filtered[0]*gain*gateGain, r=filtered[1]*gain*gateGain;
        const auto echoes=delay.process(l,r);
        const auto doubled=doubler.process(l,r,.65f,.4f,.3f,.6f);
        const auto aligned=dry.process(l,r,17);
        const float y=.65f*l + .08f*echoes[0]+.10f*doubled[0]+.17f*aligned[0];
        const float yr=.65f*r + .08f*echoes[1]+.10f*doubled[1]+.17f*aligned[1];
        assert(std::isfinite(y)&&std::isfinite(yr));
        peak=std::max(peak,std::max(std::abs(y),std::abs(yr)));
        if(n>0 && n%2511!=0 && n%3377!=0)
            derivative=std::max(derivative,std::abs(y-previous));
        previous=y;
    }
    const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::cout<<"rate="<<sr<<" Hz, "<<cycles<<"s audio, wall="<<elapsed<<"s, "
             <<"estimated one-thread x-real-time="<<cycles/elapsed
             <<", peak="<<peak<<", max sample jump="<<derivative<<"\n";
    assert(peak<1.f); // moderate input is not accidentally boosted into clipping
    assert(derivative<.7f); // guard against obvious automation discontinuities
}

static void testDenormalDrain() {
    Compressor compressor;compressor.prepare(48000.);
    compressor.set(-22.f,3.f,2.f,100.f,0.f);
    Gate gate;gate.prepare(48000.);gate.threshold=-40.f;
    Delay delay;delay.prepare(48000.);delay.configure(100,.38f);
    const auto tiny=std::numeric_limits<float>::denorm_min();
    for(int i=0;i<48000*7;++i) {
        const float v=(i==0)?.2f:tiny;
        const float gain=compressor.gain(v);
        const float gated=gate.processGain(v);
        const auto echoes=delay.process(v,v);
        assert(std::isfinite(gain)&&std::isfinite(gated));
        assert(std::fpclassify(echoes[0])!=FP_SUBNORMAL);
    }
    assert(compressor.envelope==0.);
    assert(gate.envelope==0. && gate.gain==0.);
    for(const auto& line:delay.memory)
        for(float v:line)assert(std::fpclassify(v)!=FP_SUBNORMAL);
}
static void testDynamicPdcCrossfade() {
    // Invariant 1: unchanged latency must be sample-identical to the existing
    // dry alignment, including an impulse at delay 0 and a fixed 64 samples.
    for(int fixed:{0,64,256}) {
        StereoSampleDelay reference,smoothed;
        reference.prepare(512);smoothed.prepare(512);
        for(int n=0;n<4096;++n) {
            const float l=n==0?1.f:.125f*std::sin(.17f*(float)n);
            const float r=n==0?-.5f:.093f*std::cos(.11f*(float)n);
            const auto a=reference.process(l,r,fixed);
            const auto b=smoothed.processSmoothed(l,r,fixed,480);
            assert(a[0]==b[0] && a[1]==b[1]);
        }
    }

    // Invariant 2: unlike a hard delay-tap switch, automation from zero PDC
    // to 128 samples and back does not create a full-scale single-sample step.
    const auto sr=48000.;
    StereoSampleDelay oldTap,newTap;
    oldTap.prepare(256);newTap.prepare(256);
    float precedingOld=0.f,precedingNew=0.f;
    float maxOldJump=0.f,maxNewJump=0.f;
    for(int n=0;n<12000;++n) {
        const int desired=(n>=3200 && n<7800)?128:0;
        const float input=.25f*std::sin(2.f*pi*523.25f*(float)n/(float)sr);
        const auto old=oldTap.process(input,-input,desired);
        const auto out=newTap.processSmoothed(input,-input,desired,480);
        assert(std::isfinite(out[0]) && std::isfinite(out[1]));
        assert(std::fpclassify(out[0])!=FP_SUBNORMAL);
        assert(std::abs(out[0])<=.251f && std::abs(out[1])<=.251f);
        if(n>1) {
            maxOldJump=std::max(maxOldJump,std::abs(old[0]-precedingOld));
            maxNewJump=std::max(maxNewJump,std::abs(out[0]-precedingNew));
        }
        precedingOld=old[0];precedingNew=out[0];
    }
    assert(maxOldJump>.17f); // strong test signal demonstrates old failure
    assert(maxNewJump<.08f); // new line has neither jump nor peak increase

    // Invariant 3: repeated, even mid-ramp, PDC updates are finite and
    // produce no out-of-range memory taps or allocations on the audio thread.
    StereoSampleDelay moving;
    moving.prepare(1024);
    const auto pdcStarted=std::chrono::steady_clock::now();
    for(int n=0;n<250000;++n) {
        const int requested=(n/47)%3==0?0:(n/47)%3==1?400:1023;
        const auto output=moving.processSmoothed(
            n%499==0?std::numeric_limits<float>::denorm_min():
              n%733==0?std::numeric_limits<float>::quiet_NaN():.07f,
            n%607==0?std::numeric_limits<float>::infinity():
              n%911==0?std::numeric_limits<float>::denorm_min():-.07f,
            requested,441);
        assert(std::isfinite(output[0]) && std::isfinite(output[1]));
        assert(std::fpclassify(output[0])!=FP_SUBNORMAL);
        assert(std::fpclassify(output[1])!=FP_SUBNORMAL);
        assert(std::abs(output[0])<=.071f && std::abs(output[1])<=.071f);
    }
    const auto pdcSeconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-pdcStarted).count();
    std::cout<<"PASS stereo PDC crossfade / old max step "<<maxOldJump
             <<" / smoothed max step "<<maxNewJump
             <<" / 250k stress frames "<<pdcSeconds<<"s"
             <<" / x48k realtime "<<(250000./48000.)/std::max(1.e-9,pdcSeconds)<<"\n";
}
int main(){
    testDynamicPdcCrossfade();
    testDenormalDrain();
    testCompressorAutomation();
    for(double sr:{44100.,48000.,96000.})stressRate(sr);
    std::cout<<"PASS realtime coefficient/parameter smoothing, finite output, headroom and latency fixtures\n";
}
