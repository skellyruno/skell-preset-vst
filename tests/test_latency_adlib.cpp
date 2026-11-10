#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace skellpreset::dsp;
int main() {
    StereoSampleDelay dry, bypass;
    dry.prepare(384);bypass.prepare(384);
    const int types[]{1,3,2,3,0,0,0,0,0,0};
    const int summed=seriesLatencySamples(types,10,3,144);
    assert(summed==288);
    for(int n=0;n<600;++n){
        const auto a=dry.process(n==0?1.f:0.f,n==0?-.5f:0.f,summed);
        const auto b=bypass.process(n==0?1.f:0.f,n==0?-.5f:0.f,summed);
        assert(a==b);
        assert(a[0]==(n==summed?1.f:0.f));
        assert(a[1]==(n==summed?-.5f:0.f));
    }
    dry.reset();
    for(int n=0;n<20;++n){auto a=dry.process(0,0,summed);assert(a[0]==0&&a[1]==0);}
    AdlibDoubler stack;stack.prepare(48000);
    for(int n=0;n<48000;++n){
        const float x=.2f*std::sin((float)(2*pi*220.*n/48000.));
        const auto y=stack.process(x,x,.8f,.5f,.55f,.7f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        assert(std::abs(y[0])<=.25f&&std::abs(y[1])<=.25f);
    }
    stack.reset();
    for(int n=0;n<8000;++n){auto o=stack.process(0,0,.5f,.5f,.5f,.5f);assert(o[0]==0&&o[1]==0);}
    std::cout<<"PASS sample-exact dry/bypass PDC and bounded stereo adlib doubler\n";
}
