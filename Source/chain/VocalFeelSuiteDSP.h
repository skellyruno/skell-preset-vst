#pragma once
// Skell-native five-return vocal character engine.
// Concepts studied from user-supplied Vocal Feel source (ROHIT JI); this is
// an independent, JUCE-free integration using existing Skell DSP primitives.
// All memory is prepared off the audio callback. No I/O, mutexes or new/delete
// in set()/process(), and no extra algorithmic delay on the main vocal.
#include "BasicDSP.h"
#include "VoxRackFX.h"
#include "VocalFeelOptoDSP.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace skellpreset::dsp {

struct VocalFeelVoice {
    const char* name;
    float low,mid,high,deess,magic,presence,attitudeLo,attitudeHi,slapMs;
};
inline constexpr std::array<VocalFeelVoice,3> vocalFeelVoices{{
    {"MALE",150.f,1000.f,9000.f,6000.f,2600.f,3200.f,180.f,6500.f,110.f},
    {"MALE 2",120.f,800.f,8000.f,5500.f,2200.f,2700.f,150.f,5500.f,125.f},
    {"FEMALE",220.f,1400.f,11000.f,7500.f,3600.f,4500.f,250.f,8000.f,95.f}
}};
inline constexpr std::array<float,16> vocalFeelNeutral{{
    0.f,.5f,.5f,.5f,.5f,0.f,3500.f/9000.f,0.f,0.f,.7f,
    0.f,0.f,0.f,0.f,0.f,.7f
}};

// Source fader law: 0 is silent, 7 is unity, 10 adds 9dB.
inline float vfFader(float normalized) noexcept {
    const float v=clamp(normalized,0.f,1.f)*10.f;
    return v<.05f?0.f:dbToGain((v-7.f)*3.f);
}
inline float vfReturn(float normalized) noexcept {
    const float v=clamp(normalized,0.f,1.f);
    return v*std::sqrt(v);
}

class VocalFeelLine {
public:
    void prepare(int samples) {
        data.assign((size_t)std::max(32,samples+4),0.f);head=0;
    }
    void reset() noexcept {std::fill(data.begin(),data.end(),0.f);head=0;}
    void push(float sample) noexcept {
        if(data.empty())return;
        data[head]=std::isfinite(sample)?clamp(sample,-8.f,8.f):0.f;
        if(++head==data.size())head=0;
    }
    float read(float samples) const noexcept {
        if(data.empty())return 0.f;
        const float d=std::clamp(samples,1.f,(float)data.size()-3.f);
        const size_t whole=(size_t)d;
        const float fraction=d-(float)whole;
        const size_t a=(head+data.size()-whole)%data.size();
        const size_t b=(a+data.size()-1)%data.size();
        return data[a]+(data[b]-data[a])*fraction;
    }
private:
    std::vector<float> data;
    size_t head=0;
};

inline void vfShelf(Biquad& out,double sampleRate,double hz,float gainDb,bool high) noexcept {
    const double sr=std::clamp(sampleRate,8000.,192000.);
    const double frequency=std::clamp(hz,20.,sr*.44);
    const double A=std::pow(10.,std::clamp((double)gainDb,-12.,12.)/40.);
    const double w=2.*pi*frequency/sr,cosw=std::cos(w),sinw=std::sin(w);
    const double alpha=sinw*.5*std::sqrt(2.);
    const double beta=2.*std::sqrt(A)*alpha;
    const double ap=A+1.,am=A-1.;
    double b0,b1,b2,a0,a1,a2;
    if(high) {
        b0=A*(ap+am*cosw+beta);b1=-2.*A*(am+ap*cosw);
        b2=A*(ap+am*cosw-beta);
        a0=ap-am*cosw+beta;a1=2.*(am-ap*cosw);a2=ap-am*cosw-beta;
    } else {
        b0=A*(ap-am*cosw+beta);b1=2.*A*(am-ap*cosw);
        b2=A*(ap-am*cosw-beta);
        a0=ap+am*cosw+beta;a1=-2.*(am+ap*cosw);a2=ap+am*cosw-beta;
    }
    out.b0=b0/a0;out.b1=b1/a0;out.b2=b2/a0;
    out.a1=a1/a0;out.a2=a2/a0;
}

