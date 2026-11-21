#include "../Source/chain/AdvancedVocalDSP.h"
#include <cassert>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#if defined(NDEBUG)
#error "EQ cache parity assertions must remain enabled in Release"
#endif

using namespace skellpreset::dsp;

// Independent *uncached* baseline of the original EightBandTone setup path.
// Both versions use identical SmoothedBiquad processing and smoothing.
struct UncachedEightBandTone {
    std::array<SmoothedBiquad,8> bell{};
    SmoothedBiquad lowCut,highCut;
    void prepare(double sr) noexcept {
        for(auto& b:bell)b.prepare(sr);
        lowCut.prepare(sr);highCut.prepare(sr);
    }
    void setCuts(double sr,float low,float high) noexcept {
        const float lo=std::isfinite(low)?std::clamp(low,0.f,1.f):0.f;
        const float hi=std::isfinite(high)?std::clamp(high,0.f,1.f):0.f;
        if(lo<=.0001f)lowCut.aim(Biquad{});
        else lowCut.setHighPass(sr,EightBandTone::lowCutFrequency(lo));
        if(hi<=.0001f)highCut.aim(Biquad{});
        else highCut.setLowPass(sr,EightBandTone::highCutFrequency(hi));
    }
    void set(double sr,const std::array<float,16>& p,
             const std::array<float,8>& q) noexcept {
        if(!std::isfinite(sr)||sr<8000.)sr=48000.;
        for(int i=0;i<8;++i) {
            const auto freqVal=p[(size_t)i*2];
            const auto gainVal=p[(size_t)i*2+1];
            const float freqNorm=std::isfinite(freqVal)?std::clamp(freqVal,0.f,1.f):.5f;
            const float gainNorm=std::isfinite(gainVal)?std::clamp(gainVal,0.f,1.f):.5f;
            const float frequency=EightBandTone::edges[(size_t)i]*
                std::pow(EightBandTone::edges[(size_t)i+1]/
                         EightBandTone::edges[(size_t)i],freqNorm);
            const float db=(gainNorm-.5f)*30.f;
            bell[(size_t)i].setPeak(sr,frequency,db,
                                    EightBandTone::qFromNormalized(q[(size_t)i]));
        }
    }
    std::array<float,2> process(float l,float r) noexcept {
        const auto hp=lowCut.process(l,r);
        l=hp[0];r=hp[1];
        for(auto& band:bell) {
            const auto y=band.process(l,r);
            l=y[0];r=y[1];
        }
        return highCut.process(l,r);
    }
};
int main() {
    double worst=0.;
    for(const double sr:{44100.,48000.,96000.}) {
        for(const int blockSize:{16,64,256,512}) {
            EightBandTone cached;
            UncachedEightBandTone uncached;
            cached.prepare(sr);uncached.prepare(sr);
            std::array<float,16> p{};
            std::array<float,8> q{};
            p.fill(.5f);q.fill(.5f);
            float low=0.f,high=0.f;
            constexpr int frames=10000;
            for(int base=0;base<frames;base+=blockSize) {
                const int block=base/blockSize;
                if(block%7==0) {
                    // Host automation in several independent bands, plus
                    // recurring bypass/restore of the HP and LP filters.
                    p[(size_t)(block/7%8)*2]=(float)((block%37)+1)/38.f;
                    p[(size_t)(block/7%8)*2+1]=.20f+.60f*(float)(block%13)/12.f;
                    q[(size_t)(block/7%8)]=.20f+.60f*(float)(block%9)/8.f;
                    low=(block%3==0)?0.f:.20f+(float)(block%5)*.12f;
                    high=(block%4==0)?0.f:.10f+(float)(block%7)*.11f;
                }
                cached.set(sr,p,q);
                uncached.set(sr,p,q);
                cached.setCuts(sr,low,high);
                uncached.setCuts(sr,low,high);
                for(int n=base;n<std::min(frames,base+blockSize);++n) {
                    const float l=.09f*std::sin((float)(n*.054))
                                 +.017f*std::sin((float)(n*.611));
                    const float r=.072f*std::sin((float)(n*.083))
                                 -.011f*std::sin((float)(n*.31));
                    const auto a=cached.process(l,r);
                    const auto b=uncached.process(l,r);
                    for(int ch=0;ch<2;++ch) {
                        assert(std::isfinite(a[(size_t)ch])&&std::isfinite(b[(size_t)ch]));
                        const double err=std::abs((double)a[(size_t)ch]-b[(size_t)ch]);
                        worst=std::max(worst,err);
                        assert(err<=2.e-6); // near bit-identical sonic parity
                    }
                }
            }
        }
    }
    std::cout<<"PASS 8-band cached-vs-original multi-rate automated EQ parity, worst delta="
             <<worst<<"\n";
}
