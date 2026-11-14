#include "../Source/chain/MicModelDSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>

using skellpreset::dsp::MicModelTone;
static constexpr double pi=3.14159265358979323846;

int main() {
    static_assert(MicModelTone::profileCount==4,"Preset UI/DSP profile count");
    assert(MicModelTone::index(0.f)==0);
    assert(MicModelTone::index(.5f)==1 || MicModelTone::index(.5f)==2);
    assert(MicModelTone::index(1.f)==3);
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
            assert(std::isfinite(n[0])&&std::isfinite(a[0]));
            assert(std::abs(n[0]-x)<1.e-5f);
            assert(std::abs(n[0]+n[1])<1.e-5f);
            assert(std::abs(a[0]-b[0])<1.e-6f);
            assert(std::abs(a[0]+a[1])<1.e-5f);
            assert(std::abs(a[0])<3.f);
            if(i>rate*.2) {
                moved+=std::abs(a[0]-n[0]);
                energy+=std::abs(n[0]);
            }
        }
        assert(moved>energy*.005);
        auto bad=color.process(std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity());
        assert(std::isfinite(bad[0])&&std::isfinite(bad[1]));
        // Automation may jump discrete model types but output parameters
        // must glide; check the step against a constant input signal.
        color.set(0.f,0.f,0.f,.5f);
        for(int i=0;i<50000;++i)color.process(.2f,.2f);
        const float before=color.process(.2f,.2f)[0];
        color.set(.75f,1.f,1.f,.95f);
        const float first=color.process(.2f,.2f)[0];
        assert(std::abs(first-before)<.002f);
        for(int i=0;i<5000;++i)color.process(.2f,.2f);
        const float after=color.process(.2f,.2f)[0];
        assert(std::isfinite(after));
        assert(std::abs(after-first)>.001f);
        // All three modeled bands report the same physical deltas used
        // by the editor profile graph, not an unrelated decorative curve.
        auto deltas=MicModelTone::bandDeltaDb(0,3,1.f);
        assert(deltas[2]>3.f);
        auto neutralDeltas=MicModelTone::bandDeltaDb(0,0,.3f);
        assert(neutralDeltas[0]==0 && neutralDeltas[1]==0 && neutralDeltas[2]==0);
    }
    return 0;
}
