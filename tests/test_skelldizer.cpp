#include "../Source/chain/Skelldizer.h"
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
int main() {
    skellpreset::dsp::Skelldizer fx;
    for(double rate : {44100.,48000.,96000.}) {
        fx.prepare(rate);
        fx.set(.7f,.6f,.48f,.50f);
        double maxAbs=0, sum=0;
        for(int n=0;n<static_cast<int>(rate);++n) {
            const auto x=.22f*std::sin(6.283185307179586*220.0*n/rate);
            const auto y=fx.process(x,x);
            assert(std::isfinite(y[0]) && std::isfinite(y[1]));
            assert(std::abs(y[0]-y[1])<1.e-6f);
            maxAbs=std::max(maxAbs,std::abs((double)y[0]));
            sum+=std::abs((double)y[0]);
        }
        assert(maxAbs>0.05 && maxAbs<4.0 && sum>0.0);
        fx.set(1.f,1.f,1.f,1.f);
        for(int n=0;n<static_cast<int>(rate/10);++n) {
            auto y=fx.process(1.f,-1.f);
            assert(std::isfinite(y[0]) && std::isfinite(y[1]));
            assert(std::abs(y[0])<=4.0f && std::abs(y[1])<=4.0f);
        }
        auto nonfinite=fx.process(std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity());
        assert(std::isfinite(nonfinite[0]) && std::isfinite(nonfinite[1]));
        fx.prepare(rate);
        fx.set(0.f,.5f,.5f,.5f);
        for(int n=0;n<static_cast<int>(rate/3);++n) {
            auto y=fx.process(0.f,0.f);
            assert(y[0]==0.f && y[1]==0.f);
        }
    }
}
