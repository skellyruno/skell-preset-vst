#pragma once
// Native causal phase rotator: one editable second-order all-pass section.
// No lookahead, allocation, target audio, or extra reported host latency.
#include "BasicDSP.h"
#include <limits>
namespace skellpreset::dsp {
class PhaseRotation {
public:
    static double frequency(float n) noexcept {return 40.*std::pow(350.,clamp(n,0.f,1.f));}
    static double resonance(float n) noexcept {return .25*std::pow(32.,clamp(n,0.f,1.f));}
    static float trimDb(float n) noexcept {return (clamp(n,0.f,1.f)-.5f)*12.f;}
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        filter.prepare(sr);ramp=std::max(1,(int)(sr*.012));reset();
    }
    void reset() noexcept {filter.current.reset();initialized=false;remaining=0;}
    void set(float frequencyControl,float qControl,float invert,float trim) noexcept {
        const std::array<float,4> p{clamp(frequencyControl,0,1),clamp(qControl,0,1),clamp(invert,0,1),clamp(trim,0,1)};
        if(initialized && p==last)return;
        last=p;
        const double w=2.*pi*std::min(frequency(p[0]),sr*.45)/sr;
        const double a=std::sin(w)/(2.*resonance(p[1]));
        Biquad next;next.a1=-2.*std::cos(w)/(1.+a);next.a2=(1.-a)/(1.+a);
        next.b0=next.a2;next.b1=next.a1;next.b2=1.;
        const float target=(p[2]>=.5f?-1.f:1.f)*dbToGain(trimDb(p[3]));
        if(!initialized){filter.current=next;filter.target=next;filter.remaining=0;gain=target;targetGain=target;remaining=0;initialized=true;}
        else{filter.aim(next);if(target!=targetGain){targetGain=target;remaining=ramp;}}
    }
    std::array<float,2> process(float l,float r) noexcept {
        if(!initialized)return {l,r};
        const auto y=filter.process(l,r);
        if(remaining>0){gain+=(targetGain-gain)/remaining;--remaining;}
        return {y[0]*gain,y[1]*gain};
    }
private:
    SmoothedBiquad filter;double sr=48000.;
    std::array<float,4> last{};bool initialized=false;
    float gain=1.f,targetGain=1.f;int ramp=576,remaining=0;
};
} // namespace skellpreset::dsp
