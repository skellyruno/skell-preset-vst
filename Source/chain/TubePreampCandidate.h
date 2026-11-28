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
        setLinear(std::pow(10.,inputDb/20.),amount,std::pow(10.,airDb/20.)-1.);
    }
    void setLinear(double inputGain,double driveAmount,double air) noexcept {
        gain=inputGain;amount=driveAmount;airGain=air;
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
// Opt-in MIC stage. Cache targets per block; smooth linear gain, drive,
// air, trim and enable over 20 ms. No allocations, locks or file access.
struct MicTubeStage {
    struct Ramp {
        double current=0.,target=0.,step=0.;int remaining=0;
        void snap(double v) noexcept {current=target=v;step=0.;remaining=0;}
        void set(double v,int length) noexcept {
            if(v==target)return;
            target=v;remaining=length;step=(target-current)/length;
        }
        double tick() noexcept {if(remaining>0){current+=step;if(--remaining==0)current=target;}return current;}
    };
    TubePreampCandidate tube;
    Ramp wet,drive,gain,air,trim;
    int length=960;bool running=false;
    static double safe(double v,double lo,double hi,double fallback=0.) noexcept {
        return std::clamp(std::isfinite(v)?v:fallback,lo,hi);
    }
    static double linear(double db) noexcept {return std::pow(10.,safe(db,-12.,12.)/20.);}
    void prepare(double rate,bool on,double inputDb,double amount,double airDb,double outputDb) noexcept {
        rate=safe(rate,8000.,192000.,48000.);length=std::max(1,(int)std::round(rate*.020));
        tube.prepare(rate);running=false;wet.snap(on?1.:0.);
        gain.snap(linear(inputDb));drive.snap(safe(amount,0.,1.));air.snap(linear(safe(airDb,0.,6.))-1.);trim.snap(linear(outputDb));
    }
    void set(bool on,double inputDb,double amount,double airDb,double outputDb) noexcept {
        wet.set(on?1.:0.,length);gain.set(linear(inputDb),length);
        drive.set(safe(amount,0.,1.),length);air.set(linear(safe(airDb,0.,6.))-1.,length);trim.set(linear(outputDb),length);
    }
    void process(float& l,float& r) noexcept {
        const double w=wet.tick(),a=drive.tick(),g=gain.tick(),hi=air.tick(),out=trim.tick();
        if(w<=0. || (a==0. && g==1. && hi==0. && out==1.)){
            if(running){tube.reset();running=false;}return;
        }
        running=true;tube.setLinear(g,a,hi);
        const float yl=(float)(tube.process(l,0)*out),yr=(float)(tube.process(r,1)*out);
        l+=(yl-l)*(float)w;r+=(yr-r)*(float)w;
    }
};
}
