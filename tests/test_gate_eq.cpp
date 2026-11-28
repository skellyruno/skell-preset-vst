#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace skellpreset::dsp;
struct LegacyCompressor {
    double sampleRate=48000, envelope=0;
    float threshold=-18,ratio=3,attackMs=12,releaseMs=90,makeupDb=0,lastReductionDb=0;
    // Parameter smoothing guards against clicks when the host writes automation
    // at block boundaries. Envelope smoothing remains independent.
    double smoothedThreshold=-18,smoothedRatio=3,smoothedMakeup=0,paramPole=.998;
    void prepare(double sr) noexcept {
        sampleRate=std::max(8000.,sr);
        paramPole=std::exp(-1. / (sampleRate*.012));
        smoothedThreshold=threshold;smoothedRatio=ratio;smoothedMakeup=makeupDb;reset();
    }
    void reset() noexcept {envelope=0;}
    void set(float thr,float r,float attack,float release,float makeup) noexcept {
        threshold=clamp(thr,-60,0); ratio=clamp(r,1,20);attackMs=clamp(attack,.2f,200);releaseMs=clamp(release,5,2000);makeupDb=clamp(makeup,-24,24);
    }
    float gain(float detector) noexcept {
        smoothedThreshold=paramPole*smoothedThreshold+(1.-paramPole)*threshold;
        smoothedRatio=paramPole*smoothedRatio+(1.-paramPole)*ratio;
        smoothedMakeup=paramPole*smoothedMakeup+(1.-paramPole)*makeupDb;
        const auto val=std::isfinite(detector)?std::min(10000.f,std::abs(detector)):0.f;
        const auto a=std::exp(-1./(sampleRate*.001*(val>envelope?attackMs:releaseMs)));
        envelope=a*envelope+(1-a)*val;
        if(envelope<1.e-20)envelope=0.; // no persistent denormal detector state
        const auto lvl=gainToDb(static_cast<float>(envelope));
        const auto gr=lvl>smoothedThreshold?(lvl-smoothedThreshold)*(1.-1./smoothedRatio):0.;
        lastReductionDb=(float)gr;
        return dbToGain((float)(smoothedMakeup-gr));
    }
};
struct LegacyGate {
    double sr=48000,envelope=0,gain=0;
    float threshold=-45.f,release=90.f,holdMs=55.f,hysteresisDb=3.f;
    float attackMs=4.f; // historical gate attack default
    int holdRemaining=0;
    bool open=false;
    void prepare(double rate) noexcept {sr=std::max(8000.,rate);envelope=0;gain=0;holdRemaining=0;open=false;}
    float processGain(float detector) noexcept {
        // Schmitt thresholds and a hold timer prevent quiet-consonant chatter.
        const auto a=std::exp(-1./(sr*.004));
        const double input=std::isfinite(detector)?std::abs((double)detector):0.;
        envelope=a*envelope+(1-a)*input;
        if(envelope<1.e-20)envelope=0.;
        const float level=gainToDb((float)envelope);
        if(level>=threshold){open=true;holdRemaining=(int)(sr*.001*clamp(holdMs,0.f,250.f));}
        else {
            // Count real elapsed hold samples even inside the hysteresis band.
            // Otherwise an intermediate-level signal pauses the hold timer.
            if(holdRemaining>0)--holdRemaining;
            if(open && holdRemaining==0 && level<threshold-clamp(hysteresisDb,0.f,12.f))
                open=false;
        }
        const float goal=open?1.f:0.f;
        const auto smooth=std::exp(-1./(sr*.001*(goal>gain?(double)clamp(attackMs,.5f,25.f):(double)std::max(20.f,release))));
        gain=smooth*gain+(1-smooth)*goal;
        if(gain<1.e-20)gain=0.;
        return clamp((float)gain,0.f,1.f);
    }
};

static bool timingParity() {
    for(double rate:{44100.,48000.,96000.,192000.}) {
        Gate a;LegacyGate b;Compressor c;LegacyCompressor d;
        a.prepare(rate);b.prepare(rate);c.prepare(rate);d.prepare(rate);
        for(int n=0;n<96000;++n) {
            if(n%127==0) {
                a.attackMs=b.attackMs=.5f+(n%29)*.8f;
                a.release=b.release=20.f+(n%1231)*.5f;
                a.threshold=b.threshold=-55.f+(n%29);
                a.holdMs=b.holdMs=(float)(n%81);
                a.hysteresisDb=b.hysteresisDb=(float)(n%12);
                c.set(-40.f+(n%29),1.f+(n%11),.2f+(n%47),5.f+(n%241),3.f);
                d.set(-40.f+(n%29),1.f+(n%11),.2f+(n%47),5.f+(n%241),3.f);
            }
            const float x=n%997==0?NAN:(n/1300)%2?.4f*std::sin(n*.031f):0.f;
            if(a.processGain(x)!=b.processGain(x) || a.open!=b.open || a.holdRemaining!=b.holdRemaining ||
                a.gain!=b.gain || c.gain(x)!=d.gain(x) || c.lastReductionDb!=d.lastReductionDb)return false;
        }
    }
    return true;
}

int main() {
    if(!timingParity()){std::cerr<<"FAIL timing parity\n";return 1;}
    for(double rate:{44100.,48000.,96000.}) {
        Biquad bell,cut;
        bell.setPeak(rate,1800.,9.,1.2); cut.setHighPass(rate,100.);
        const auto centre=bell.magnitudeDb(1800.,rate);
        assert(std::isfinite(centre)&&std::abs(centre-9.)<.05);
        assert(std::abs(bell.magnitudeDb(20.,rate))<.12);
        assert(cut.magnitudeDb(20.,rate)<-20.);
        assert(std::isfinite(cut.magnitudeDb(20000.,rate)));
        Gate gate; gate.prepare(rate); gate.threshold=-30;gate.release=35;
        gate.holdMs=65;gate.hysteresisDb=6;
        for(int i=0;i<(int)(rate*.12);++i)gate.processGain(.10f);
        assert(gate.open && gate.gain>.95);
        for(int i=0;i<(int)(rate*.025);++i)gate.processGain(0.f);
        assert(gate.open);
        for(int i=0;i<(int)(rate*.60);++i)gate.processGain(0.f);
        assert(!gate.open && gate.gain<.02);
        assert(std::isfinite(gate.processGain(NAN)));
        Gate fast,slow;fast.prepare(rate);slow.prepare(rate);
        fast.threshold=-40.f;slow.threshold=-40.f;fast.attackMs=.5f;slow.attackMs=25.f;
        for(int n=0;n<(int)(rate*.004);++n){fast.processGain(.35f);slow.processGain(.35f);}
        assert(fast.gain>slow.gain+.10);
    }
    std::cout<<"PASS gate hold/hysteresis and analytical EQ response across 44.1/48/96 kHz\n";
}
