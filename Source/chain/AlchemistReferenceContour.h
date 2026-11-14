#pragma once
// Alchemist vocal-reference contour — causal, stereo-linked and optional.
//
// Fitted from an ACTUAL compiled native render vs the owner's same-take
// FL Studio target, 19–35 s at 44.1 kHz (2026-11-14). Do not describe this
// as Waves/CLA/Maximus emulation or a verified VST3 sound match.
//
// The new calibration path is ONLY enabled when slot p9 > 0.75; all
// historical Match Finish presets use p9=0.5 and remain exactly as before.
// Existing VST3 parameter IDs and rack stage IDs are untouched.
//
// Per-sample: O(1), fixed state, no allocation, no lookahead, zero PDC.
#include <algorithm>
#include <array>
#include <cmath>

namespace skellpreset::dsp {
class AlchemistReferenceContour final {
public:
    void prepare(double rate) noexcept {
        const auto sampleRate=std::isfinite(rate)?
            std::clamp(rate,8000.,192000.):48000.;
        sr=(float)sampleRate;
        detectorPole=std::exp(-1.f/(sr*.035f)); // 35ms RMS power
        gainPole=detectorPole;                 // smoothed causal gain
        wetPole=std::exp(-1.f/(sr*.025f));     // artifact-free toggles
        shelf.configure(sr,14500.f,9.f,.70f);
        reset();
    }
    void reset() noexcept {
        power=0.f;gainDb=0.f;wet=0.f;airWet=0.f;
        shelf.reset();
    }
    // Existing (unpublished) slot parameters p9..p12:
    // p9: 0..0.75 = legacy engine; >0.75 enables calibrated reference contour
    // p10: strength, 0.5..1.0 = 0..100% of learned downward dynamic slope
    // p11: 0.5..1.0 = 0..100% of optional 9 dB ultrahigh air shelf
    // p12: nominal level trim, 0..1 adjusts reference 2 dB by +/-2 dB
    void set(float enable,float strength,float air,float trim) noexcept {
        const auto unit=[](float v) noexcept {
            return std::isfinite(v)?std::clamp(v,0.f,1.f):.5f;
        };
        enabled=unit(enable)>.75f?1.f:0.f;
        intensity=std::clamp((unit(strength)-.5f)*2.f,0.f,1.f);
        airAmount=std::clamp((unit(air)-.5f)*2.f,0.f,1.f);
        baseDb=2.f+4.f*(unit(trim)-.5f);
    }
    std::array<float,2> process(float left,float right) noexcept {
        if(!std::isfinite(left))left=0.f;
        if(!std::isfinite(right))right=0.f;
        wet=advance(wet,enabled,wetPole);
        if(enabled==0.f && wet<1.e-5f)return {{left,right}};

        // Same output of the previous six rack stages is the detector input.
        const float p=std::clamp(.5f*(left*left+right*right),0.f,64.f);
        power=advance(power,p,detectorPole);
        const float rmsDb=10.f*std::log10(power+1.e-14f);
        const float speech=std::clamp((rmsDb+59.f)/13.f,0.f,1.f);
        // Reference 20ms RMS regression: low-level phrases need distinctly
        // more makeup than already-loud phrases. The two fixed coefficients
        // were calibrated on the actual owner's native-vs-FL A/B, not guessed.
        const float targetDb=std::clamp(
            (baseDb+.38f*(-20.f-rmsDb))*speech*intensity,-1.f,10.5f);
        gainDb=advance(gainDb,targetDb,gainPole);
        const float gain=std::exp(gainDb*.11512925465f);
        const auto processed=shelf.process(left*gain,right*gain);
        airWet=advance(airWet,airAmount,wetPole);
        const float shapedL=left*gain+(processed[0]-left*gain)*airWet;
        const float shapedR=right*gain+(processed[1]-right*gain)*airWet;
        const float safeL=softCeiling(shapedL);
        const float safeR=softCeiling(shapedR);
        const float a=std::clamp(wet,0.f,1.f);
        const float outL=left+(safeL-left)*a;
        const float outR=right+(safeR-right)*a;
        return {{std::isfinite(outL)?std::clamp(outL,-1.f,1.f):0.f,
                 std::isfinite(outR)?std::clamp(outR,-1.f,1.f):0.f}};
    }
    float currentGainDb() const noexcept {return gainDb;}
private:
    struct HighShelf {
        double b0=1,b1=0,b2=0,a1=0,a2=0;
        std::array<double,2> z1{},z2{};
        void reset() noexcept {z1.fill(0.);z2.fill(0.);}
        void configure(double sr,double freq,double boostDb,double steepness) noexcept {
            constexpr double pi=3.14159265358979323846;
            const double w=2.*pi*std::clamp(freq,1000.,sr*.46)/sr;
            const double co=std::cos(w),sn=std::sin(w);
            const double A=std::pow(10.,boostDb/40.);
            const double alpha=sn*.5*std::sqrt((A+1./A)*(1./steepness-1.)+2.);
            const double beta=2.*std::sqrt(A)*alpha;
            const double a0=(A+1.)-(A-1.)*co+beta;
            b0=A*((A+1.)+(A-1.)*co+beta)/a0;
            b1=-2.*A*((A-1.)+(A+1.)*co)/a0;
            b2=A*((A+1.)+(A-1.)*co-beta)/a0;
            a1=2.*((A-1.)-(A+1.)*co)/a0;
            a2=((A+1.)-(A-1.)*co-beta)/a0;
        }
        float processOne(float input,int channel) noexcept {
            const auto sample=(double)input;
            const double output=b0*sample+z1[(size_t)channel];
            z1[(size_t)channel]=b1*sample-a1*output+z2[(size_t)channel];
            z2[(size_t)channel]=b2*sample-a2*output;
            if(std::abs(z1[(size_t)channel])<1.e-25)z1[(size_t)channel]=0.;
            if(std::abs(z2[(size_t)channel])<1.e-25)z2[(size_t)channel]=0.;
            return std::isfinite(output)?(float)output:0.f;
        }
        std::array<float,2> process(float l,float r) noexcept {
            return {{processOne(l,0),processOne(r,1)}};
        }
    };
    static float advance(float current,float desired,float pole) noexcept {
        const float next=pole*current+(1.f-pole)*desired;
        return std::abs(next)<1.e-20f?0.f:next;
    }
    // Soft-knee output safety: derivative is one at the 0.90 threshold;
    // near-full-scale transients approach 0.997, no hard clipping steps.
    static float softCeiling(float in) noexcept {
        constexpr float threshold=.90f,range=.097f;
        const auto a=std::abs(in);
        if(a<=threshold)return in;
        const float out=threshold+range*std::tanh((a-threshold)/range);
        return std::copysign(out,in);
    }
    HighShelf shelf;
    float sr=48000.f,detectorPole=0.f,gainPole=0.f,wetPole=0.f;
    float power=0.f,gainDb=0.f,wet=0.f,airWet=0.f;
    float enabled=0.f,intensity=0.f,airAmount=0.f,baseDb=2.f;
};
} // namespace skellpreset::dsp
