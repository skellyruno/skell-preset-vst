#pragma once
// Zero-lookahead, realtime-safe Skell vocal wet-return ducker.
// Stereo-linked and fed from the vocal BEFORE rack effects, not the FX tail.
#include <algorithm>
#include <cmath>

namespace skellpreset::dsp {
class VoiceWetDucker final {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,384000.):48000.;
        envelope=0.f;gain=1.f;configured=false;
        detectAttack=std::exp(-1.0/(sr*.004));
        detectRelease=std::exp(-1.0/(sr*.065));
        gainAttack=std::exp(-1.0/(sr*.012));
    }
    void set(bool on,float amount,float thresholdDb,float releaseMs) noexcept {
        enabled=on;
        depth=finiteClamp(amount,0.f,1.f);
        const float db=finiteClamp(thresholdDb,-60.f,-12.f);
        threshold=std::pow(10.f,db/20.f);
        const float ms=finiteClamp(releaseMs,50.f,1500.f);
        gainRelease=std::exp(-1.0/(sr*.001*(double)ms));
        configured=true;
    }
    float processGain(float left,float right) noexcept {
        if(!configured)return 1.f;
        // Single-precision IIR steps can become smaller than one float ULP
        // near unity; snap only once within ~0.004 dB of exact dry.
        if(!enabled && gain>=.9995f) {
            gain=1.f;envelope=0.f;return 1.f;
        }
        const float a=std::isfinite(left)?std::min(8.f,std::abs(left)):0.f;
        const float b=std::isfinite(right)?std::min(8.f,std::abs(right)):0.f;
        const float level=std::max(a,b);
        const double detector=level>envelope?detectAttack:detectRelease;
        envelope=(float)(detector*envelope+(1.-detector)*level);
        if(envelope<1.e-18f)envelope=0.f;
        const float activity=enabled?
            std::clamp((envelope-threshold)/(threshold*2.f),0.f,1.f):0.f;
        const float target=1.f-depth*activity;
        const double pole=target<gain?gainAttack:gainRelease;
        gain=(float)(pole*gain+(1.-pole)*target);
        if(target>=.999999f && gain>=.9995f)gain=1.f;
        return std::clamp(gain,0.f,1.f);
    }
    float currentGain() const noexcept {return gain;}
    static float finiteClamp(float x,float low,float high) noexcept {
        return std::isfinite(x)?std::clamp(x,low,high):low;
    }
private:
    double sr=48000.,detectAttack=.99,detectRelease=.999,
           gainAttack=.998,gainRelease=.999;
    float envelope=0.f,gain=1.f,threshold=.025f,depth=.65f;
    bool enabled=false,configured=false;
};
} // namespace skellpreset::dsp
