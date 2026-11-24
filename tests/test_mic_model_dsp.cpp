#include "../Source/chain/MicModelDSP.h"
#include <array>
#include <cstdio>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)
#include <cmath>
#include <limits>

using skellpreset::dsp::MicModelTone;
static constexpr double pi=3.14159265358979323846;

int main() {
    static_assert(MicModelTone::profileCount==4,"Preset UI/DSP profile count");
    static_assert(MicModelTone::spectralZones==5,"Five separate response zones");
    for(int input=0;input<4;++input)for(int target=0;target<4;++target) {
        const auto five=MicModelTone::spectralDeltaDb(input,target,1.f);
        const auto graph=MicModelTone::bandDeltaDb(input,target,1.f);
        SKELL_REQUIRE(graph[0]==five[0] && graph[1]==five[2] && graph[2]==five[4]);
        for(float gain:five)SKELL_REQUIRE(std::isfinite(gain) && std::abs(gain)<20.f);
        const auto bypass=MicModelTone::spectralDeltaDb(input,target,0.f);
        for(float v:bypass)SKELL_REQUIRE(v==0.f);
    }
    SKELL_REQUIRE(MicModelTone::index(0.f)==0);
    SKELL_REQUIRE(MicModelTone::index(.5f)==1 || MicModelTone::index(.5f)==2);
    SKELL_REQUIRE(MicModelTone::index(1.f)==3);
    // Profile selection is discrete, so fractional changes within one
    // index must not change the effective target response.
    for(int i=0;i<4;++i){
        const float p=(i+.5f)/4.f;
        SKELL_REQUIRE(MicModelTone::index(p)==i);
    }
    for(double rate:{8000.,44100.,48000.,96000.,192000.}) {
        MicModelTone clean,color,copy;
        clean.prepare(rate);color.prepare(rate);copy.prepare(rate);
        clean.set(0.f,0.f,1.f,.5f); // input = target, 0 dB trim
        color.set(1.f,1.f,1.f,.5f); // condenser -> Air
        copy.set(1.f,1.f,1.f,.5f);
        double moved=0.,energy=0.;
        for(int i=0;i<(int)(rate*.4);++i) {
            const float x=.14f*(float)std::sin(2.*pi*170.*i/rate)
                         +.065f*(float)std::sin(2.*pi*3800.*i/rate);
            auto n=clean.process(x,-x);
            auto a=color.process(x,-x);
            auto b=copy.process(x,-x);
            SKELL_REQUIRE(std::isfinite(n[0])&&std::isfinite(a[0]));
            SKELL_REQUIRE(std::abs(n[0]-x)<1.e-5f);
            SKELL_REQUIRE(std::abs(n[0]+n[1])<1.e-5f);
            SKELL_REQUIRE(std::abs(a[0]-b[0])<1.e-6f);
            SKELL_REQUIRE(std::abs(a[0]+a[1])<1.e-5f);
            SKELL_REQUIRE(std::abs(a[0])<3.f);
            if(i>rate*.2) {
                moved+=std::abs(a[0]-n[0]);
                energy+=std::abs(n[0]);
            }
        }
        SKELL_REQUIRE(moved>energy*.005);
        auto bad=color.process(std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity());
        SKELL_REQUIRE(std::isfinite(bad[0])&&std::isfinite(bad[1]));
        // Automation may jump discrete model types but output parameters
        // must glide; check the step against a constant input signal.
        color.set(0.f,0.f,0.f,.5f);
        for(int i=0;i<50000;++i)color.process(.2f,.2f);
        const float before=color.process(.2f,.2f)[0];
        color.set(.75f,1.f,1.f,.95f);
        const float first=color.process(.2f,.2f)[0];
        SKELL_REQUIRE(std::abs(first-before)<.002f);
        for(int i=0;i<5000;++i)color.process(.2f,.2f);
        const float after=color.process(.2f,.2f)[0];
        SKELL_REQUIRE(std::isfinite(after));
        SKELL_REQUIRE(std::abs(after-first)>.001f);
        // All three modeled bands report the same physical deltas used
        // by the editor profile graph, not an unrelated decorative curve.
        MicModelTone once,repeated;
        once.prepare(rate);repeated.prepare(rate);
        once.set(.72f,.86f,.73f,.61f);
        for(int i=0;i<8192;++i){
            repeated.set(.72f,.86f,.73f,.61f);
            const float x=.3f*std::sin(float(i)*.043f);
            SKELL_REQUIRE(once.process(x,-x)==repeated.process(x,-x));
        }
        // Reset must invalidate the cache so the same controls can be
        // restored immediately after a sample-rate/transport reset.
        once.reset();repeated.prepare(rate);
        once.set(.72f,.86f,.73f,.61f);
        repeated.set(.72f,.86f,.73f,.61f);
        for(int i=0;i<4096;++i){
            const float x=.2f*std::sin(float(i)*.17f);
            SKELL_REQUIRE(once.process(x,x)==repeated.process(x,x));
        }
        once.set(NAN,INFINITY,NAN,INFINITY);
        for(int i=0;i<1024;++i){
            const auto x=once.process(.2f,-.1f);
            SKELL_REQUIRE(std::isfinite(x[0])&&std::isfinite(x[1]));
        }
        auto deltas=MicModelTone::bandDeltaDb(0,3,1.f);
        SKELL_REQUIRE(deltas[2]>3.f);
        auto neutralDeltas=MicModelTone::bandDeltaDb(0,0,.3f);
        SKELL_REQUIRE(neutralDeltas[0]==0 && neutralDeltas[1]==0 && neutralDeltas[2]==0);
    }
    return 0;
}
