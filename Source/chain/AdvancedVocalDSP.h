#pragma once
// Portable native SKELL vocal effects, informed by the user-authorized FLAMEEWRLD
// Studio Vocal Chain (FET -> opto, harshness suppression) and MIT Auralis Voice
// Changer (8 independent cascaded EQ bands). No DAW/JUCE calls in process().
#include "BasicDSP.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <limits>
namespace skellpreset::dsp {

// Eight independent peak bands. The sixteen existing rack parameters describe
// eight log-spaced frequencies and gains; all eight Q controls are independently
// automatable and saved in the compatible native preset schema.
// No extra per-sample allocations or changes to old parameter IDs.
struct EightBandTone {
    static constexpr int bands=8;
    std::array<SmoothedBiquad,bands> bell{};
    SmoothedBiquad lowCut, highCut;
    // Native parameters are stable for the vast majority of 64/128-sample
    // callbacks. Avoid recomputing 8x trig/pow-heavy biquad designs unless
    // the host actually changes a control or sample rate. SmoothedBiquad
    // still advances its existing 12ms transition per audio sample.
    std::array<float,16> lastParameters{};
    std::array<float,8> lastQ{};
    double lastBandsRate=0.,lastCutsRate=0.;
    float lastLowCut=0.f,lastHighCut=0.f;
    bool bandsConfigured=false,cutsConfigured=false;
    static constexpr std::array<float,bands+1> edges{
        35.f,90.f,185.f,380.f,820.f,1700.f,3600.f,7500.f,17500.f};
    void prepare(double sr) noexcept {
        for(auto& b:bell)b.prepare(sr);
        lowCut.prepare(sr);highCut.prepare(sr);
        bandsConfigured=false;cutsConfigured=false;
    }
    static float lowCutFrequency(float value) noexcept {
        return 20.f*std::pow(20.f,std::clamp(value,0.f,1.f)); // 20–400 Hz
    }
    static float highCutFrequency(float value) noexcept {
        return 20000.f*std::pow(.09f,std::clamp(value,0.f,1.f)); // 20k–1.8k
    }
    // A zero parameter is a flat, identity filter, preserving old projects.
    void setCuts(double sr,float low,float high) noexcept {
        const float lo=std::isfinite(low)?std::clamp(low,0.f,1.f):0.f;
        const float hi=std::isfinite(high)?std::clamp(high,0.f,1.f):0.f;
        if(cutsConfigured && lastCutsRate==sr &&
           lastLowCut==lo && lastHighCut==hi)return;
        lastCutsRate=sr;lastLowCut=lo;lastHighCut=hi;cutsConfigured=true;
        if(lo<=.0001f)lowCut.aim(Biquad{});
        else lowCut.setHighPass(sr,lowCutFrequency(lo));
        if(hi<=.0001f)highCut.aim(Biquad{});
        else highCut.setLowPass(sr,highCutFrequency(hi));
    }
    static float qFromNormalized(float value) noexcept {
        const float x=std::isfinite(value)?std::clamp(value,0.f,1.f):.5f;
        return .4f*std::pow(20.f,x); // Q 0.4–8.0
    }
    static float normalizedFromQ(float q) noexcept {
        if(!std::isfinite(q))return .5f;
        return std::clamp(std::log(std::max(.4f,q)/.4f)/std::log(20.f),0.f,1.f);
    }
    void set(double sr,const std::array<float,16>& p,
             const std::array<float,8>& q) noexcept {
        if(!std::isfinite(sr)||sr<8000.)sr=48000.;
        if(bandsConfigured && lastBandsRate==sr &&
           lastParameters==p && lastQ==q)return;
        lastBandsRate=sr;lastParameters=p;lastQ=q;bandsConfigured=true;
        for(int i=0;i<bands;++i){
            const auto freqVal=p[(size_t)i*2];
            const auto gainVal=p[(size_t)i*2+1];
            const float freqNorm=std::isfinite(freqVal)?std::clamp(freqVal,0.f,1.f):.5f;
            const float gainNorm=std::isfinite(gainVal)?std::clamp(gainVal,0.f,1.f):.5f;
            const float frequency=edges[(size_t)i]*
                std::pow(edges[(size_t)i+1]/edges[(size_t)i],freqNorm);
            const float db=(gainNorm-.5f)*30.f;
            bell[(size_t)i].setPeak(sr,frequency,db,qFromNormalized(q[(size_t)i]));
        }
    }
    // Legacy test and neutral defaults need not construct a second array.
    void set(double sr,const std::array<float,16>& p) noexcept {
        std::array<float,8> defaultQ{};defaultQ.fill(.5f);
        set(sr,p,defaultQ);
    }
    std::array<float,2> process(float l,float r) noexcept {
        if(!std::isfinite(l))l=0.f;
        if(!std::isfinite(r))r=0.f;
        const auto hp=lowCut.process(l,r);
        l=hp[0];r=hp[1];
        for(auto& band:bell){
            const auto out=band.process(l,r);
            l=out[0];r=out[1];
        }
        return highCut.process(l,r);
    }
};

namespace detail {
// Shared, immutable dB-to-linear table for the serial FET/RMS detector.
// Initialized from prepare() (never on the audio callback); 4096 intervals
// over 0..144 dB keep interpolation error below 1e-5 relative.
class DbGainLookup final {
public:
    static constexpr int intervals=4096;
    static constexpr float maxDb=144.f;

