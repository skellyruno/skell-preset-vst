// Real JUCE-free five-return vocal suite regression; synth fixtures only.
// Never claim these synthetic signals replace owner same-take vocal A/B.
#include "../Source/chain/VocalFeelSuiteDSP.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <limits>
#include <memory>
#include <vector>
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
    // Invalid fractional delay readout is explicitly rejected rather than
    // allowing NaN -> size_t conversion inside the ring index.
    {
        VocalFeelLine line;
        line.prepare(48000);
        for(int n=0;n<250;++n)line.push(.2f);
        assert(line.read(std::numeric_limits<float>::quiet_NaN())==0.f);
        assert(line.read(std::numeric_limits<float>::infinity())==0.f);
        assert(std::isfinite(line.read(-1000.f)));
        assert(std::isfinite(line.read(1.e9f)));
    }
    // Changing one parameter must recompute the proper filters, while
    // unchanged updates never disturb a compressor's state, across rates.
    for(double rate:{44100.,48000.,96000.}) {
        VocalFeelSuite stable,repeated;
        stable.prepare(rate);repeated.prepare(rate);
        auto params=vocalFeelNeutral;
        params[2]=.78f;params[4]=.25f;params[5]=.45f;
        params[7]=.76f;params[8]=.3f;
        for(int k=10;k<15;++k)params[(size_t)k]=.4f;
        stable.set(params);repeated.set(params);
        stable.beginBlock();repeated.beginBlock();
        for(int n=0;n<(int)(rate*.35);++n) {
            if(n==(int)(rate*.12)) {
                params[4]=.62f;params[2]=.55f;params[7]=.51f;
                stable.set(params);repeated.set(params);
            }
            if(n%32==0)repeated.set(params);
            const float x=sample(n,rate);
            const auto a=stable.process(x,-x*.42f);
            const auto b=repeated.process(x,-x*.42f);
            assert(a==b);
            assert(std::isfinite(a[0]) && std::isfinite(a[1]));
        }
        for(int k=0;k<5;++k)
            assert(std::abs(stable.chainRms(k)-repeated.chainRms(k))<1.e-6f);
    }
    // Invalid host automation is zero-clamped and must not poison filters,
    // meters or a later valid voice. Test both stereo channels separately.
    {
        VocalFeelSuite hostile;
        hostile.prepare(48000.);
        auto params=vocalFeelNeutral;
        params.fill(std::numeric_limits<float>::quiet_NaN());
        hostile.set(params);
        for(int n=0;n<512;++n) {
            const auto a=hostile.process(
                n%2?std::numeric_limits<float>::infinity():.6f,
                n%3?std::numeric_limits<float>::quiet_NaN():-.6f);
            assert(std::isfinite(a[0]) && std::isfinite(a[1]));
            assert(std::abs(a[0])<=16.f && std::abs(a[1])<=16.f);
        }
        params=vocalFeelNeutral;
        hostile.set(params);hostile.beginBlock();
        for(int n=0;n<2048;++n){
            const auto a=hostile.process(sample(n,48000.),0.f);
            assert(std::isfinite(a[0]) && std::isfinite(a[1]));
        }
        for(int chain=0;chain<5;++chain)
            assert(std::isfinite(hostile.chainRms(chain)));
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
    // Diagnostic only. No flaky CI CPU threshold and no false FL Studio
    // projection: these timings are native DSP-only runner measurements.
    for(int instances:{8,16}) {
        std::vector<std::unique_ptr<VocalFeelSuite>> engines;
        engines.reserve((size_t)instances);
        for(int k=0;k<instances;++k) {
            auto effect=std::make_unique<VocalFeelSuite>();
            effect->prepare(48000.);
            effect->set(p);
            engines.push_back(std::move(effect));
        }
        float sum=0.f;
        const auto start=std::chrono::steady_clock::now();
        for(int n=0;n<48000;++n) {
            const float input=sample(n,48000.);
            for(auto& fx:engines) {
                if(n%128==0)fx->set(p);
                const auto output=fx->process(input,-input*.7f);
                sum+=output[0]*.0000005f;
            }
        }
        const auto elapsed=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-start).count();
        std::printf("BENCH: Vocal Feel Suite @48kHz full FX 128-sample blocks %d instances, %.2f ms total, %.2f ms/instance, checksum %.6f\n",
            instances,elapsed,elapsed/instances,(double)sum);
        assert(std::isfinite(sum));
    }
    return 0;
}
