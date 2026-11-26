#pragma once
#include <algorithm>
#include <cmath>
namespace skellpreset::research {
// Independent tube-inspired nonlinear core, not a model of specific hardware.
// First-order antiderivative antialiasing (ADAA) is experimental: it has
// frequency-dependent magnitude/phase and is not an oversampling substitute.
struct TubeCore {
    double drive=3.,bias=.12,previous=0.;
    bool primed=false;
    void set(double d,double b) noexcept {
        drive=std::clamp(std::isfinite(d)?d:1.,1.,8.);
        bias=std::clamp(std::isfinite(b)?b:0.,-.35,.35);
    }
    void reset() noexcept {previous=0.;primed=false;}
    double curve(double x) const noexcept {
        const double tb=std::tanh(bias);
        return (std::tanh(drive*x+bias)-tb)/(drive*(1.-tb*tb));
    }
    double primitive(double x) const noexcept {
        const double z=std::abs(drive*x+bias),tb=std::tanh(bias);
        const double lc=z+std::log1p(std::exp(-2.*z))-.6931471805599453094;
        return (lc/drive-x*tb)/(drive*(1.-tb*tb));
    }
    double process(double x) noexcept {
        x=std::clamp(std::isfinite(x)?x:0.,-4.,4.);
        if(!primed){primed=true;previous=x;return curve(x);}
        const double delta=x-previous;
        const double y=std::abs(delta)<1.e-5?curve(.5*(x+previous)):
            (primitive(x)-primitive(previous))/delta;
        previous=x;return y;
    }
};
// Candidate front end: independent per-channel histories; optional drive,
// gain, air shelf and DC rejection on the nonlinear component only.
struct TubePreampCandidate {
    TubeCore core[2];double low[2]{},dc[2]{},poleAir=.5,poleDc=.99;
    double gain=1.,amount=0.,airGain=0.;
    void prepare(double rate) noexcept {
        rate=std::clamp(std::isfinite(rate)?rate:48000.,8000.,192000.);
        constexpr double tau=6.283185307179586;
        poleAir=std::exp(-tau*std::min(10000.,rate*.36)/rate);
        poleDc=std::exp(-tau*15./rate);reset();
    }
    void reset() noexcept {for(int ch=0;ch<2;++ch){core[ch].reset();low[ch]=dc[ch]=0.;}}
    void set(double inputDb,double driveAmount,double airDb) noexcept {
        inputDb=std::clamp(std::isfinite(inputDb)?inputDb:0.,-12.,12.);
        amount=std::clamp(std::isfinite(driveAmount)?driveAmount:0.,0.,1.);
        airDb=std::clamp(std::isfinite(airDb)?airDb:0.,0.,6.);
        gain=std::pow(10.,inputDb/20.);airGain=std::pow(10.,airDb/20.)-1.;
        for(auto& c:core)c.set(1.+5.*amount,.12*amount);
    }
    float process(float input,int channel) noexcept {
        const int ch=channel==0?0:1;
        const double x=(std::isfinite(input)?input:0.f)*gain;
        if(amount==0.&&airGain==0.&&gain==1.){core[ch].reset();low[ch]=dc[ch]=0.;return std::isfinite(input)?input:0.f;}
        double y=x;
        if(amount>0.){const double nonlinear=core[ch].process(x)-x;dc[ch]=poleDc*dc[ch]+(1.-poleDc)*nonlinear;y+=amount*(nonlinear-dc[ch]);}
        else {core[ch].reset();dc[ch]=0.;}
        low[ch]=poleAir*low[ch]+(1.-poleAir)*y;
        return (float)(y+airGain*(y-low[ch]));
    }
};
}
