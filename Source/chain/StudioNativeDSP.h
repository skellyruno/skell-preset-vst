#pragma once
// Skell Studio / Lemon Studio GPL-3.0-or-later DSP translations and adapters.
// Copyright (C) 2026 ItsAlphaNeon (upstream formulae); adaptations by Skell Audio.
// Original Rust: https://github.com/skellyruno/skell-studio/tree/main/crates/os-engine/src/effects
// This is not a 1:1 port of Studio's HQ+/LIN EQ or its 18-control Limiter.
// Rack module IDs are opt-in: legacy Skell Preset algorithms and states are unaffected.
#include "BasicDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace skellpreset::studio {
namespace detail {
inline float unit(float x) noexcept { return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f; }
inline float safe(float x) noexcept { return std::isfinite(x)?std::clamp(x,-8.f,8.f):0.f; }
inline float level(int v,double b) noexcept {
    const float n=(float)v*.001f;
    if(n==1.f)return 1.f;
    const float x=n*(float)std::log(b);
    return (float)((std::exp((double)x)-1.)*(double)(float)(1./(b-1.)));
}
inline double timeMs(int v) noexcept {
    const float x=(float)((double)v*.0001);
    return (double)(float)((std::exp((double)x*std::log(501.))-1.)*2.);
}
inline float levelCurve(float x) noexcept {
    if(x==1.f)return 1.f;
    const float p=x*(float)std::log(11.);
    return (float)((std::exp((double)p)-1.)*(double)0.1f);
}
inline float towardZero(double x) noexcept {
    if(!std::isfinite(x))return 0.f;
    const float y=(float)x;
    return std::abs((double)y)>std::abs(x)?std::nextafter(y,0.f):y;
}
}

// Seven-band, ordinary (zero-PDC) coefficient design from Studio Parametric EQ.
// Stereo frame smoothing and coefficient caching are Skell Preset improvements.
// The existing 8-band EQ remains available and is not replaced.
class ParametricEQ final {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        for(auto& f:filters)f.prepare(sr);
        initialized=false;
    }
    void reset() noexcept {for(auto& f:filters)f.reset();}
    void set(const std::array<float,16>& p,const std::array<float,8>& widths) noexcept {
        if(initialized && p==lastP && widths==lastWidths)return;
        initialized=true;lastP=p;lastWidths=widths;
        for(int k=0;k<7;++k){
            const double freq=std::clamp(20.*std::pow(1000.,(double)detail::unit(p[(size_t)k*2])),20.,sr*.46);
            const double gain=36.*((double)detail::unit(p[(size_t)k*2+1])-.5);
            const double width=(double)detail::unit(widths[(size_t)k]);
            const int kind=k==0?5:(k==6?7:6); // Studio: low shelf, five peaks, high shelf.
            const double angle=2.*skellpreset::dsp::pi/sr;
            const double w=std::min(freq*angle,kind==6?2.1991147994995117:3.138451099395752);
            const double sn=std::sin(w),cs=std::cos(w);
            skellpreset::dsp::Biquad c;
            if(std::abs(gain)>1.e-7){
                constexpr double dbUnit=.1151292546497022842; // ln(10)/20
                if(kind==6){
                    const double bw=width*4.9+.1;
                    const double a=std::exp(gain*.5*dbUnit);
                    const double alpha=sn*std::sinh((std::log(2.)*.5)*bw*w/std::max(sn,1.e-6));
                    const double down=alpha/a,up=alpha*a,n=1./(1.+down);
                    c.b0=n*(up+1.);c.b1=n*cs*-2.;c.b2=n*(1.-up);
                    c.a1=c.b1;c.a2=n*(1.-down);
                }else{
                    const double A=std::exp(gain*.5*dbUnit);
                    const double slope=(1.-width)*1.7192982456140353+1./3.;
                    const double bigS=std::sqrt(A)*std::sqrt(std::max(0.,(A+1./A)*(1./slope-1.)+2.));
                    const double d=(A-1.)*cs,beta=bigS*sn;
                    if(kind==5){
                        const double n=1./(A+1.+d+beta);
                        c.a1=n*((A-1.)+(A+1.)*cs)*-2.;
                        c.a2=n*(A+1.+d-beta);
                        c.b0=n*(A*(A+1.-d+beta));
                        c.b1=n*(A*2.*((A-1.)-(A+1.)*cs));
                        c.b2=n*(A*(A+1.-d-beta));
                    }else{
                        const double n=1./(A+1.-d+beta);
                        c.a1=n*((A-1.)-(A+1.)*cs)*2.;
                        c.a2=n*(A+1.-d-beta);
                        c.b0=n*(A*(A+1.+d+beta));
                        c.b1=n*(A*-2.*((A-1.)+(A+1.)*cs));
                        c.b2=n*(A*(A+1.+d-beta));
                    }
                }
            }
            if(!std::isfinite(c.b0)||!std::isfinite(c.a2))c=skellpreset::dsp::Biquad{};
            filters[(size_t)k].aim(c);
        }
    }
    std::array<float,2> process(float l,float r) noexcept {
        for(auto& filter:filters){const auto y=filter.process(l,r);l=y[0];r=y[1];}
        return {detail::safe(l),detail::safe(r)};
    }
    // Separate, immutable target coefficients for the UI frequency response.
    double magnitudeDb(double frequency) const noexcept {
        double total=0.;
        for(const auto& filter:filters)
            total+=filter.target.magnitudeDb(frequency,sr);
        return std::isfinite(total)?total:0.;
    }
