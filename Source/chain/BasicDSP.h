#pragma once
// Portable, allocation-free-in-process DSP primitives. Shared by realtime chain and MATCH tests.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <cstdint>

namespace skellpreset::dsp {
constexpr double pi = 3.14159265358979323846;
inline float clamp(float v, float lo, float hi) noexcept { return std::max(lo, std::min(hi, std::isfinite(v)?v:0.f)); }
inline float dbToGain(float dB) noexcept { return std::pow(10.f, dB/20.f); }
inline float gainToDb(float x) noexcept { return 20.f*std::log10(std::max(x,1.e-9f)); }

struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    std::array<double,2> z1{},z2{};
    void reset() noexcept { z1.fill(0);z2.fill(0); }
    void setPeak(double sampleRate, double frequency, double gainDb, double q) noexcept {
        const auto f=std::clamp(frequency, 15., sampleRate*.46);
        const auto w=2*pi*f/sampleRate, a=std::sin(w)/(2*std::clamp(q,.2,18.));
        const auto A=std::pow(10.,std::clamp(gainDb,-24.,24.)/40.);
        const auto den=1+a/A;
        b0=(1+a*A)/den; b1=(-2*std::cos(w))/den; b2=(1-a*A)/den;
        a1=b1; a2=(1-a/A)/den;
    }
    void setHighPass(double sampleRate, double frequency, double q=.7071068) noexcept {
        const auto w=2*pi*std::clamp(frequency,15.,sampleRate*.46)/sampleRate;
        const auto a=std::sin(w)/(2*q), c=std::cos(w), norm=1/(1+a);
        b0=(1+c)*.5*norm;b1=-(1+c)*norm;b2=b0;a1=-2*c*norm;a2=(1-a)*norm;
    }
    float process(float s,int channel) noexcept {
        auto x=std::isfinite(s)?static_cast<double>(s):0.;
        const auto y=b0*x+z1[(size_t)channel];
        z1[(size_t)channel]=b1*x-a1*y+z2[(size_t)channel];
        z2[(size_t)channel]=b2*x-a2*y;
        if(std::abs(z1[(size_t)channel])<1e-25) z1[(size_t)channel]=0;
        if(std::abs(z2[(size_t)channel])<1e-25) z2[(size_t)channel]=0;
        return std::isfinite(y)?static_cast<float>(y):0.f;
    }
};

struct Compressor {
    double sampleRate=48000, envelope=0;
    float threshold=-18,ratio=3,attackMs=12,releaseMs=90,makeupDb=0;
    void prepare(double sr) noexcept {sampleRate=std::max(8000.,sr); reset();}
    void reset() noexcept {envelope=0;}
    void set(float thr,float r,float attack,float release,float makeup) noexcept {
        threshold=clamp(thr,-60,0); ratio=clamp(r,1,20);attackMs=clamp(attack,.2f,200);releaseMs=clamp(release,5,2000);makeupDb=clamp(makeup,-24,24);
    }
    float gain(float detector) noexcept {
        const auto val=std::max(0.f,std::abs(detector));
        const auto a=std::exp(-1./(sampleRate*.001*(val>envelope?attackMs:releaseMs)));
        envelope=a*envelope+(1-a)*val;
        const auto lvl=gainToDb(static_cast<float>(envelope));
        const auto gr=lvl>threshold?(lvl-threshold)*(1-1/ratio):0.f;
        return dbToGain(makeupDb-gr);
    }
};

struct Gate {
    double sr=48000,envelope=0,gain=1;
    float threshold=-45.f,release=90.f;
    void prepare(double rate) noexcept {sr=std::max(8000.,rate);envelope=0;gain=1;}
    float processGain(float detector) noexcept {
        const auto a=std::exp(-1./(sr*.004));
        envelope=a*envelope+(1-a)*std::abs(detector);
        const float goal=gainToDb((float)envelope)>=threshold?1.f:0.f;
        const auto smooth=std::exp(-1./(sr*.001*(goal>gain?4.:std::max(20.f,release))));
        gain=smooth*gain+(1-smooth)*goal;return (float)gain;
    }
};

