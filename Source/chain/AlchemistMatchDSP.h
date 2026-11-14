#pragma once
// Native, zero-lookahead, real-time-safe adaptation of the user's *last*
// Alchemist v0.7 offline finishing pass. NOT the offline phase fitter or the
// original Waves / FL Studio signal chain. No JUCE, Python, allocations or
// buffered reference audio in process(). Stereo-linked detection.
//
// v0.7 source: fit_harmonics(), dynamic_features(), control(), fit_dynamics()
// in Skelly_Sound_Matching_v0_7/vocal_match_v07.py.
// The symmetric gaussian, 10ms block aggregation, and same-take optimizer
// cannot run as-is in a causal VST3. Here we use continuous IIR detectors,
// causal one-pole gain smoothing, calibrated v0.7 coefficients and explicit
// 0ms host latency. A/B equivalence is NOT claimed.
#include <algorithm>
#include <array>
#include <cmath>

namespace skellpreset::dsp {
class AlchemistMatchFinish {
public:
    void prepare(double sampleRate) noexcept {
        sampleRate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        sr=(float)sampleRate;
        fastPole=std::exp(-1.f/(sr*.020f));
        gatePole=std::exp(-1.f/(sr*.120f));
        slowPole=std::exp(-1.f/(sr*.280f));
        reset();
        set(.2001f,1.f,.323236f,0.f,.670724f,.335037f,.53875f,.270833f);
    }
    void reset() noexcept {
        power20=power120=power280=0.f;
        gainDb=0.f;
        currentCubic=0.f;
    }
    // Eight normalized, automatable values, original slot parameter storage.
    // 0 harmonic coeff -0.030..+0.030 (neutral 0.5).
    // 1 envelope intensity 0..1 (0 bypasses dynamic gain).
    // 2 slow level coefficient -0.35..+0.35.
    // 3 fast-minus-slow coefficient -0.35..+0.35.
    // 4 calibration of active 280ms RMS centre, -40..-10dBFS.
    // 5 nominal 99th-percentile input level, .05.. .80 amplitude.
    // 6 post-shaping gain trim -1..+1 dB.
    // 7 causal smoothing 2..50ms.
    void set(float harmonic,float dynamics,float slowWeight,float fastWeight,
             float center,float p99,float trim,float smooth) noexcept {
        auto unit=[](float v)noexcept{
            return std::isfinite(v)?std::clamp(v,0.f,1.f):.5f;
        };
        targetCubic=(unit(harmonic)-.5f)*.06f;
        dynamicsAmount=unit(dynamics);
        aSlow=(unit(slowWeight)-.5f)*.7f;
        aFast=(unit(fastWeight)-.5f)*.7f;
        centerDb=-40.f+30.f*unit(center);
        const float calibration=.05f+.75f*unit(p99);
        cubicDenominator=1.f/std::max(.0025f,calibration*calibration);
        trimGain=std::pow(10.f,(-1.f+2.f*unit(trim))/20.f);
        gainPole=std::exp(-1.f/(sr*(.002f+.048f*unit(smooth))));
        cubicPole=std::exp(-1.f/(sr*.020f));
    }
    std::array<float,2> process(float left,float right) noexcept {
        if(!std::isfinite(left))left=0.f;
        if(!std::isfinite(right))right=0.f;
        left=std::clamp(left,-8.f,8.f);
        right=std::clamp(right,-8.f,8.f);
        const float d=std::min(64.f,.5f*(left*left+right*right));
        power20=advance(power20,d,fastPole);
        power120=advance(power120,d,gatePole);
        power280=advance(power280,d,slowPole);
        const float db20=10.f*std::log10(power20+1.e-14f);
        const float db120=10.f*std::log10(power120+1.e-14f);
        const float db280=10.f*std::log10(power280+1.e-14f);
        const float gate=std::clamp((db120+49.f)/11.f,0.f,1.f);
        const float shapeSlow=(db280-centerDb)*.1f;
        const float shapeFast=(db20-db280)*.1f;
        const float proposed=(aSlow*shapeSlow+aFast*shapeFast);
        const float capped=std::clamp(proposed,-.6f,.6f)*gate*dynamicsAmount;
        gainDb=advance(gainDb,capped,gainPole);
        currentCubic=advance(currentCubic,targetCubic,cubicPole);
        const float dyn=std::exp(gainDb*.11512925465f); // ln(10)/20
        const float amount=currentCubic*cubicDenominator;
        const float mult=trimGain*dyn;
        const float outL=(left+amount*left*left*left)*mult;
        const float outR=(right+amount*right*right*right)*mult;
        return {std::isfinite(outL)?std::clamp(outL,-8.f,8.f):0.f,
                std::isfinite(outR)?std::clamp(outR,-8.f,8.f):0.f};
    }
    float lastGainDb() const noexcept {return gainDb;}
private:
    static float advance(float previous,float next,float pole) noexcept {
        const float v=pole*previous+(1.f-pole)*next;
        return std::abs(v)<1.e-20f?0.f:v;
    }
    float sr=48000.f;
    float fastPole=0.f,gatePole=0.f,slowPole=0.f;
    float power20=0.f,power120=0.f,power280=0.f;
    float gainDb=0.f,currentCubic=0.f;
    float gainPole=.995f,cubicPole=.999f;
    float targetCubic=-.017994f,dynamicsAmount=1.f;
    float aSlow=-.123735f,aFast=-.35f;
    float centerDb=-19.87828f,cubicDenominator=1.f/(.3012775f*.3012775f);
    float trimGain=1.00896f;
};
} // namespace skellpreset::dsp