class VocalFeelSuite final {
public:
    static constexpr int latencySamples() noexcept {return 0;}
    void prepare(double sampleRate) {
        sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,8000.,192000.);
        for(auto* filter:{&lo,&mid,&hi,&magicHp,&magicPeak,&slapHp,&slapLp,
                           &attackBp,&attitudeHp,&attitudeLp,&presenceBp,&presenceAir})
            *filter=Biquad{};
        deess.prepare(sr);opto.prepare(sr);
        magicCompressor.prepare(sr);attitudeCompressor.prepare(sr);
        presenceCompressor.prepare(sr);
        magicCompressor.set(-34.f,8.f,4.f,90.f,0.f);
        attitudeCompressor.set(-30.f,12.f,1.5f,60.f,0.f);
        presenceCompressor.set(-36.f,5.f,3.f,110.f,0.f);
        doublerL.prepare((int)(sr*.05));doublerR.prepare((int)(sr*.05));
        slap.prepare((int)(sr*.21));
        room.prepare(sr);
        room.set(.32f,.52f,25.f,180.f);
        phaseL=0.f;phaseR=.37f;slapFeedback=0.f;roomTailActive=0;
        fastEnv=slowEnv=0.f;
        fastAttack=pole(2.f);fastRelease=pole(18.f);
        slowAttack=pole(35.f);slowRelease=pole(120.f);
        outputSlew=1.f-pole(16.f);
        chainG.fill(0.f);mainG=masterG=1.f;sensG=1.f;
        cached.fill(-5.f);p=vocalFeelNeutral;lastProfile=-1;
        beginBlock();
        set(vocalFeelNeutral);
    }
    void set(const std::array<float,16>& parameters) noexcept {
        std::array<float,16> next{};
        for(size_t i=0;i<next.size();++i)next[i]=clamp(parameters[i],0.f,1.f);
        p=next;
        const int voice=std::clamp((int)std::lround(p[0]*2.f),0,2);
        if(voice!=lastProfile || next[2]!=cached[2] ||
           next[3]!=cached[3] || next[4]!=cached[4] ||
           next[6]!=cached[6]) {
            const auto& v=vocalFeelVoices[(size_t)voice];
            vfShelf(lo,sr,v.low,(p[2]-.5f)*24.f,false);
            mid.setPeak(sr,v.mid,(p[3]-.5f)*24.f,.9);
            vfShelf(hi,sr,v.high,(p[4]-.5f)*24.f,true);
            magicHp.setHighPass(sr,150.f);
            magicPeak.setPeak(sr,v.magic,6.f,.7);
            slapHp.setHighPass(sr,300.f);slapLp.setLowPass(sr,4000.f);
            attackBp.setPeak(sr,2500.f,4.f,.55);
            attitudeHp.setHighPass(sr,v.attitudeLo);
            attitudeLp.setLowPass(sr,v.attitudeHi);
            presenceBp.setPeak(sr,v.presence,6.f,.8);
            vfShelf(presenceAir,sr,std::min(9000.f,(float)sr*.42f),4.f,true);
            lastProfile=voice;
        }
        // The existing Skell SplitDeEsser attenuates only the detected
        // sibilance band, preserving the rest of the voice without a dry PDC.
        // Avoid expensive exp/pow calls every audio block when the
        // host parameters have not moved. Both engines keep their envelopes.
        if(next[5]!=cached[5] || next[6]!=cached[6]) {
            const float cutoff=3000.f+9000.f*p[6];
            deess.set(cutoff,-10.f-40.f*p[5],6.f+12.f*p[5],60.f);
        }
        if(next[7]!=cached[7] || next[8]!=cached[8] ||
           next[0]!=cached[0])
            opto.set(p[7],p[8],p[0],1.f);
        cached=next;
        for(int k=0;k<5;++k)
            targetChain[(size_t)k]=vfReturn(p[(size_t)(10+k)]);
        targetSens=dbToGain((p[1]-.5f)*24.f);
        targetMain=vfFader(p[9]);targetMaster=vfFader(p[15]);
        slapDelay=(float)(vocalFeelVoices[(size_t)voice].slapMs*.001*sr);
        doubleIncL=.23f/(float)sr;doubleIncR=.31f/(float)sr;
        doubleOffsetL=.011f*(float)sr;doubleOffsetR=.0165f*(float)sr;
        doubleDepth=.0005f*(float)sr;
    }
    void beginBlock() noexcept {
        chainEnergy.fill(0.);frames=0;compMax=0.f;deessMax=0.f;
    }
    std::array<float,2> process(float sampleL,float sampleR) noexcept {
        float l=clamp(sampleL,-8.f,8.f),r=clamp(sampleR,-8.f,8.f);
        sensG+=(targetSens-sensG)*outputSlew;
        l*=sensG;r*=sensG;
        if(p[2]!=.5f) {l=lo.process(l,0);r=lo.process(r,1);}
        if(p[3]!=.5f) {l=mid.process(l,0);r=mid.process(r,1);}
        if(p[4]!=.5f) {l=hi.process(l,0);r=hi.process(r,1);}
        if(p[5]>0.0001f) {
            const auto result=deess.process(l,r);l=result[0];r=result[1];
            deessMax=std::max(deessMax,-gainToDb((float)deess.reductionGain));
        }
        const float sourceL=l,sourceR=r,sourceMono=.5f*(l+r);
        if(p[7]>.0001f) {
            const auto compressed=opto.process(l,r);
            l=compressed[0];r=compressed[1];
            compMax=std::max(compMax,opto.gainReductionDb());
        }
        mainG+=(targetMain-mainG)*outputSlew;
        masterG+=(targetMaster-masterG)*outputSlew;
        for(int k=0;k<5;++k)
            chainG[(size_t)k]+=(targetChain[(size_t)k]-chainG[(size_t)k])*outputSlew;
        float outputL=l*mainG,outputR=r*mainG;
        std::array<float,2> mixed{};
        // MAGIC: bright compressed voice + stereo micro-double.
        if(chainG[0]>1.e-6f){
            float ml=magicPeak.process(magicHp.process(sourceL,0),0);
            float mr=magicPeak.process(magicHp.process(sourceR,1),1);
            const float detector=std::max(std::abs(ml),std::abs(mr));
            const float compression=magicCompressor.gain(detector);
            ml*=compression*2.7f;mr*=compression*2.7f;
            doublerL.push(.5f*(ml+mr));doublerR.push(.5f*(ml+mr));
            phaseL+=doubleIncL;phaseR+=doubleIncR;
            if(phaseL>=1.f)phaseL-=1.f;if(phaseR>=1.f)phaseR-=1.f;
            // Use single-precision modulation on the audio thread. A
            // double-to-float narrowing here raised MSVC /W4 concerns and
            // the computed delay needs only sub-sample float precision.
            constexpr float phaseTwoPi=6.2831853071795864769f;
            const float a=doublerL.read(doubleOffsetL+
                doubleDepth*std::sin(phaseTwoPi*phaseL));
            const float b=doublerR.read(doubleOffsetR+
                doubleDepth*std::sin(phaseTwoPi*phaseR));
            mixed[0]=(.55f*ml+.6f*a)*.37f*chainG[0];
            mixed[1]=(.55f*mr+.6f*b)*.37f*chainG[0];
            outputL+=mixed[0];outputR+=mixed[1];chainEnergy[0]+=.5*((double)mixed[0]*mixed[0]+(double)mixed[1]*mixed[1]);
        }else {
            doublerL.push(0.f);doublerR.push(0.f);
        }
        // SPACE: voice-specific slap into a short damped stereo plate.
        if(chainG[1]>1.e-6f){
            const float echoInput=slapLp.process(slapHp.process(sourceMono+.22f*slapFeedback,0),0);
            slap.push(echoInput);
            const float tapL=slap.read(slapDelay),tapR=slap.read(slapDelay*1.09f);
            slapFeedback=tapL;
            const auto plate=room.process(sourceL,sourceR);
            mixed[0]=(.75f*tapL+.55f*plate[0])*.92f*chainG[1];
            mixed[1]=(.75f*tapR+.55f*plate[1])*.92f*chainG[1];
            outputL+=mixed[0];outputR+=mixed[1];chainEnergy[1]+=.5*((double)mixed[0]*mixed[0]+(double)mixed[1]*mixed[1]);
        }else{
            slap.push(0.f);slapFeedback=0.f;
            // Continue draining the feedback network while a fader fades.
            // No abrupt tail is summed when the return is fully muted.
            if(roomTailActive>0) {room.process(0.f,0.f);--roomTailActive;}
        }
        if(chainG[1]>1.e-6f)roomTailActive=(int)(sr*1.8);
        // ATTACK: transient difference of two envelopes on the articulation band.
        if(chainG[2]>1.e-6f){
            const float a=attackBp.process(sourceL,0),b=attackBp.process(sourceR,1);
            const float detector=std::max(std::abs(a),std::abs(b));
            fastEnv+=(detector-fastEnv)*(detector>fastEnv?1.f-fastAttack:1.f-fastRelease);
            slowEnv+=(detector-slowEnv)*(detector>slowEnv?1.f-slowAttack:1.f-slowRelease);
            const float transient=clamp(fastEnv/(slowEnv+1.e-5f)-1.f,0.f,2.5f);
            mixed[0]=a*transient*.48f*chainG[2];
            mixed[1]=b*transient*.48f*chainG[2];
            outputL+=mixed[0];outputR+=mixed[1];chainEnergy[2]+=.5*((double)mixed[0]*mixed[0]+(double)mixed[1]*mixed[1]);
        }
        // ATTITUDE: deliberately driven, heavily compressed voice mids.
        if(chainG[3]>1.e-6f){
            const float a=attitudeLp.process(attitudeHp.process(sourceL,0),0);
            const float b=attitudeLp.process(attitudeHp.process(sourceR,1),1);
            const float driveL=std::tanh(3.f*a),driveR=std::tanh(3.f*b);
            const float attenuation=attitudeCompressor.gain(
                std::max(std::abs(driveL),std::abs(driveR)));
            mixed[0]=driveL*attenuation*.60f*chainG[3];
            mixed[1]=driveR*attenuation*.60f*chainG[3];
            outputL+=mixed[0];outputR+=mixed[1];chainEnergy[3]+=.5*((double)mixed[0]*mixed[0]+(double)mixed[1]*mixed[1]);
        }
        // PRESENCE: voice-profile articulation with smooth upper air.
        if(chainG[4]>1.e-6f){
            float a=presenceBp.process(sourceL,0),b=presenceBp.process(sourceR,1);
            const float attenuation=presenceCompressor.gain(
                std::max(std::abs(a),std::abs(b)));
            a=presenceAir.process(std::tanh(a*attenuation*2.f),0);
            b=presenceAir.process(std::tanh(b*attenuation*2.f),1);
            mixed[0]=a*.36f*chainG[4];mixed[1]=b*.36f*chainG[4];
            outputL+=mixed[0];outputR+=mixed[1];chainEnergy[4]+=.5*((double)mixed[0]*mixed[0]+(double)mixed[1]*mixed[1]);
        }
        ++frames;
        outputL=clamp(outputL*masterG,-16.f,16.f);
        outputR=clamp(outputR*masterG,-16.f,16.f);
        return {outputL,outputR};
    }
    float compressorReductionDb() const noexcept {return compMax;}
    float deesserReductionDb() const noexcept {return deessMax;}
    float chainRms(int chain) const noexcept {
        if(chain<0||chain>=5 || frames<=0)return 0.f;
        return (float)std::sqrt(chainEnergy[(size_t)chain]/(double)frames);
    }
