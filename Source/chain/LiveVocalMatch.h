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
    bool valid() const noexcept {
        return std::isfinite(elapsed)&&std::isfinite(active)&&std::isfinite(rmsDb)&&std::isfinite(peak)&&std::isfinite(lowDb)&&std::isfinite(highDb)&&std::isfinite(crest)&&std::isfinite(clipped)&&elapsed>=5.99f&&active>=3.99f&&rmsDb>=-50.f&&rmsDb<=-.5f&&peak>0.f&&clipped>=0.f&&clipped<.005f;
    }
};
class Analyzer {
public:
    void prepare(double rate) noexcept {
        *this=Analyzer{};
        sr=std::clamp(std::isfinite(rate)?rate:48000.,8000.,192000.);
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
        Profile p;p.elapsed=(float)(frames/sr);p.active=(float)(activeFrames/sr);
        if(activeFrames){
            p.rmsDb=db(activeEnergy/activeFrames);p.peak=(float)peak;
            p.lowDb=db(lowEnergy/std::max(1.e-20,activeEnergy));
            p.highDb=db(highEnergy/std::max(1.e-20,activeEnergy));
            p.crest=std::max(0.f,db(peak*peak)-p.rmsDb);
            p.clipped=(float)((double)(clipped+bad)/activeFrames);
        }
        return p;
    }
private:
    static float db(double energy) noexcept {return (float)(10.*std::log10(std::max(1.e-12,energy)));}
    void sample(float l,float r) noexcept {
        ++frames;const std::array<double,2> x{{l,r}};
        const double pk=std::max(std::abs(x[0]),std::abs(x[1]));
        winPeak=std::max(winPeak,pk);if(pk>=.999)++winClip;
        for(size_t k=0;k<2;++k){low[k]+=lo*(x[k]-low[k]);high[k]+=hi*(x[k]-high[k]);const double h=x[k]-high[k];winEnergy+=.5*x[k]*x[k];winLow+=.5*low[k]*low[k];winHigh+=.5*h*h;}
        if(++winFrames>=window){
            if(winEnergy/window>=1.e-5){activeFrames+=winFrames;activeEnergy+=winEnergy;lowEnergy+=winLow;highEnergy+=winHigh;peak=std::max(peak,winPeak);clipped+=winClip;}
            winFrames=winClip=0;winEnergy=winLow=winHigh=winPeak=0.;
        }
    }
    double sr=48000,lo=0,hi=0;int window=960,winFrames=0,winClip=0;
    std::uint64_t frames=0,activeFrames=0,clipped=0,bad=0;
    std::array<double,2> low{},high{};
    double winEnergy=0,winLow=0,winHigh=0,winPeak=0,activeEnergy=0,lowEnergy=0,highEnergy=0,peak=0;
};
struct Recipe {float low=0,high=0,threshold=-24,ratio=2,makeup=0;bool deess=false;float output=-3;};
struct Audio {double rate=48000.;std::vector<float> stereo;size_t frames() const noexcept {return stereo.size()/2;}};
inline bool comparable(const Profile& dry,const Profile& ref) noexcept {
    return dry.valid()&&ref.valid()&&(std::abs(dry.lowDb-ref.lowDb)>1.5f||std::abs(dry.highDb-ref.highDb)>1.5f||std::abs(dry.crest-ref.crest)>2.f);
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
        const std::array<float,8> data{{p.elapsed,p.active,p.rmsDb,p.peak,p.lowDb,p.highDb,p.crest,p.clipped}};
        for(size_t k=0;k<data.size();++k)values[k].store(data[k],std::memory_order_relaxed);
        published.store(n,std::memory_order_release);
        if(analyzer.finished()){completedRate=rate;completedFrames=(size_t)after;finished.store(n,std::memory_order_release);}
    }
    bool read(Profile& p,bool& done) const noexcept {
        const auto n=command.load(std::memory_order_acquire);
        done=false;if(!(n&1u)||published.load(std::memory_order_acquire)!=n){p={};return false;}
        done=finished.load(std::memory_order_acquire)==n;
        std::array<float,8> v{};for(size_t k=0;k<v.size();++k)v[k]=values[k].load(std::memory_order_relaxed);
        p={v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]};return true;
    }
private:
    static_assert(std::atomic<float>::is_always_lock_free,"Match snapshots require lock-free float atomics");
    static constexpr size_t maxFrames=192000u*20u;
    std::unique_ptr<float[]> pcm;size_t completedFrames=0;double completedRate=48000.;
    Analyzer analyzer;unsigned seen=0,lastWindow=0;double lastRate=0;
    std::atomic<unsigned> command{0},published{0},finished{0};
    std::array<std::atomic<float>,8> values{};
};
}
