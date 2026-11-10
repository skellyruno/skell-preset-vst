#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
#if defined(NDEBUG)
#error "Assertions must remain enabled in Release"
#endif
using namespace skellpreset::dsp;

static void automation(double rate) {
    AdlibDoubler d;
    d.prepare(rate);
    float previous=0.f,maximumJump=0.f;
    for(int n=0;n<(int)rate;++n) {
        const bool flip=(n/23)%2==0;
        const float param=flip?0.f:1.f;
        const float input=.15f*std::sin(float(n)*.029f);
        const auto out=d.process(input,input,param,param,param,param);
        assert(std::isfinite(out[0])&&std::isfinite(out[1]));
        assert(std::abs(out[0])<=.16f&&std::abs(out[1])<=.16f);
        if(n>5000)maximumJump=std::max(maximumJump,std::abs(out[0]-previous));
        previous=out[0];
    }
    assert(maximumJump<.10f);
    const auto initial=d.depthSmooth;
    (void)d.process(.08f,.08f,1,1,1,1);
    // One sample must not teleport a doubler read head by 24 ms.
    assert(d.depthSmooth<initial+.003f);
    for(int n=0;n<(int)(rate*.5);++n) (void)d.process(0,0,1,1,1,1);
    assert(d.depthSmooth>.99f);
    for(float hostile:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()})
        (void)d.process(hostile,hostile,hostile,hostile,hostile,hostile);
    for(int i=0;i<(int)(rate*.15);++i){
        auto y=d.process(0,0,0,0,0,0);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
    }
}
int main(){for(double rate:{44100.,48000.,96000.})automation(rate);std::cout<<"PASS dezippered adlib doubler parameters\n";}
