#pragma once
// Portable, allocation-free-in-process DSP primitives. Shared by realtime chain and MATCH tests.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <cstdint>

namespace skellpreset::dsp {
constexpr double pi = 3.14159265358979323846;
inline float clamp(float v, float lo, float hi) noexcept { return std::max(lo, std::min(hi, std::isfinite(v)?v:0.f)); }
inline float dbToGain(float dB) noexcept { return std::pow(10.f, dB/20.f); }
inline float gainToDb(float x) noexcept { return 20.f*std::log10(std::max(x,1.e-9f)); }

struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    std::array<double,2> z1{},z2{};
    void reset() noexcept { z1.fill(0);z2.fill(0); }
    void setPeak(double sampleRate, double frequency, double gainDb, double q) noexcept {
        const auto f=std::clamp(frequency, 15., sampleRate*.46);
        const auto w=2*pi*f/sampleRate, a=std::sin(w)/(2*std::clamp(q,.2,18.));
        const auto A=std::pow(10.,std::clamp(gainDb,-24.,24.)/40.);
        const auto den=1+a/A;
        b0=(1+a*A)/den; b1=(-2*std::cos(w))/den; b2=(1-a*A)/den;
        a1=b1; a2=(1-a/A)/den;
    }
    void setHighPass(double sampleRate, double frequency, double q=.7071068) noexcept {
        const auto w=2*pi*std::clamp(frequency,15.,sampleRate*.46)/sampleRate;
        const auto a=std::sin(w)/(2*q), c=std::cos(w), norm=1/(1+a);
        b0=(1+c)*.5*norm;b1=-(1+c)*norm;b2=b0;a1=-2*c*norm;a2=(1-a)*norm;
    }
    // Exact magnitude response of the coefficients used in realtime processing.
    double magnitudeDb(double frequency,double sampleRate) const noexcept {
        if(!(sampleRate>0) || !std::isfinite(frequency))return 0.;
        const double w=2*pi*std::clamp(frequency,0.,sampleRate*.5)/sampleRate;
        const double c=std::cos(w),s=std::sin(w),c2=std::cos(2*w),s2=std::sin(2*w);
        const double nr=b0+b1*c+b2*c2,ni=-b1*s-b2*s2;
        const double dr=1+a1*c+a2*c2,di=-a1*s-a2*s2;
        const double num=nr*nr+ni*ni,den=dr*dr+di*di;
        return 10.*std::log10(std::max(1.e-24,num)/std::max(1.e-24,den));
    }
    float process(float s,int channel) noexcept {
        auto x=std::isfinite(s)?static_cast<double>(s):0.;
        const auto y=b0*x+z1[(size_t)channel];
        z1[(size_t)channel]=b1*x-a1*y+z2[(size_t)channel];
        z2[(size_t)channel]=b2*x-a2*y;
        if(std::abs(z1[(size_t)channel])<1e-25) z1[(size_t)channel]=0;
        if(std::abs(z2[(size_t)channel])<1e-25) z2[(size_t)channel]=0;
        return std::isfinite(y)?static_cast<float>(y):0.f;
    }
};

// Slews biquad coefficients without reallocating or interpolating audio buffers.
// The Schur-stable denominator region of a second-order biquad is convex,
// so a linear transition between stable filters remains stable.
// Coefficients advance ONCE PER STEREO FRAME, not once per channel.
struct SmoothedBiquad {
    Biquad current, target;
    int remaining = 0, rampSamples = 480;
    void prepare(double sr) noexcept {
        rampSamples = std::max(1, (int)(std::max(8000.,sr) * .012));
        current = Biquad{}; target = Biquad{}; remaining = 0;
    }
    void reset() noexcept {current.reset();}
    void aim(const Biquad& next) noexcept {
        const double diff = std::abs(next.b0-target.b0)+std::abs(next.b1-target.b1)
            +std::abs(next.b2-target.b2)+std::abs(next.a1-target.a1)+std::abs(next.a2-target.a2);
        if(diff < 1e-11) return;
        target.b0=next.b0; target.b1=next.b1; target.b2=next.b2;
        target.a1=next.a1; target.a2=next.a2;
        remaining=rampSamples;
    }
    void setPeak(double sr,double f,double db,double q) noexcept {
        Biquad next;next.setPeak(sr,f,db,q);aim(next);
    }
    void setHighPass(double sr,double f) noexcept {
        Biquad next;next.setHighPass(sr,f);aim(next);
    }
    void tick() noexcept {
        if(remaining<=0)return;
        const double a=1. / remaining;
        current.b0+=(target.b0-current.b0)*a;
        current.b1+=(target.b1-current.b1)*a;
        current.b2+=(target.b2-current.b2)*a;
        current.a1+=(target.a1-current.a1)*a;
        current.a2+=(target.a2-current.a2)*a;
        --remaining;
    }
    std::array<float,2> process(float left,float right) noexcept {
        tick();return {current.process(left,0),current.process(right,1)};
    }
};

