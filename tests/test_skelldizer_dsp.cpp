#include "../Source/chain/SkelldizerDSP.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <limits>
using skellpreset::dsp::Skelldizer;
int main() {
    assert(Skelldizer::characterIndex(0.f)==0);
    assert(Skelldizer::characterIndex(1.f)==3);
    assert(Skelldizer::characterIndex(2.f/3.f)==2);
    assert(Skelldizer::characterIndex(std::numeric_limits<float>::quiet_NaN())==0);
    for(double sr:{44100.,48000.,96000.}) {
        Skelldizer e;
        e.prepare(sr);
        e.set(0.f,1.f,1.f,1.f);
        for(int i=0;i<250;++i) {
            const float v=.3f*(float)std::sin(i*.07);
            const auto y=e.process(v,-v);
            assert(y[0]==v&&y[1]==-v);
        }
        for(int mode=0;mode<4;++mode) {
            e.reset();e.set(.8f,mode/3.f,.5f,1.f);
            double in=0.,out=0.,difference=0.,maximum=0.;
            for(int i=0;i<(int)sr*2;++i) {
                const float x=.09f*(float)std::sin(2.*3.14159265358979323846*i*211./sr)
                    +.015f*(float)std::sin(2.*3.14159265358979323846*i*5400./sr);
                const auto y=e.process(x,x*.7f);
                for(float z:y){assert(std::isfinite(z));maximum=std::max(maximum,(double)std::abs(z));}
                if(i>(int)(sr*.5)) {
                    in+=x*x;out+=y[0]*y[0];difference+=std::abs((double)y[0]-x);
                }
            }
            const double db=10.*std::log10(out/in);
            assert(maximum<.99&&difference>1.&&db>-5.&&db<5.);
        }
        e.reset();e.set(1.f,1.f,.5f,1.f);
        for(int i=0;i<500;++i) {
            const auto y=e.process(i%2?1.f:-1.f,1.2f);
            for(float z:y)assert(std::isfinite(z)&&std::abs(z)<1.f);
        }
    }
    return 0;
}
