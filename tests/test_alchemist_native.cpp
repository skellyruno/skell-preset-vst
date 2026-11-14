// Alchemist Native Preview: source-derived stage invariants plus audio-path
// sanity tests. No JUCE dependency or proprietary plugin code.
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/chain/FactoryPresets.h"
#include "../Source/chain/AdvancedVocalDSP.h"
#include "../Source/chain/VoxRackFX.h"
#include "../Source/chain/VocalEffects.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

using namespace skellpreset;
using namespace skellpreset::dsp;

int main() {
    static_assert(factoryPresets.size()==44,"Append only; preserve prior programs");
    static_assert(alchemist::factoryIndex==43,"Alchemist is appended after Studio Six");
    assert(std::strcmp(factoryPresets[0].name,"Clean Lead")==0);
    assert(std::strcmp(factoryPresets[42].name,"Studio Six")==0);
    assert(std::strcmp(factoryPresets[43].name,alchemist::name)==0);
    assert(std::strcmp(factoryPresets[43].bank,"Main Vocals")==0);

    const int expectedTypes[6]{1,26,2,19,17,5};
    for(int i=0;i<6;++i) {
        const auto& stage=alchemist::stages[(size_t)i];
        assert(stage.module==expectedTypes[i]);
        assert(stage.wet>=0.f && stage.wet<=1.f);
        for(float v:stage.p)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
        for(float v:stage.q)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
    }
    // The final default Fruity Limiter tested effectively transparent on
    // Alchemist's source. Do not add a lookahead limiter to this template.
    for(const auto& stage:alchemist::stages)assert(stage.module!=3);

    for(const double sr:{44100.,48000.,96000.}) {
        SmoothedBiquad hp,peak;
        hp.prepare(sr);peak.prepare(sr);
        const auto& eq=alchemist::stages[0];
        hp.setHighPass(sr,20.+250.*eq.p[3]);
        peak.setPeak(sr,40.*std::pow(300.,eq.p[0]),(eq.p[1]-.5)*24.,.3+7.*eq.p[2]);
        EightBandTone tone;
        tone.prepare(sr);
        tone.set(sr,alchemist::stages[1].p,alchemist::stages[1].q);

        Compressor fet;
        fet.prepare(sr);
        const auto& fast=alchemist::stages[2];
        fet.set(-48.f+42.f*fast.p[0],1.f+11.f*fast.p[1],
                2.f+98.f*fast.p[2],20.f+480.f*fast.p[3],
                (fast.p[4]-.5f)*24.f);
        VoxRackSmoothCompressor opto;
        opto.prepare(sr);
        const auto& slow=alchemist::stages[3];
        opto.set(-55.f+53.f*slow.p[0],1.f+11.f*slow.p[1],
                 18.f*slow.p[2],40.f+410.f*slow.p[3]);
        ThreeBandDynamics multiband;
        multiband.prepare(sr);
        const auto& mb=alchemist::stages[4];
        multiband.set(mb.p[0],mb.p[1],mb.p[2],mb.p[3]);
        const auto& sat=alchemist::stages[5];
        const float drive=dbToGain(sat.p[0]*24.f);
        const float shape=.5f+sat.p[1];

        double dryEnergy=0,wetEnergy=0;
        for(int i=0;i<(int)(sr*.28);++i) {
            const float x=.095f*std::sin(float(2.*pi*226.*i/sr))
                         +.043f*std::sin(float(2.*pi*2950.*i/sr));
            const auto a=hp.process(x,-x);
            const auto b=peak.process(a[0],a[1]);
            const auto c=tone.process(b[0],b[1]);
            float l=c[0],r=c[1];

            const float gain=fet.gain(std::max(std::abs(l),std::abs(r)));
            l+=(l*gain-l)*fast.wet;
            r+=(r*gain-r)*fast.wet;

            const auto d=opto.process(l,r);
            l+=(d[0]-l)*slow.wet;
            r+=(d[1]-r)*slow.wet;

            const auto e=multiband.process(l,r);
            l+=(e[0]-l)*mb.wet;
            r+=(e[1]-r)*mb.wet;

            const float coloredL=std::tanh(l*drive*shape)/std::max(.5f,drive*.6f);
            const float coloredR=std::tanh(r*drive*shape)/std::max(.5f,drive*.6f);
            l+=(coloredL-l)*sat.wet;
            r+=(coloredR-r)*sat.wet;

            assert(std::isfinite(l)&&std::isfinite(r));
            assert(std::abs(l)<1.f&&std::abs(r)<1.f);
            if(i>sr*.12) { dryEnergy+=(double)x*x;wetEnergy+=(double)l*l; }
        }
        assert(dryEnergy>1e-3 && wetEnergy>1e-3);
        const double ratio=wetEnergy/dryEnergy;
        assert(ratio>.005 && ratio<8.);
    }
    return 0;
}
