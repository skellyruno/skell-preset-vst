// Portable regression tests for ZIP-derived native SKELL vocal modules.
// No JUCE, no heap operations in per-sample DSP. Test many host rates.
#include "../Source/chain/AdvancedVocalDSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <algorithm>
using namespace skellpreset::dsp;

int main(){
    for(const double sr:{8000.,44100.,48000.,96000.,192000.}){
        EightBandTone eq;eq.prepare(sr);
        std::array<float,16> params{};params.fill(.5f);
        eq.set(sr,params);
        for(int n=0;n<2200;++n){
            const float x=.18f*std::sin(n*.015f);
            const auto y=eq.process(x,-x*.8f);
            assert(std::isfinite(y[0]) && std::isfinite(y[1]));
            assert(std::abs(y[0]-x)<.005f);
        }
        params[9]=.90f; // fifth-band gain
        std::array<float,8> narrowQ{};
        narrowQ.fill(.85f);
        eq.set(sr,params,narrowQ);
        assert(EightBandTone::qFromNormalized(.85f)>
               EightBandTone::qFromNormalized(.15f));
        assert(std::abs(EightBandTone::normalizedFromQ(
            EightBandTone::qFromNormalized(.83f))-.83f)<.001f);
        for(int n=0;n<1400;++n){
            const auto y=eq.process(.17f*std::sin(n*.21f),.09f);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0])<20.f);
        }
        // Parametric EQ cuts must perform real, stable filtering at each
        // supported sample rate; legacy-neutral state is exact bypass.
        EightBandTone cuts;
        std::array<float,16> neutral{};
        neutral.fill(.5f);
        cuts.prepare(sr);cuts.set(sr,neutral);cuts.setCuts(sr,1.f,0.f);
        double dryPower=0.,lowCutPower=0.;
        for(int n=0;n<(int)(sr*.28);++n) {
            const float x=.12f*std::sin(float(2.*pi*70.*n/sr));
            const auto y=cuts.process(x,-x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            if(n>sr*.12) {dryPower+=(double)x*x;lowCutPower+=(double)y[0]*y[0];}
        }
        assert(lowCutPower<dryPower*.2);
        cuts.prepare(sr);cuts.set(sr,neutral);cuts.setCuts(sr,0.f,1.f);
        dryPower=0.;double highCutPower=0.;
        const double highTestHz=std::min(sr*.31,4500.);
        for(int n=0;n<(int)(sr*.14);++n){
            const float x=.12f*std::sin(float(2.*pi*highTestHz*n/sr));
            const auto y=cuts.process(x,x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            if(n>sr*.06){dryPower+=(double)x*x;highCutPower+=(double)y[0]*y[0];}
        }
        assert(highCutPower<dryPower*.78);
        // Zero is OFF. Switching back to identity does not leave a permanent
        // low-pass colouration from the old filter coefficients.
        cuts.setCuts(sr,0.f,0.f);
        for(int n=0;n<(int)(sr*.1);++n){
            const float x=.1f*std::sin(float(2.*pi*300.*n/sr));
            const auto y=cuts.process(x,-x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            if(n>sr*.08)assert(std::abs(y[0]-x)<.006f);
        }
        SerialVocalDynamics dynamics;
        dynamics.prepare(sr);
        dynamics.set(.35f,.8f,.25f,0.f);
        for(int n=0;n<1200;++n){
            const auto y=dynamics.process(0.f,0.f);
            assert(y[0]==0.f && y[1]==0.f);
        }
        float maximumReduction=0.f;
        for(int n=0;n<(int)sr;++n){
            const float x=.6f*std::sin(n*.4f);
            const auto y=dynamics.process(x,x*.7f);
            maximumReduction=std::max(maximumReduction,dynamics.reductionDb());
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0])<=std::abs(x)+.00001f);
        }
        assert(maximumReduction>1.f);
        // Even very large *finite* peak samples must not poison the
        // detector and leave the compressor permanently silent or nonfinite.
        const auto pathological=dynamics.process(1e30f,-1e30f);
        assert(std::isfinite(pathological[0])&&std::isfinite(pathological[1]));
        assert(std::isfinite(dynamics.reductionDb()));
        for(int n=0;n<(int)sr/2;++n){
            const auto y=dynamics.process(.12f*std::sin(n*.031f),
                                          -.07f*std::sin(n*.031f));
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::isfinite(dynamics.reductionDb()));
        }
        // Parallel colour should be gain-neutral on quiet signals and only
        // compress, not unexpectedly amplify, the full-scale peak.
        SerialVocalDynamics parallel;
        parallel.prepare(sr);
        parallel.set(1.f,0.f,.5f,1.f);
        for(int n=0;n<300;++n){
            const float input=.001f*std::sin((float)n*.047f);
            const auto y=parallel.process(input,-input);
            assert(std::abs(y[0])<=std::abs(input)+.000002f);
            assert(std::abs(y[1])<=std::abs(input)+.000002f);
        }
        DynamicHarshness harsh;
        harsh.prepare(sr);harsh.set(.5f,.5f,0.f,.4f);
        for(int n=0;n<3000;++n){
            const float x=.15f*std::sin(n*.37f);
            const auto y=harsh.process(x,x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        }
        harsh.set(.5f,.3f,1.f,.5f);
        const auto burst=harsh.process(1e30f,-1e30f);
        assert(std::isfinite(burst[0])&&std::isfinite(burst[1]));
        for(int n=0;n<3000;++n){
            const auto y=harsh.process(.3f*std::sin(n*.12f),.17f);
            assert(std::isfinite(y[0]) && std::isfinite(y[1]));
            assert(harsh.reductionDb()>=0.f && harsh.reductionDb()<=10.f);
        }
        AdaptiveNoiseControl noise;
        noise.prepare(sr);
        noise.set(.45f,.9f,.1f,.4f);
        float end=0.f;
        for(int n=0;n<(int)sr;++n){
            const auto y=noise.process(.001f,.001f);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            end=y[0];
        }
        assert(end<.0009f);
        noise.set(.45f,0.f,.1f,.4f);
        for(int n=0;n<(int)sr;++n){
            const auto y=noise.process(.001f,.001f);
            if(n>(int)sr/2)assert(std::abs(y[0]-.001f)<1.e-5f);
        }
        auto bad=noise.process(std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN());
        assert(bad[0]==0.f && bad[1]==0.f);
    }
    return 0;
}
