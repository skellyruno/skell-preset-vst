#pragma once
// JUCE-free, real-time-safe opt-in opto compressor.
// Independently implemented from the user-provided Vocal Feel project's
// Flat/Warm/Tube colours and male/male2/female detector concept.
#include <algorithm>
#include <array>
#include <cmath>

namespace skellpreset::dsp {
class VocalFeelOpto final {
public:
    void prepare(double sampleRate) noexcept {
        fs=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        reset();set(amount,colour,voice,blend);
    }
    void reset() noexcept {
        detectorL=detectorR=warmL=warmR=0.f;
        envelope=fastGr=slowGr=memory=reportedGr=0.f;
    }
    void set(float a,float c,float v,float b) noexcept {
        amount=unit(a);colour=unit(c);voice=unit(v);blend=unit(b);
        profile=voice<.25f?0:voice<.75f?1:2;
        constexpr float cutoffs[]{80.f,65.f,105.f};
        lowPole=1.f-std::exp(-6.28318530718f*cutoffs[profile]/(float)fs);
        rmsPole=pole(10.f);attackPole=pole(9.f);releasePole=pole(90.f);
        memoryUp=1.f-pole(2300.f);memoryDown=1.f-pole(4900.f);
        const float warmHz=profile==2?225.f:profile==1?125.f:160.f;
        warmPole=1.f-std::exp(-6.28318530718f*warmHz/(float)fs);
        threshold=-6.f-36.f*amount;
        ratioSlope=1.f-1.f/(1.f+2.5f*amount);
    }
    std::array<float,2> process(float inputL,float inputR) noexcept {
        const float l=std::isfinite(inputL)?inputL:0.f;
        const float r=std::isfinite(inputR)?inputR:0.f;
        if(amount<=0.f||blend<=0.f){reportedGr=0.f;return {l,r};}
        detectorL+=lowPole*(l-detectorL);
        detectorR+=lowPole*(r-detectorR);
        if(std::abs(detectorL)<1.e-20f)detectorL=0.f;
        if(std::abs(detectorR)<1.e-20f)detectorR=0.f;
        const float hpL=l-detectorL,hpR=r-detectorR;
        const float linked=std::max(std::abs(hpL),std::abs(hpR));
        envelope=rmsPole*envelope+(1.f-rmsPole)*linked*linked;
        if(envelope<1.e-20f)envelope=0.f;
        const float level=10.f*std::log10(std::max(envelope,1.e-16f));
        const float over=level-threshold;
        constexpr float halfKnee=5.f;
        float target=0.f;
        if(over>halfKnee)target=ratioSlope*over;
        else if(over>-halfKnee){
            const float kneeOver=over+halfKnee;
            target=ratioSlope*kneeOver*kneeOver/(4.f*halfKnee);
        }
        if(!std::isfinite(target))target=0.f;
        target=std::clamp(target,0.f,48.f);
        const bool held=target>1.5f;
        memory+=(held?memoryUp:memoryDown)*((held?1.f:0.f)-memory);
        const float slowRel=220.f+1050.f*memory;
        const float slowPole=std::exp(-1000.f/(slowRel*(float)fs));
        fastGr=target+(fastGr-target)*(target>fastGr?attackPole:releasePole);
        slowGr=target+(slowGr-target)*(target>slowGr?attackPole:slowPole);
        if(fastGr<1.e-15f)fastGr=0.f;
        if(slowGr<1.e-15f)slowGr=0.f;
        if(memory<1.e-15f)memory=0.f;
        reportedGr=std::clamp(.65f*fastGr+.35f*slowGr,0.f,48.f);
        if(reportedGr<1.e-15f)reportedGr=0.f;
        const float gain=std::pow(10.f,-.05f*(reportedGr-std::min(3.f,.20f*reportedGr)));
        float wetL=l*gain,wetR=r*gain;
        const int model=colour<.25f?0:colour<.75f?1:2;
        if(model==1){
            warmL+=warmPole*(wetL-warmL);warmR+=warmPole*(wetR-warmR);
            if(std::abs(warmL)<1.e-20f)warmL=0.f;
            if(std::abs(warmR)<1.e-20f)warmR=0.f;
            const float a=wetL+.08f*warmL,b=wetR+.08f*warmR;
            wetL+=.22f*amount*(soft(a)-wetL);
            wetR+=.22f*amount*(soft(b)-wetR);
        }else if(model==2){
            wetL+=.32f*amount*(tube(wetL)-wetL);
            wetR+=.32f*amount*(tube(wetR)-wetR);
        }
        const float outputL=l+(wetL-l)*blend,outputR=r+(wetR-r)*blend;
        return {std::isfinite(outputL)?outputL:0.f,
                std::isfinite(outputR)?outputR:0.f};
    }
    float gainReductionDb() const noexcept {return reportedGr;}
    static constexpr int latencySamples() noexcept {return 0;}
private:
    static float unit(float v) noexcept {
        return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;
    }
    float pole(float ms) const noexcept {
        return std::exp(-1000.f/(std::max(.1f,ms)*(float)fs));
    }
    static float soft(float x) noexcept {
        return std::tanh(1.12f*x)/.807568916f;
    }
    static float tube(float x) noexcept {
        constexpr float bias=.24f;
        // constexpr numeric coefficients avoid thread-safe static local
        // initialization/guard on the real-time audio callback.
        constexpr float baseline=.2354957495f;
        constexpr float inv=1.0587144485f;
        return (std::tanh(x+bias)-baseline)*inv;
    }
    double fs=48000.;
    float amount=0.f,colour=0.f,voice=.5f,blend=1.f;
    float threshold=-6.f,ratioSlope=0.f;
    float lowPole=.01f,rmsPole=.99f,attackPole=.99f,releasePole=.999f;
    float memoryUp=.00001f,memoryDown=.00001f,warmPole=.02f;
    float detectorL=0.f,detectorR=0.f,warmL=0.f,warmR=0.f;
    float envelope=0.f,fastGr=0.f,slowGr=0.f,memory=0.f,reportedGr=0.f;
    int profile=1;
};
} // namespace skellpreset::dsp
