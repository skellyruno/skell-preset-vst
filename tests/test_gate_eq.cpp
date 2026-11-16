#include "../Source/chain/BasicDSP.h"
#include <cstdio>
#define SKELL_REQUIRE(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL: %s (%s:%d)\\n", #expr, __FILE__, __LINE__); return 1; } } while (false)
#include <cmath>
#include <iostream>
using namespace skellpreset::dsp;
int main() {
    for(double rate:{44100.,48000.,96000.}) {
        Biquad bell,cut;
        bell.setPeak(rate,1800.,9.,1.2); cut.setHighPass(rate,100.);
        const auto centre=bell.magnitudeDb(1800.,rate);
        SKELL_REQUIRE(std::isfinite(centre)&&std::abs(centre-9.)<.05);
        SKELL_REQUIRE(std::abs(bell.magnitudeDb(20.,rate))<.12);
        SKELL_REQUIRE(cut.magnitudeDb(20.,rate)<-20.);
        SKELL_REQUIRE(std::isfinite(cut.magnitudeDb(20000.,rate)));
        Gate gate; gate.prepare(rate); gate.threshold=-30;gate.release=35;
        gate.holdMs=65;gate.hysteresisDb=6;
        for(int i=0;i<(int)(rate*.12);++i)gate.processGain(.10f);
        SKELL_REQUIRE(gate.open && gate.gain>.95);
        for(int i=0;i<(int)(rate*.025);++i)gate.processGain(0.f);
        SKELL_REQUIRE(gate.open);
        for(int i=0;i<(int)(rate*.60);++i)gate.processGain(0.f);
        SKELL_REQUIRE(!gate.open && gate.gain<.02);
        SKELL_REQUIRE(std::isfinite(gate.processGain(NAN)));
    }
    std::cout<<"PASS gate hold/hysteresis and analytical EQ response across 44.1/48/96 kHz\n";
}
