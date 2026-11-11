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
    float smRate=.7f,smDepth=5.f,smVoices=3.f,smSpread=.55f,glide=.001f;
    std::array<modulation_detail::FractionalLine,2> line;
    void prepare(double sampleRate) {
        sr=std::clamp(sampleRate,8000.,192000.);
        glide=1.f-(float)std::exp(-1./(sr*.016));
        for(auto& x:line)x.prepare(sr,55.f);
        reset();
    }
    void reset() noexcept {
        phase=0.;for(auto& x:line)x.reset();
        smRate=rate;smDepth=depthMs;smVoices=voices;smSpread=spread;
    }
    void set(float hz,float depth,float voiceCount,float stereo) noexcept {
        rate=modulation_detail::bound(hz,.05f,7.f);
        depthMs=modulation_detail::bound(depth,0.f,12.f);
        voices=modulation_detail::bound(voiceCount,2.f,5.f);
        spread=modulation_detail::bound(stereo,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        // Continuous voice weighting means automating 2 -> 5 voices no
        // longer adds/removes a full-strength delayed copy in one sample.
        smRate+=(rate-smRate)*glide;
        smDepth+=(depthMs-smDepth)*glide;
        smVoices+=(voices-smVoices)*glide;
        smSpread+=(spread-smSpread)*glide;
        const float input[]{modulation_detail::safe(l),modulation_detail::safe(r)};
        const float base=(float)(sr*.001*16.f);
        const float depth=(float)(sr*.001)*smDepth;
        std::array<float,2> result{};
        for(int c=0;c<2;++c){
            float mixed=0.f,sumWeight=0.f;
            for(int voice=0;voice<5;++voice){
                const float weight=std::clamp(smVoices-(float)voice,0.f,1.f);
                if(weight<.000001f)continue;
                const double offset=(double)voice/5.+
                    (c==1?(.17*smSpread):0.);
                const float mod=std::sin((float)(modulation_detail::tau*(phase+offset)));
                mixed+=weight*line[(size_t)c].read(base+depth*mod);
                sumWeight+=weight;
            }
            result[(size_t)c]=mixed/std::max(1.f,sumWeight);
            line[(size_t)c].write(input[c]);
        }
        phase+=smRate/sr;phase-=std::floor(phase);
        return result;
    }
};

struct VocalFlanger {
    double sr=48000.,phase=0.;
    float rate=.3f,depthMs=4.f,feedback=.3f,spread=.4f;
    float smRate=.3f,smDepth=4.f,smFeedback=.3f,smSpread=.4f,glide=.001f;
    std::array<float,2> feedbackMemory{};
    std::array<modulation_detail::FractionalLine,2> line;
    void prepare(double sampleRate){
        sr=std::clamp(sampleRate,8000.,192000.);
        glide=1.f-(float)std::exp(-1./(sr*.016));
        for(auto& x:line)x.prepare(sr,24.f);
        reset();
    }
    void reset() noexcept {
        phase=0.;feedbackMemory={};for(auto& x:line)x.reset();
        smRate=rate;smDepth=depthMs;smFeedback=feedback;smSpread=spread;
    }
    void set(float hz,float depth,float feedbackAmount,float stereo) noexcept {
        rate=modulation_detail::bound(hz,.05f,8.f);
        depthMs=modulation_detail::bound(depth,0.f,10.f);
        feedback=modulation_detail::bound(feedbackAmount,-.85f,.85f);
        spread=modulation_detail::bound(stereo,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        smRate+=(rate-smRate)*glide;
        smDepth+=(depthMs-smDepth)*glide;
        smFeedback+=(feedback-smFeedback)*glide;
        smSpread+=(spread-smSpread)*glide;
        const float input[]{modulation_detail::safe(l),modulation_detail::safe(r)};
        std::array<float,2> wet{};
        for(int c=0;c<2;++c){
            const double offset=c==1?.25*smSpread:0.;
            const float mod=.5f+.5f*std::sin((float)(modulation_detail::tau*(phase+offset)));
            const float delay=(float)(sr*.001)*(.5f+smDepth*mod);
            wet[(size_t)c]=line[(size_t)c].read(delay);
            const float next=input[c]+feedbackMemory[(size_t)c]*smFeedback;
            line[(size_t)c].write(next);
            feedbackMemory[(size_t)c]=modulation_detail::safe(wet[(size_t)c]);
        }
        phase+=smRate/sr;phase-=std::floor(phase);
        return wet;
    }
};

struct VocalRingMod {
    double sr=48000.,phase=0.;
    float freq=80.f,depth=.75f,shape=0.f,spread=0.f;
    float smFreq=80.f,smDepth=.75f,smShape=0.f,smSpread=0.f,glide=.001f;
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(sampleRate,8000.,192000.);
        glide=1.f-(float)std::exp(-1./(sr*.012));
        reset();
    }
    void reset() noexcept {
        phase=0.;
        smFreq=freq;smDepth=depth;smShape=shape;smSpread=spread;
    }
    void set(float hz,float amount,float shapeAmount,float stereo) noexcept {
        freq=modulation_detail::bound(hz,.05f,1600.f);
        depth=modulation_detail::bound(amount,0.f,1.f);
        shape=modulation_detail::bound(shapeAmount,0.f,1.f);
        spread=modulation_detail::bound(stereo,0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        smFreq+=(freq-smFreq)*glide;
        smDepth+=(depth-smDepth)*glide;
        smShape+=(shape-smShape)*glide;
        smSpread+=(spread-smSpread)*glide;
        const float a=modulation_detail::wave(phase,smShape);
        const float b=modulation_detail::wave(phase+smSpread*.5,smShape);
        const float output[]{modulation_detail::safe(l)*(1.f-smDepth+smDepth*a),
                             modulation_detail::safe(r)*(1.f-smDepth+smDepth*b)};
        phase+=smFreq/sr;phase-=std::floor(phase);
        return {output[0],output[1]};
    }
};
} // namespace skellpreset::dsp
