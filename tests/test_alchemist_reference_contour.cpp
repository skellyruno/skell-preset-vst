// Portable synthetic regression; private owner reference audio is NEVER in CI.
// Verifies legacy neutrality, causal stereo link, finite output, transient
// ceiling, and low- vs high-level gain behaviour across supported rates.
#include "../Source/chain/AlchemistReferenceContour.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
using skellpreset::dsp::AlchemistReferenceContour;

int main() {
    for(const double sr:{44100.,48000.,96000.}) {
        AlchemistReferenceContour original,matched;
        original.prepare(sr);matched.prepare(sr);
        original.set(.5f,1.f,1.f,.5f); // off: old engine unchanged
        matched.set(1.f,1.f,1.f,.5f);  // measured reference contour
        double lowE=0.,outLow=0.,highE=0.,outHigh=0.;
        float maximum=0.f;
        for(int i=0;i<(int)(sr*2);++i) {
            constexpr double pi=3.14159265358979323846;
            const float level=i<(int)sr?.022f:.19f;
            const float x=level*(float)(std::sin(2.*pi*211.*i/sr)
                           +.10*std::sin(2.*pi*16000.*i/sr));
            auto old=original.process(x,-x);
            auto y=matched.process(x,-x);
            assert(old[0]==x && old[1]==-x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0]+y[1])<1.e-5f);
            maximum=std::max(maximum,std::abs(y[0]));
            if(i>sr*.55 && i<sr*.95) {
                lowE+=x*x;outLow+=y[0]*y[0];
            }
            if(i>sr*1.55) {
                highE+=x*x;outHigh+=y[0]*y[0];
            }
        }
        assert(maximum<=1.f);
        const double lowDb=10.*std::log10(outLow/lowE);
        const double highDb=10.*std::log10(outHigh/highE);
        assert(lowDb>highDb+2.); // actual reference needs quieter phrases lifted
        assert(highDb>0. && highDb<7.);
        // Extreme and nonfinite DAW samples cannot poison the next block.
        matched.process(std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN());
        const auto after=matched.process(.03f,-.03f);
        assert(std::isfinite(after[0])&&std::isfinite(after[1]));
        // Disable and confirm an automatable, continuous fade to old output.
        matched.set(0.f,1.f,1.f,.5f);
        const float pre=matched.process(.12f,.12f)[0];
        const float immediate=matched.process(.12f,.12f)[0];
        assert(std::abs(immediate-pre)<.02f);
        for(int i=0;i<(int)(sr*.30);++i)matched.process(.12f,.12f);
        const float rested=matched.process(.12f,.12f)[0];
        assert(std::abs(rested-.12f)<1.e-4f);
    }
    return 0;
}
