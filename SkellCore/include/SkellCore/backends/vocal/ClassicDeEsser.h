#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace skell::research {
// Independent zero-lookahead stereo-linked split/wide de-esser.
class ClassicDeEsser {
public:
    static constexpr int latencySamples=0;
    void prepare(double fs) noexcept {
        rate=std::clamp(std::isfinite(fs)?fs:48000.,8000.,384000.);
        attack=coef(.5); release=coef(65); smooth=coef(12); reset();
    }
    void set(float hz,float thresholdDb,float rangeDb,float releaseMs,bool wide,bool band,bool listen=false) noexcept {
        hz=bound(hz,2000.f,float(std::min(16000.,rate*.45)),6500.f);
        auto g=std::tan(3.141592653589793*hz/rate);targetPole=float(g/(1+g));
        g=std::tan(3.141592653589793*std::min(1.8*hz,.48*rate)/rate);targetUpper=float(g/(1+g));
        targetThreshold=std::exp(.11512925465f*bound(thresholdDb,-80,0,-30));
        targetMinGain=std::exp(-.11512925465f*bound(rangeDb,0,24,8));
        release=coef(bound(releaseMs,5,500,65));
        targetWide=wide?1.f:0.f;targetBand=band?1.f:0.f;targetListen=listen?1.f:0.f;
    }
    void reset() noexcept { low={}; upper={}; env=0; gain=1; seeded=false; }
    float reductionDb() const noexcept {return -8.685889638f*std::log(std::max(gain,1.e-8f));}
    std::array<float,2> process(float l,float r) noexcept {
        std::array<float,2> x{bound(l,-32,32,0),bound(r,-32,32,0)},hi{},det{};
        if(!seeded){pole=targetPole;upperPole=targetUpper;threshold=targetThreshold;minGain=targetMinGain;wide=targetWide;band=targetBand;listen=targetListen;seeded=true;}
        pole+=smooth*(targetPole-pole);upperPole+=smooth*(targetUpper-upperPole);threshold+=smooth*(targetThreshold-threshold);minGain+=smooth*(targetMinGain-minGain);
        wide+=smooth*(targetWide-wide);band+=smooth*(targetBand-band);listen+=smooth*(targetListen-listen);
        for(int c=0;c<2;++c){const float v=pole*(x[c]-low[c]), lp=low[c]+v;low[c]=lp+v;hi[c]=x[c]-lp;const float w=upperPole*(hi[c]-upper[c]), bp=upper[c]+w;upper[c]=bp+w;det[c]=hi[c]+band*(bp-hi[c]);if(std::abs(low[c])<1.e-20f)low[c]=0;if(std::abs(upper[c])<1.e-20f)upper[c]=0;}
        const float level=std::max(std::abs(det[0]),std::abs(det[1]));
        env+=(level>env?attack:release)*(level-env);if(env<1.e-20f)env=0;
        const float want=std::max(minGain,std::min(1.f,threshold/std::max(env,1.e-8f)));
        gain+=(want<gain?attack:release)*(want-gain);
        for(int c=0;c<2;++c){const float y=x[c]+(gain-1.f)*(hi[c]+wide*(x[c]-hi[c]));x[c]=y+listen*(det[c]-y);}
        return x;
    }
private:
    static float bound(float v,float lo,float hi,float fallback) noexcept{return std::clamp(std::isfinite(v)?v:fallback,lo,hi);}
    float coef(float ms) const noexcept{return float(-std::expm1(-1000./(ms*rate)));}
    double rate=48000.;float attack=0,release=0,smooth=0,env=0,gain=1;
    float pole=.5f,upperPole=.7f,threshold=.0316228f,minGain=.398107f,wide=0,band=0,listen=0;
    float targetPole=.5f,targetUpper=.7f,targetThreshold=.0316228f,targetMinGain=.398107f,targetWide=0,targetBand=0,targetListen=0;
    std::array<float,2> low{},upper{};bool seeded=false;
};
}
