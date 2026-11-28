#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skellpreset::audition {
inline double db(double value) noexcept {return 20.*std::log10(std::max(1.e-12,value));}
struct Metrics {
    double peak=0.,energy=0.,leftEnergy=0.,rightEnergy=0.,cross=0.,activeEnergy=0.;
    std::uint64_t frames=0,activeFrames=0,overSamples=0;
    bool add(float l,float r) noexcept {
        if(!std::isfinite(l)||!std::isfinite(r))return false;
        const double a=l,b=r;
        peak=std::max({peak,std::abs(a),std::abs(b)});
        leftEnergy+=a*a;rightEnergy+=b*b;cross+=a*b;
        const double e=.5*(a*a+b*b);energy+=e;++frames;
        overSamples+=(std::abs(a)>1.)+(std::abs(b)>1.);
        // Explicit sample-frame gate, NOT perceptual loudness or a VAD.
        if(std::max(std::abs(a),std::abs(b))>=.00316227766){activeEnergy+=e;++activeFrames;}
        return true;
    }
    double rms() const noexcept {return frames?std::sqrt(energy/frames):0.;}
    double activeRms() const noexcept {return activeFrames?std::sqrt(activeEnergy/activeFrames):0.;}
    double correlation() const noexcept {return leftEnergy*rightEnergy>1.e-24?std::clamp(cross/std::sqrt(leftEnergy*rightEnergy),-1.,1.):0.;}
    double crestDb() const noexcept {return rms()>0.?db(peak/rms()):0.;}
    double sideDb() const noexcept {return db(frames?std::sqrt(std::max(0.,(leftEnergy+rightEnergy-2.*cross)/(4.*frames))):0.);}
};
// Attenuation-only comparison: keep both audition files below -1 dBFS
// sample peak. Never boost silence or introduce a limiter into the A/B.
struct Match {double dry=1.,wet=1.;bool valid=false;};
inline Match match(const Metrics& dry,const Metrics& wet) noexcept {
    const double a=dry.rms(),b=wet.rms();
    if(a<1.e-6||b<1.e-6)return {};
    const double ratio=a/b;
    if(ratio>4. || ratio<.25)return {}; // refuse extreme or misleading matches
    Match m{std::min(1.,1./ratio),std::min(1.,ratio),true};
    const double peak=std::max(dry.peak*m.dry,wet.peak*m.wet);
    const double common=peak>0.?std::min(1.,.891250938/peak):1.;
    m.dry*=common;m.wet*=common;return m;
}
// Stable bounded variation, independent of std::random implementation.
inline float variation(unsigned seed,unsigned dimension) noexcept {
    std::uint32_t x=seed*0x9e3779b9u+dimension*0x85ebca6bu+0x27d4eb2du;
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
    return (float)(x&0xffffu)/32767.5f-1.f;
}
}
