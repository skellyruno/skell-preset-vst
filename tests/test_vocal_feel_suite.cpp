// Real JUCE-free five-return vocal suite regression; synth fixtures only.
// Never claim these synthetic signals replace owner same-take vocal A/B.
#include "../Source/chain/VocalFeelSuiteDSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <chrono>
using namespace skellpreset::dsp;
constexpr double pi2=6.2831853071795864769;
static float sample(int n,double rate) {
    const float time=(float)(n/rate);
    const float syllable=(n%(int)(rate*.38)<(int)(rate*.28))?1.f:0.f;
    const float voice=.2f*std::sin((float)(pi2*190.*time))
                     +.07f*std::sin((float)(pi2*380.*time));
    return voice*syllable;
}
int main(){
    static_assert(VocalFeelSuite::latencySamples()==0);
    static_assert(vocalFeelVoices.size()==3);
    static_assert(vocalFeelVoices[0].mid!=vocalFeelVoices[1].mid);
    static_assert(vocalFeelVoices[2].slapMs<vocalFeelVoices[1].slapMs);
    for(double rate:{44100.,48000.,96000.}) {
        VocalFeelSuite clean;clean.prepare(rate);
        clean.set(vocalFeelNeutral);
        clean.beginBlock();
        for(int n=0;n<2048;++n) {
            const float x=sample(n,rate);
            const auto y=clean.process(x,x);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0]-x)<.0001f&&std::abs(y[1]-x)<.0001f);
        }
        // Each return produces actual audio, never merely a display parameter.
        for(int chain=0;chain<5;++chain) {
            VocalFeelSuite fx;fx.prepare(rate);
            auto p=vocalFeelNeutral;p[(size_t)(10+chain)]=1.f;fx.set(p);
            fx.beginBlock();
            double delta=0.;float peak=0.f;
            for(int n=0;n<(int)(rate*.9);++n) {
                const float x=sample(n,rate);
                const auto y=fx.process(x,x);
                assert(std::isfinite(y[0])&&std::isfinite(y[1]));
                peak=std::max(peak,std::max(std::abs(y[0]),std::abs(y[1])));
                delta+=std::abs(y[0]-x);
            }
            std::printf("SUITE %.0f CHAIN %d delta %.3f peak %.3f rms %.4f\n",
                        rate,chain,delta,peak,fx.chainRms(chain));
            assert(delta>1.0&&peak<4.f&&fx.chainRms(chain)>0.0001f);
        }
        // Full processing with sibilance and voice-dependent tones.
        for(int voice=0;voice<3;++voice) {
            VocalFeelSuite fx;fx.prepare(rate);
            auto p=vocalFeelNeutral;
            p[0]=voice*.5f;p[2]=.69f;p[3]=.56f;p[4]=.64f;
            p[5]=.82f;p[6]=.44f;p[7]=.72f;p[8]=.5f;
            for(int k=10;k<15;++k)p[(size_t)k]=.4f;
            fx.set(p);fx.beginBlock();
            for(int n=0;n<(int)(rate*.75);++n) {
                const float x=(n%4000)<3200?sample(n,rate):
                    .1f*std::sin((float)(pi2*8200.*n/rate));
                const auto y=fx.process(x,x*.84f);
                assert(std::isfinite(y[0])&&std::isfinite(y[1]));
                assert(std::abs(y[0])<16.f&&std::abs(y[1])<16.f);
            }
            assert(fx.compressorReductionDb()>=0.f);
            assert(fx.deesserReductionDb()>=0.f);
        }
        // When MAIN is fully muted, only enabled return remains.
        VocalFeelSuite solo;solo.prepare(rate);
        auto p=vocalFeelNeutral;p[9]=0.f;p[12]=.95f;solo.set(p);
        solo.beginBlock();double loud=0.;
        for(int n=0;n<(int)(rate*.5);++n){
            const auto y=solo.process(sample(n,rate),sample(n,rate));
            loud+=std::abs(y[0]);
        }
        assert(loud>.001);
    }
    // Stereo meter regression: the right-hand return must not disappear
    // simply because the original left vocal channel is silent.
    for(double rate:{44100.,48000.,96000.}) {
        VocalFeelSuite rightOnly;rightOnly.prepare(rate);
        auto settings=vocalFeelNeutral;
        settings[12]=1.f; // ATTACK return is processed independently L/R
        rightOnly.set(settings);
        rightOnly.beginBlock();
        float rightPeak=0.f;
        for(int n=0;n<(int)(rate*.5);++n) {
            const auto y=rightOnly.process(0.f,sample(n,rate));
            rightPeak=std::max(rightPeak,std::abs(y[1]));
        }
        assert(rightPeak>.01f);
        assert(rightOnly.chainRms(2)>.00001f);
    }
    // Repeating stable APVTS values at buffer boundaries cannot reset
    // opto/de-ess state, change the output, or add CPU-heavy reconfiguration.
    for(double rate:{44100.,48000.,96000.}) {
        VocalFeelSuite stable,repeated;
        stable.prepare(rate);repeated.prepare(rate);
        auto settings=vocalFeelNeutral;
        settings[5]=.65f;settings[6]=.48f;
        settings[7]=.7f;settings[8]=.5f;
        for(int k=10;k<15;++k)settings[(size_t)k]=.5f;
        stable.set(settings);repeated.set(settings);
        for(int n=0;n<(int)(rate*.6);++n) {
            if(n%256==0)repeated.set(settings);
            const float x=sample(n,rate);
            const float sib=.05f*std::sin((float)(pi2*7900.*n/rate));
            const auto one=stable.process(x+sib,x*.7f-sib);
            const auto two=repeated.process(x+sib,x*.7f-sib);
            assert(one[0]==two[0] && one[1]==two[1]);
        }
        // A fresh prepare must clear old return tails deterministically.
        stable.prepare(rate);repeated.prepare(rate);
        stable.set(settings);repeated.set(settings);
        for(int n=0;n<1024;++n) {
            const float x=sample(n,rate);
            const auto one=stable.process(x,x*.6f);
            const auto two=repeated.process(x,x*.6f);
            assert(one[0]==two[0] && one[1]==two[1]);
        }
    }
    VocalFeelSuite stress;stress.prepare(48000.);
    auto p=vocalFeelNeutral;
    for(int k=10;k<15;++k)p[(size_t)k]=.7f;
    p[5]=.4f;p[7]=.5f;stress.set(p);
    stress.beginBlock();
    const auto begin=std::chrono::steady_clock::now();
    float checksum=0.f;
    for(int n=0;n<48000;++n) {
        auto output=stress.process(sample(n,48000.),sample(n,48000.)*.87f);
        checksum+=output[0];
    }
    const auto ms=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-begin).count();
    std::printf("PASS: 3 rates, five real returns, full suite, solo return, voice profiles, clean unity, 0 PDC; one stereo second DSP-only %.2f ms, checksum %.3f\n",ms,checksum);
    return 0;
}
