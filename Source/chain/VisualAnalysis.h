#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace skellpreset::visual {
// UI-only pitch estimation on a real, box-filtered, downsampled history.
// No pitch quantization or correction is performed by this measurement.
class PitchHistory final {
public:
    static constexpr int capacity=2048;
    using Samples=std::array<float,capacity>;
    void reset() noexcept {
        count=0;sum=0.;used=0;
        for(auto& x:data)x.store(0.f,std::memory_order_relaxed);
        published.store(0,std::memory_order_relaxed);
    }
    void push(const float* l,const float* r,int n,double sr) noexcept {
        if(!l || n<=0)return;
        const int stride=std::max(1,(int)std::ceil(std::clamp(sr,8000.,384000.)/12000.));
        for(int j=0;j<n;++j){
            const float a=std::isfinite(l[j])?std::clamp(l[j],-8.f,8.f):0.f;
            const float b=r?(std::isfinite(r[j])?std::clamp(r[j],-8.f,8.f):0.f):a;
            sum+=.5*(a+b);
            if(++used>=stride){
                data[count%capacity].store((float)(sum/used),std::memory_order_relaxed);
                ++count;used=0;sum=0.;
            }
        }
        published.store(count,std::memory_order_relaxed);
        rate.store((float)(sr/stride),std::memory_order_relaxed);
    }
    bool snapshot(Samples& out,float& sr) const noexcept {
        const auto end=published.load(std::memory_order_relaxed);
        sr=rate.load(std::memory_order_relaxed);
        if(end<capacity)return false;
        for(int j=0;j<capacity;++j)
            out[(size_t)j]=data[(end-capacity+(unsigned)j)%capacity].load(std::memory_order_relaxed);
        return true;
    }
private:
    std::array<std::atomic<float>,capacity> data{};
    std::atomic<unsigned> published{0};
    std::atomic<float> rate{12000.f};
    unsigned count=0;int used=0;double sum=0.;
};
struct PitchReading {float hz=0.f,confidence=0.f,midi=0.f;};
inline PitchReading estimatePitch(const PitchHistory::Samples& x,float sr) noexcept {
    if(!std::isfinite(sr) || sr<4000.f || sr>16000.f)return {};
    constexpr int window=1024;
    const int minLag=std::max(2,(int)(sr/1100.f));
    const int maxLag=std::min(511,(int)(sr/65.f));
    if(minLag>=maxLag)return {};
    double mean=0.,energy=0.;
    for(int j=0;j<window;++j){const float a=x[(size_t)(j+512)];if(!std::isfinite(a))return {};mean+=a;energy+=a*a;}
    if(energy/window-(mean/window)*(mean/window)<1.e-7)return {};
    std::array<double,513> diff{};double cumulative=0.;
    const int start=PitchHistory::capacity-window-maxLag-1;
    for(int lag=1;lag<=maxLag+1;++lag){
        double d=0.;
        for(int j=0;j<window;++j){
            const double a=x[(size_t)(start+j)],b=x[(size_t)(start+j+lag)];
            if(!std::isfinite(a)||!std::isfinite(b))return {};
            const double delta=a-b;d+=delta*delta;
        }
        cumulative+=d;diff[(size_t)lag]=cumulative>1.e-20?d*lag/cumulative:1.;
    }
    for(int lag=minLag;lag<=maxLag;++lag){
        if(diff[(size_t)lag]>.16 || diff[(size_t)lag]>diff[(size_t)lag+1])continue;
        const double a=diff[(size_t)lag-1],b=diff[(size_t)lag],c=diff[(size_t)lag+1];
        const double den=a-2.*b+c;
        const double offset=std::abs(den)>1.e-12?std::clamp(.5*(a-c)/den,-.5,.5):0.;
        const float hz=(float)(sr/(lag+offset));
        if(hz<65.f || hz>1100.f)return {};
        return {hz,(float)std::clamp(1.-b,0.,1.),69.f+12.f*std::log2(hz/440.f)};
    }
    return {};
}
// Same static core law as the legacy saturator. Rack dry/wet and smoothing
// are deliberately excluded from the labelled core-transfer display.
inline float saturatorCore(float x,float drive,float blend,float denominator) noexcept {
    return std::tanh(x*drive*blend)/denominator;
}
struct TailReading {
    bool armed=false,active=false;
    float elapsed=0.f,reference=0.f,dropDb=0.f;
    void update(float input,float wet,float seconds) noexcept {
        if(!std::isfinite(input)||!std::isfinite(wet)||!std::isfinite(seconds))return;
        if(input>.001f){
            armed=true;active=false;elapsed=0.f;reference=0.f;dropDb=0.f;return;
        }
        if(!armed)return;
        if(!active && wet>.0001f){active=true;reference=wet;elapsed=0.f;}
        if(active){
            elapsed+=std::clamp(seconds,0.f,.25f);
            reference=std::max(reference,wet);
            dropDb=std::clamp(20.f*std::log10(std::max(1.e-8f,wet)/std::max(1.e-8f,reference)),-100.f,0.f);
        }
    }
};
}
