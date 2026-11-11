#pragma once
// Authorized VoxRack DSP adaptations, from user-owned
// voxrack-main/apo/fx.h::FxReverb and apo/dsp.h::Engine::stComp.
// Independent stereo state per rack slot; memory is reserved in prepare(),
// never in the audio callback. No Windows APO or interprocess dependencies.
#include "BasicDSP.h"
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>

namespace skellpreset::dsp {

// VoxRack's eight damped combs followed by four allpasses, adapted into two
// independent stereo channels with right-channel decorrelation and click-safe
// 20 ms predelay crossfades. Pure wet output (slot wet mixes with original).
struct VoxRackRoom {
    struct Comb {
        std::vector<float> data;
        size_t pos=0;
        float filter=0.f;
        void prepare(int count) {data.assign((size_t)std::max(16,count),0.f);pos=0;filter=0.f;}
        void reset() noexcept {std::fill(data.begin(),data.end(),0.f);pos=0;filter=0.f;}
        float tick(float input,float damping,float feedback) noexcept {
            if(data.empty())return 0.f;
            const float output=data[pos];
            filter=output*(1.f-damping)+filter*damping;
            if(std::abs(filter)<1.e-20f)filter=0.f;
            data[pos]=clamp(input+filter*feedback,-8.f,8.f);
            if(++pos==data.size())pos=0;
            return output;
        }
    };
    struct Allpass {
        std::vector<float> data;
        size_t pos=0;
        void prepare(int count){data.assign((size_t)std::max(8,count),0.f);pos=0;}
        void reset() noexcept {std::fill(data.begin(),data.end(),0.f);pos=0;}
        float tick(float x) noexcept {
            if(data.empty())return 0.f;
            const float tap=data[pos];
            data[pos]=clamp(x+tap*.5f,-8.f,8.f);
            if(++pos==data.size())pos=0;
            return tap-x;
        }
    };
    static constexpr std::array<int,8> combLengths{
        1116,1188,1277,1356,1422,1491,1557,1617};
    static constexpr std::array<int,4> allpassLengths{556,441,341,225};
    std::array<std::array<Comb,8>,2> combs;
    std::array<std::array<Allpass,4>,2> allpasses;
    std::array<std::vector<float>,2> predelay;
    std::array<float,2> highpass{};
    size_t head=0;
    double sr=48000.;
    int delaySamples=0,oldDelaySamples=0,targetDelaySamples=0;
    float fade=1.f,fadeStep=.001f;
    float damping=.2f,requestedDamping=.2f;
    float feedback=.83f,requestedFeedback=.83f;
    float hpCoeff=.02f,targetHpCoeff=.02f,slew=.001f;
    void prepare(double sampleRate) {
        sr=std::clamp(sampleRate,8000.,192000.);
        const float multiplier=(float)(sr/44100.);
        for(int ch=0;ch<2;++ch) {
            for(int i=0;i<8;++i)combs[(size_t)ch][(size_t)i].prepare(
                std::max(16,(int)std::round(combLengths[(size_t)i]*multiplier)
                +(ch==1?23:0)));
            for(int i=0;i<4;++i)allpasses[(size_t)ch][(size_t)i].prepare(
                std::max(8,(int)std::round(allpassLengths[(size_t)i]*multiplier)
                +(ch==1?23:0)));
            predelay[(size_t)ch].assign((size_t)(sr*.205)+4,0.f);
        }
        fadeStep=1.f/std::max(1.f,(float)(sr*.020));
        slew=1.f-(float)std::exp(-1./(sr*.05));
        set(.55f,.5f,0.f,120.f);
        feedback=requestedFeedback;
        damping=requestedDamping;
        hpCoeff=targetHpCoeff;
        reset();
    }
    void reset() noexcept {
        for(int ch=0;ch<2;++ch) {
            for(auto& c:combs[(size_t)ch])c.reset();
            for(auto& a:allpasses[(size_t)ch])a.reset();
            std::fill(predelay[(size_t)ch].begin(),predelay[(size_t)ch].end(),0.f);
        }
        highpass={};head=0;fade=1.f;
        delaySamples=oldDelaySamples=targetDelaySamples;
    }
    // Room size 0..1, damping 0..1, predelay milliseconds, HP frequency Hz.
    void set(float size,float damp,float predelayMs,float lowCutHz) noexcept {
        requestedFeedback=.7f+.28f*clamp(size,0.f,1.f);
        requestedDamping=.4f*clamp(damp,0.f,1.f);
        targetHpCoeff=1.f-(float)std::exp(-2.*pi*
            clamp(lowCutHz,20.f,std::min(2000.f,(float)sr*.44f))/sr);
        if(!predelay[0].empty()) {
            targetDelaySamples=std::clamp(
                (int)std::round((double)clamp(predelayMs,0.f,200.f)*sr*.001),
                0,(int)predelay[0].size()-2);
            if(fade>=1.f && targetDelaySamples!=delaySamples) {
                oldDelaySamples=delaySamples;
                delaySamples=targetDelaySamples;
                fade=0.f;
            }
        }
    }
    std::array<float,2> process(float l,float r) noexcept {
        if(predelay[0].empty())return {0.f,0.f};
        feedback+=(requestedFeedback-feedback)*slew;
        damping+=(requestedDamping-damping)*slew;
        hpCoeff+=(targetHpCoeff-hpCoeff)*slew;
        const float a=clamp(l,-8.f,8.f),b=clamp(r,-8.f,8.f);
        const float input[2]{a,b};
        float output[2]{};
        const size_t capacity=predelay[0].size();
        const float alpha=std::clamp(fade,0.f,1.f);
        const size_t tapNow=(head+capacity-(size_t)delaySamples)%capacity;
        const size_t tapOld=(head+capacity-(size_t)oldDelaySamples)%capacity;
        for(int ch=0;ch<2;++ch) {
            highpass[(size_t)ch]+=(input[ch]-highpass[(size_t)ch])*hpCoeff;
            if(std::abs(highpass[(size_t)ch])<1.e-20f)highpass[(size_t)ch]=0.f;
            auto& pre=predelay[(size_t)ch];
            pre[head]=input[ch]-highpass[(size_t)ch];
            const float after=(pre[tapOld]*(1.f-alpha)+pre[tapNow]*alpha)*.015f;
            float y=0.f;
            for(auto& c:combs[(size_t)ch])y+=c.tick(after,damping,feedback);
            for(auto& a:allpasses[(size_t)ch])y=a.tick(y);
            output[ch]=clamp(y*3.f,-8.f,8.f);
        }
        if(++head==capacity)head=0;
        fade=std::min(1.f,fade+fadeStep);
        if(fade>=1.f && targetDelaySamples!=delaySamples) {
            oldDelaySamples=delaySamples;
            delaySamples=targetDelaySamples;
            fade=0.f;
        }
        return {output[0],output[1]};
    }
};

// Direct adaptation of VoxRack's linked, soft-knee compressor transfer and
// gain-reduction attack/release, with 20 ms parameter interpolation.
// All values are bounded; no lookahead or stereo timing offset.
struct VoxRackSmoothCompressor {
    double sr=48000.;
    float threshold=-24.f,ratio=3.f,knee=6.f,releaseMs=120.f;
    double smoothedThreshold=-24.,smoothedRatio=3.,smoothedKnee=6.,gainReduction=0.;
    double paramPole=.998,attackPole=.99,releasePole=.999;
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(sampleRate,8000.,192000.);
        paramPole=std::exp(-1./(sr*.020));
        attackPole=std::exp(-1./(sr*.003));
        set(-24.f,3.f,6.f,120.f);
        smoothedThreshold=threshold;smoothedRatio=ratio;smoothedKnee=knee;
        gainReduction=0.;
    }
    void reset() noexcept {gainReduction=0.;}
    void set(float thr,float r,float kneeDb,float release) noexcept {
        threshold=clamp(thr,-60.f,0.f);
        ratio=clamp(r,1.f,16.f);
        knee=clamp(kneeDb,0.f,24.f);
        releaseMs=clamp(release,10.f,900.f);
        releasePole=std::exp(-1./(sr*.001*releaseMs));
    }
    std::array<float,2> process(float l,float r) noexcept {
        smoothedThreshold=paramPole*smoothedThreshold+(1.-paramPole)*threshold;
        smoothedRatio=paramPole*smoothedRatio+(1.-paramPole)*ratio;
        smoothedKnee=paramPole*smoothedKnee+(1.-paramPole)*knee;
        const float x=clamp(l,-8.f,8.f),y=clamp(r,-8.f,8.f);
        const float det=std::max(std::abs(x),std::abs(y));
        const double over=gainToDb(det)-(float)smoothedThreshold;
        const double slope=1.-1./std::max(1.,smoothedRatio);
        double target=0.;
        if(smoothedKnee>.0001 && 2.*std::abs(over)<=smoothedKnee) {
            const double t=over+smoothedKnee*.5;
            target=slope*t*t/(2.*smoothedKnee);
        } else if(over>0.)target=slope*over;
        gainReduction=target+(gainReduction-target)*
                      (target>gainReduction?attackPole:releasePole);
        if(gainReduction<1.e-18)gainReduction=0.;
        const float gain=dbToGain((float)-gainReduction);
        return {x*gain,y*gain};
    }
};
} // namespace skellpreset::dsp
