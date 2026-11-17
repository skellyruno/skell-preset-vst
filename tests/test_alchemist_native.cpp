// Alchemist Native Preview: source-derived stage invariants plus audio-path
// sanity tests. No JUCE dependency or proprietary plugin code.
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/chain/NativeSoundMatchTrials.h"
#include "../Source/chain/FactoryPresets.h"
#include "../Source/chain/AdvancedVocalDSP.h"
#include "../Source/chain/VoxRackFX.h"
#include "../Source/chain/VocalEffects.h"
#include "../Source/chain/AlchemistMatchDSP.h"
#include "../Source/chain/SkelldizerDSP.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

using namespace skellpreset;
using namespace skellpreset::dsp;

int main() {
    static_assert(factoryPresets.size()==52,"Append only; preserve prior programs");
    static_assert(factoryPresets[49].name==std::string_view("Alchemist Lighter Dynamics (Trial)"),
                  "Alchemist lighter-dynamics candidate must append at index 49");
    static_assert(alchemist::factoryIndex==43,"Alchemist is appended after Studio Six");
    assert(std::strcmp(factoryPresets[0].name,"Clean Lead")==0);
    assert(std::strcmp(factoryPresets[42].name,"Studio Six")==0);
    assert(std::strcmp(factoryPresets[43].name,alchemist::name)==0);
    assert(std::strcmp(factoryPresets[43].bank,"Main Vocals")==0);
    assert(std::strcmp(factoryPresets[44].name,deuce::name)==0);
    assert(std::strcmp(factoryPresets[44].bank,"Main Vocals")==0);
    assert(std::strcmp(factoryPresets[45].name,alchemist_trial::name)==0);
    assert(std::strcmp(factoryPresets[46].name,deuce_v25::name)==0);
    assert(std::strcmp(factoryPresets[47].name,alchemist_peak_guard_trial::name)==0);
    assert(std::strcmp(factoryPresets[47].bank,"Main Vocals")==0);
    assert(alchemist_peak_guard_trial::stages.size()==8);
    for(std::size_t i=0;i<alchemist_trial::stages.size();++i)
        assert(alchemist_peak_guard_trial::stages[i].module==alchemist_trial::stages[i].module);
    assert(alchemist_peak_guard_trial::stages[7].module==3);
    assert(alchemist_peak_guard_trial::stages[7].wet==1.f);
    assert(std::abs(alchemist_peak_guard_trial::stages[7].p[0]-.89f)<1.e-7f);
    assert(alchemist_peak_guard_trial::stages[7].p[1]==0.f);
    assert(std::strcmp(factoryPresets[48].name,alchemist_tone_match_trial::name)==0);
    assert(std::strcmp(factoryPresets[48].bank,"Main Vocals")==0);
    assert(std::strcmp(factoryPresets[49].name,alchemist_lighter_dynamics_trial::name)==0);
    assert(std::strcmp(factoryPresets[49].bank,"Main Vocals")==0);
    assert(alchemist_lighter_dynamics_trial::stages[2].wet==.20f);
    assert(alchemist_lighter_dynamics_trial::stages[3].wet==.50f);
    assert(std::strcmp(factoryPresets[50].name,alchemist_fast_blend_trial::name)==0);
    assert(std::strcmp(factoryPresets[50].bank,"Main Vocals")==0);
    assert(std::strcmp(factoryPresets[51].name,alchemist_slow_blend_trial::name)==0);
    assert(std::strcmp(factoryPresets[51].bank,"Main Vocals")==0);
    assert(alchemist_fast_blend_trial::stages[2].wet==.20f);
    assert(alchemist_fast_blend_trial::stages[3].wet==.70f);
    assert(alchemist_slow_blend_trial::stages[2].wet==.35f);
    assert(alchemist_slow_blend_trial::stages[3].wet==.50f);
    for(std::size_t i=0;i<alchemist_tone_match_trial::stages.size();++i) {
        const auto& base=alchemist_tone_match_trial::stages[i];
        const auto& fast=alchemist_fast_blend_trial::stages[i];
        const auto& slow=alchemist_slow_blend_trial::stages[i];
        assert(base.module==fast.module && base.p==fast.p && base.q==fast.q);
        assert(base.module==slow.module && base.p==slow.p && base.q==slow.q);
        assert(std::abs(fast.wet-(i==2?.20f:base.wet))<1.e-7f);
        assert(std::abs(slow.wet-(i==3?.50f:base.wet))<1.e-7f);
        for(float v:fast.p)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
        for(float v:slow.p)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
        for(float v:fast.q)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
        for(float v:slow.q)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
    }
    assert(alchemist_tone_match_trial::stages.size()==8);
    assert(alchemist_tone_match_trial::stages[7].module==3);
    assert(alchemist_tone_match_trial::stages[7].wet==1.f);
    assert(std::abs(alchemist_tone_match_trial::stages[1].p[11]-.478534f)<1.e-7f);
    assert(std::abs(alchemist_tone_match_trial::stages[1].p[13]-.738915f)<1.e-7f);
    assert(std::abs(alchemist_tone_match_trial::stages[1].p[15]-.830000f)<1.e-7f);
    for(std::size_t i=0;i<alchemist_peak_guard_trial::stages.size();++i)
        assert(alchemist_tone_match_trial::stages[i].module==alchemist_peak_guard_trial::stages[i].module);
    assert(std::strcmp(factoryPresets[46].bank,"Main Vocals")==0);
    assert(deuce_v25::stages.size()==13);
    assert(deuce_v25::stages[12].module==26);
    for(std::size_t k=0;k<deuce::stages.size();++k){
        assert(deuce_v25::stages[k].module==deuce::stages[k].module);
        assert(deuce_v25::stages[k].wet==deuce::stages[k].wet);
    }
    for(const auto& stage:deuce::stages){
        assert(stage.module>=1 && stage.module<38);
        assert(stage.wet>=0.f&&stage.wet<=1.f);
        for(float value:stage.p)assert(std::isfinite(value)&&value>=0.f&&value<=1.f);
    }
    assert(alchemist::stages[6].p[12]==.5f);
    assert(alchemist_trial::stages[6].p[12]==1.f);


    const int expectedTypes[7]{1,26,2,19,33,5,31};
    for(int i=0;i<7;++i) {
        const auto& stage=alchemist::stages[(size_t)i];
        assert(stage.module==expectedTypes[i]);
        assert(stage.wet>=0.f && stage.wet<=1.f);
        for(float v:stage.p)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
        for(float v:stage.q)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
    }
    // Only this new factory program enables the p9 reference contour.
    // All older Match Finish effects keep p9=.5 and their original sound.
    assert(alchemist::stages[6].p[8]>.75f);
    assert(alchemist::stages[6].p[9]==1.f);
    assert(alchemist::stages[6].p[10]==1.f);
    assert(alchemist::stages[6].p[11]==.58f); // factory-only; historic saved states unchanged
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
        Skelldizer skelldizer;
        skelldizer.prepare(sr);
        const auto& mb=alchemist::stages[4];
        skelldizer.set(mb.p[0],mb.p[1],mb.p[2],mb.p[3]);
        const auto& sat=alchemist::stages[5];
        const float drive=dbToGain(sat.p[0]*24.f);
        const float shape=.5f+sat.p[1];
        AlchemistMatchFinish finish;
        finish.prepare(sr);
        const auto& fm=alchemist::stages[6];
        finish.set(fm.p[0],fm.p[1],fm.p[2],fm.p[3],
                   fm.p[4],fm.p[5],fm.p[6],fm.p[7],
                   fm.p[8],fm.p[9],fm.p[10],fm.p[11]);

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

            const auto e=skelldizer.process(l,r);
            l+=(e[0]-l)*mb.wet;
            r+=(e[1]-r)*mb.wet;

            const float coloredL=std::tanh(l*drive*shape)/std::max(.5f,drive*.6f);
            const float coloredR=std::tanh(r*drive*shape)/std::max(.5f,drive*.6f);
            l+=(coloredL-l)*sat.wet;
            r+=(coloredR-r)*sat.wet;
            const auto finished=finish.process(l,r);
            l+=(finished[0]-l)*fm.wet;
            r+=(finished[1]-r)*fm.wet;

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