struct Compressor {
    double sampleRate=48000, envelope=0;
    float threshold=-18,ratio=3,attackMs=12,releaseMs=90,makeupDb=0;
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
        return dbToGain((float)(smoothedMakeup-gr));
    }
};

struct Gate {
    double sr=48000,envelope=0,gain=0;
    float threshold=-45.f,release=90.f,holdMs=55.f,hysteresisDb=3.f;
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
        const auto smooth=std::exp(-1./(sr*.001*(goal>gain?4.:std::max(20.f,release))));
        gain=smooth*gain+(1-smooth)*goal;
        if(gain<1.e-20)gain=0.;
        return clamp((float)gain,0.f,1.f);
    }
};

// VoxRack DSP port, licensed by its original owner for Skell-Preset.
// The low-pass/high-pass feedback topology comes from VoxRack's
// apo/fx.h::FxDelay. Skell-Preset keeps its existing stereo ring buffer and
// 20ms read-head crossfade to preserve all existing automation IDs and PDC.
struct Delay {
    std::array<std::vector<float>,2> memory;
    size_t pos=0;
    int samples=4000;
    int previousSamples=4000;
    int requestedSamples=4000; // latest automation target; queued during an active fade
    float feedback=.25f;
    float transition=1.f,transitionStep=.001f;
    // VoxRack's tone-filtered feedback, extended to true stereo.
    double sampleRate=48000.;
    float lowpassCoefficient=.4f,lowpassTarget=.4f,highpassCoefficient=.019f;
    float toneSmoothStep=.001f;
    std::array<float,2> lowpass{},highpass{};
    void prepare(double sr,float maxSeconds=2.0f) {
        sampleRate=std::clamp(sr,8000.,192000.);
        const auto capacity=std::max<size_t>(32,(size_t)(sampleRate*maxSeconds)+2);
        for(auto& line:memory)line.assign(capacity,0.f);
        pos=0;
        samples=previousSamples=requestedSamples=std::clamp(samples,1,(int)capacity-1);
        transition=1.f;
        transitionStep=1.f/std::max(1.f,(float)(sampleRate*.020));
        toneSmoothStep=1.f-(float)std::exp(-1./(.01*sampleRate));
        highpassCoefficient=1.f-(float)std::exp(-2.*pi*150./sampleRate);
        lowpassTarget=lowpassCoefficient=1.f-(float)std::exp(-2.*pi*8000./sampleRate);
        lowpass.fill(0.f);highpass.fill(0.f);
    }
    void reset() noexcept {
        for(auto& line:memory)std::fill(line.begin(),line.end(),0.f);
        pos=0;transition=1.f;previousSamples=requestedSamples=samples;
        lowpass.fill(0.f);highpass.fill(0.f);
    }
    // The third parameter is the VoxRack tone frequency. Old callers
    // without a tone argument retain an 8 kHz default.
    void configure(int delaySamples,float fb,float toneHz=8000.f) noexcept {
        if(memory[0].empty())return;
        requestedSamples=std::clamp(delaySamples,1,(int)memory[0].size()-1);
        if(transition>=1.f && requestedSamples!=samples) {
            previousSamples=samples;samples=requestedSamples;transition=0.f;
        }
        feedback=clamp(fb,0.f,.9f);
        const float nyquistLimit=std::max(500.f,(float)sampleRate*.45f);
        const float cutoff=clamp(toneHz,500.f,std::min(18000.f,nyquistLimit));
        lowpassTarget=1.f-(float)std::exp(-2.*pi*cutoff/sampleRate);
    }
    std::array<float,2> process(float l,float r) noexcept {
        if(memory[0].empty())return {l,r};
        const size_t cap=memory[0].size();
        const size_t read=(pos+cap-(size_t)samples)%cap;
        const size_t oldRead=(pos+cap-(size_t)previousSamples)%cap;
        const float blend=std::clamp(transition,0.f,1.f);
        const float wetL=memory[0][oldRead]*(1.f-blend)+memory[0][read]*blend;
        const float wetR=memory[1][oldRead]*(1.f-blend)+memory[1][read]*blend;
        // Smooth the tone itself rather than jumping LP coefficients during
        // automation; this remains allocation-free and low latency.
        lowpassCoefficient+=toneSmoothStep*(lowpassTarget-lowpassCoefficient);
        const float wet[2]{wetL,wetR};
        const float input[2]{l,r};
        for(int ch=0;ch<2;++ch) {
            auto& lp=lowpass[(size_t)ch];
            auto& hp=highpass[(size_t)ch];
            lp+=(wet[ch]-lp)*lowpassCoefficient;
            hp+=(lp-hp)*highpassCoefficient;
            if(std::abs(lp)<1.e-20f)lp=0.f;
            if(std::abs(hp)<1.e-20f)hp=0.f;
            const float filtered=lp-hp; // VoxRack: band-limited feedback loop
            const float next=clamp(clamp(input[ch],-8.f,8.f)+filtered*feedback,
                                   -8.f,8.f);
            memory[(size_t)ch][pos]=std::abs(next)>1.e-20f?next:0.f;
        }
        if(++pos==cap)pos=0;
        transition=std::min(1.f,transition+transitionStep);
        if(transition>=1.f && requestedSamples!=samples) {
            previousSamples=samples;samples=requestedSamples;transition=0.f;
        }
        return {wetL,wetR};
    }
};

