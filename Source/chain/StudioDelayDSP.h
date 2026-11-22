#pragma once
// Opt-in Skell Studio-style stereo echo. JUCE-free C++ adaptation of the
// Studio Delay control concepts; not bit-identical to Lemon/FL Delay 3.
// Historical Skell Preset Delay instances/automation are unaffected.
// All buffers allocate only in prepare(); process() is allocation-free.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace skellpreset::studio {
class StereoDelay final {
public:
    void prepare(double sampleRate) {
        sr=std::clamp(sampleRate,8000.,384000.);
        const auto capacity=static_cast<std::size_t>(std::ceil(sr*6.2))+8;
        for(auto& line:history)line.assign(capacity,0.f);
        const auto diffuserSize=static_cast<std::size_t>(std::ceil(sr*.013))+2;
        for(auto& line:diffuser)line.assign(diffuserSize,0.f);
        reset();
    }
    void reset() noexcept {
        for(auto& line:history)std::fill(line.begin(),line.end(),0.f);
        for(auto& line:diffuser)std::fill(line.begin(),line.end(),0.f);
        filterLow.fill(0.f);filterHigh.fill(0.f);
        write=diffWrite=0;phase=0.;
        delayL=delayR=targetL=targetR=sr*.25;
        // The first callback must honor the selected time immediately.
        // Subsequent automation ramps remain pitch-continuous over 25 ms.
        configured=false;
    }
    // p[0..15] are normalized host controls, read once per callback.
    void set(const std::array<float,16>& p,double bpm) noexcept {
        auto get=[&](int i){return std::isfinite(p[static_cast<std::size_t>(i)])
            ?std::clamp(p[static_cast<std::size_t>(i)],0.f,1.f):0.f;};
        double time=0.025*std::pow(80.,get(0));
        if(get(12)>=.5f) {
            constexpr std::array<double,8> beats{{4.,2.,1.,.5,.25,.125,.0625,.03125}};
            const int index=std::clamp(static_cast<int>(get(13)*7.999f),0,7);
            time=(60./std::clamp(std::isfinite(bpm)?bpm:120.,40.,260.))
                *beats[static_cast<std::size_t>(index)];
        }
        const double offset=.05*get(3);
        targetL=std::clamp(time*sr,1.,sr*6.10);
        targetR=std::clamp((time+offset)*sr,1.,sr*6.15);
        if(!configured) {
            delayL=targetL;delayR=targetR;
            configured=true;
        }
        feedback=.94f*get(1);
        mode=std::clamp(static_cast<int>(get(2)*3.999f),0,3);
        const double lowHz=12.+1600.*get(4)*get(4);
        const double highHz=400.*std::pow(45.,get(5));
        const double lp=std::exp(-2.*3.141592653589793*std::min(highHz,.45*sr)/sr);
        const double hp=std::exp(-2.*3.141592653589793*lowHz/sr);
        lowPole=static_cast<float>(lp);
        highPole=static_cast<float>(hp);
        drive=1.f+7.f*get(6);
        saturation=get(7);
        lfoRate=.05+5.*get(8);
        modDepth=sr*.012*get(9);
        wet=get(10);
        dry=get(11);
        tone=get(14);
        diffusion=.65f*get(15);
        glide=1.-std::exp(-1./(sr*.025));
        shapeDenom=std::max(.001f,std::tanh(drive));
    }
    std::array<float,2> process(float left,float right,float echoDuck=1.f) noexcept {
        if(history[0].empty()||diffuser[0].empty())return {left,right};
        const float l=std::isfinite(left)?std::clamp(left,-8.f,8.f):0.f;
        const float r=std::isfinite(right)?std::clamp(right,-8.f,8.f):0.f;
        delayL+=(targetL-delayL)*glide;
        delayR+=(targetR-delayR)*glide;
        phase+=2.*3.141592653589793*lfoRate/sr;
        if(phase>=2.*3.141592653589793)phase-=2.*3.141592653589793;
        // Avoid per-sample trig in the default zero-modulation mode.
        const double sway=modDepth>0.?std::sin(phase)*modDepth:0.;
        float echo[2]{read(0,delayL+sway),read(1,delayR-sway)};
        if(mode==0) {
            const float mono=.5f*(echo[0]+echo[1]);
            echo[0]=echo[1]=mono;
        }
        const float input[2]{mode==0?.5f*(l+r):l,mode==0?.5f*(l+r):r};
        for(int ch=0;ch<2;++ch) {
            const float source=echo[mode==2?1-ch:ch];
            auto& low=filterLow[static_cast<std::size_t>(ch)];
            auto& high=filterHigh[static_cast<std::size_t>(ch)];
            low=(1.f-lowPole)*source+lowPole*low;
            high=(1.f-highPole)*low+highPole*high;
            const float filtered=low-high;
            const float coloured=filtered*(1.f-.45f*tone);
            // No nonlinear DSP when saturation is OFF. Preserve the exact
            // original linear output while saving two tanh calls per frame.
            float nonlinear=coloured;
            if(saturation>0.f) {
                const float shaped=std::tanh(coloured*drive)/shapeDenom;
                nonlinear+=(shaped-coloured)*saturation;
            }
            auto& allpass=diffuser[static_cast<std::size_t>(ch)];
            const float delayed=allpass[diffWrite];
            const float diffused=delayed-diffusion*nonlinear;
            allpass[diffWrite]=std::clamp(nonlinear+diffusion*diffused,-8.f,8.f);
            const float next=input[ch]+feedback*(nonlinear*(1.f-diffusion)+diffused*diffusion);
            history[static_cast<std::size_t>(ch)][write]=std::clamp(next,-8.f,8.f);
        }
        write=(write+1)%history[0].size();
        diffWrite=(diffWrite+1)%diffuser[0].size();
        const float echoGain=(mode==3?0.f:wet)*
            (std::isfinite(echoDuck)?std::clamp(echoDuck,0.f,1.f):1.f);
        return {std::clamp(dry*l+echoGain*echo[0],-8.f,8.f),
                std::clamp(dry*r+echoGain*echo[1],-8.f,8.f)};
    }
private:
    float read(int ch,double samples) const noexcept {
        const auto& line=history[static_cast<std::size_t>(ch)];
        const double delay=std::clamp(samples,1.,static_cast<double>(line.size()-3));
        const auto integral=static_cast<std::size_t>(delay);
        const auto frac=static_cast<float>(delay-static_cast<double>(integral));
        const auto a=(write+line.size()-integral)%line.size();
        const auto b=(a+line.size()-1)%line.size();
        return line[a]+frac*(line[b]-line[a]);
    }
    double sr=48000.,targetL=12000.,targetR=12000.,delayL=12000.,delayR=12000.;
    double phase=0.,lfoRate=.25,modDepth=0.,glide=.001;
    std::array<std::vector<float>,2> history,diffuser;
    std::array<float,2> filterLow{},filterHigh{};
    std::size_t write=0,diffWrite=0;
    float feedback=.25f,lowPole=.5f,highPole=.99f,drive=1.f,shapeDenom=.76f;
    float saturation=0.f,wet=.4f,dry=1.f,tone=.5f,diffusion=.1f;
    int mode=1;
    bool configured=false; // set() initializes delay time only once per prepare/reset
};
} // namespace skellpreset::studio
