// Release-safe regression for the measured Alchemist Tone Match factory.
// The EQ's normalized gain law is dB=(normalized-0.5)*30; checking only
// normalized values would miss a preset whose labels and audible gains disagree.
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
    constexpr std::array<float,3> expectedDb{{1.0f,-0.6f,-1.1f}};
    constexpr std::array<float,3> expectedNormalized{{.53333336f,.48f,.46333334f}};
    const auto& stage=alchemist_tone_match_trial::stages[1];

    for(std::size_t k=0;k<bands.size();++k) {
        const int band=bands[k];
        const float normalized=stage.p[(std::size_t)band*2+1];
        const float physical=(normalized-.5f)*30.f;
        if(!std::isfinite(normalized) ||
           std::abs(normalized-expectedNormalized[k])>1.e-6f ||
           std::abs(physical-expectedDb[k])>.001f) {
            std::fprintf(stderr,
                "Alchemist tone-match band %d: normalized %.8f maps to %.4f dB; expected %.4f dB\n",
                band+1,normalized,physical,expectedDb[k]);
            return 1;
        }
    }

    // Measure the actual EightBandTone response at each configured band
    // center, isolating one band at a time. This exercises the same DSP class
    // used by the live rack rather than repeating only its parameter formula.
    constexpr double pi=3.1415926535897932384626433832795;
    for(const double rate:{44100.,48000.,96000.}) {
        for(std::size_t k=0;k<bands.size();++k) {
            const int band=bands[k];
            auto parameters=stage.p;
            for(int i=0;i<8;++i)
                parameters[(std::size_t)i*2+1]=.5f;
            parameters[(std::size_t)band*2+1]=stage.p[(std::size_t)band*2+1];
            const float freqNorm=parameters[(std::size_t)band*2];
            const double frequency=EightBandTone::edges[(std::size_t)band]*
                std::pow(EightBandTone::edges[(std::size_t)band+1]/
                         EightBandTone::edges[(std::size_t)band],freqNorm);
            std::array<float,8> q=stage.q;
            EightBandTone eq;
            eq.prepare(rate);
            eq.set(rate,parameters,q);

            double inputEnergy=0.,outputEnergy=0.;
            const int discard=(int)(rate*.15);
            const int measure=(int)(rate*.35);
            for(int i=0;i<discard+measure;++i) {
                const float input=.1f*(float)std::sin(2.*pi*frequency*i/rate);
                const auto output=eq.process(input,input);
                if(!std::isfinite(output[0]) || !std::isfinite(output[1])) {
                    std::fprintf(stderr,"Non-finite Alchemist EQ output at %.0f Hz / band %d\n",
                                 rate,band+1);
                    return 2;
                }
                if(i>=discard) {
                    inputEnergy+=(double)input*input;
                    outputEnergy+=(double)output[0]*output[0];
                }
            }
            const double measured=10.*std::log10(outputEnergy/inputEnergy);
            if(!std::isfinite(measured) ||
               std::abs(measured-expectedDb[k])>.12) {
                std::fprintf(stderr,
                    "Alchemist EQ response mismatch at %.0f Hz / band %d: %.3f dB expected %.3f dB\n",
                    rate,band+1,measured,expectedDb[k]);
                return 3;
            }
        }
    }
    std::puts("PASS: Alchemist factory 48 normalized gains and live 8-band response match +1.0/-0.6/-1.1 dB at 44.1/48/96 kHz");
    return 0;
}