inline float softLimit(float x,float ceiling=0.96f) noexcept {
    // Zero-lookahead saturation limiter, NOT a true-peak limiter.
    return ceiling*std::tanh(x/ceiling);
}


// Preallocated PDC line: all dry/wet alignment runs without reallocations or locks.
struct StereoSampleDelay {
    std::array<std::vector<float>,2> channel;
    size_t head=0;
    // Dual-tap crossfade state, used only when the rack's PDC changes.
    // The original sample-exact process() path remains unchanged for fixed
    // lookahead and all existing latency/impulse regression fixtures.
    int fadeFrom=0,fadeTo=-1,fadeRemaining=0,fadeDuration=1;
    void prepare(int maxDelay) {
        const size_t capacity=(size_t)std::max(1,maxDelay)+2;
        for(auto& line:channel)line.assign(capacity,0.f);
        head=0;
        fadeFrom=0;fadeTo=-1;fadeRemaining=0;fadeDuration=1;
    }
    void reset() noexcept {
        for(auto& line:channel)std::fill(line.begin(),line.end(),0.f);
        head=0;
        fadeFrom=0;fadeTo=-1;fadeRemaining=0;fadeDuration=1;
    }
    std::array<float,2> process(float l,float r,int delay) noexcept {
        if(channel[0].empty())return {l,r};
        const auto capacity=channel[0].size();
        const auto d=(size_t)std::clamp(delay,0,(int)capacity-2);
        channel[0][head]=std::isfinite(l)?l:0.f;
        channel[1][head]=std::isfinite(r)?r:0.f;
        const size_t read=(head+capacity-d)%capacity;
        const std::array<float,2> output{channel[0][read],channel[1][read]};
        head=(head+1)%capacity;
        return output;
    }
    // When a hosted VST3 changes oversampling/lookahead, snapping the read
    // head across a PDC buffer causes an audible discontinuity. Crossfade the
    // old/new taps over a short fixed interval; no allocation, lock, or
    // per-sample delay-line reinitialization. Stable delay is sample-exact.
    std::array<float,2> processSmoothed(float l,float r,int delay,
                                        int transitionSamples) noexcept {
        if(channel[0].empty())return {std::isfinite(l)?l:0.f,
                                     std::isfinite(r)?r:0.f};
        const auto size=channel[0].size();
        const int requested=std::clamp(delay,0,(int)size-2);
        if(fadeTo<0) {
            fadeFrom=fadeTo=requested;fadeRemaining=0;
        } else if(requested!=fadeTo && fadeRemaining==0) {
            // Do NOT restart a fade from an approximated intermediate tap.
            // A weighted average of two audio samples is not equivalent to
            // reading one sample at an interpolated delay: doing so produced
            // severe discontinuities under fast automation. Finish the active
            // fade, then transition to the latest requested value.
            fadeFrom=fadeTo;
            fadeTo=requested;
            fadeDuration=std::max(1,transitionSamples);
            fadeRemaining=fadeDuration;
        }
        // Retire inaudible subnormal noise before it enters a long PDC ring.
        // This path is only used for dynamically switched latency; the
        // original fixed-tap sample-exact process() remains untouched.
        channel[0][head]=std::isfinite(l) && std::abs(l)>=1.e-25f?l:0.f;
        channel[1][head]=std::isfinite(r) && std::abs(r)>=1.e-25f?r:0.f;
        const auto previousTap=(head+size-(size_t)fadeFrom)%size;
        const auto nextTap=(head+size-(size_t)fadeTo)%size;
        float blend=1.f;
        if(fadeRemaining>0) {
            blend=(float)(fadeDuration-fadeRemaining+1)/(float)fadeDuration;
            --fadeRemaining;
        }
        std::array<float,2> out{};
        for(size_t ch=0;ch<2;++ch) {
            const float a=channel[ch][previousTap],b=channel[ch][nextTap];
            const float sample=a+(b-a)*blend;
            out[ch]=std::isfinite(sample) && std::abs(sample)>=1.e-25f
                ?sample:0.f;
        }
        head=(head+1)%size;
        return out;
    }
};


