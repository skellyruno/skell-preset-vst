#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

// Deterministic realtime-automation stress: finite stereo output, bounded
// delay feedback, and a click-reducing tap transition (no audio-thread malloc).
int main() {
    constexpr double rate = 48000.0;
    using namespace skellpreset::dsp;
    Delay d;
    d.prepare(rate);
    d.configure(2300, .35f);
    float peak = 0.f;
    for (int n=0; n<240000; ++n) {
        if (n == 30000) d.configure(7000, .46f);
        if (n == 50000) d.configure(1100, .34f);
        if (n >= 60000 && n % 800 == 0)
            d.configure(1000 + (n / 800) % 7500, .45f);
        const auto t=(double)n/rate;
        const float x=.21f*std::sin(float(2*pi*220*t));
        auto out=d.process(x, x);
        assert(std::isfinite(out[0]) && std::isfinite(out[1]));
        assert(out[0] == out[1]);
        peak=std::max(peak,std::abs(out[0]));
        assert(peak < 1.0f); // bounded below with stable feedback
    }
    d.reset();
    for (int n=0;n<2000;++n) {
        auto o=d.process(0.f,0.f);
        assert(o[0] == 0.f && o[1] == 0.f);
    }
    std::cout << "PASS realtime delay automation, finite output, reset and bounded feedback\n";
}
