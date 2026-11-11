// Standalone C++17 checks for the permission-based vocal DSP adaptations.
// Does not require JUCE, a DAW, audio hardware or non-portable libraries.
#include "../Source/chain/ImportedVocalFX.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>

using namespace skellpreset::dsp;

int main() {
    for(double sampleRate : {8000.,44100.,48000.,96000.,192000.}) {
        VocalRider rider;
        rider.prepare(sampleRate);
        rider.set(0.f,-18.f,12.f,0.f);
        for(int i=0;i<3000;++i) {
            const float x=.23f*std::sin((float)i*.03f);
            const auto out=rider.process(x,-x*.7f);
            assert(std::abs(out[0]-x)<1.e-6f);
            assert(std::abs(out[1]+x*.7f)<1.e-6f);
        }
        rider.prepare(sampleRate);
        rider.set(1.f,-18.f,12.f,.25f);
        for(int i=0;i<9000;++i) {
            const auto x=(i%177==0)?0.f:.0004f;
            const auto y=rider.process(x,x);
            assert(std::isfinite(y[0]) && std::isfinite(y[1]));
            assert(std::abs(y[0])<=.001f); // silence/room noise must not be boosted
        }
        const auto bad=rider.process(std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity());
        assert(bad[0]==0.f && bad[1]==0.f);

        DuckingPingPongDelay delay;
        delay.prepare(sampleRate);
        delay.set(120.,1.f,.6f,0.f,.5f);
        const int tap=delay.delaySamples();
        assert(std::abs(tap-(int)std::round(sampleRate*.5))<=1);
        float primary=0.f,secondary=0.f;
        for(int i=0;i<tap*3;++i) {
            const auto echo=delay.process(i==0?1.f:0.f,0.f);
            assert(std::isfinite(echo[0]) && std::isfinite(echo[1]));
            if(i==tap)primary=echo[0];
            if(i==tap*2)secondary=std::abs(echo[1]);
        }
        assert(primary>.8f && primary<=1.0001f);
        assert(secondary>.001f && secondary<primary); // feedback crosses channels
        for(float time : {0.f,.25f,.5f,.75f,1.f}) {
            delay.set(90.,time,1.f,.9f,1.f);
            for(int i=0;i<700;++i) {
                const auto out=delay.process(.3f*std::sin(i*.011f),-.2f);
                assert(std::isfinite(out[0]) && std::isfinite(out[1]));
            }
        }
        VocalVibrato vib;
        vib.prepare(sampleRate);
        vib.set(.4f,0.f,0.f,0.f);
        const int fixedTap=(int)std::round(sampleRate*.008);
        float impulseL=0.f,impulseR=0.f;
        for(int i=0;i<fixedTap+8;++i) {
            const auto sample=vib.process(i==0?1.f:0.f,i==0?1.f:0.f);
            assert(std::isfinite(sample[0]) && std::isfinite(sample[1]));
            if(i>=fixedTap-2 && i<=fixedTap+1){
                impulseL+=sample[0];impulseR+=sample[1];
            }
        }
        assert(impulseL>.95f && impulseR>.95f); // fractional tap spreads across two samples
        vib.set(.95f,1.f,1.f,.75f);
        for(int i=0;i<(int)sampleRate/2;++i){
            const float in=.3f*std::sin((float)i*.117f);
            const auto y=vib.process(in,in);
            assert(std::isfinite(y[0])&&std::isfinite(y[1]));
            assert(std::abs(y[0])<1.001f && std::abs(y[1])<1.001f);
        }
    }

    // The rider should narrow gross changes in vocal phrase loudness.
    VocalRider rider;
    rider.prepare(48000.);
    rider.set(1.f,-18.f,15.f,0.f);
    double quiet=0.,loud=0.;
    for(int i=0;i<48000*5;++i) {
        const bool second=i>=48000*2;
        const float amp=second?.30f:.035f;
        const auto out=rider.process(amp*std::sin(i*.032f),amp*std::sin(i*.032f));
        if(i>=48000 && i<48000*2)quiet+=out[0]*out[0];
        if(i>=48000*4)loud+=out[0]*out[0];
    }
    const double ratio=std::sqrt(loud/quiet);
    assert(ratio>1. && ratio<7.); // originally 8.57x

    // Host tempo fallback and invalid automation never produce invalid audio.
    DuckingPingPongDelay delay;
    delay.prepare(48000.);
    delay.set(std::numeric_limits<double>::quiet_NaN(),-1.f,3.f,5.f,
              std::numeric_limits<float>::quiet_NaN());
    assert(delay.delaySamples()==6000); // 1/16 at fallback 120 BPM
    for(int i=0;i<40000;++i) {
        const auto y=delay.process(i==0?1.f:0.f,0.f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
    }
    return 0;
}