// Zero-lookahead stereo-linked split-band de-esser. The dry sample is
// reconstructed from low + high, so a 0 dB reduction is sample-exact unity.
// A first-order crossover costs little CPU, and attack/release smoothing
// prevents hard switching. Not a neural or phrase-aware detector.
struct SplitDeEsser {
    double sampleRate=48000.0;
    double lowPole=.5, attackPole=.9, releasePole=.999;
    double envelope=0.0, reductionGain=1.0;
    double lowL=0.0,lowR=0.0;
    double threshold=0.018,minimumGain=.25;
    void prepare(double sr) noexcept {
        sampleRate=std::clamp(sr,8000.,192000.);
        lowL=lowR=envelope=0.0;reductionGain=1.0;
        set(6500.f,-33.f,12.f,80.f);
    }
    void reset() noexcept {lowL=lowR=envelope=0.0;reductionGain=1.0;}
    void set(float frequency,float thresholdDb,float maxReductionDb,float releaseMs) noexcept {
        const double cutoff=std::clamp((double)frequency,1500.,sampleRate*.45);
        lowPole=std::exp(-2.*pi*cutoff/sampleRate);
        threshold=dbToGain(clamp(thresholdDb,-60.f,-6.f));
        minimumGain=dbToGain(-clamp(maxReductionDb,0.f,24.f));
        attackPole=std::exp(-1./(sampleRate*.0015));
        releasePole=std::exp(-1./(sampleRate*.001*clamp(releaseMs,10.f,600.f)));
    }
    std::array<float,2> process(float left,float right) noexcept {
        const double l=clamp(left,-8.f,8.f),r=clamp(right,-8.f,8.f);
        lowL=lowPole*lowL+(1.-lowPole)*l;
        lowR=lowPole*lowR+(1.-lowPole)*r;
        const double highL=l-lowL,highR=r-lowR;
        const double detector=std::max(std::abs(highL),std::abs(highR));
        const double detectorPole=detector>envelope?attackPole:releasePole;
        envelope=detectorPole*envelope+(1.-detectorPole)*detector;
        const double target=envelope>threshold
            ?std::max(minimumGain,threshold/std::max(envelope,1.e-12)):1.0;
        const double pole=target<reductionGain?attackPole:releasePole;
        reductionGain=pole*reductionGain+(1.-pole)*target;
        if(std::abs(lowL)<1.e-25)lowL=0.;
        if(std::abs(lowR)<1.e-25)lowR=0.;
        if(envelope<1.e-25)envelope=0.;
        const double outL=l-highL*(1.-reductionGain);
        const double outR=r-highR*(1.-reductionGain);
        return {std::isfinite(outL)?(float)outL:0.f,
                std::isfinite(outR)?(float)outR:0.f};
    }
};

