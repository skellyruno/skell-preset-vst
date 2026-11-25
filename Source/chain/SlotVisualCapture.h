#pragma once
#include "LiveSpectrum.h"
namespace skellpreset::visual {
// A single on-demand stereo measurement pair per processor. The message
// thread requests a slot; only the audio callback resets/writes capture.
// Generation tags prevent showing the previous effect after switching slots.
class SlotVisualCapture final {
public:
    using Samples=std::array<float,LiveSpectrumCapture::capacity>;
    struct Frame {
        Samples input{},output{};
        float inputRms=0.f,outputRms=0.f,sideRms=0.f;
        std::uint64_t serial=0;
    };
    void request(int slot) noexcept {
        const unsigned code=slot>=0 && slot<16?(unsigned)slot+1:0;
        const auto old=requested.load(std::memory_order_relaxed);
        if((old&31u)!=code)
            requested.store(((old+32u)&~31u)|code,std::memory_order_release);
    }
    int beginBlock() noexcept {
        const auto next=requested.load(std::memory_order_acquire);
        if(next!=active){
            published.store(0,std::memory_order_release);
            input.reset();output.reset();active=next;
        }
        return (int)(active&31u)-1;
    }
    void invalidate() noexcept {
        published.store(0,std::memory_order_release);
        input.reset();output.reset();
    }
    void captureInput(const float* l,const float* r,int n) noexcept {
        input.pushBlock(l,r,n);
        inRms.store(rms(l,r,n,false),std::memory_order_relaxed);
    }
    void captureOutput(const float* l,const float* r,int n) noexcept {
        output.pushBlock(l,r,n);
        outRms.store(rms(l,r,n,false),std::memory_order_relaxed);
        side.store(rms(l,r,n,true),std::memory_order_relaxed);
        published.store(active,std::memory_order_release);
    }
    bool read(int slot,Frame& frame) const noexcept {
        if(slot<0 || slot>=16)return false;
        const auto key=requested.load(std::memory_order_acquire);
        if((key&31u)!=(unsigned)slot+1 || published.load(std::memory_order_acquire)!=key)return false;
        input.snapshot(frame.input);output.snapshot(frame.output);
        frame.inputRms=inRms.load(std::memory_order_relaxed);
        frame.outputRms=outRms.load(std::memory_order_relaxed);
        frame.sideRms=side.load(std::memory_order_relaxed);
        frame.serial=output.samplesCaptured();
        return published.load(std::memory_order_acquire)==key &&
               requested.load(std::memory_order_acquire)==key && frame.serial>0;
    }
private:
    static float rms(const float* l,const float* r,int n,bool difference) noexcept {
        if(!l || n<=0)return 0.f;
        double energy=0.;
        for(int i=0;i<n;++i){
            const double a=std::isfinite(l[i])?std::clamp(l[i],-8.f,8.f):0.f;
            const double b=r?(std::isfinite(r[i])?std::clamp(r[i],-8.f,8.f):0.f):a;
            const double d=(a-b)*.5;
            energy+=difference?d*d:.5*(a*a+b*b);
        }
        return (float)std::sqrt(energy/n);
    }
    LiveSpectrumCapture input,output;
    std::atomic<unsigned> requested{0},published{0};
    unsigned active=0; // audio thread only
    std::atomic<float> inRms{0},outRms{0},side{0};
};
}
