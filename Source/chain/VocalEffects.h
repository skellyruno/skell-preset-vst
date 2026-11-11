#pragma once
// Additional zero-lookahead vocal inserts for Skell-Preset.
//
// Audio-mixer-main (MIT) supplied the idea of 3 independently compressed
// bands. Local-mic-effects-main supplied the effect selection and
// configurable radio/bitcrush/tremolo/width behavior. DSP implementations
// are written for Skell-Preset's existing block and per-slot architecture.
// No Electron, WebAudio, AudioWorklet, microphone I/O or heap operations run
// inside process(). Existing physical slot IDs and module indices are stable.
#include "BasicDSP.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skellpreset::dsp {

struct BroadcastRadio {
    double sr=48000.0;
    float highPole=.9f,lowStep=.2f,highTarget=.9f,lowTarget=.2f;
    float toneSlew=.001f,drive=1.f;
    std::array<std::array<float,2>,2> highStates{},lowStates{};
    SmoothedBiquad presence;
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(sampleRate,8000.,192000.);
        toneSlew=1.f-(float)std::exp(-1./(sr*.014));
        highStates={};lowStates={};
        presence.prepare(sr);
        set(320.f,3700.f,.35f,.0f);
        highPole=highTarget;lowStep=lowTarget;
    }
    void reset() noexcept {
        highStates={};lowStates={};
        presence.reset();
    }
    // Low/High are absolute frequencies, nasal and drive are normalized.
    void set(float lowHz,float highHz,float nasal,float grit) noexcept {
        const float low=clamp(lowHz,100.f,
                              std::min(2400.f,(float)sr*.24f));
        const float high=clamp(highHz,std::max(low*1.5f,350.f),
                               (float)sr*.44f);
        highTarget=(float)std::exp(-2.*pi*low/sr);
        lowTarget=1.f-(float)std::exp(-2.*pi*high/sr);
        presence.setPeak(sr,std::sqrt((double)low*high),clamp(nasal,0.f,1.f)*7.f,1.25);
        drive=1.f+3.f*clamp(grit,0.f,1.f);
    }
    std::array<float,2> process(float left,float right) noexcept {
        highPole+=(highTarget-highPole)*toneSlew;
        lowStep+=(lowTarget-lowStep)*toneSlew;
        const float input[2]{clamp(left,-8.f,8.f),clamp(right,-8.f,8.f)};
        float result[2]{};
        for(int c=0;c<2;++c) {
            float x=input[c];
            for(int k=0;k<2;++k){
                float& z=highStates[(size_t)k][(size_t)c];
                z=highPole*z+(1.f-highPole)*x; x-=z;
                if(std::abs(z)<1.e-20f)z=0.f;
            }
            for(int k=0;k<2;++k){
                float& z=lowStates[(size_t)k][(size_t)c];
                z+=(x-z)*lowStep; x=z;
                if(std::abs(z)<1.e-20f)z=0.f;
            }
            result[c]=x;
        }
        const auto boosted=presence.process(result[0],result[1]);
        const float norm=1.f/std::tanh(drive);
        return {std::tanh(boosted[0]*drive)*norm,
                std::tanh(boosted[1]*drive)*norm};
    }
};

struct RetroCrusher {
    double sr=48000.;
    float levels=128.f,step=1.f,phase=1.f,drive=1.f,dither=0.f;
    std::array<float,2> held{};
    uint32_t seed=0x9e3779b9u;
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(sampleRate,8000.,192000.);
        reset();set(11.f,(float)sr,0.f,0.f);
    }
    void reset() noexcept {held={};phase=1.f;seed=0x9e3779b9u;}
    void set(float bits,float rate,float driveAmount,float ditherAmount) noexcept {
        levels=std::exp2(clamp(bits,2.f,16.f)-1.f);
        step=clamp(rate/(float)sr,.00005f,1.f);
        drive=1.f+3.f*clamp(driveAmount,0.f,1.f);
        dither=clamp(ditherAmount,0.f,1.f)/levels;
    }
    float tpdf() noexcept {
        seed=1664525u*seed+1013904223u;
        const float a=(float)((seed>>16)&0xffffu)/65535.f;
        seed=1664525u*seed+1013904223u;
        const float b=(float)((seed>>16)&0xffffu)/65535.f;
        return (a-b)*dither;
    }
    std::array<float,2> process(float left,float right) noexcept {
        phase+=step;
        if(phase>=1.f){
            phase-=1.f;
            const float inputs[2]{clamp(left,-8.f,8.f),clamp(right,-8.f,8.f)};
            for(int ch=0;ch<2;++ch) {
                const float x=std::tanh(inputs[ch]*drive);
                const float noise=dither>0.f && std::abs(x)>1.e-9f?tpdf():0.f;
                held[(size_t)ch]=std::round((x+noise)*levels)/levels;
            }
        }
        return held;
    }
};

