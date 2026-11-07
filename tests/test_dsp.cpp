#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    using namespace skellpreset::dsp;
    for(double sr:{44100.,48000.,96000.}) {
        Biquad eq;eq.setPeak(sr,1500.,6.,1.);
        Compressor comp;comp.prepare(sr);comp.set(-18,4,8,100,0);
        Delay delay;delay.prepare(sr);delay.configure((int)(sr*.12),.3f);
        for(int i=0;i<(int)sr;i++){
            if(i==(int)(sr*.35))delay.configure((int)(sr*.24),.4f);
            if(i==(int)(sr*.52))delay.configure((int)(sr*.07),.5f);
            float sample=.25f*std::sin((float)(2*pi*440*i/sr));
            auto p=eq.process(sample,0)*comp.gain(sample);
            auto echo=delay.process(p,p);
            auto y=softLimit(p+echo[0]*.35f);
            assert(std::isfinite(y));assert(std::abs(y)<=1.001f);
        }
    }
    std::cout<<"PASS portable DSP rates 44.1/48/96k\n";
}
