// Portable reference-tone regression; checks that the preview EQ now
// boosts the actual Alchemist high-frequency region instead of cutting it.
// This is a spectral trend test, not a null-test or a source-plugin clone.
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/chain/AdvancedVocalDSP.h"
#include <cassert>
#include <cmath>
#include <algorithm>
using skellpreset::alchemist::stages;
using skellpreset::dsp::Biquad;
using skellpreset::dsp::EightBandTone;
int main(){
    static_assert(stages[1].module==26,"Measured tone must remain native eight-band EQ");
    const auto& tone=stages[1];
    auto response=[&](double sr,double hz){
        double sum=0;
        for(int b=0;b<8;++b){
            const float f=EightBandTone::edges[(size_t)b]*
                std::pow(EightBandTone::edges[(size_t)b+1]/
                         EightBandTone::edges[(size_t)b],tone.p[(size_t)b*2]);
            const float db=(tone.p[(size_t)b*2+1]-.5f)*30.f;
            const float q=EightBandTone::qFromNormalized(tone.q[(size_t)b]);
            Biquad biquad;biquad.setPeak(sr,f,db,q);
            sum+=biquad.magnitudeDb(hz,sr);
        }
        return sum;
    };
    for(double sr:{44100.,48000.,96000.}){
        const double air=response(sr,9000.);
        const double brightness=response(sr,6000.);
        // The old profile had NEGATIVE high-band gain, unlike the actual
        // same-take processed reference. Preserve a broad positive lift.
        assert(std::isfinite(air) && std::isfinite(brightness));
        assert(air>6. && air<18.);
        assert(brightness>5. && brightness<19.);
        assert(response(sr,250.)<8.);
        assert(response(sr,250.)>-10.);
    }
    for(float x:tone.p) assert(std::isfinite(x) && x>=0.f && x<=1.f);
    for(float x:tone.q) assert(std::isfinite(x) && x>=0.f && x<=1.f);
    return 0;
}
