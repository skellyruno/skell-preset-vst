#pragma once
// Optional Alchemist refinement candidate, NOT a validated reference match.
// Five causal, stereo-linked, zero-lookahead refinements:
// 1. 18 Hz DC/subsonic rejection and measured low-body cleanup;
// 2. 6.2 kHz dynamic sibilance management + 14.5 kHz air restoration;
// 3. gently gated low-level phrase density;
// 4. stereo-linked transient preservation;
// 5. continuous soft-knee sample-peak safety (not certified true-peak).
// All states are fixed size, process() performs no allocation or locks.
// Bypass reproduces its input sample-exactly; no historical preset uses it.
#include <algorithm>
#include <array>
#include <cmath>
#include "AlchemistFastMath.h"

namespace skellpreset::dsp {
class AlchemistRefinedDSP final {
public:
    void prepare(double rate) noexcept {
        if(!std::isfinite(rate))rate=48000.;
        sr=(float)std::clamp(rate,8000.,192000.);
        constexpr float refinedPi=3.14159265358979323846f;
        hpPole=std::exp(-2.f*refinedPi*18.f/sr);
        lpStep=1.f-std::exp(-2.f*refinedPi*std::min(6200.f,sr*.43f)/sr);
        bassStep=1.f-std::exp(-2.f*refinedPi*240.f/sr);
        airShelf.configure(sr,14500.,3.5,.70);
        fastStep=1.f-std::exp(-1.f/(sr*.0025f));
        slowStep=1.f-std::exp(-1.f/(sr*.070f));
        gainStep=1.f-std::exp(-1.f/(sr*.045f));
        wetStep=1.f-std::exp(-1.f/(sr*.025f));
        reset();
    }
    void reset() noexcept {
        dcPrevIn.fill(0.f);dcPrevOut.fill(0.f);low.fill(0.f);
        bassLow.fill(0.f);airShelf.reset();
        speechPower=0.f;sibEnv=0.f;peakEnv=0.f;
        densityGain=1.f;transientBlend=0.f;sibGain=1.f;
        wet=0.f;lastSibReductionDb=0.f;
    }
    void set(float enable,float sib,float density,float transient) noexcept {
        auto unit=[](float value) noexcept {
            return std::isfinite(value)?std::clamp(value,0.f,1.f):.5f;
        };
        enabled=unit(enable)>.75f?1.f:0.f;
        sibStrength=unit(sib);
        densityStrength=unit(density);
        transientStrength=unit(transient);
    }
    std::array<float,2> process(float l,float r) noexcept {
        l=std::isfinite(l)?std::clamp(l,-8.f,8.f):0.f;
        r=std::isfinite(r)?std::clamp(r,-8.f,8.f):0.f;
        wet+=wetStep*(enabled-wet);
        if(enabled==0.f && wet<1.e-5f)return {{l,r}};
        // (1) DC / inaudible subsonic removal; each channel independent.
        const float input[2]{l,r};
        float highpass[2],hi[2];
        for(int c=0;c<2;++c) {
            const float dc=input[c]-dcPrevIn[(size_t)c]+hpPole*dcPrevOut[(size_t)c];
            dcPrevIn[(size_t)c]=input[c];
            dcPrevOut[(size_t)c]=sanitize(dc);
            highpass[c]=dcPrevOut[(size_t)c];
            low[(size_t)c]+=lpStep*(highpass[c]-low[(size_t)c]);
            low[(size_t)c]=sanitize(low[(size_t)c]);
            bassLow[(size_t)c]+=bassStep*(highpass[c]-bassLow[(size_t)c]);
            bassLow[(size_t)c]=sanitize(bassLow[(size_t)c]);
            hi[c]=highpass[c]-low[(size_t)c];
        }
        // (2) Independent treble attenuation follows a stereo-linked HF
        // envelope; the low/mid body remains untouched by the de-esser.
        const float sib=std::max(std::abs(hi[0]),std::abs(hi[1]));
        const float sibStep=sib>sibEnv?fastStep:slowStep;
        sibEnv=sanitize(sibEnv+sibStep*(sib-sibEnv));
        const float desiredSib=1.f/
            (1.f+std::max(0.f,sibEnv-.020f)*9.f*sibStrength);
        sibGain+=gainStep*(std::clamp(desiredSib,.72f,1.f)-sibGain);
        sibGain=sanitize(sibGain);
        lastSibReductionDb=alchemist_fast_math::gainDb(std::max(sibGain,.01f));
        // (3) Controlled low-level density is gated off in silence.
        const float energy=.5f*(highpass[0]*highpass[0]+highpass[1]*highpass[1]);
        speechPower=sanitize(speechPower+slowStep*(energy-speechPower));
        const float rmsDb=alchemist_fast_math::powerDb(speechPower,1.e-12f);
        const float voiceGate=std::clamp((rmsDb+67.f)/20.f,0.f,1.f);
        const float densityDb=1.05f*densityStrength*voiceGate*
            (1.f-std::clamp((rmsDb+33.f)/14.f,0.f,1.f));
        const float targetDensity=alchemist_fast_math::gainFromDb(densityDb);
        densityGain+=gainStep*(targetDensity-densityGain);
        densityGain=sanitize(densityGain);
        // (4) Protect fast consonant peaks against over-compression while
        // leaving quiet body tones processed.
        const float framePeak=std::max(std::abs(highpass[0]),std::abs(highpass[1]));
        peakEnv=sanitize(peakEnv+fastStep*(framePeak-peakEnv));
        const float slowAmp=std::sqrt(std::max(speechPower,1.e-12f));
        const float transient=std::clamp((peakEnv-2.2f*slowAmp)/
            std::max(.12f,3.f*slowAmp),0.f,1.f);
        transientBlend+=gainStep*(.10f*transientStrength*transient-transientBlend);
        transientBlend=sanitize(transientBlend);
        std::array<float,2> result{};
        for(int c=0;c<2;++c) {
            // 0.22 dB of low-body cleanup is conservative against the
            // older archived native-vs-target 80–500 Hz excess. The
            // separate 3.5 dB shelf targets a measured 14–18.5k deficit.
            float shaped=(low[(size_t)c]+hi[c]*sibGain-
                          .025f*bassLow[(size_t)c])*densityGain;
            shaped+=transientBlend*(highpass[c]-shaped);
            shaped=airShelf.processOne(shaped,c);
            // (5) Differentiable, bounded sample-peak safety above -1 dBFS.
            // This is NOT a 4x oversampled true-peak limiter.
            shaped=alchemist_fast_math::softCeiling(shaped,.89f,.08f);
            // Measured same-take native trial reached +0.33dBTP at 4x.
            // Reserve 1.11 dB extra sample-domain headroom on THIS OPT-IN
            // candidate only. Original factory index 43 remains bit-exact.
            constexpr float refinedHeadroom=.88f;
            const float mixed=input[c]+(shaped*refinedHeadroom-input[c])*wet;
            result[(size_t)c]=std::isfinite(mixed)?
                std::clamp(mixed,-8.f,8.f):0.f;
        }
        return result;
    }
    float lastSibilanceReductionDb() const noexcept {return lastSibReductionDb;}
    float wetAmount() const noexcept {return wet;}
private:
    static float sanitize(float x) noexcept {
        return std::isfinite(x) && std::abs(x)>=1.e-18f?x:0.f;
    }
    struct HighShelf final {
        double b0=1.,b1=0.,b2=0.,a1=0.,a2=0.;
        std::array<double,2> z1{},z2{};
        void reset() noexcept {z1.fill(0.);z2.fill(0.);}
        void configure(double sampleRate,double frequency,double boostDb,double slope) noexcept {
            constexpr double shelfPi=3.14159265358979323846;
            const double f=std::clamp(frequency,1000.,sampleRate*.46);
            const double w=2.*shelfPi*f/sampleRate;
            const double co=std::cos(w),sn=std::sin(w);
            const double A=std::pow(10.,boostDb/40.);
            const double alpha=sn*.5*std::sqrt((A+1./A)*(1./slope-1.)+2.);
            const double beta=2.*std::sqrt(A)*alpha;
            const double den=(A+1.)-(A-1.)*co+beta;
            b0=A*((A+1.)+(A-1.)*co+beta)/den;
            b1=-2.*A*((A-1.)+(A+1.)*co)/den;
            b2=A*((A+1.)+(A-1.)*co-beta)/den;
            a1=2.*((A-1.)-(A+1.)*co)/den;
            a2=((A+1.)-(A-1.)*co-beta)/den;
        }
        float processOne(float input,int channel) noexcept {
            const double x=(double)input;
            const double y=b0*x+z1[(size_t)channel];
            z1[(size_t)channel]=b1*x-a1*y+z2[(size_t)channel];
            z2[(size_t)channel]=b2*x-a2*y;
            if(std::abs(z1[(size_t)channel])<1.e-24)z1[(size_t)channel]=0.;
            if(std::abs(z2[(size_t)channel])<1.e-24)z2[(size_t)channel]=0.;
            return std::isfinite(y)?(float)y:0.f;
        }
    } airShelf;
    float sr=48000.f,hpPole=.997f,lpStep=.30f,bassStep=.03f;
    float fastStep=.005f,slowStep=.0003f,gainStep=.0005f,wetStep=.0008f;
    float enabled=0.f,sibStrength=.5f,densityStrength=.5f,transientStrength=.5f;
    float wet=0.f,speechPower=0.f,sibEnv=0.f,peakEnv=0.f;
    float sibGain=1.f,densityGain=1.f,transientBlend=0.f,lastSibReductionDb=0.f;
    std::array<float,2> dcPrevIn{},dcPrevOut{},low{},bassLow{};
};
} // namespace skellpreset::dsp
