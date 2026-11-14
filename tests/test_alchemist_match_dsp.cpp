// Portable Alchemist Finish test: same mathematics as the VST3, no JUCE.
// Distinguishes a live *causal* port from the v0.7 offline symmetric filter.
#include "../Source/chain/AlchemistMatchDSP.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

using skellpreset::dsp::AlchemistMatchFinish;
using skellpreset::alchemist::stages;

static void configure(AlchemistMatchFinish& f) {
    const auto& p=stages[6].p;
    f.set(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
}

int main() {
    static_assert(stages.size()==7,"Alchemist must include the match-finish module");
    static_assert(stages[6].module==31,"Append-only processor ID");
    for(double rate:{8000.,44100.,48000.,96000.,192000.}) {
        AlchemistMatchFinish p,clone,neutral;
        p.prepare(rate);clone.prepare(rate);neutral.prepare(rate);
        configure(p);configure(clone);
        neutral.set(.5f,0.f,.5f,.5f,.5f,.5f,.5f,.5f);
        for(int i=0;i<512;++i) {
            const auto z=p.process(0.f,0.f);
            const auto z2=clone.process(0.f,0.f);
            assert(z[0]==0.f && z[1]==0.f);
            assert(z2[0]==0.f && z2[1]==0.f);
        }
        // Test stereo linking, gain bounds, and continuous response to
        // recorded-tone plus short transients at all live sample rates.
        double sum=0,changed=0;
        const int samples=(int)(rate*.7);
        for(int i=0;i<samples;++i) {
            const float x=.14f*std::sin(float(i*2.*3.141592653589793*210./rate))
                    +.04f*std::sin(float(i*2.*3.141592653589793*2180./rate))
                    +(i%997==0?.08f:0.f);
            const auto y=p.process(x,-x);
            const auto j=clone.process(x,-x);
            const auto n=neutral.process(x,-x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0])<.9f && std::abs(y[1])<.9f);
            assert(std::abs(y[0]+y[1])<1.e-5f);
            assert(std::abs(y[0]-j[0])<1.e-6f);
            assert(std::abs(n[0]-x)<1.e-5f); // explicit neutral at all rates
            if(i>rate*.28) {
                sum+=double(x)*x;
                changed+=double(y[0]-x)*(y[0]-x);
            }
        }
        assert(sum>1.e-3 && changed>1.e-8);
        // If input is malformed, processing must never contaminate state.
        auto bad=p.process(std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity());
        assert(std::isfinite(bad[0])&&std::isfinite(bad[1]));
        for(int i=0;i<2000;++i) {
            const auto out=p.process(0.f,0.f);
            assert(std::isfinite(out[0])&&std::isfinite(out[1]));
            assert(out[0]==0.f&&out[1]==0.f);
        }
        // Reset is deterministic (DAW playback restart).
        p.reset();clone.reset();
        const auto one=p.process(.2f,.2f);
        const auto two=clone.process(.2f,.2f);
        assert(std::abs(one[0]-two[0])<1.e-7f);
    }
    return 0;
}
