#include "../Source/chain/LimiterDSP.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    for(double rate:{44100.,48000.,96000.}) {
        skellcore::TruePeakLimiter legacy;
        skellpreset::dsp::CompatibleLimiter updated;
        legacy.prepare(rate);updated.prepare(rate);legacy.setCeilingDb(-6.f);updated.setCeilingDb(-6.f);
        updated.setTiming(0.f,70.f);
        assert(updated.latencySamples()==legacy.latencySamples());
        for(int n=0;n<(int)rate;++n) {
            const float input[2]{1.6f*std::sin(n*.173f),.93f*std::sin(n*.109f)};
            float before[2]{},after[2]{};legacy.processFrame(input,before,2);updated.processFrame(input,after,2);
            assert(before[0]==after[0] && before[1]==after[1]);
        }
        auto render=[&](float attack,float release) {
            skellpreset::dsp::CompatibleLimiter limiter;limiter.prepare(rate);limiter.setCeilingDb(-6.f);
            limiter.setTiming(attack,release);std::vector<float> result;
            for(int n=0;n<(int)(rate*.35);++n) {
                const float amplitude=n<(int)(rate*.025)?1.8f:.07f;
                const float input[2]{amplitude*std::sin(n*.173f),amplitude*.8f*std::cos(n*.131f)};
                float out[2]{};limiter.processFrame(input,out,2);
                assert(std::isfinite(out[0]) && std::isfinite(out[1]));
                if(attack>0.f)assert(std::abs(out[0])<.5013f && std::abs(out[1])<.5013f);
                result.push_back(out[0]);
            }
            return result;
        };
        const auto fast=render(0.f,15.f),slow=render(0.f,400.f),attack=render(2.f,15.f);
        double releaseDifference=0.,attackDifference=0.;
        for(size_t n=0;n<fast.size();++n) {
            if(n>(size_t)(rate*.05))releaseDifference+=std::abs(fast[n]-slow[n]);
            if(n<(size_t)(rate*.025))attackDifference+=std::abs(fast[n]-attack[n]);
        }
        assert(releaseDifference>.01 && attackDifference>.001);
        std::printf("PASS limiter %.0f Hz: default bit-exact, PDC %d, attack delta %.6f, release delta %.6f\n",
                    rate,updated.latencySamples(),attackDifference,releaseDifference);
    }
}
