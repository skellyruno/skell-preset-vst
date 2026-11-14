#pragma once
// Skelldizer original three-band coloration. No third-party code/presets.
// All DSP runs in constant memory, no allocations, locks or lookahead.
#include <algorithm>
#include <array>
#include <cmath>
namespace skellpreset::dsp {
class Skelldizer final {
public:
    static int characterIndex(float v) noexcept {
        const float x=std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;
        return std::clamp((int)std::lround(x*3.f),0,3);
    }
    static const char* characterName(int i) noexcept {
        constexpr const char* names[]={"Punch","Warm","Air","Grit"};
        return names[std::clamp(i,0,3)];
    }
    void prepare(double sampleRate) noexcept {
        const float sr=(float)std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        constexpr float pi=3.14159265358979323846f;
        lowPole=std::exp(-2.f*pi*220.f/sr);
        highPole=std::exp(-2.f*pi*3400.f/sr);
        attack=std::exp(-1.f/(sr*.012f));
        release=std::exp(-1.f/(sr*.150f));
        levelPole=std::exp(-1.f/(sr*.220f));
        controlPole=std::exp(-1.f/(sr*.012f));
        reset();
    }
    void reset() noexcept {
        lowState.fill(0.f); highState.fill(0.f); bandPower.fill(0.f);
        inputPower=outputPower=0.f;
        amount=0.f;trim=1.f;match=0.f;
        weights={1.f,1.f,1.f};drive=.25f;compression=.28f;
    }
    void set(float strength,float character,float output,float levelMatch) noexcept {
        desiredAmount=unit(strength);
        mode=characterIndex(character);
        desiredTrim=std::pow(10.f,12.f*(unit(output)-.5f)/20.f);
        desiredMatch=unit(levelMatch)>=.5f?1.f:0.f;
    }
    std::array<float,2> process(float left,float right) noexcept {
        if(!std::isfinite(left))left=0.f;
        if(!std::isfinite(right))right=0.f;
        if(desiredAmount==0.f && amount<1.e-6f)return {{left,right}};
        amount=advance(amount,desiredAmount,controlPole);
        trim=advance(trim,desiredTrim,controlPole);
        match=advance(match,desiredMatch,controlPole);
        constexpr float colors[4][5]={
            {1.06f,1.02f,.96f,.25f,.34f},
            {1.19f,1.03f,.77f,.29f,.28f},
            {.82f,1.02f,1.23f,.23f,.25f},
            {1.03f,1.09f,1.03f,.51f,.45f}
        };
        for(int b=0;b<3;++b)weights[(size_t)b]=
            advance(weights[(size_t)b],colors[mode][b],controlPole);
        drive=advance(drive,colors[mode][3],controlPole);
        compression=advance(compression,colors[mode][4],controlPole);
        const std::array<float,2> input{{left,right}};
        float bands[2][3]{};
        for(int ch=0;ch<2;++ch) {
            const float x=input[(size_t)ch];
            lowState[(size_t)ch]=advance(lowState[(size_t)ch],x,lowPole);
            highState[(size_t)ch]=advance(highState[(size_t)ch],x,highPole);
            bands[ch][0]=lowState[(size_t)ch];
            bands[ch][1]=highState[(size_t)ch]-lowState[(size_t)ch];
            bands[ch][2]=x-highState[(size_t)ch];
        }
        constexpr float thresholds[3]={.035f,.025f,.017f};
        float bandGain[3]{};
        for(int b=0;b<3;++b) {
            const float power=.5f*(bands[0][b]*bands[0][b]+bands[1][b]*bands[1][b]);
            bandPower[(size_t)b]=advance(bandPower[(size_t)b],power,
                                         power>bandPower[(size_t)b]?attack:release);
            const float excess=std::max(0.f,
                std::sqrt(std::max(0.f,bandPower[(size_t)b]))/thresholds[b]-1.f);
            bandGain[b]=1.f/(1.f+compression*excess/(1.f+.6f*excess));
        }
        std::array<float,2> wet{};
        for(int ch=0;ch<2;++ch) {
            for(int b=0;b<3;++b) {
                const float x=bands[ch][b];
                const float d=drive*(b==0?.5f:b==1?1.f:1.3f);
                wet[(size_t)ch]+=x*(1.f+.16f*d)/(1.f+d*std::abs(x))
                                  *weights[(size_t)b]*bandGain[b];
            }
        }
        inputPower=advance(inputPower,.5f*(left*left+right*right),levelPole);
        outputPower=advance(outputPower,.5f*(wet[0]*wet[0]+wet[1]*wet[1]),levelPole);
        const float matched=std::clamp(
            std::sqrt((inputPower+1.e-8f)/(outputPower+1.e-8f)),.72f,1.25f);
        const float level=trim*(1.f+match*(matched-1.f));
        std::array<float,2> result{};
        for(int ch=0;ch<2;++ch) {
            const float dry=input[(size_t)ch];
            const float x=dry+(wet[(size_t)ch]*level-dry)*amount;
            // Sample peak safety; not a true-peak or LUFS certification.
            const float magnitude=std::abs(x);
            const float ceiling=magnitude<=.945f?magnitude:
                .945f+.04f*std::tanh((magnitude-.945f)/.04f);
            result[(size_t)ch]=std::isfinite(x)?std::copysign(ceiling,x):0.f;
        }
        return result;
    }
private:
    static float unit(float v) noexcept {return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
    static float advance(float prior,float desired,float pole) noexcept {
        const float result=prior*pole+(1.f-pole)*desired;
        return std::abs(result)<1.e-20f?0.f:result;
    }
    float lowPole=0.f,highPole=0.f,attack=0.f,release=0.f,levelPole=0.f,controlPole=0.f;
    std::array<float,2> lowState{},highState{};
    std::array<float,3> bandPower{},weights{{1.f,1.f,1.f}};
    float inputPower=0.f,outputPower=0.f,amount=0.f,trim=1.f,match=0.f;
    float desiredAmount=0.f,desiredTrim=1.f,desiredMatch=0.f;
    float drive=.25f,compression=.28f;
    int mode=0;
};
}