private:
    double sr=48000.;bool initialized=false;
    std::array<float,16> lastP{};
    std::array<float,8> lastWidths{};
    std::array<skellpreset::dsp::SmoothedBiquad,7> filters{};
};

// Studio Dynamics transfers the original threshold/ratio/knee laws, a stereo
// RMS detector and compression envelope to a real-time-safe no-allocation rack.
// This is a subset, not byte-for-byte parity with Studio's multistage smoothers.
class Compressor final {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        rmsCount=std::clamp((int)std::nearbyint(detail::timeMs(650)*.001*sr),1,(int)window.size());
        reset();
    }
    void reset() noexcept {window.fill(0.f);index=0;sum=0.;envelope=0.;}
    void set(float threshold,float ratio,float attack,float release,float knee=0.f) noexcept {
        const float thr=detail::level((int)std::lround(detail::unit(threshold)*1000.),7.);
        thresholdAmp=std::max(thr,0.001f);
        thresholdDb=20.f*std::log10(thresholdAmp);
        const float c=(float)(std::exp((double)(detail::unit(ratio)*(float)std::log(51.)))-1.);
        slope=1.f/(1.f+c*.38f)-1.f;
        const float k=(float)(std::exp((double)(detail::unit(knee)*(float)std::log(201.)))-1.);
        kneeA=k*.005f;kneeB=kneeA>0.f?1.f/std::log1p(kneeA):0.f;
        attackPole=std::exp(-1./(sr*.001*std::max(.2,detail::timeMs((int)(detail::unit(attack)*10000.)))));
        releasePole=std::exp(-1./(sr*.001*std::max(3.,detail::timeMs((int)(detail::unit(release)*10000.))*2.)));
    }
    float gain(float left,float right) noexcept {
        const double l=(double)detail::safe(left),r=(double)detail::safe(right);
        const float square=(float)(l*l+r*r);
        sum+= (double)square-(double)window[index];
        window[index]=square;
        index=(index+1==(size_t)rmsCount)?0:index+1;
        const double rms=std::sqrt(std::max(0.,sum)/(2.*(double)rmsCount));
        const double pole=rms>envelope?attackPole:releasePole;
        envelope=pole*envelope+(1.-pole)*rms;
        if(envelope<1.e-20)envelope=0.;
        const float lvl=(float)std::min(envelope,100.);
        if(lvl<=thresholdAmp)return 1.f;
        const float over=20.f*std::log10(lvl)-thresholdDb;
        const float db=(kneeA>1.e-5f)
            ? ((std::log1p((over+1.f)*kneeA)*kneeB-1.f)*(slope+1.f)-over)
            : over*slope;
        return std::clamp(std::pow(10.f,db/20.f),0.f,1.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float g=gain(l,r);return {detail::safe(l*g),detail::safe(r*g)};
    }
private:
    double sr=48000.,sum=0.,envelope=0.,attackPole=.9,releasePole=.999;
    float thresholdAmp=.25f,thresholdDb=-12.f,slope=-.7f,kneeA=0.f,kneeB=0.f;
    std::array<float,4096> window{};
    size_t index=0;int rmsCount=64;
};

