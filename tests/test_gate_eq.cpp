#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace skellpreset::dsp;
int main() {
    for(double rate:{44100.,48000.,96000.}) {
        Biquad bell,cut;
        bell.setPeak(rate,1800.,9.,1.2); cut.setHighPass(rate,100.);
        const auto centre=bell.magnitudeDb(1800.,rate);
        assert(std::isfinite(centre)&&std::abs(centre-9.)<.05);
        assert(std::abs(bell.magnitudeDb(20.,rate))<.12);
        assert(cut.magnitudeDb(20.,rate)<-20.);
        assert(std::isfinite(cut.magnitudeDb(20000.,rate)));
        Gate gate; gate.prepare(rate); gate.threshold=-30;gate.release=35;
        gate.holdMs=65;gate.hysteresisDb=6;
        for(int i=0;i<(int)(rate*.12);++i)gate.processGain(.10f);
        assert(gate.open && gate.gain>.95);
        for(int i=0;i<(int)(rate*.025);++i)gate.processGain(0.f);
        assert(gate.open);
        for(int i=0;i<(int)(rate*.60);++i)gate.processGain(0.f);
        assert(!gate.open && gate.gain<.02);
        assert(std::isfinite(gate.processGain(NAN)));
    }
    std::cout<<"PASS gate hold/hysteresis and analytical EQ response across 44.1/48/96 kHz\n";
}
