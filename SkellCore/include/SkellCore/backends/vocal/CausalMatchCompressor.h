#pragma once
#include "AsymmetricGainCurve.h"
// Experimental stereo-linked, causal adapter. No history lookahead, heap or
// host buffer dependency. Configure outside the audio callback. No FL raw-state
// mapping or legacy compressor replacement is implied.
namespace skell::match {
class CausalMatchCompressor {
public:
    using Params=AsymmetricGainCurve::Params;
    static constexpr int latencySamples=0;
    bool prepare(double rate,int domain,int detector,int interval,const Params& p) noexcept {
        ready=false;
        if(!std::isfinite(rate)||rate<32000||rate>192000||detector<0||detector>2||interval<1||interval>64)return false;
        if(!curve.prepare(rate/interval,domain,p))return false;
        kind=detector;period=interval;initial=static_cast<float>(std::pow(10.,p[4]/20.));ready=true;reset();return true;
    }
    void reset() noexcept {curve.reset();count=0;acc=0;peak=0;gain=target=initial;step=0;}
    void process(float& left,float& right) noexcept {
        if(!ready)return;
        const auto clean=[](float v){return std::isfinite(v)&&std::abs(v)>=1e-30f?std::clamp(v,-10000.f,10000.f):0.f;};
        const float l=clean(left),r=clean(right);const float a=std::abs(l),b=std::abs(r);
        // Output uses only gain from already completed control frames. A
        // frame's statistics never modify samples preceding its final sample.
        gain+=step;left=l*gain;right=r*gain;
        if(kind==0)peak=std::max({peak,a,b});
        else if(kind==1)acc+=(double(a)*a+double(b)*b)*.5;
        else acc+=(double(a)+b)*.5;
        if(++count==period){
            const double mean=acc/period;
            const float d=kind==0?peak:static_cast<float>(kind==1?std::sqrt(mean):mean);
            target=static_cast<float>(std::pow(10.,curve.process(d)/20.));
            step=(target-gain)/period;acc=0;peak=0;count=0;
        }
    }
    void processBlock(float* left,float* right,int n) noexcept {if(!left||!right||n<=0)return;for(int i=0;i<n;++i)process(left[i],right[i]);}
private:
    AsymmetricGainCurve curve;
    double acc=0;float peak=0,initial=1,gain=1,target=1,step=0;
    int count=0,period=16,kind=0;bool ready=false;
};
}
