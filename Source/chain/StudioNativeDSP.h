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

// Studio ordinary-mode filter design, with optional eighth band and HP/LP.
// All new band shapes/enables are appended host controls. Prior UI and
// automatic p/Q automation identifiers are not replaced.
class ParametricEQ final {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        for(auto& f:filters)f.prepare(sr);
        for(auto& row:extras)for(auto& f:row)f.prepare(sr);
        stages.fill(0);
        for(auto& d:dynamic){d={};d.cut.prepare(sr);d.attackPole=std::exp(-1./(.012*sr));d.releasePole=std::exp(-1./(.110*sr));}
        cadence=std::max(8,(int)(sr*.002));
        initialized=false;
    }
    void reset() noexcept {for(auto& f:filters)f.reset();for(auto& row:extras)for(auto& f:row)f.reset();for(auto& d:dynamic){d.detector.reset();d.cut.reset();d.env=0.;d.applied=0.f;d.counter=0;d.cut.aim(skellpreset::dsp::Biquad{});}}
    static constexpr std::array<int,8> defaultKinds{{3,0,0,0,0,0,4,0}};
    static constexpr std::array<bool,8> defaultEnabled{{true,true,true,true,true,true,true,false}};
    void set(const std::array<float,16>& p,const std::array<float,8>& widths) noexcept {
        set(p,widths,defaultKinds,defaultEnabled);
    }
    void set(const std::array<float,16>& p,const std::array<float,8>& widths,
             const std::array<int,8>& kinds,
             const std::array<bool,8>& enabled) noexcept {
        set(p,widths,kinds,enabled,std::array<float,8>{});
    }
    void set(const std::array<float,16>& p,const std::array<float,8>& widths,
             const std::array<int,8>& kinds,const std::array<bool,8>& enabled,const std::array<float,8>& slopes) noexcept {
        if(initialized && p==lastP && widths==lastWidths &&
           kinds==lastKinds && enabled==lastEnabled && slopes==lastSlopes)return;
        initialized=true;lastP=p;lastWidths=widths;
        lastKinds=kinds;lastEnabled=enabled;lastSlopes=slopes;
        for(int k=0;k<8;++k){
            const double freq=std::clamp(20.*std::pow(1000.,(double)detail::unit(p[(size_t)k*2])),20.,sr*.46);
            const double gain=36.*((double)detail::unit(p[(size_t)k*2+1])-.5);
            const double width=(double)detail::unit(widths[(size_t)k]);
            const int style=std::clamp(kinds[(size_t)k],0,4); // peak,HP,LP,low shelf,high shelf
            const int kind=style==0?6:(style==3?5:7);
            const double angle=2.*skellpreset::dsp::pi/sr;
            const double w=std::min(freq*angle,kind==6?2.1991147994995117:3.138451099395752);
            const double sn=std::sin(w),cs=std::cos(w);
            skellpreset::dsp::Biquad c;
            const size_t b=(size_t)k;
            stages[b]=enabled[b]&&(style==1||style==2)?(int)std::round(detail::unit(slopes[b]*.5f)*2.f):0;
            for(int i=0;i<2;++i){skellpreset::dsp::Biquad next;if(i<stages[b]){if(style==1)next.setHighPass(sr,freq);else next.setLowPass(sr,freq);}extras[b][(size_t)i].aim(next);}
            if(!enabled[(size_t)k]){
                filters[(size_t)k].aim(c);
                continue;
            }
            if(style==1 || style==2) {
                const double q=.35*std::pow(12.,width);
                if(style==1)c.setHighPass(sr,freq,q);
                else c.setLowPass(sr,freq,q);
            }else if(std::abs(gain)>1.e-7){
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
                    if(style==3){
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
    // Opt-in linked stereo dynamic bell cut: adjustable timing/threshold.
    // No lookahead or allocations.
    void setDynamics(const std::array<float,8>& amounts) noexcept {
        std::array<float,8> th,a,r;th.fill(-24.f);a.fill(12.f);r.fill(110.f);setDynamics(amounts,th,a,r);
    }
    void setDynamics(const std::array<float,8>& amounts,const std::array<float,8>& thresholds,const std::array<float,8>& attacks,const std::array<float,8>& releases) noexcept {
        for(size_t k=0;k<dynamic.size();++k){
            auto& d=dynamic[k];
            const float amount=lastEnabled[k] && lastKinds[k]==0?detail::unit(amounts[k]):0.f;
            if(amount==0.f){if(d.amount>0.f){d.cut.aim(skellpreset::dsp::Biquad{});d.env=0.;d.applied=0.f;d.detector.reset();}d.amount=0.f;continue;}
            d.threshold=skellpreset::dsp::clamp(thresholds[k],-80.f,0.f);
            const float a=skellpreset::dsp::clamp(attacks[k],.5f,200.f),r=skellpreset::dsp::clamp(releases[k],5.f,2000.f);
            if(d.attackMs!=a){d.attackMs=a;d.attackPole=std::exp(-1./(.001*a*sr));}
            if(d.releaseMs!=r){d.releaseMs=r;d.releasePole=std::exp(-1./(.001*r*sr));}
            const float frequencyControl=detail::unit(lastP[k*2]),widthControl=detail::unit(lastWidths[k]);
            if(d.amount==amount && d.frequencyControl==frequencyControl && d.widthControl==widthControl)continue;
            const double f=std::clamp(20.*std::pow(1000.,(double)frequencyControl),20.,sr*.46);
            // Match the existing bell's octave bandwidth approximately.
            const double bw=.1+4.9*detail::unit(lastWidths[k]);
            const double q=std::clamp(1./(2.*std::sinh(std::log(2.)*.5*bw)),.2,12.);
            if(d.amount==amount && d.freq==f && d.q==q)continue;
            d.amount=amount;d.freq=f;d.q=q;d.frequencyControl=frequencyControl;d.widthControl=widthControl;
            const double w=2.*skellpreset::dsp::pi*f/sr,alpha=std::sin(w)/(2.*q),n=1./(1.+alpha);
            d.detector.b0=alpha*n;d.detector.b1=0.;d.detector.b2=-alpha*n;
            d.detector.a1=-2.*std::cos(w)*n;d.detector.a2=(1.-alpha)*n;
            d.counter=0;
        }
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float dryL=l,dryR=r;
        for(size_t k=0;k<filters.size();++k){
            const auto y=filters[k].process(l,r);l=y[0];r=y[1];
            for(size_t i=0;i<2;++i){auto& f=extras[k][i];if((int)i<stages[k]||f.remaining>0){const auto z=f.process(l,r);l=z[0];r=z[1];}}
            auto& d=dynamic[k];
            if(d.amount>0.f){
                const double a=d.detector.process(dryL,0),b=d.detector.process(dryR,1);
                const double power=.5*(a*a+b*b),pole=power>d.env?d.attackPole:d.releasePole;
                d.env=power+(d.env-power)*pole;
                if(--d.counter<=0){
                    d.counter=cadence;
                    const float over=(float)(10.*std::log10(std::max(1.e-12,d.env))-d.threshold);
                    const float target=-12.f*d.amount*std::clamp(over/18.f,0.f,1.f);
                    if(std::abs(target-d.applied)>.045f || (target==0.f && d.applied!=0.f)){
                        d.applied=target;
                        d.cut.setPeak(sr,d.freq,target,d.q);
                    }
                }
            }
            if(d.amount>0.f || d.cut.remaining>0){const auto wet=d.cut.process(l,r);l=wet[0];r=wet[1];}
        }
        return {detail::safe(l),detail::safe(r)};
    }
    // Separate, immutable target coefficients for the UI frequency response.
    float reductionDb(size_t band) const noexcept {
        if(band>=dynamic.size())return 0.f;const auto& d=dynamic[band];
        return d.freq>0.&&(d.amount>0.f||d.cut.remaining>0)?(float)std::clamp(-d.cut.current.magnitudeDb(d.freq,sr),0.,12.):0.f;
    }
    double bandMagnitudeDb(size_t band,double frequency) const noexcept {
        if(band>=filters.size())return 0.;double db=filters[band].target.magnitudeDb(frequency,sr);
        for(int i=0;i<stages[band];++i)db+=extras[band][(size_t)i].target.magnitudeDb(frequency,sr);return db;
    }
    double magnitudeDb(double frequency) const noexcept {
        double total=0.;
        for(size_t k=0;k<filters.size();++k)total+=bandMagnitudeDb(k,frequency);
        return std::isfinite(total)?total:0.;
    }
private:
    double sr=48000.;bool initialized=false;
    std::array<float,16> lastP{};
    std::array<float,8> lastWidths{};
    std::array<int,8> lastKinds=defaultKinds;
    std::array<bool,8> lastEnabled=defaultEnabled;
    std::array<skellpreset::dsp::SmoothedBiquad,8> filters{};
    std::array<std::array<skellpreset::dsp::SmoothedBiquad,2>,8> extras{};std::array<int,8> stages{};std::array<float,8> lastSlopes{};
    struct DynamicBand {skellpreset::dsp::Biquad detector;skellpreset::dsp::SmoothedBiquad cut;double env=0.,freq=0.,q=1.;float amount=0.f,applied=0.f,frequencyControl=-1.f,widthControl=-1.f,threshold=-24.f,attackMs=12.f,releaseMs=110.f;double attackPole=0.,releasePole=0.;int counter=0;};
    std::array<DynamicBand,8> dynamic{};
    int cadence=96;
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
        if(lvl<=thresholdAmp){measuredGain=1.f;measuredReductionDb=0.f;return 1.f;}
        const float over=20.f*std::log10(lvl)-thresholdDb;
        const float db=(kneeA>1.e-5f)
            ? ((std::log1p((over+1.f)*kneeA)*kneeB-1.f)*(slope+1.f)-over)
            : over*slope;
        measuredReductionDb=-db;
        measuredGain=std::clamp(std::pow(10.f,db/20.f),0.f,1.f);return measuredGain;
    }
    std::array<float,2> process(float l,float r) noexcept {
        const float g=gain(l,r);return {detail::safe(l*g),detail::safe(r*g)};
    }
    float gainReductionDb() const noexcept {return measuredReductionDb;}
private:
    double sr=48000.,sum=0.,envelope=0.,attackPole=.9,releasePole=.999;
    float measuredGain=1.f,measuredReductionDb=0.f;
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
        // 20 x 8193 doubles = 1.31 MiB. NEVER construct a temporary
        // Tables object on the stack: Windows test and host threads can
        // have a 1 MiB default stack, causing a stack-overflow SEH crash
        // before any audio processing. The table is zero-initialized in
        // static storage, populated once during prepare(), and reused.
        static Tables bank{};
        static const bool populated=[]() noexcept {
            for(int kind=0;kind<2;++kind)for(int threshold=1;threshold<=10;++threshold){
                auto& tab=bank[(size_t)(kind*10+threshold-1)];
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
            return true;
        }();
        (void)populated;
        return bank;
    }
    size_t index=0;double scale=8191.;float wet=1.f,dry=0.f;
};
}