// A modest decorrelated stereo double for isolated adlibs. It is NOT a true pitch shifter.
struct AdlibDoubler {
    std::array<std::vector<float>,2> memory;
    size_t head=0;
    double rate=48000,phase=0;
    float widthSmooth=.5f,depthSmooth=.5f,motionSmooth=.5f,intensitySmooth=.5f;
    float smoothingStep=.001f;
    void prepare(double sr) {
        rate=std::max(8000.,sr);head=0;phase=0;
        smoothingStep=1.f-(float)std::exp(-1./(rate*.012));
        widthSmooth=depthSmooth=motionSmooth=intensitySmooth=.5f;
        for(auto& v:memory)v.assign((size_t)(rate*.09)+4,0.f);
    }
    void reset() noexcept {for(auto& v:memory)std::fill(v.begin(),v.end(),0.f);head=0;phase=0;}
    std::array<float,2> process(float l,float r,float width,float depth,float rateControl,float intensity) noexcept {
        if(memory[0].empty())return {l,r};
        // Dezipper modulated read heads; abrupt delay jumps make adlib clicks.
        widthSmooth+=smoothingStep*(clamp(width,0.f,1.f)-widthSmooth);
        depthSmooth+=smoothingStep*(clamp(depth,0.f,1.f)-depthSmooth);
        motionSmooth+=smoothingStep*(clamp(rateControl,0.f,1.f)-motionSmooth);
        intensitySmooth+=smoothingStep*(clamp(intensity,0.f,1.f)-intensitySmooth);
        for(auto* v:{&widthSmooth,&depthSmooth,&motionSmooth,&intensitySmooth})
            if(std::abs(*v)<1.e-20f)*v=0.f;
        const float baseMs=12.f+24.f*depthSmooth;
        const float modMs=(1.f+4.f*widthSmooth)*(.3f+1.4f*intensitySmooth);
        phase+=2.*pi*(.13+2.*motionSmooth)/rate;
        if(phase>=2*pi)phase-=2*pi;
        const float safeL=clamp(l,-8.f,8.f),safeR=clamp(r,-8.f,8.f);
        memory[0][head]=std::abs(safeL)>1.e-20f?safeL:0.f;
        memory[1][head]=std::abs(safeR)>1.e-20f?safeR:0.f;
        std::array<float,2> out{};
        for(int c=0;c<2;++c) {
            const double delay=rate*.001*(baseMs+modMs*std::sin(phase+(c==0?0.:pi*.78)));
            const auto whole=(size_t)delay;
            const float alpha=(float)(delay-whole);
            const auto cap=memory[c].size();
            const float a=memory[c][(head+cap-whole)%cap];
            const float b=memory[c][(head+cap-whole-1)%cap];
            out[(size_t)c]=a+(b-a)*alpha;
        }
        head=(head+1)%memory[0].size();
        return out;
    }
};

// PDC policy for this version: limiter types reserve lookahead even when bypassed.
// This makes each slot's bypass phase-aligned with its active processing path.
inline int seriesLatencySamples(const int* slotTypes,int count,int limiterType,int limiterLatency) noexcept {
    int total=0;
    for(int i=0;i<count;++i)if(slotTypes[i]==limiterType)total+=std::max(0,limiterLatency);
    return total;
}
}
