#include "../Source/chain/ModulationFX.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "Modulation DSP tests require enabled assertions"
#endif
using namespace skellpreset::dsp;
static bool finite(const std::array<float,2>& s){return std::isfinite(s[0])&&std::isfinite(s[1]);}
// Compare the optimized chorus oscillator against independent direct sine
// taps, including wraparound, full voice counts and stereo phase offset.
static void chorusReference(double sr,float voices,float spread) {
    VocalChorus chorus;
    constexpr float rate=.73f,depthMs=5.4f;
    chorus.set(rate,depthMs,voices,spread);
    chorus.prepare(sr);
    std::array<modulation_detail::FractionalLine,2> reference;
    for(auto& line:reference)line.prepare(sr,55.f);
    double phase=0.;
    for(int i=0;i<(int)(sr*.08);++i) {
        const float left=.39f*std::sin((float)(i*6.283185307179586*229./sr));
        const float right=.24f*std::cos((float)(i*6.283185307179586*311./sr));
        const float input[2]{left,right};
        const auto actual=chorus.process(left,right);
        for(int ch=0;ch<2;++ch) {
            float sum=0.f,weightSum=0.f;
            for(int voice=0;voice<(int)std::ceil(voices);++voice) {
                const float weight=std::clamp(voices-(float)voice,0.f,1.f);
                if(weight<.000001f)continue;
                const double offset=(double)voice/5.+(ch==1?.17*spread:0.);
                const float mod=std::sin((float)(modulation_detail::tau*(phase+offset)));
                const float delay=(float)(sr*.001*16.)+(float)(sr*.001)*depthMs*mod;
                sum+=weight*reference[(size_t)ch].read(delay);
                weightSum+=weight;
            }
            const float expected=sum/std::max(1.f,weightSum);
            assert(std::abs(actual[(size_t)ch]-expected)<5.e-5f);
            reference[(size_t)ch].write(input[ch]);
        }
        phase+=rate/sr;phase-=std::floor(phase);
    }
}

static void fractionalLineAndHostGuards() {
    modulation_detail::FractionalLine line;
    line.prepare(8000.,2.f);
    for(int i=0;i<400;++i) {
        if(i>=3) {
            const float a=(float)(((i-1)%23)-11)*.01f;
            const float b=(float)(((i-2)%23)-11)*.01f;
            assert(std::abs(line.read(1.f)-a)<1.e-6f);
            assert(std::abs(line.read(1.5f)-.5f*(a+b))<1.e-6f);
        }
        line.write((float)((i%23)-11)*.01f);
    }
    const float subnormal=std::numeric_limits<float>::denorm_min();
    line.reset();line.write(subnormal);
    assert(modulation_detail::safe(subnormal)==0.f);
    assert(line.read(1.f)==0.f);
    // Invalid host sample-rate input must not create huge delay buffers.
    const double invalid=std::numeric_limits<double>::quiet_NaN();
    VocalChorus chorus;VocalFlanger flange;VocalRingMod ring;
    chorus.prepare(invalid);flange.prepare(invalid);ring.prepare(invalid);
    assert(finite(chorus.process(.1f,-.1f)));
    assert(finite(flange.process(.1f,-.1f)));
    assert(finite(ring.process(.1f,-.1f)));
}

static void run(double sr) {
    VocalChorus chorus;VocalFlanger flange;VocalRingMod ring;
    chorus.prepare(sr);flange.prepare(sr);ring.prepare(sr);
    chorus.set(.9f,6.f,4.f,.7f);flange.set(.4f,3.8f,.35f,.5f);
    ring.set(120.f,.83f,.7f,.5f);
    const int n=(int)(sr*.3);
    double chorusEnergy=0.,flangeEnergy=0.,ringEnergy=0.,stereoDiff=0.;
    for(int i=0;i<n;++i){
        const float x=.4f*std::sin((float)(i*6.283185307179586*260./sr));
        const auto a=chorus.process(x,x);const auto b=flange.process(x,x);
        const auto c=ring.process(x,x);
        assert(finite(a)&&finite(b)&&finite(c));
        chorusEnergy+=std::abs(a[0]);flangeEnergy+=std::abs(b[0]);ringEnergy+=std::abs(c[0]);
        stereoDiff+=std::abs(b[0]-b[1]);
    }
    assert(chorusEnergy>1.&&flangeEnergy>1.&&ringEnergy>1.);
    assert(stereoDiff>.001);
    chorus.reset();flange.reset();ring.reset();
    for(int i=0;i<(int)(sr*.12);++i){
        assert(chorus.process(0.f,0.f)==(std::array<float,2>{0.f,0.f}));
        assert(flange.process(0.f,0.f)==(std::array<float,2>{0.f,0.f}));
        assert(ring.process(0.f,0.f)==(std::array<float,2>{0.f,0.f}));
    }
    ring.set(82.f,0.f,.8f,.3f);
    // Parameter automation is deliberately smoothed. After five
    // envelope time constants the depth is effectively neutral.
    for(int i=0;i<(int)(sr*.20);++i)ring.process(.35f,-.22f);
    for(int i=0;i<128;++i) {
        auto y=ring.process(.35f,-.22f);
        assert(std::abs(y[0]-.35f)<1.e-5f);
        assert(std::abs(y[1]+.22f)<1.e-5f);
    }
    // Fractional chorus voice count must remain finite during live changes.
    for(int i=0;i<18000;++i){
        if(i%113==0)chorus.set(.3f+(i%7)*.3f,1.f+(i%8),2.f+(i%4),.42f);
        const auto y=chorus.process(i%37==0?.25f:0.f,0.f);
        assert(finite(y));
    }
    float bad=std::numeric_limits<float>::infinity();
    assert(finite(chorus.process(bad,bad)));
    assert(finite(flange.process(bad,bad)));
    assert(finite(ring.process(bad,bad)));
    for(int i=0;i<50000;++i){
        if(i%17==0)flange.set(.25f+(i%13),10.f,(i%2)?.85f:-.85f,(i%3)*.5f);
        const auto y=flange.process(i%167==0?3.f:0.f,0.f);
        assert(finite(y) && std::abs(y[0])<=8.f && std::abs(y[1])<=8.f);
    }
}
int main(){
 fractionalLineAndHostGuards();
 for(double sr:{8000.,44100.,48000.,96000.}){
    run(sr);
    for(float voices:{2.f,3.5f,5.f})for(float spread:{0.f,.75f,1.f})
        chorusReference(sr,voices,spread);
 }
 std::cout<<"PASS modulation: 8k/44.1k/48k/96k, optimized chorus parity, circular line, denormals, host guards, automation, finite\n";
}
