// Release-safe regression for the measured Alchemist Tone Match factory.
// Its three values are *relative EQ corrections* from protected factory 43,
// not absolute dB settings: delta dB = (trialNorm - baseNorm) * 30.
#include "../Source/chain/AdvancedVocalDSP.h"
#include "../Source/chain/NativeSoundMatchTrials.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

int main() {
    using namespace skellpreset;
    using namespace skellpreset::dsp;
    constexpr std::array<int,3> bands{{5,6,7}};
    constexpr std::array<float,3> expectedDeltaDb{{1.0f,-0.6f,-1.1f}};
    const auto& base=alchemist_peak_guard_trial::stages[1];
    const auto& trial=alchemist_tone_match_trial::stages[1];
    if(base.module!=26 || trial.module!=26)return 1;

    for(std::size_t k=0;k<bands.size();++k) {
        const int band=bands[k];
        const std::size_t frequencyIndex=(std::size_t)band*2;
        const std::size_t gainIndex=frequencyIndex+1;
        const float baseNorm=base.p[gainIndex];
        const float trialNorm=trial.p[gainIndex];
        const float delta=(trialNorm-baseNorm)*30.f;
        if(!std::isfinite(delta) ||
           std::abs(trial.p[frequencyIndex]-base.p[frequencyIndex])>1.e-7f ||
           std::abs(delta-expectedDeltaDb[k])>.001f) {
            std::fprintf(stderr,
                "Alchemist band %d relative correction %.4f dB; expected %.4f dB\\n",
                band+1,delta,expectedDeltaDb[k]);
            return 2;
        }
    }
    // Factory 48 may change only the three measured gain controls. Frequency
    // positions, Q, all other EQ gains, and the protected factory 43 stay fixed.
    for(std::size_t i=0;i<16;++i) {
        const bool adjusted=i==11 || i==13 || i==15;
        if(!adjusted && std::abs(trial.p[i]-base.p[i])>1.e-7f)return 3;
    }
    if(trial.q!=base.q)return 4;
    for(float v:trial.p)if(!std::isfinite(v)||v<0.f||v>1.f)return 5;
    for(float v:trial.q)if(!std::isfinite(v)||v<0.f||v>1.f)return 6;

    // Measure the actual EightBandTone response for base and trial at each
    // affected band's center, isolating that band. This confirms the relative
    // correction through the live filter code at three common sample rates.
    constexpr double pi=3.1415926535897932384626433832795;
    for(const double rate:{44100.,48000.,96000.}) {
        for(std::size_t k=0;k<bands.size();++k) {
            const int band=bands[k];
            auto baseParameters=base.p;
            auto trialParameters=trial.p;
            for(int i=0;i<8;++i) {
                baseParameters[(std::size_t)i*2+1]=.5f;
                trialParameters[(std::size_t)i*2+1]=.5f;
            }
            baseParameters[(std::size_t)band*2+1]=base.p[(std::size_t)band*2+1];
            trialParameters[(std::size_t)band*2+1]=trial.p[(std::size_t)band*2+1];
            const float freqNorm=base.p[(std::size_t)band*2];
            const double frequency=EightBandTone::edges[(std::size_t)band]*
                std::pow(EightBandTone::edges[(std::size_t)band+1]/
                         EightBandTone::edges[(std::size_t)band],freqNorm);
            const std::array<float,8> baseQ=base.q;
            const std::array<float,8> trialQ=trial.q;
            EightBandTone baseEq,trialEq;
            baseEq.prepare(rate);
            trialEq.prepare(rate);
            baseEq.set(rate,baseParameters,baseQ);
            trialEq.set(rate,trialParameters,trialQ);

            double baseEnergy=0.,trialEnergy=0.;
            const int discard=(int)(rate*.15);
            const int measure=(int)(rate*.35);
            for(int i=0;i<discard+measure;++i) {
                const float input=.1f*(float)std::sin(2.*pi*frequency*i/rate);
                const auto a=baseEq.process(input,input);
                const auto b=trialEq.process(input,input);
                if(!std::isfinite(a[0])||!std::isfinite(b[0]))return 7;
                if(i>=discard) {
                    baseEnergy+=(double)a[0]*a[0];
                    trialEnergy+=(double)b[0]*b[0];
                }
            }
            const double measuredDelta=10.*std::log10(trialEnergy/baseEnergy);
            if(!std::isfinite(measuredDelta) ||
               std::abs(measuredDelta-expectedDeltaDb[k])>.12) {
                std::fprintf(stderr,
                    "Alchemist live EQ relative response mismatch at %.0f Hz / band %d: %.3f dB expected %.3f dB\\n",
                    rate,band+1,measuredDelta,expectedDeltaDb[k]);
                return 8;
            }
        }
    }
    std::puts("PASS: factory 48 relative EQ corrections +1.0/-0.6/-1.1 dB vs factory 43 at 44.1/48/96 kHz");
    return 0;
}
