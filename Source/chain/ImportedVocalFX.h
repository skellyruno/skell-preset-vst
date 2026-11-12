#pragma once
// Skell Preset vocal-chain extensions: JUCE-independent, allocation-free in process().
// Authored for the Skell rack after reviewing the user-provided FLAMEEWRLD Studio
// Vocal Chain (VocalRiderAndPreamp / DuckingStereoDelay) and MIT Auralis Voice
// Changer (smoothed stereo voice chain, dry/wet/latency contracts). The original
// effect classes were NOT dropped in unmodified: these have bounded parameters,
// stereo-linked level detection, delay-time crossfades, and no file/host work in DSP.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace skellpreset::dsp {

class VocalRider {
public:
    void prepare(double sampleRate) noexcept {
        sr = std::isfinite(sampleRate) ? std::clamp(sampleRate,8000.,192000.) : 48000.;
        envCoeff = std::exp(-1.f/(.080f*(float)sr));
        gainAtt = std::exp(-1.f/(.024f*(float)sr));
        gainRel = std::exp(-1.f/(.140f*(float)sr));
        reset();
    }
    void reset() noexcept { envelope=0.f; currentGain=1.f; }
    // Amount, target LU-ish envelope, max gain range, tape-like warmth.
    // Target is an envelope level, NOT a standards-compliant integrated LUFS target.
    void set(float amount,float targetDb,float rangeDb,float warmth) noexcept {
        strength=clamp01(amount);
        const float db=std::isfinite(targetDb)?std::clamp(targetDb,-30.f,-10.f):-18.f;
        targetLevel=std::pow(10.f,db/20.f);
        const float range=std::isfinite(rangeDb)?std::clamp(rangeDb,0.f,15.f):0.f;
        const float limit=std::pow(10.f,range/20.f);
        minGain=1.f/limit;maxGain=limit;
        colour=clamp01(warmth);
    }
    std::array<float,2> process(float left,float right) noexcept {
        left=finiteOrZero(left);right=finiteOrZero(right);
        const float detector=.5f*(std::abs(left)+std::abs(right));
        envelope=envCoeff*envelope+(1.f-envCoeff)*detector;
        if(!std::isfinite(envelope)||envelope<1.e-20f)envelope=0.f;
        // Keep low-level background/room noise from being chased upward.
        const float goal=envelope>0.0056f
            ? std::clamp(targetLevel/std::max(envelope,1.e-8f),minGain,maxGain)
            : 1.f;
        const float wanted=1.f+strength*(goal-1.f);
        const float coeff=wanted<currentGain?gainAtt:gainRel;
        currentGain=coeff*currentGain+(1.f-coeff)*wanted;
        if(!std::isfinite(currentGain))currentGain=1.f;
        const float l=left*currentGain,r=right*currentGain;
        if(colour<=0.0001f)return {l,r}; // exact linear identity at Warmth=0
        // A restrained optional asymmetry, normalized against a unity reference.
        const float drive=1.f+.8f*colour;
        const float normalizer=1.f/std::tanh(drive);
        auto warm=[&](float s) noexcept {
            const float shaped=std::tanh((s+.07f*colour*s*std::abs(s))*drive)*normalizer;
            return finiteOrZero(s+(shaped-s)*colour);
        };
        return {warm(l),warm(r)};
    }
    float gain() const noexcept {return currentGain;}
private:
    static float finiteOrZero(float v) noexcept {return std::isfinite(v)?v:0.f;}
    static float clamp01(float v) noexcept {return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
    double sr=48000.;
    float envCoeff=.999f,gainAtt=.999f,gainRel=.999f;
    float envelope=0.f,currentGain=1.f;
    float strength=0.f,targetLevel=.125f,minGain=.5f,maxGain=2.f,colour=0.f;
};

class DuckingPingPongDelay {
public:
    void prepare(double sampleRate) {
        sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        // Long enough for quarter notes at 40 BPM; allocated only in prepare().
        const int count=(int)(sr*1.52)+8;
        memory.assign((size_t)count,{0.f,0.f});
        attack=std::exp(-1.f/(.005f*(float)sr));
        release=std::exp(-1.f/(.170f*(float)sr));
        reset();
    }
    void reset() noexcept {
        for(auto& pair:memory)pair={0.f,0.f};
        lowState={0.f,0.f};dcState={0.f,0.f};
        write=0;voiceEnv=0.f;previousTap=targetTap=1;fadeRemaining=0;
    }
    // Time: four musical divisions; feedback < .80; duck controls voice masking;
    // tone adjusts feedback damping. A stable 120-BPM fallback is used if host
    // tempo cannot be obtained. Dry/wet mixing stays in Skell's existing slot.
    void set(double bpm,float time,float feedback,float duck,float tone) noexcept {
        if(memory.size()<8)return;
        const double tempo=std::isfinite(bpm)?std::clamp(bpm,40.,260.):120.;
        const float t=clamp01(time);
        const double division=t<.25f?.25:(t<.5f?.5:(t<.75f?.75:1.));
        const int delay=(int)std::round((60./tempo)*division*sr);
        const int bounded=std::clamp(delay,1,(int)memory.size()-4);
        if(bounded!=targetTap) {
            previousTap=targetTap;
            targetTap=bounded;
            fadeRemaining=std::max(1,(int)(sr*.024));
        }
        fb=.79f*clamp01(feedback);
        duckAmount=clamp01(duck);
        const float lpHz=1100.f+6900.f*clamp01(tone);
        const float hpHz=130.f;
        const float nyquist=(float)sr*.42f;
        lowCoeff=std::exp(-2.f*3.14159265f*std::min(lpHz,nyquist)/(float)sr);
        dcCoeff=std::exp(-2.f*3.14159265f*hpHz/(float)sr);
    }
    std::array<float,2> process(float left,float right) noexcept {
        left=finiteOrZero(left);right=finiteOrZero(right);
        if(memory.size()<8)return {0.f,0.f};
        const float detector=std::max(std::abs(left),std::abs(right));
        voiceEnv=(detector>voiceEnv?attack:release)*voiceEnv+
            (1.f-(detector>voiceEnv?attack:release))*detector;
        if(!std::isfinite(voiceEnv)||voiceEnv<1.e-20f)voiceEnv=0.f;
        auto readAt=[&](int tap) noexcept {
            int rp=write-tap;
            if(rp<0)rp+=(int)memory.size();
            return memory[(size_t)rp];
        };
        const auto newWet=readAt(targetTap);
        auto wet=newWet;
        if(fadeRemaining>0) {
            const auto oldWet=readAt(previousTap);
            const float progress=1.f-(float)fadeRemaining/std::max(1.f,(float)(sr*.024));
            wet[0]=oldWet[0]+(newWet[0]-oldWet[0])*progress;
            wet[1]=oldWet[1]+(newWet[1]-oldWet[1])*progress;
            --fadeRemaining;
        }
        const float duckGain=1.f/(1.f+voiceEnv*(8.f*duckAmount));
        std::array<float,2> filtered{};
        for(int ch=0;ch<2;++ch){
            lowState[(size_t)ch]=lowCoeff*lowState[(size_t)ch]+
                                 (1.f-lowCoeff)*wet[(size_t)ch];
            dcState[(size_t)ch]=dcCoeff*dcState[(size_t)ch]+
                                (1.f-dcCoeff)*lowState[(size_t)ch];
            filtered[(size_t)ch]=lowState[(size_t)ch]-dcState[(size_t)ch];
            if(std::abs(lowState[(size_t)ch])<1e-20f)lowState[(size_t)ch]=0.f;
            if(std::abs(dcState[(size_t)ch])<1e-20f)dcState[(size_t)ch]=0.f;
        }
        // Cross-feedback creates a ping-pong spread; mono input also spreads.
        memory[(size_t)write]={finiteOrZero(left+filtered[1]*fb),
                               finiteOrZero(right*.4f+filtered[0]*fb)};
        if(++write==(int)memory.size())write=0;
        return {finiteOrZero(wet[0]*duckGain),finiteOrZero(wet[1]*duckGain)};
    }
    int delaySamples() const noexcept {return targetTap;}
private:
    static float finiteOrZero(float v) noexcept {return std::isfinite(v)?v:0.f;}
    static float clamp01(float v) noexcept {return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
    double sr=48000.;
    std::vector<std::array<float,2>> memory;
    std::array<float,2> lowState{},dcState{};
    int write=0,previousTap=1,targetTap=1,fadeRemaining=0;
    float attack=.995f,release=.999f,voiceEnv=0.f;
    float fb=.3f,duckAmount=.6f,lowCoeff=.85f,dcCoeff=.99f;
};


// Dedicated vibrato from the MIT-licensed Auralis/Voice-Changer colour-stage
// concept, adapted to the Skell stereo rack with guarded fractional reads.
class VocalVibrato {
public:
    void prepare(double sampleRate) {
        sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        memory.assign((size_t)(sr*.024)+8,{0.f,0.f});
        reset();
    }
    void reset() noexcept {
        for(auto& sample:memory)sample={0.f,0.f};
        write=0;phase=0.;
    }
    void set(float rate,float depth,float stereo,float shape) noexcept {
        speed=.3f+11.7f*clamp01(rate);
        depthSamples=(float)sr*.0035f*clamp01(depth);
        phaseSpread=3.14159265f*clamp01(stereo);
        shapeMix=clamp01(shape);
    }
    std::array<float,2> process(float left,float right) noexcept {
        if(memory.size()<8)return {left,right};
        memory[(size_t)write]={finiteOrZero(left),finiteOrZero(right)};
        auto read=[&](int channel,float angle) noexcept {
            // Angle is in [0, 3*pi] here. Piecewise triangle avoids the
            // extra asin(sin()) call per channel on the audio thread.
            float wrapped=angle;
            if(wrapped>=6.28318531f)wrapped-=6.28318531f;
            const float cycle=wrapped*0.63661977f;
            const float triangle=cycle<1.f?cycle:(cycle<3.f?2.f-cycle:cycle-4.f);
            const float shape=(shapeMix<.9999f?std::sin(angle)*(1.f-shapeMix):0.f)
                               +triangle*shapeMix;
            const float tap=(float)(sr*.008)+depthSamples*shape;
            float pos=(float)write-tap;
            if(pos<0.f)pos+=(float)memory.size();
            const int i0=(int)pos;
            const int i1=(i0+1==(int)memory.size())?0:i0+1;
            const float mix=pos-(float)i0;
            const float a=memory[(size_t)i0][(size_t)channel];
            const float b=memory[(size_t)i1][(size_t)channel];
            return a+(b-a)*mix;
        };
        const float a=(float)phase;
        const std::array<float,2> result{{read(0,a),read(1,a+phaseSpread)}};
        phase+=6.283185307179586*speed/sr;
        if(phase>=6.283185307179586)phase-=6.283185307179586;
        if(++write==(int)memory.size())write=0;
        return result;
    }
private:
    static float finiteOrZero(float v) noexcept {return std::isfinite(v)?v:0.f;}
    static float clamp01(float v) noexcept {return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
    double sr=48000.,phase=0.;
    std::vector<std::array<float,2>> memory;
    int write=0;
    float speed=5.f,depthSamples=0.f,phaseSpread=0.f,shapeMix=0.f;
};

} // namespace skellpreset::dsp