struct Delay {
    std::array<std::vector<float>,2> memory;
    size_t pos=0;
    int samples=4000;
    int previousSamples=4000;
    float feedback=.25f;
    float transition=1.f;
    float transitionStep=.001f;
    void prepare(double sampleRate,float maxSeconds=2.0f) {
        auto capacity=std::max<size_t>(32,(size_t)(sampleRate*maxSeconds)+2);
        for(auto &x:memory)x.assign(capacity,0);
        pos=0; samples=previousSamples=std::min<int>(samples,(int)capacity-1);
        transition=1.f;
        transitionStep=1.f/std::max(1.f,(float)(sampleRate*.020));
    }
    void reset() noexcept {
        for(auto &v:memory)std::fill(v.begin(),v.end(),0.f);
        pos=0;transition=1.f;previousSamples=samples;
    }
    void configure(int delaySamples,float fb) noexcept {
        if(memory[0].empty())return;
        const int target=std::clamp(delaySamples,1,(int)memory[0].size()-1);
        if(target!=samples) {
            // A new time change starts at the current dominant tap. Crossfade
            // the read heads so automation does not cause an instantaneous jump.
            previousSamples=(transition<.5f)?previousSamples:samples;
            samples=target;
            transition=0.f;
        }
        feedback=clamp(fb,0,.9f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        if(memory[0].empty())return {l,r};
        const auto cap=memory[0].size();
        const auto read=(pos+cap-(size_t)samples)%cap;
        const auto oldRead=(pos+cap-(size_t)previousSamples)%cap;
        const float blend=std::clamp(transition,0.f,1.f);
        const float dl=memory[0][oldRead]*(1.f-blend)+memory[0][read]*blend;
        const float dr=memory[1][oldRead]*(1.f-blend)+memory[1][read]*blend;
        memory[0][pos]=l+dl*feedback;memory[1][pos]=r+dr*feedback;
        if(++pos==cap)pos=0;
        transition=std::min(1.f,transition+transitionStep);
        return {dl,dr};
    }
};

inline float softLimit(float x,float ceiling=0.96f) noexcept {
    // Zero-lookahead saturation limiter, NOT a true-peak limiter.
    return ceiling*std::tanh(x/ceiling);
}


// Preallocated PDC line: all dry/wet alignment runs without reallocations or locks.
struct StereoSampleDelay {
    std::array<std::vector<float>,2> channel;
    size_t head=0;
    void prepare(int maxDelay) {
        const size_t capacity=(size_t)std::max(1,maxDelay)+2;
        for(auto& line:channel)line.assign(capacity,0.f);
        head=0;
    }
    void reset() noexcept {for(auto& line:channel)std::fill(line.begin(),line.end(),0.f);head=0;}
    std::array<float,2> process(float l,float r,int delay) noexcept {
        if(channel[0].empty())return {l,r};
        const auto capacity=channel[0].size();
        const auto d=(size_t)std::clamp(delay,0,(int)capacity-2);
        channel[0][head]=std::isfinite(l)?l:0.f;
        channel[1][head]=std::isfinite(r)?r:0.f;
        const size_t read=(head+capacity-d)%capacity;
        const std::array<float,2> output{channel[0][read],channel[1][read]};
        head=(head+1)%capacity;
        return output;
    }
};

// A modest decorrelated stereo double for isolated adlibs. It is NOT a true pitch shifter.
struct AdlibDoubler {
    std::array<std::vector<float>,2> memory;
    size_t head=0;
    double rate=48000,phase=0;
    void prepare(double sr) {
        rate=std::max(8000.,sr);head=0;phase=0;
        for(auto& v:memory)v.assign((size_t)(rate*.09)+4,0.f);
    }
    void reset() noexcept {for(auto& v:memory)std::fill(v.begin(),v.end(),0.f);head=0;phase=0;}
    std::array<float,2> process(float l,float r,float width,float depth,float rateControl,float intensity) noexcept {
        if(memory[0].empty())return {l,r};
        const float baseMs=12.f+24.f*std::clamp(depth,0.f,1.f);
        const float modMs=(1.f+4.f*std::clamp(width,0.f,1.f))*(.3f+1.4f*std::clamp(intensity,0.f,1.f));
        phase+=2.*pi*(.13+2.*std::clamp((double)rateControl,0.,1.))/rate;
        if(phase>=2*pi)phase-=2*pi;
        memory[0][head]=l;memory[1][head]=r;
        std::array<float,2> out{};
        for(int c=0;c<2;++c) {
            const double delay=rate*.001*(baseMs+modMs*std::sin(phase+(c==0?0.:pi*.78)));
            const auto whole=(size_t)delay;
            const float alpha=(float)(delay-whole);
            const auto cap=memory[c].size();
            const float a=memory[c][(head+cap-whole)%cap];
            const float b=memory[c][(head+cap-whole-1)%cap];
            out[(size_t)c]=a+(b-a)*alpha;
        }
        head=(head+1)%memory[0].size();
        return out;
    }
};

// PDC policy for this version: limiter types reserve lookahead even when bypassed.
// This makes each slot's bypass phase-aligned with its active processing path.
inline int seriesLatencySamples(const int* slotTypes,int count,int limiterType,int limiterLatency) noexcept {
    int total=0;
    for(int i=0;i<count;++i)if(slotTypes[i]==limiterType)total+=std::max(0,limiterLatency);
    return total;
}
}
