#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace skellpreset::liveMatch {
// Streaming bounded statistics only. No recording, allocation, network or
// inference in the callback. Active-window energy is not a vocal classifier.
struct Profile {
    float elapsed=0,active=0,rmsDb=-120,peak=0,lowDb=-120,highDb=-120,crest=0,clipped=0;
    float invalid=0,sideDb=-120,correlation=0;
    // Detailed features are worker-only; live capture retains its original cost.
    bool detailed=false;
    std::array<float,6> bands{};
    float envelopeContrast=0,quietDb=-120,activeContrast=0;
    std::array<float,4> envelopeLag{};
    bool finite() const noexcept {
        if(detailed){for(float b:bands)if(!std::isfinite(b))return false;if(!std::isfinite(envelopeContrast)||!std::isfinite(quietDb)||!std::isfinite(activeContrast))return false;for(float v:envelopeLag)if(!std::isfinite(v))return false;}
        return std::isfinite(elapsed)&&std::isfinite(active)&&std::isfinite(rmsDb)&&std::isfinite(peak)&&std::isfinite(lowDb)&&std::isfinite(highDb)&&std::isfinite(crest)&&std::isfinite(clipped)&&std::isfinite(invalid)&&std::isfinite(sideDb)&&std::isfinite(correlation);
    }
    bool valid() const noexcept {
        return finite()&&invalid==0.f&&elapsed>=5.99f&&active>=3.99f&&rmsDb>=-50.f&&rmsDb<=-.5f&&peak>0.f&&clipped>=0.f&&clipped<.005f;
    }
};
class Analyzer {
public:
    void prepare(double rate,bool detail=false) noexcept {
        *this=Analyzer{};
        sr=std::clamp(std::isfinite(rate)?rate:48000.,8000.,192000.);
        detailed=detail;
        if(detailed){constexpr std::array<double,5> hz{{150,400,1000,2500,6000}};for(size_t k=0;k<hz.size();++k)bandPole[k]=1.-std::exp(-6.283185307179586*std::min(hz[k],sr*.42)/sr);}
        window=std::max(1,(int)std::round(sr*.020));
        lo=1.-std::exp(-6.283185307179586*300./sr);
        hi=1.-std::exp(-6.283185307179586*3500./sr);
    }
    void push(const float* l,const float* r,int n) noexcept {
        if(!l||n<=0)return;
        for(int i=0;i<n&&!finished();++i){
            const float a=l[i],b=r?r[i]:a;
            if(!std::isfinite(a)||!std::isfinite(b)){++bad;sample(0,0);}else sample(a,b);
        }
    }
    bool finished() const noexcept {return (frames>=6.*sr&&activeFrames>=4.*sr)||frames>=20.*sr;}
    std::uint64_t frameCount() const noexcept {return frames;}
    unsigned windowCount() const noexcept {return (unsigned)(frames/(std::uint64_t)window);}
    Profile result() const noexcept {
        Profile p;p.elapsed=(float)(frames/sr);p.active=(float)(activeFrames/sr);p.invalid=(float)bad;
        if(activeFrames){
            p.rmsDb=db(activeEnergy/activeFrames);p.peak=(float)peak;
            p.lowDb=db(lowEnergy/std::max(1.e-20,activeEnergy));
            p.highDb=db(highEnergy/std::max(1.e-20,activeEnergy));
            p.crest=std::max(0.f,db(peak*peak)-p.rmsDb);
            p.clipped=(float)((double)clipped/activeFrames);
            p.sideDb=db(sideEnergy/std::max(1.e-20,midEnergy));
            p.detailed=detailed;
            if(detailed){for(size_t k=0;k<p.bands.size();++k)p.bands[k]=db(bandEnergy[k]/std::max(1.e-20,activeEnergy));
                const double activeMean=activeEnvSum/std::max(1u,activeEnvCount);p.activeContrast=(float)std::sqrt(std::max(0.,activeEnvSq/std::max(1u,activeEnvCount)-activeMean*activeMean));
                const double mean=envSum/std::max(1u,envCount);p.envelopeContrast=(float)std::sqrt(std::max(0.,envSq/std::max(1u,envCount)-mean*mean));
                unsigned cumulative=0;const unsigned goal=std::max(1u,envCount/4);for(size_t k=0;k<envelopeHistogram.size();++k){cumulative+=envelopeHistogram[k];if(cumulative>=goal){p.quietDb=std::clamp(-80.f+2.f*(float)k-p.rmsDb,-80.f,0.f);break;}}
                for(size_t k=0;k<p.envelopeLag.size();++k)p.envelopeLag[k]=lagStats[k].correlation();}
            const double denom=std::sqrt(leftEnergy*rightEnergy);
            p.correlation=denom>1.e-20?(float)std::clamp(crossEnergy/denom,-1.,1.):0.f;
        }
        return p;
    }
private:
    static float db(double energy) noexcept {return (float)(10.*std::log10(std::max(1.e-12,energy)));}
    void sample(float l,float r) noexcept {
        ++frames;const std::array<double,2> x{{l,r}};
        const double pk=std::max(std::abs(x[0]),std::abs(x[1]));
        const double m=.5*(x[0]+x[1]),s=.5*(x[0]-x[1]);
        winMid+=m*m;winSide+=s*s;winLeft+=x[0]*x[0];winRight+=x[1]*x[1];winCross+=x[0]*x[1];
        winPeak=std::max(winPeak,pk);if(pk>=.999)++winClip;
        for(size_t k=0;k<2;++k){low[k]+=lo*(x[k]-low[k]);high[k]+=hi*(x[k]-high[k]);const double h=x[k]-high[k];winEnergy+=.5*x[k]*x[k];winLow+=.5*low[k]*low[k];winHigh+=.5*h*h;}
        if(detailed){for(size_t ch=0;ch<2;++ch){double previous=0;for(size_t k=0;k<5;++k){auto& z=bandState[ch][k];z+=bandPole[k]*(x[ch]-z);const double v=z-previous;winBands[k]+=.5*v*v;previous=z;}const double v=x[ch]-previous;winBands[5]+=.5*v*v;}}
        if(++winFrames>=window){
            if(detailed){const double e=winEnergy/window;const double env=std::max(-80.f,db(e));envSum+=env;envSq+=env*env;++envCount;++envelopeHistogram[(size_t)std::clamp((int)((env+80.)*.5),0,40)];
                constexpr std::array<unsigned,4> lags{{3,6,12,24}};for(size_t k=0;k<lags.size();++k)if(envCount>lags[k])lagStats[k].push(env,envRing[(envCount-1+envRing.size()-lags[k])%envRing.size()]);envRing[(envCount-1)%envRing.size()]=env;
                if(e>=1.e-5){activeEnvSum+=env;activeEnvSq+=env*env;++activeEnvCount;for(size_t k=0;k<6;++k)bandEnergy[k]+=winBands[k];}winBands.fill(0.); }
            if(winEnergy/window>=1.e-5){activeFrames+=winFrames;activeEnergy+=winEnergy;lowEnergy+=winLow;highEnergy+=winHigh;peak=std::max(peak,winPeak);clipped+=winClip;midEnergy+=winMid;sideEnergy+=winSide;leftEnergy+=winLeft;rightEnergy+=winRight;crossEnergy+=winCross;}
            winFrames=winClip=0;winEnergy=winLow=winHigh=winPeak=winMid=winSide=winLeft=winRight=winCross=0.;
        }
    }
    bool detailed=false;std::array<double,5> bandPole{};std::array<std::array<double,5>,2> bandState{};std::array<double,6> winBands{},bandEnergy{};
    struct Lag {double x=0,y=0,xx=0,yy=0,xy=0;unsigned n=0;void push(double a,double b) noexcept {x+=a;y+=b;xx+=a*a;yy+=b*b;xy+=a*b;++n;}float correlation() const noexcept {if(n<3)return 0;const double d=std::sqrt(std::max(0.,(xx-x*x/n)*(yy-y*y/n)));return d>1.e-9?(float)std::clamp((xy-x*y/n)/d,-1.,1.):0.f;}};
    std::array<Lag,4> lagStats{};std::array<double,25> envRing{};std::array<unsigned,41> envelopeHistogram{};
    double envSum=0,envSq=0,activeEnvSum=0,activeEnvSq=0;unsigned envCount=0,activeEnvCount=0;
    double sr=48000,lo=0,hi=0;int window=960,winFrames=0,winClip=0;
    std::uint64_t frames=0,activeFrames=0,clipped=0,bad=0;
    std::array<double,2> low{},high{};
    double winEnergy=0,winLow=0,winHigh=0,winPeak=0,activeEnergy=0,lowEnergy=0,highEnergy=0,peak=0;
    double winMid=0,winSide=0,winLeft=0,winRight=0,winCross=0,midEnergy=0,sideEnergy=0,leftEnergy=0,rightEnergy=0,crossEnergy=0;
};
struct EffectChoice {float wet=0;std::array<float,4> p{{.5f,.5f,.5f,.5f}};};
struct Recipe {float low=0,high=0,threshold=-24,ratio=2,makeup=0;bool deess=false;float output=-3;
    int modulation=0; // 0 none, 1 chorus, 2 flanger
    EffectChoice mod,delay,room,rider,harshness,saturation;
};
struct Audio {double rate=48000.;std::vector<float> stereo;size_t frames() const noexcept {return stereo.size()/2;}};
inline bool comparable(const Profile& dry,const Profile& ref) noexcept {
    return dry.valid()&&ref.valid()&&(std::abs(dry.lowDb-ref.lowDb)>1.5f||std::abs(dry.highDb-ref.highDb)>1.5f||std::abs(dry.crest-ref.crest)>2.f||std::abs(dry.rmsDb-ref.rmsDb)>2.f||std::abs(std::max(-60.f,dry.sideDb)-std::max(-60.f,ref.sideDb))>6.f||std::abs(dry.correlation-ref.correlation)>.35f);
}
inline Recipe fit(const Profile& dry,const Profile* ref=nullptr) noexcept {
    Recipe r;
    // Conservative starting voicing, not a trained voice identity model.
    const float lowTarget=ref?ref->lowDb:-8.f,highTarget=ref?ref->highDb:-15.f;
    r.low=std::clamp((lowTarget-dry.lowDb)*.55f,-4.f,3.f);
    r.high=std::clamp((highTarget-dry.highDb)*.45f,-3.f,4.f);
    const float targetCrest=ref?ref->crest:12.f;
    r.ratio=std::clamp(1.5f+(dry.crest-targetCrest)*.23f,1.5f,4.f);
    r.threshold=std::clamp(dry.rmsDb+3.f,-45.f,-9.f);
    r.makeup=std::clamp((-18.f-dry.rmsDb)*.18f,0.f,3.f);
    r.deess=dry.highDb>-10.f && r.high<=0.f;
    return r;
}
// Audio thread owns analyzer, GUI owns command. Odd commands capture, even
// commands cancel. PCM is allocated on the message thread only, never saved.
// Completed generations are immutable until the next message-thread start.
class Capture {
public:
    bool start() noexcept {
        if(!pcm){pcm.reset(new(std::nothrow) float[maxFrames*2]);if(!pcm)return false;}
        const auto n=(command.load(std::memory_order_relaxed)|1u)+2u;command.store(n,std::memory_order_release);return true;
    }
    bool copyCompleted(Audio& out) const {
        const auto n=command.load(std::memory_order_acquire);
        if(!(n&1u)||finished.load(std::memory_order_acquire)!=n||!pcm)return false;
        out.rate=completedRate;out.stereo.assign(pcm.get(),pcm.get()+completedFrames*2);return true;
    }
    void cancel() noexcept {const auto n=command.load(std::memory_order_relaxed);if(n&1u)command.store(n+1u,std::memory_order_release);}
    void push(const float* l,const float* r,int count,double rate) noexcept {
        const auto n=command.load(std::memory_order_acquire);
        if(!(n&1u)||finished.load(std::memory_order_acquire)==n||!std::isfinite(rate)||rate<8000.||rate>192000.)return;
        if(n!=seen||rate!=lastRate){seen=n;lastRate=rate;lastWindow=0;analyzer.prepare(rate);finished.store(0,std::memory_order_release);published.store(0,std::memory_order_release);}
        if(analyzer.finished())return;
        const auto before=analyzer.frameCount();
        analyzer.push(l,r,count);
        const auto after=std::min<std::uint64_t>(analyzer.frameCount(),maxFrames);
        for(auto frame=before;frame<after;++frame){const auto i=(size_t)(frame-before);pcm[(size_t)frame*2]=l[i];pcm[(size_t)frame*2+1]=r?r[i]:l[i];}
        const auto win=analyzer.windowCount();
        if(win==lastWindow&&!analyzer.finished())return;
        lastWindow=win;const auto p=analyzer.result();
        const std::array<float,11> data{{p.elapsed,p.active,p.rmsDb,p.peak,p.lowDb,p.highDb,p.crest,p.clipped,p.invalid,p.sideDb,p.correlation}};
        for(size_t k=0;k<data.size();++k)values[k].store(data[k],std::memory_order_relaxed);
        published.store(n,std::memory_order_release);
        if(analyzer.finished()){completedRate=rate;completedFrames=(size_t)after;finished.store(n,std::memory_order_release);}
    }
    bool read(Profile& p,bool& done) const noexcept {
        const auto n=command.load(std::memory_order_acquire);
        done=false;if(!(n&1u)||published.load(std::memory_order_acquire)!=n){p={};return false;}
        done=finished.load(std::memory_order_acquire)==n;
        std::array<float,11> v{};for(size_t k=0;k<v.size();++k)v[k]=values[k].load(std::memory_order_relaxed);
        p={v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10]};return true;
    }
private:
    static_assert(std::atomic<float>::is_always_lock_free,"Match snapshots require lock-free float atomics");
    static constexpr size_t maxFrames=192000u*20u;
    std::unique_ptr<float[]> pcm;size_t completedFrames=0;double completedRate=48000.;
    Analyzer analyzer;unsigned seen=0,lastWindow=0;double lastRate=0;
    std::atomic<unsigned> command{0},published{0},finished{0};
    std::array<std::atomic<float>,11> values{};
};
}