// Studio's gate level law + smooth close/open, followed in the plugin by the
// existing SkellCore true-peak limiter. The combined processor is a hybrid.
class Gate final {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        gain=1.;
    }
    void set(float threshold,float reduction,float release) noexcept {
        // Level below -inf when threshold zero, otherwise Studio level law.
        below=detail::level((int)std::lround(detail::unit(threshold)*1000.),7.);
        floor=std::clamp(detail::unit(reduction),0.f,1.f);
        closePole=std::exp(-4.60517018598809/(std::max(1.,detail::timeMs((int)std::lround(detail::unit(release)*10000.))*2.)*.001*sr));
        openPole=std::exp(-1./(sr*.002));
    }
    float processGain(float l,float r) noexcept {
        const float peak=std::max(std::abs(detail::safe(l)),std::abs(detail::safe(r)));
        const double target=(below>0.f&&peak<below)?(double)floor:1.;
        const double pole=target<gain?closePole:openPole;
        gain=pole*gain+(1.-pole)*target;
        if(gain<1.e-20)gain=0.;
        return (float)gain;
    }
private:
    double sr=48000.,gain=1.,closePole=.999,openPole=.99;
    float below=0.f,floor=1.f;
};

// Studio Fast Dist, 8192-point type A/B lookup and the same gain law.
// Static tables are generated once during prepare(), never on the DSP thread.
class FastDist final {
public:
    static constexpr size_t tableLength=8193;
    void prepare(double) noexcept { (void)tables(); }
    void setRaw(int pre,int threshold,int type,int mix,int post) noexcept {
        pre=std::clamp(pre,0,300);threshold=std::clamp(threshold,1,10);
        type=std::clamp(type,0,1);mix=std::clamp(mix,0,128);post=std::clamp(post,0,160);
        const float preGain=detail::levelCurve((float)pre*.0078125f);
        const float postGain=detail::levelCurve((float)post*.0078125f);
        const float wetGain=detail::levelCurve((float)mix*.0078125f)*postGain;
        index=(size_t)(type*10+threshold-1);
        scale=(double)preGain*8191.;
        wet=wetGain;
        dry=(postGain-wetGain)*preGain*postGain;
    }
    void set(float pre,float threshold,float type,float mix,float post) noexcept {
        setRaw((int)std::lround(64.f+128.f*detail::unit(pre)),
               (int)std::lround(1.f+9.f*detail::unit(threshold)),
               detail::unit(type)>=.5f?1:0,
               (int)std::lround(128.f*detail::unit(mix)),
               (int)std::lround(128.f*detail::unit(post)));
    }
    float sample(float x) const noexcept {
        if(!std::isfinite(x))return 0.f;
        const double p=std::abs((double)x*scale);
        double shape=0.;
        if(p<2147483648.){
            const size_t at=(size_t)std::min(8191.,p);
            const double frac=p-std::trunc(p);
            const auto& tab=tables()[index];
            shape=(tab[at]+(tab[at+1]-tab[at])*frac)*(double)wet;
            if(std::signbit(x))shape=-shape;
        }
        return detail::towardZero((double)x*(double)dry+shape);
    }
    std::array<float,2> process(float l,float r) const noexcept {return {sample(l),sample(r)};}
private:
    using Table=std::array<double,tableLength>;
    using Tables=std::array<Table,20>;
    static const Tables& tables() noexcept {
        static const Tables bank=[] {
            Tables t{};
            for(int kind=0;kind<2;++kind)for(int threshold=1;threshold<=10;++threshold){
                auto& tab=t[(size_t)(kind*10+threshold-1)];
                const double step=(double)(10-threshold);
                for(size_t i=1;i<8192;++i){
                    const double db=20.*std::log10((double)i/8191.);
                    const double shaped=kind==0
                        ? (db>=-25.?((db+25.)*(step+1.)*.1-25.):db)+(9.-step)*1.6
                        : -std::pow(-db,step*.02+.8);
                    tab[i]=std::nearbyint(std::pow(10.,shaped/20.)*32767.)/32768.;
                }
                tab[8192]=tab[8191];
            }
            return t;
        }();
        return bank;
    }
    size_t index=0;double scale=8191.;float wet=1.f,dry=0.f;
};
}
