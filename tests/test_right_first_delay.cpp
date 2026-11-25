#include "../Source/chain/StudioDelayDSP.h"
#include "../Source/chain/WetDuckerDSP.h"
#include <array>
#include <cmath>
#include <cstdio>
int main() {
    for(double rate:{44100.,48000.,96000.,192000.}) {
        skellpreset::studio::StereoDelay a,b,c;
        a.prepare(rate);b.prepare(rate);c.prepare(rate);
        std::array<float,16> p{};
        p[0]=float(std::log(.978/.025)/std::log(80.));
        p[1]=.7f;p[2]=.625f;p[5]=1.f;p[10]=1.f;
        a.set(p,120.,true);b.set(p,120.);c.set(p,120.,false);
        const int first=int(std::round(rate*.978)),second=2*first;
        float rightPeak=0.f,leftPeak=0.f;
        for(int n=0;n<second+20;++n) {
            const float x=n==0?1.f:0.f;
            if(n%127==0)a.set(p,120.,true);
            const auto y=a.process(x,x),old=b.process(x,x),explicitOff=c.process(x,x);
            if(old!=explicitOff || !std::isfinite(y[0])||!std::isfinite(y[1]))return 1;
            if(n<first-2&&(std::abs(y[0])+std::abs(y[1])>1.e-6f))return 2;
            if(std::abs(n-first)<=2) {
                if(std::abs(y[0])>1.e-6f)return 3;
                rightPeak=std::max(rightPeak,std::abs(y[1]));
            }
            if(std::abs(n-second)<=4)leftPeak=std::max(leftPeak,std::abs(y[0]));
        }
        if(rightPeak<.4f||leftPeak<.08f)return 4;
        skellpreset::dsp::VoiceWetDucker duck;
        duck.prepare(rate);duck.set(true,.8f,-32.f,320.f,5.f);
        float gain=1.f;
        for(int n=0;n<int(rate*.5);++n)gain=duck.processGain(.6f,-.6f);
        if(gain>.25f||skellpreset::dsp::duckDubReturn(.5f,gain)>.125f)return 5;
        for(int n=0;n<int(rate*2.);++n)gain=duck.processGain(0.f,0.f);
        if(gain<.99f)return 6;
        std::printf("PASS %.0f Hz right-first impulse (%.3f, %.3f), legacy parity, dry-sourced duck recovery\n",rate,rightPeak,leftPeak);
    }
}