private:
    float pole(float milliseconds) const noexcept {
        return (float)std::exp(-1000./(std::max(.1f,milliseconds)*sr));
    }
    double sr=48000.;
    std::array<float,16> p=vocalFeelNeutral,cached{};
    int lastProfile=-1;
    Biquad lo,mid,hi,magicHp,magicPeak,slapHp,slapLp,attackBp;
    Biquad attitudeHp,attitudeLp,presenceBp,presenceAir;
    SplitDeEsser deess;
    VocalFeelOpto opto;
    Compressor magicCompressor,attitudeCompressor,presenceCompressor;
    VocalFeelLine doublerL,doublerR,slap;
    VoxRackRoom room;
    float phaseL=0.f,phaseR=.37f,slapFeedback=0.f;
    float fastEnv=0.f,slowEnv=0.f,fastAttack=.99f,fastRelease=.99f;
    float slowAttack=.999f,slowRelease=.999f,outputSlew=.001f;
    float sensG=1.f,mainG=1.f,masterG=1.f,targetSens=1.f,targetMain=1.f,targetMaster=1.f;
    float slapDelay=4800.f,doubleIncL=0.f,doubleIncR=0.f,doubleOffsetL=0.f,doubleOffsetR=0.f,doubleDepth=0.f;
    std::array<float,5> targetChain{},chainG{};
    // Equal-power two-channel RMS, matching both stereo return outputs.
    std::array<double,5> chainEnergy{};
    int frames=0,roomTailActive=0;
    float compMax=0.f,deessMax=0.f;
};

} // namespace skellpreset::dsp
