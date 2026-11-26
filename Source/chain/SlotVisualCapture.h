#pragma once
#include "LiveSpectrum.h"
#include "VisualAnalysis.h"
#include "../../SkellCore/include/SkellCore/Telemetry.h"
namespace skellpreset::visual {
// A single on-demand stereo measurement pair per processor. The message
// thread requests a slot; only the audio callback resets/writes capture.
// Generation tags prevent showing the previous effect after switching slots.
class SlotVisualCapture final {
public:
    using Samples=std::array<float,LiveSpectrumCapture::capacity>;
    static constexpr int stereoPoints=128, maxBlock=8192;
    using Metrics=skellcore::telemetry::LevelMetrics;
    using Bucket=skellcore::telemetry::Bucket;
    using SideInfo=skellcore::telemetry::SideInfo;
    SlotVisualCapture() noexcept {prepare(48000.);}
    void prepare(double sr) noexcept {tap.prepare(sr);invalidate();}
    struct Frame {
        Samples input{},output{};
        PitchHistory::Samples pitchInput{},pitchOutput{};
        float pitchRate=0.f;
        bool pitchReady=false,returnAvailable=false;
        std::array<float,2> returnRms{},tapMs{};
        std::array<float,stereoPoints> mid{},side{};
        float inputRms=0.f,outputRms=0.f,sideRms=0.f,correlation=0.f,leftRms=0.f,rightRms=0.f;
        Metrics sharedInput{},sharedOutput{};
        std::array<Bucket,90> slow{};
        size_t slowCount=0;
        bool sharedValid=false;
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
            input.reset();output.reset();pitchIn.reset();pitchOut.reset();tap.reset();active=next;
            tap.setConsumerActive((active&31u)!=0);
        }
        return (int)(active&31u)-1;
    }
    void invalidate() noexcept {
        published.store(0,std::memory_order_release);
        input.reset();output.reset();pitchIn.reset();pitchOut.reset();tap.reset();
    }
    void captureInput(const float* l,const float* r,int n,bool pitch=false,double sr=48000.) noexcept {
        sequence.fetch_add(1,std::memory_order_acq_rel); // odd while the pair is being written
        input.pushBlock(l,r,n);
        sharedFrames=l && n>0 && n<=maxBlock?n:0;
        if(sharedFrames){std::copy_n(l,n,sharedL.data());std::copy_n(r?r:l,n,sharedR.data());}
        pitchMode=pitch;pitchSampleRate=sr;
        if(pitch)pitchIn.push(l,r,n,sr);
        wetAvailable.store(false,std::memory_order_relaxed);
        inRms.store(rms(l,r,n,false),std::memory_order_relaxed);
    }
    void setReturn(float l,float r,float timeL=0.f,float timeR=0.f) noexcept {
        wetLeft.store(l,std::memory_order_relaxed);wetRight.store(r,std::memory_order_relaxed);
        tapLeft.store(timeL,std::memory_order_relaxed);tapRight.store(timeR,std::memory_order_relaxed);
        wetAvailable.store(true,std::memory_order_relaxed);
    }
    void captureOutput(const float* l,const float* r,int n,const SideInfo& dynamics={}) noexcept {
        if(sharedFrames==n && n>0)tap.process(sharedL.data(),sharedR.data(),l,r,n,dynamics);
        output.pushBlock(l,r,n);
        if(pitchMode)pitchOut.push(l,r,n,pitchSampleRate);
        pitchAvailable.store(pitchMode,std::memory_order_relaxed);
        double ll=0.,rr=0.,lr=0.,ss=0.;
        if(l && n>0){
            for(int i=0;i<n;++i){
                const double a=bounded(l[i]),b=r?bounded(r[i]):a;
                ll+=a*a;rr+=b*b;lr+=a*b;
                const double d=.5*(a-b);ss+=d*d;
            }
            for(int i=0;i<stereoPoints;++i){
                const int index=(int)((std::int64_t)i*(n-1)/(stereoPoints-1));
                const float a=bounded(l[index]),b=r?bounded(r[index]):a;
                midPoints[(size_t)i].store(.5f*(a+b),std::memory_order_relaxed);
                sidePoints[(size_t)i].store(.5f*(a-b),std::memory_order_relaxed);
            }
        }else {
            for(int i=0;i<stereoPoints;++i){
                midPoints[(size_t)i].store(0.f,std::memory_order_relaxed);
                sidePoints[(size_t)i].store(0.f,std::memory_order_relaxed);
            }
        }
        const double count=std::max(1,n);
        outRms.store((float)std::sqrt((ll+rr)*.5/count),std::memory_order_relaxed);
        side.store((float)std::sqrt(ss/count),std::memory_order_relaxed);
        left.store((float)std::sqrt(ll/count),std::memory_order_relaxed);
        right.store((float)std::sqrt(rr/count),std::memory_order_relaxed);
        correlation.store(ll*rr>1.e-24?(float)std::clamp(lr/std::sqrt(ll*rr),-1.,1.):0.f,
                          std::memory_order_relaxed);
        published.store(active,std::memory_order_release);
        sequence.fetch_add(1,std::memory_order_release); // even when both sides are published
    }
    bool read(int slot,Frame& frame) const noexcept {
        if(slot<0 || slot>=16)return false;
        const auto key=requested.load(std::memory_order_acquire);
        if((key&31u)!=(unsigned)slot+1 || published.load(std::memory_order_acquire)!=key)return false;
        const auto before=sequence.load(std::memory_order_acquire);
        if(before&1u)return false;
        input.snapshot(frame.input);output.snapshot(frame.output);
        frame.inputRms=inRms.load(std::memory_order_relaxed);
        frame.outputRms=outRms.load(std::memory_order_relaxed);
        frame.sideRms=side.load(std::memory_order_relaxed);
        frame.leftRms=left.load(std::memory_order_relaxed);frame.rightRms=right.load(std::memory_order_relaxed);
        frame.correlation=correlation.load(std::memory_order_relaxed);
        for(int i=0;i<stereoPoints;++i){
            frame.mid[(size_t)i]=midPoints[(size_t)i].load(std::memory_order_relaxed);
            frame.side[(size_t)i]=sidePoints[(size_t)i].load(std::memory_order_relaxed);
        }
        frame.pitchReady=false;
        if(pitchAvailable.load(std::memory_order_relaxed)){
            float outputRate=0.f;
            frame.pitchReady=pitchIn.snapshot(frame.pitchInput,frame.pitchRate) &&
                pitchOut.snapshot(frame.pitchOutput,outputRate) && outputRate==frame.pitchRate;
        }
        frame.returnAvailable=wetAvailable.load(std::memory_order_relaxed);
        frame.returnRms={wetLeft.load(std::memory_order_relaxed),wetRight.load(std::memory_order_relaxed)};
        frame.tapMs={tapLeft.load(std::memory_order_relaxed),tapRight.load(std::memory_order_relaxed)};
        frame.sharedValid=tap.readInput(frame.sharedInput) && tap.readOutput(frame.sharedOutput) &&
            frame.sharedInput.endSample()==frame.sharedOutput.endSample() && frame.sharedOutput.blockSamples>0;
        frame.slowCount=frame.sharedValid?tap.readSlow(frame.slow.data(),frame.slow.size()):0;
        frame.serial=output.samplesCaptured();
        return sequence.load(std::memory_order_acquire)==before &&
               published.load(std::memory_order_acquire)==key &&
               requested.load(std::memory_order_acquire)==key && frame.serial>0;
    }
private:
    static float bounded(float v) noexcept {return std::isfinite(v)?std::clamp(v,-8.f,8.f):0.f;}
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
    skellcore::telemetry::EffectTelemetry tap;
    std::array<float,maxBlock> sharedL{},sharedR{};
    int sharedFrames=0;
    PitchHistory pitchIn,pitchOut;
    bool pitchMode=false;double pitchSampleRate=48000.;
    std::atomic<bool> pitchAvailable{false},wetAvailable{false};
    std::atomic<float> wetLeft{0},wetRight{0},tapLeft{0},tapRight{0};
    std::atomic<unsigned> requested{0},published{0},sequence{0};
    unsigned active=0; // audio thread only
    std::atomic<float> inRms{0},outRms{0},side{0},left{0},right{0},correlation{0};
    std::array<std::atomic<float>,stereoPoints> midPoints{},sidePoints{};
};
}