struct VocalTremolo {
    double sr=48000.,phase=0.0;
    float rate=3.f,depth=.5f,shape=0.f,stereo=.0f;
    void prepare(double sampleRate) noexcept {sr=std::clamp(sampleRate,8000.,192000.);phase=0.;}
    void reset() noexcept {phase=0.;}
    void set(float hz,float amount,float waveform,float spread) noexcept {
        rate=clamp(hz,.05f,25.f);depth=clamp(amount,0.f,1.f);
        shape=clamp(waveform,0.f,1.f);stereo=clamp(spread,0.f,1.f)*.5f;
    }
    std::array<float,2> process(float l,float r) noexcept {
        phase+=rate/sr;
        phase-=std::floor(phase);
        const float in[2]{clamp(l,-8.f,8.f),clamp(r,-8.f,8.f)};
        float out[2]{};
        for(int ch=0;ch<2;++ch) {
            const float t=(float)(phase+((ch==1)?stereo:0.));
            const float wrapped=t-std::floor(t);
            const float sinLfo=.5f-.5f*std::cos(2.f*(float)pi*wrapped);
            const float tri=1.f-std::abs(2.f*wrapped-1.f);
            const float wave=sinLfo*(1.f-shape)+tri*shape;
            out[ch]=in[ch]*(1.f-depth*wave);
        }
        return {out[0],out[1]};
    }
};

struct VocalWidth {
    double sr=48000.;
    float width=1.f,pan=0.f,bassAmt=0.f,bassPole=.99f;
    float currentPole=.99f,slew=.001f;
    float filteredSide=0.f;
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(sampleRate,8000.,192000.);
        slew=1.f-(float)std::exp(-1./(sr*.016));
        filteredSide=0.f;currentPole=bassPole;
    }
    void reset() noexcept {filteredSide=0.f;}
    void set(float widthAmount,float monoBassFrequency,float balance,float monoBass) noexcept {
        width=clamp(widthAmount,0.f,2.f);
        const float hz=clamp(monoBassFrequency,50.f,400.f);
        bassPole=(float)std::exp(-2.*pi*hz/sr);
        pan=clamp(balance,-1.f,1.f);
        bassAmt=clamp(monoBass,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float a=clamp(l,-8.f,8.f),b=clamp(r,-8.f,8.f);
        const float mid=.5f*(a+b);
        const float side=.5f*(a-b);
        currentPole+=(bassPole-currentPole)*slew;
        filteredSide=currentPole*filteredSide+(1.f-currentPole)*side;
        if(std::abs(filteredSide)<1.e-20f)filteredSide=0.f;
        const float enhanced=(side-filteredSide*bassAmt)*width;
        return {(mid+enhanced)*(pan>0?1.f-pan:1.f),
                (mid-enhanced)*(pan<0?1.f+pan:1.f)};
    }
};

// Adapted from the MIT-licensed audio-mixer's 3-band dynamics concept,
// using a complementary first-order zero-lookahead split. Splitting as
// low + mid + high = input prevents crossover cancellation at unity.
struct ThreeBandDynamics {
    double sr=48000.;
    float lowCoeff=.028f,highCoeff=.35f;
    std::array<float,2> lowState{},midState{};
    std::array<Compressor,3> dynamics;
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(sampleRate,8000.,192000.);
        lowCoeff=1.f-(float)std::exp(-2.*pi*240./sr);
        highCoeff=1.f-(float)std::exp(-2.*pi*3400./sr);
        lowState={};midState={};
        for(auto& c:dynamics)c.prepare(sr);
        set(0.f,0.f,0.f,.5f);
    }
    void reset() noexcept {
        lowState={};midState={};
        for(auto& c:dynamics)c.reset();
    }
    void set(float low,float mid,float high,float release) noexcept {
        const float values[3]{clamp(low,0.f,1.f),clamp(mid,0.f,1.f),clamp(high,0.f,1.f)};
        for(int i=0;i<3;++i){
            const float amount=values[i];
            dynamics[(size_t)i].set(-31.f+amount*7.f,
                          1.f+amount*6.f,3.5f,
                          30.f+clamp(release,0.f,1.f)*370.f,0.f);
        }
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float input[2]{clamp(l,-8.f,8.f),clamp(r,-8.f,8.f)};
        float bands[3][2]{};
        for(int c=0;c<2;++c){
            float& low=lowState[(size_t)c],&mid=midState[(size_t)c];
            low+=(input[c]-low)*lowCoeff;
            const float rest=input[c]-low;
            mid+=(rest-mid)*highCoeff;
            bands[0][c]=low;
            bands[1][c]=mid;
            bands[2][c]=rest-mid;
            if(std::abs(low)<1.e-20f)low=0.f;
            if(std::abs(mid)<1.e-20f)mid=0.f;
        }
        float out[2]{};
        for(int i=0;i<3;++i){
            const float detector=std::max(std::abs(bands[i][0]),std::abs(bands[i][1]));
            const float gain=dynamics[(size_t)i].gain(detector);
            out[0]+=bands[i][0]*gain;
            out[1]+=bands[i][1]*gain;
        }
        return {std::isfinite(out[0])?out[0]:0.f,
                std::isfinite(out[1])?out[1]:0.f};
    }
};
} // namespace skellpreset::dsp
