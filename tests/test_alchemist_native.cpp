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
#include <limits>

using namespace skellpreset;
using namespace skellpreset::dsp;

int main() {
    // The serial FET/RMS gain conversion is table-interpolated after prepare().
    // Compare against the original exp2 law over the complete 0..144 dB range.
    detail::DbGainLookup::warm();
    float previousGain=detail::DbGainLookup::gain(0.f);
    float maxRelativeGainError=0.f;
    for(int i=0;i<=14400;++i) {
        const float db=(float)i*.01f;
        const float actual=detail::DbGainLookup::gain(db);
        const float reference=std::exp2(-db*detail::DbGainLookup::dbToLog2);
        const float relative=std::abs(actual-reference)/std::max(reference,1.e-30f);
        maxRelativeGainError=std::max(maxRelativeGainError,relative);
        if(!std::isfinite(actual)||actual<0.f||actual>previousGain+1.e-7f)
            return 48;
        previousGain=actual;
    }
    if(maxRelativeGainError>1.e-5f
       || detail::DbGainLookup::gain(-1.f)!=1.f
       || detail::DbGainLookup::gain(std::numeric_limits<float>::quiet_NaN())!=0.f)
        return 49;
    // Above the table's 144 dB domain, retain the original exact exp2 law.
    // A clamped table value would plateau and under-attenuate pathological peaks.
    for(const float db:{150.f,160.f,200.f,400.f,800.f,1600.f}) {
        const float actual=detail::DbGainLookup::gain(db);
        const float reference=std::exp2(-db*detail::DbGainLookup::dbToLog2);
        if(!std::isfinite(actual)||actual!=reference)return 50;
    }
    if(!(detail::DbGainLookup::gain(160.f)<detail::DbGainLookup::gain(144.f))
       || !(detail::DbGainLookup::gain(200.f)<detail::DbGainLookup::gain(160.f)))
        return 51;
    static_assert(factoryPresets.size()==53,"Append only; preserve prior programs");
    static_assert(factoryPresets[49].name==std::string_view("Alchemist Lighter Dynamics (Trial)"),
                  "Alchemist lighter-dynamics candidate must remain at index 49");
    static_assert(factoryPresets[50].name==std::string_view("Alchemist Fast Compressor Blend (Trial)"),
                  "Fast-compressor blend trial must append at index 50");
    static_assert(factoryPresets[51].name==std::string_view("Alchemist Smooth Compressor Blend (Trial)"),
                  "Smooth-compressor blend trial must append at index 51");
    static_assert(factoryPresets[52].name==std::string_view("Alchemist Serial Dynamics (Trial)"),
                  "Serial FET/RMS trial must append at index 52");
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
    assert(std::strcmp(factoryPresets[50].name,alchemist_fast_compressor_blend_trial::name)==0);
    assert(std::strcmp(factoryPresets[50].bank,"Main Vocals")==0);
    assert(alchemist_fast_compressor_blend_trial::stages[2].wet==.20f);
    assert(alchemist_fast_compressor_blend_trial::stages[3].wet==.70f);
    assert(std::strcmp(factoryPresets[51].name,alchemist_smooth_compressor_blend_trial::name)==0);
    assert(std::strcmp(factoryPresets[51].bank,"Main Vocals")==0);
    assert(alchemist_smooth_compressor_blend_trial::stages[2].wet==.35f);
    assert(alchemist_smooth_compressor_blend_trial::stages[3].wet==.50f);
    assert(std::strcmp(factoryPresets[52].name,alchemist_serial_dynamics_trial::name)==0);
    assert(std::strcmp(factoryPresets[52].bank,"Main Vocals")==0);
    assert(alchemist_serial_dynamics_trial::stages.size()==7);
    assert(alchemist_serial_dynamics_trial::stages[2].module==27);
    assert(alchemist_serial_dynamics_trial::stages[3].module==33);
    assert(alchemist_serial_dynamics_trial::stages[5].module==31);
    assert(alchemist_serial_dynamics_trial::stages[6].module==3);
    assert(alchemist_serial_dynamics_trial::stages[2].wet==.70f);
    assert(std::abs(alchemist_serial_dynamics_trial::stages[2].p[0]-.428571f)<1.e-6f);
    assert(std::abs(alchemist_serial_dynamics_trial::stages[2].p[1]-.5f)<1.e-6f);
    assert(std::abs(alchemist_serial_dynamics_trial::stages[2].p[2]-.35f)<1.e-6f);
    assert(std::abs(alchemist_serial_dynamics_trial::stages[2].p[3]-.15f)<1.e-6f);
    for(const auto& stage:alchemist_serial_dynamics_trial::stages) {
        assert(stage.module>=1 && stage.module<38);
        assert(stage.wet>=0.f && stage.wet<=1.f);
        for(float v:stage.p)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
        for(float v:stage.q)assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
    }
    // Audit every factory label, cover index and macro range; names must be unique.
    for(std::size_t i=0;i<factoryPresets.size();++i) {
        const auto& preset=factoryPresets[i];
        if(!preset.name || !*preset.name || !preset.bank || !*preset.bank ||
           preset.colour<0 || preset.colour>5 ||
           !std::isfinite(preset.tone) || !std::isfinite(preset.punch) ||
           !std::isfinite(preset.space) || !std::isfinite(preset.grit) ||
           preset.tone<0.f || preset.tone>1.f || preset.punch<0.f || preset.punch>1.f ||
           preset.space<0.f || preset.space>1.f || preset.grit<0.f || preset.grit>1.f)
            return 30;
        for(std::size_t j=0;j<i;++j)
            if(std::strcmp(preset.name,factoryPresets[j].name)==0)
                return 31;
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
    // Exercise the exact native FET/RMS processor used by factory 52.
    // The compressor remains stereo-linked, finite and bounded at all host rates.
    const auto& serialStage=alchemist_serial_dynamics_trial::stages[2];
    for(const double sr:{44100.,48000.,96000.,192000.}) {
        SerialVocalDynamics serial;
        serial.prepare(sr);
        serial.set(serialStage.p[0],serialStage.p[1],
                   serialStage.p[2],serialStage.p[3]);
        double inputEnergy=0.,outputEnergy=0.;
        for(int i=0;i<(int)(sr*.75);++i) {
            const float amplitude=i<(int)(sr*.35)?.28f:.12f;
            const float wave=amplitude*(
                .65f*std::sin(float(2.*pi*220.*i/sr))+
                .35f*std::sin(float(2.*pi*3100.*i/sr)));
            const auto y=serial.process(wave,-wave);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0]+y[1])<1.e-6f);
            assert(std::abs(y[0])<1.f&&std::abs(y[1])<1.f);
            inputEnergy+=(double)wave*wave;
            outputEnergy+=(double)y[0]*y[0];
        }
        assert(inputEnergy>1.e-3&&outputEnergy>1.e-3);
        assert(std::isfinite(serial.reductionDb())&&serial.reductionDb()>0.f);
        serial.reset();
        for(int i=0;i<2048;++i) {
            const auto y=serial.process(0.f,0.f);
            assert(y[0]==0.f&&y[1]==0.f);
        }
    }
    // Long phrase-to-silence regression: both serial gain-reduction followers
    // must decay to exact zero without spending seconds in IEEE-754 subnormals.
    // This catches denormal CPU spikes on hosts that do not enable FTZ/DAZ.
    for(const double sr:{44100.,48000.,96000.,192000.}) {
        SerialVocalDynamics tail;
        tail.prepare(sr);
        tail.set(serialStage.p[0],serialStage.p[1],
                 serialStage.p[2],serialStage.p[3]);
        for(int i=0;i<(int)(sr*.5);++i) {
            const float wave=.28f*(
                .65f*std::sin(float(2.*pi*220.*i/sr))+
                .35f*std::sin(float(2.*pi*3100.*i/sr)));
            const auto y=tail.process(wave,-wave);
            if(!std::isfinite(y[0])||!std::isfinite(y[1]))return 40;
        }
        for(int i=0;i<(int)(sr*25.);++i) {
            const auto y=tail.process(0.f,0.f);
            const float reduction=tail.reductionDb();
            if(!std::isfinite(y[0])||!std::isfinite(y[1])||
               !std::isfinite(reduction)||reduction<0.f)return 41;
            if(std::fpclassify(y[0])==FP_SUBNORMAL ||
               std::fpclassify(y[1])==FP_SUBNORMAL ||
               std::fpclassify(reduction)==FP_SUBNORMAL)return 42;
        }
        if(tail.reductionDb()!=0.f)return 43;
    }
    // The processor boundary also flushes subnormal audio and rejects
    // nonfinite samples; large finite input must remain finite through the
    // detector clamp and bounded parallel saturator.
    SerialVocalDynamics inputGuard;
    inputGuard.prepare(48000.);
    inputGuard.set(serialStage.p[0],serialStage.p[1],
                   serialStage.p[2],serialStage.p[3]);
    const auto tiny=inputGuard.process(1.e-40f,-1.e-40f);
    if(tiny[0]!=0.f||tiny[1]!=0.f)return 44;
    const auto invalid=inputGuard.process(
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity());
    if(invalid[0]!=0.f||invalid[1]!=0.f)return 45;
    const auto huge=inputGuard.process(
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max());
    if(!std::isfinite(huge[0])||!std::isfinite(huge[1]))return 46;
    SerialVocalDynamics outputGuard;
    outputGuard.prepare(48000.);
    outputGuard.set(serialStage.p[0],serialStage.p[1],
                    serialStage.p[2],0.f);
    for(int i=0;i<24000;++i)outputGuard.process(.28f,-.28f);
    const auto tinyWet=outputGuard.process(1.e-37f,-1.e-37f);
    if(tinyWet[0]!=0.f||tinyWet[1]!=0.f)return 47;
    // Exercise host-style block-boundary automation across every supported
    // rate. All four normalized controls move while transients and stereo-link
    // stress are present; no nonfinite or over-range output is permitted.
    for(const double sr:{44100.,48000.,96000.,192000.}) {
        SerialVocalDynamics automated;
        automated.prepare(sr);
        for(int block=0;block<128;++block) {
            const float t=(float)block/127.f;
            automated.set(.20f+.60f*t,.15f+.75f*t,.10f+.75f*t,.05f+.20f*t);
            for(int n=0;n<256;++n) {
                const auto sample=(std::int64_t)block*256+n;
                float x=.22f*std::sin(float(2.*pi*220.*sample/sr))
                       +.09f*std::sin(float(2.*pi*3100.*sample/sr));
                if(sample%4096==0)x+=.30f;
                const auto y=automated.process(x,-x);
                assert(std::isfinite(y[0])&&std::isfinite(y[1]));
                assert(std::abs(y[0])<1.f&&std::abs(y[1])<1.f);
                assert(std::abs(y[0]+y[1])<1.e-6f);
            }
        }
    }
    return 0;
}