    static void warm() noexcept { (void)table(); }

    static float gain(float reductionDb) noexcept {
        if(!std::isfinite(reductionDb))return 0.f;
        const float position=std::clamp(reductionDb,0.f,maxDb)
            * ((float)intervals/maxDb);
        const int index=std::min((int)position,intervals-1);
        const float fraction=position-(float)index;
        const auto& values=table();
        return values[(size_t)index]
            +(values[(size_t)index+1]-values[(size_t)index])*fraction;
    }

private:
    static const std::array<float,intervals+1>& table() noexcept {
        static const auto values=[] {
            std::array<float,intervals+1> result{};
            for(int i=0;i<=intervals;++i) {
                const float db=maxDb*(float)i/(float)intervals;
                result[(size_t)i]=std::exp2(-db*dbToLog2);
            }
            return result;
        }();
        return values;
    }
};
} // namespace detail

// Serial peak catcher + slower RMS leveler; both stereo linked and entirely
// feed-forward. Controls: threshold dB, ratio, FET speed, parallel density.
// The output is not automatically boosted; unity makeup protects 0 input.
class SerialVocalDynamics {
public:
    void prepare(double rate) noexcept {
        sr=std::isfinite(rate)?std::clamp(rate,8000.,192000.):48000.;
        detail::DbGainLookup::warm(); // initialize shared table off the audio thread
        reset();
        rmsCoeff=std::exp(-1.f/(.030f*(float)sr));
        optoAtt=std::exp(-1.f/(.020f*(float)sr));
        optoRel=std::exp(-1.f/(.250f*(float)sr));
    }
    void reset() noexcept {fetGr=optoGr=power=0.f;}
    void set(float t,float ratio,float speed,float parallel) noexcept {
        t=unit(t);ratio=unit(ratio);speed=unit(speed);parallel=unit(parallel);
        threshold=-48.f+42.f*t;
        slope=1.f-1.f/(2.f+10.f*ratio);
        fetAtt=std::exp(-1.f/((.35f+8.f*speed)*.001f*(float)sr));
        fetRel=std::exp(-1.f/((45.f+320.f*speed)*.001f*(float)sr));
        smash=parallel*.36f;
    }
    std::array<float,2> process(float l,float r) noexcept {
        l=sane(l);r=sane(r);
        const float peak=std::max(std::abs(l),std::abs(r));
        const float levelDb=log2ToDb20*std::log2(std::max(peak,1e-9f));
        const float over=levelDb-threshold;
        const float knee=4.f;
        const float target=over<=-knee*.5f?0.f:
            (over>=knee*.5f?over*slope:(over+knee*.5f)*(over+knee*.5f)*slope/(2.f*knee));
        const float fCoeff=target>fetGr?fetAtt:fetRel;
        fetGr=fCoeff*fetGr+(1.f-fCoeff)*target;
        // Flush inaudibly small release state before it enters the IEEE-754
        // subnormal range on CPUs without FTZ/DAZ enabled.
        if(fetGr<1.e-20f)fetGr=0.f;
        const float fastGain=detail::DbGainLookup::gain(fetGr);
        const float fl=l*fastGain,fr=r*fastGain;
        // Limit the *detector* energy, not the audio signal. Squaring a
        // perfectly finite, pathological floating-point input can overflow,
        // poisoning the RMS follower and muting all subsequent output.
        const float dL=std::clamp(fl,-32.f,32.f);
        const float dR=std::clamp(fr,-32.f,32.f);
        power=rmsCoeff*power+(1.f-rmsCoeff)*.5f*(dL*dL+dR*dR);
        if(!std::isfinite(power)||power<1e-25f)power=0.f;
        const float rms=log2ToDb10*std::log2(std::max(power,1e-18f));
        const float optoTarget=std::max(0.f,(rms-threshold+3.f)*.43f);
        const float oCoeff=optoTarget>optoGr?optoAtt:optoRel;
        optoGr=oCoeff*optoGr+(1.f-oCoeff)*optoTarget;
        if(optoGr<1.e-20f)optoGr=0.f;
        const float gain=detail::DbGainLookup::gain(fetGr+optoGr);
        const float blend=smash;
        // Makeup-free parallel saturation limits additive gain; at 0 this is
        // exact gain reduction with no hidden increase in signal amplitude.
        // Normalise the saturator's small-signal gain to unity, so enabling
        // NY parallel colouring cannot silently boost quiet material.
        const float parallelL=tanhPade(l*3.5f)/3.5f;
        const float parallelR=tanhPade(r*3.5f)/3.5f;
        return {sane(l*gain*(1.f-blend)+parallelL*blend),
                sane(r*gain*(1.f-blend)+parallelR*blend)};
    }
    float reductionDb() const noexcept {return fetGr+optoGr;}
private:
    // Algebraically equivalent base-2 forms for the hot per-sample dB/gain
    // conversions. Avoid general pow(10,x) in the real-time audio loop.
    static constexpr float dbToLog2=0.1660964047443681f;
    static constexpr float log2ToDb20=6.020599913279624f;
    static constexpr float log2ToDb10=3.010299956639812f;
    // [7/7] Pade approximation to tanh. Over [-4,4] its maximum absolute
    // error is <1.5e-5; clamping outside keeps the saturator bounded while
    // avoiding two expensive libm tanh calls per audio sample.
    static float tanhPade(float x) noexcept {
        x=std::clamp(x,-4.f,4.f);
        const float x2=x*x;
        const float x4=x2*x2;
        return x*(135135.f+17325.f*x2+378.f*x4+x4*x2)
             /(135135.f+62370.f*x2+3150.f*x4+28.f*x4*x2);
    }
    static float unit(float v) noexcept {return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
    static float sane(float v) noexcept {
        return std::isfinite(v) && std::abs(v)>=std::numeric_limits<float>::min()?v:0.f;
    }
    double sr=48000.;
    float threshold=-20.f,slope=.65f,smash=0.f;
    float fetAtt=.98f,fetRel=.999f,rmsCoeff=.999f,optoAtt=.99f,optoRel=.999f;
    float fetGr=0.f,optoGr=0.f,power=0.f;
};

// Selective resonance control, not a full-band noise gate. A stereo-linked
// band-pass detector tracks hard/harsh resonant energy, while a smoothly
// varying peak filter applies up to maxCut dB reduction near its centre.
// Controls: centre, threshold, maximum reduction, release.
class DynamicHarshness {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        detector.prepare(sr);attenuation.prepare(sr);
        env=0.f; reduction=0.f;
        // Update the dynamic EQ on a fixed one-millisecond audio clock,
        // not at arbitrary host block boundaries.
        updateEvery=std::max(1,(int)std::lround(sr*.001));
        updateCountdown=0;
        reductionPole=std::exp(-1.f/(.0011f*(float)sr));
    }
    void set(float centre,float threshold,float amount,float release) noexcept {
        frequency=1800.f*std::pow(4.2f,unit(centre));
        trigger=std::pow(10.f,(-53.f+43.f*unit(threshold))/20.f);
        maxCut=10.f*unit(amount);
        const float relMs=30.f+350.f*unit(release);
        relCoeff=std::exp(-1.f/(relMs*.001f*(float)sr));
        attCoeff=std::exp(-1.f/(.003f*(float)sr));
        detector.setPeak(sr,frequency,12.,2.3);
        // The attenuation filter itself is updated from process(), so
        // different block sizes cannot shift its onset by an entire block.
    }
    std::array<float,2> process(float l,float r) noexcept {
        l=sane(l);r=sane(r);
        const auto filtered=detector.process(l,r);
        // Detector follows the excess of the presence band, not the bass.
        const float magnitude=std::min(32.f,std::max(std::abs(filtered[0]-l),
                                                     std::abs(filtered[1]-r)));
        const float coef=magnitude>env?attCoeff:relCoeff;
        env=coef*env+(1.f-coef)*magnitude;
        if(env<1e-20f)env=0.f;
        const float over=20.f*std::log10(std::max(env,1.e-9f)/std::max(trigger,1.e-9f));
        const float wanted=std::clamp(over*.55f,0.f,maxCut);
        reduction=reductionPole*reduction+(1.f-reductionPole)*wanted;
        if (--updateCountdown<=0) {
            // 1 kHz coefficient updates with the existing 12 ms stable
            // coefficient interpolation; no heap, locks or audio latency.
            attenuation.setPeak(sr,frequency,-(double)reduction,2.3);
            updateCountdown=updateEvery;
        }
        return attenuation.process(l,r);
    }
    float reductionDb() const noexcept{return reduction;}
private:
    static float unit(float x) noexcept{return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;}
    static float sane(float x) noexcept{return std::isfinite(x)?x:0.f;}
    double sr=48000.;
    SmoothedBiquad detector,attenuation;
    float frequency=3600.f,trigger=.02f,maxCut=0.f,env=0.f,reduction=0.f;
    float attCoeff=.9f,relCoeff=.999f,reductionPole=.98f;
    int updateEvery=48,updateCountdown=0;
};

// Noise floor follower as a safe native fallback, NOT an RNNoise/ML neural
// suppressor. Silence remains silent. Stereo-linked gain prevents image
// shifts. This is deliberately named Adaptive Noise Control in the rack.
class AdaptiveNoiseControl {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        env=0.f;gain=1.f;held=0;
        attack=std::exp(-1.f/(.006f*(float)sr));
        release=std::exp(-1.f/(.120f*(float)sr));
    }
    void set(float threshold,float strength,float hold,float releaseMs) noexcept {
        thresholdGain=std::pow(10.f,(-75.f+55.f*unit(threshold))/20.f);
        minimumGain=std::pow(10.f,-(36.f*unit(strength))/20.f);
        holdSamples=(int)(.25f*unit(hold)*(float)sr);
        release=std::exp(-1.f/((25.f+425.f*unit(releaseMs))*.001f*(float)sr));
    }
    std::array<float,2> process(float l,float r) noexcept {
        l=sane(l);r=sane(r);
        const float peak=std::max(std::abs(l),std::abs(r));
        const float coeff=peak>env?attack:release;
        env=coeff*env+(1.f-coeff)*peak;
        if(env<1e-20f)env=0.f;
        if(env>=thresholdGain)held=holdSamples;
        else if(held>0)--held;
        const float target=env>=thresholdGain||held>0?1.f:minimumGain;
        const float slew=target<gain?release:attack;
        gain=slew*gain+(1.f-slew)*target;
        return {l*gain,r*gain};
    }
private:
    static float unit(float x) noexcept{return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;}
    static float sane(float x) noexcept{return std::isfinite(x)?x:0.f;}
    double sr=48000.;
    float env=0.f,gain=1.f,attack=.99f,release=.999f;
    float thresholdGain=.005f,minimumGain=.1f;
    int held=0,holdSamples=0;
};
} // namespace skellpreset::dsp
