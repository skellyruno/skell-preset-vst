#pragma once
// Optional Alchemist refinement candidate, NOT a validated reference match.
// Five causal, stereo-linked, zero-lookahead refinements:
// 1. 18 Hz DC/subsonic rejection;
// 2. 6.2 kHz dynamic sibilance attenuation;
// 3. gently gated low-level phrase density;
// 4. stereo-linked transient preservation;
// 5. continuous soft-knee sample-peak safety (not certified true-peak).
// All states are fixed size, process() performs no allocation or locks.
// Bypass reproduces its input sample-exactly; no historical preset uses it.
#include <algorithm>
#include <array>
#include <cmath>

namespace skellpreset::dsp {
class AlchemistRefinedDSP final {
public:
    void prepare(double rate) noexcept {
        if(!std::isfinite(rate))rate=48000.;
        sr=(float)std::clamp(rate,8000.,192000.);
        constexpr float refinedPi=3.14159265358979323846f;
        hpPole=std::exp(-2.f*refinedPi*18.f/sr);
        lpStep=1.f-std::exp(-2.f*refinedPi*std::min(6200.f,sr*.43f)/sr);
        fastStep=1.f-std::exp(-1.f/(sr*.0025f));
        slowStep=1.f-std::exp(-1.f/(sr*.070f));
        gainStep=1.f-std::exp(-1.f/(sr*.045f));
        wetStep=1.f-std::exp(-1.f/(sr*.025f));
        reset();
    }
    void reset() noexcept {
        dcPrevIn.fill(0.f);dcPrevOut.fill(0.f);low.fill(0.f);
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
        lastSibReductionDb=20.f*std::log10(std::max(sibGain,.01f));
        // (3) Controlled low-level density is gated off in silence.
        const float energy=.5f*(highpass[0]*highpass[0]+highpass[1]*highpass[1]);
        speechPower=sanitize(speechPower+slowStep*(energy-speechPower));
        const float rmsDb=10.f*std::log10(speechPower+1.e-12f);
        const float voiceGate=std::clamp((rmsDb+67.f)/20.f,0.f,1.f);
        const float densityDb=1.05f*densityStrength*voiceGate*
            (1.f-std::clamp((rmsDb+33.f)/14.f,0.f,1.f));
        const float targetDensity=std::pow(10.f,densityDb/20.f);
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
            float shaped=(low[(size_t)c]+hi[c]*sibGain)*densityGain;
            shaped+=transientBlend*(highpass[c]-shaped);
            // (5) Differentiable, bounded sample-peak safety above -1 dBFS.
            // This is NOT a 4x oversampled true-peak limiter.
            const float magnitude=std::abs(shaped);
            if(magnitude>.89f)
                shaped=std::copysign(.89f+.08f*
                    std::tanh((magnitude-.89f)/.08f),shaped);
            const float mixed=input[c]+(shaped-input[c])*wet;
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
    float sr=48000.f,hpPole=.997f,lpStep=.30f;
    float fastStep=.005f,slowStep=.0003f,gainStep=.0005f,wetStep=.0008f;
    float enabled=0.f,sibStrength=.5f,densityStrength=.5f,transientStrength=.5f;
    float wet=0.f,speechPower=0.f,sibEnv=0.f,peakEnv=0.f;
    float sibGain=1.f,densityGain=1.f,transientBlend=0.f,lastSibReductionDb=0.f;
    std::array<float,2> dcPrevIn{},dcPrevOut{},low{};
};
} // namespace skellpreset::dsp
