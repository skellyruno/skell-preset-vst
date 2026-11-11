#pragma once
// Original, zero-PDC stereo modulation effects, independently implemented
// after reviewing GPL-3.0 VML. No GPL implementation is copied.
// Each effect allocates its delay buffers in prepare(), never in process().
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace skellpreset::dsp {
namespace modulation_detail {
constexpr double tau = 6.28318530717958647692;
inline float safe(float v) noexcept {return std::isfinite(v)?std::clamp(v,-8.f,8.f):0.f;}
inline float bound(float v,float a,float b) noexcept {return std::clamp(std::isfinite(v)?v:a,a,b);}
inline float wave(double phase,float shape) noexcept {
    phase-=std::floor(phase);
    const float sine=std::sin((float)(phase*tau));
    const float triangle=1.f-4.f*std::abs((float)phase-.5f);
    return sine*(1.f-shape)+triangle*shape;
}
struct FractionalLine {
    std::vector<float> data;
    size_t head=0;
    void prepare(double sr,float maxMs) {
        data.assign((size_t)std::ceil(sr*.001*maxMs)+8,0.f);head=0;
    }
    void reset() noexcept {std::fill(data.begin(),data.end(),0.f);head=0;}
    float read(float samples) const noexcept {
        if(data.empty())return 0.f;
        const float d=bound(samples,1.f,(float)data.size()-3.f);
        const int n=(int)d;
        const float f=d-(float)n;
        const size_t len=data.size();
        const float s0=data[(head+len-(size_t)n)%len];
        const float s1=data[(head+len-(size_t)(n+1))%len];
        return s0+(s1-s0)*f;
    }
    void write(float value) noexcept {
        if(data.empty())return;
        data[head]=safe(value);
        if(++head>=data.size())head=0;
    }
};
}

struct VocalChorus {
    double sr=48000.,phase=0.;
    float rate=.7f,depthMs=5.f,voices=3.f,spread=.55f;
    std::array<modulation_detail::FractionalLine,2> line;
    void prepare(double sampleRate) {
        sr=std::clamp(sampleRate,8000.,192000.);
        for(auto& x:line)x.prepare(sr,55.f);
        reset();
    }
    void reset() noexcept {phase=0.;for(auto& x:line)x.reset();}
    void set(float hz,float depth,float voiceCount,float stereo) noexcept {
        rate=modulation_detail::bound(hz,.05f,7.f);
        depthMs=modulation_detail::bound(depth,0.f,12.f);
        voices=modulation_detail::bound(voiceCount,2.f,5.f);
        spread=modulation_detail::bound(stereo,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float input[]{modulation_detail::safe(l),modulation_detail::safe(r)};
        const int count=(int)std::round(voices);
        const float base=(float)(sr*.001*16.f);
        const float depth=(float)(sr*.001)*depthMs;
        std::array<float,2> result{};
        for(int c=0;c<2;++c){
            float mixed=0.f;
            for(int voice=0;voice<count;++voice){
                const double offset=(double)voice/count+
                    (c==1?(.17*spread):0.);
                const float mod=std::sin((float)(modulation_detail::tau*(phase+offset)));
                mixed+=line[(size_t)c].read(base+depth*mod);
            }
            result[(size_t)c]=mixed/count;
            line[(size_t)c].write(input[c]);
        }
        phase+=rate/sr;phase-=std::floor(phase);
        return result;
    }
};

struct VocalFlanger {
    double sr=48000.,phase=0.;
    float rate=.3f,depthMs=4.f,feedback=.3f,spread=.4f;
    std::array<float,2> feedbackMemory{};
    std::array<modulation_detail::FractionalLine,2> line;
    void prepare(double sampleRate){
        sr=std::clamp(sampleRate,8000.,192000.);
        for(auto& x:line)x.prepare(sr,24.f);
        reset();
    }
    void reset() noexcept {phase=0.;feedbackMemory={};for(auto& x:line)x.reset();}
    void set(float hz,float depth,float feedbackAmount,float stereo) noexcept {
        rate=modulation_detail::bound(hz,.05f,8.f);
        depthMs=modulation_detail::bound(depth,0.f,10.f);
        feedback=modulation_detail::bound(feedbackAmount,-.85f,.85f);
        spread=modulation_detail::bound(stereo,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float input[]{modulation_detail::safe(l),modulation_detail::safe(r)};
        std::array<float,2> wet{};
        for(int c=0;c<2;++c){
            const double offset=c==1?.25*spread:0.;
            const float mod=.5f+.5f*std::sin((float)(modulation_detail::tau*(phase+offset)));
            const float delay=(float)(sr*.001)*(.5f+depthMs*mod);
            wet[(size_t)c]=line[(size_t)c].read(delay);
            const float next=input[c]+feedbackMemory[(size_t)c]*feedback;
            line[(size_t)c].write(next);
            feedbackMemory[(size_t)c]=modulation_detail::safe(wet[(size_t)c]);
        }
        phase+=rate/sr;phase-=std::floor(phase);
        return wet;
    }
};

struct VocalRingMod {
    double sr=48000.,phase=0.;
    float freq=80.f,depth=.75f,shape=0.f,spread=0.f;
    void prepare(double sampleRate) noexcept {sr=std::clamp(sampleRate,8000.,192000.);phase=0.;}
    void reset() noexcept {phase=0.;}
    void set(float hz,float amount,float shapeAmount,float stereo) noexcept {
        freq=modulation_detail::bound(hz,.05f,1600.f);
        depth=modulation_detail::bound(amount,0.f,1.f);
        shape=modulation_detail::bound(shapeAmount,0.f,1.f);
        spread=modulation_detail::bound(stereo,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float a=modulation_detail::wave(phase,shape);
        const float b=modulation_detail::wave(phase+spread*.5,shape);
        const float output[]{modulation_detail::safe(l)*(1.f-depth+depth*a),
                             modulation_detail::safe(r)*(1.f-depth+depth*b)};
        phase+=freq/sr;phase-=std::floor(phase);
        return {output[0],output[1]};
    }
};
} // namespace skellpreset::dsp
