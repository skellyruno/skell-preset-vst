#pragma once
// Standalone, input-driven compressor control-rate prototype. The caller
// supplies detector magnitude at a declared rate. No allocation or lookahead
// here; any analysis/interpolation of detector frames is the caller's concern.
#include <algorithm>
#include <array>
#include <cmath>
namespace skell::match {
class AsymmetricGainCurve {
public:
    // log attack/release milliseconds, threshold dB, slope, makeup dB,
    // knee dB, log gain attack/release milliseconds; physical, not FL raw state.
    using Params=std::array<double,8>;
    bool prepare(double rate, int domain, const Params& p) noexcept {
        ready=false;
        if(!std::isfinite(rate)||rate<100||rate>192000||domain<0||domain>2)return false;
        for(double v:p)if(!std::isfinite(v))return false;
        const auto in=[](double v,double lo,double hi){return v>=lo&&v<=hi;};
        if(!in(p[0],std::log(.05),std::log(200))||!in(p[1],std::log(.1),std::log(1500))||
           !in(p[2],-65,-5)||!in(p[3],.05,.995)||!in(p[4],-5,30)||!in(p[5],.05,30)||
           !in(p[6],std::log(.01),std::log(200))||!in(p[7],std::log(.01),std::log(1500)))return false;
        settings=p;mode=domain;
        a=std::exp(-1.0/(rate*.001*std::exp(p[0])));r=std::exp(-1.0/(rate*.001*std::exp(p[1])));
        ga=std::exp(-1.0/(rate*.001*std::exp(p[6])));gr=std::exp(-1.0/(rate*.001*std::exp(p[7])));
        ready=true;reset();return true;
    }
    void reset() noexcept {env=0;g=0;first=true;}
    float process(float detector) noexcept {
        if(!ready)return 0.f;
        const double threshold=settings[2],slope=settings[3],makeup=settings[4],knee=settings[5];
        double d=std::max(std::isfinite(detector)?double(detector):0.,1e-10);
        if(mode==1)d*=d;
        if(mode==2)d=20*std::log10(d);
        if(first&&mode==2)env=-100;
        first=false;
        const double c=d>env?a:r;env=c*env+(1-c)*d;
        const double level=mode==2?env:(mode==1?10:20)*std::log10(std::max(env,1e-15));
        const double z=level-threshold;
        const double ex=z<=-knee*.5?0:z>=knee*.5?z:(z+knee*.5)*(z+knee*.5)/(2*knee);
        const double v=-slope*ex;const double q=v<g?ga:gr;g=q*g+(1-q)*v;
        return static_cast<float>(makeup+g);
    }
private:
    Params settings{};int mode=0;bool ready=false,first=true;
    double a=0,r=0,ga=0,gr=0,env=0,g=0;
};
} // namespace skell::match
