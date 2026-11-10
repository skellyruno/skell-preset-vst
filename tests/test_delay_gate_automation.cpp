#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "Regression assertions must remain enabled"
#endif
using namespace skellpreset::dsp;

static void queuedDelayTimeAutomation() {
    Delay d;
    d.prepare(48000.);
    // Every sample requests a new delay time, faster than a 20 ms crossfade.
    // Previously this restarted the fade indefinitely.
    for (int n=0;n<48000;++n) {
        d.configure(300 + ((n*103)%3600), .35f);
        const float input = .12f * std::sin(float(n)*.041f);
        const auto out=d.process(input,input);
        assert(std::isfinite(out[0]) && std::isfinite(out[1]));
        assert(d.transition>=0.f && d.transition<=1.f);
    }
    constexpr int settledTime=3500;
    d.configure(settledTime,.35f);
    for (int n=0;n<5000;++n) {
        auto out=d.process(0.f,0.f);
        assert(std::isfinite(out[0]) && std::isfinite(out[1]));
    }
    assert(d.samples==settledTime);
    assert(d.requestedSamples==settledTime);
    assert(d.transition==1.f);
    // Reject NaN, Inf, and enormous finite inputs without poisoning feedback.
    for (float hostile : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::max()}) {
        d.process(hostile, hostile);
    }
    for(int n=0;n<5000;++n) {
        const auto out=d.process(0.f,0.f);
        assert(std::isfinite(out[0]) && std::isfinite(out[1]));
        assert(std::abs(out[0])<=8.01f);
    }
    for(const auto& ring:d.memory)
        for(const auto v:ring)assert(std::isfinite(v) && std::abs(v)<=8.01f);
}

static void holdExpiresInsideHysteresisBand() {
    Gate gate;
    gate.prepare(8000.);
    gate.threshold=-34.f;
    gate.hysteresisDb=4.f;
    gate.holdMs=20.f;
    gate.release=22.f;
    for(int i=0;i<4000;++i)gate.processGain(.2f);
    assert(gate.open && gate.holdRemaining>0);
    // 0.016 is between the -34 dB opening and -38 dB closing thresholds.
    for(int i=0;i<4000;++i)gate.processGain(.016f);
    assert(gate.open);
    assert(gate.holdRemaining==0);
    for(int i=0;i<500;++i)gate.processGain(0.f);
    assert(!gate.open);
}
int main(){
    queuedDelayTimeAutomation();
    holdExpiresInsideHysteresisBand();
    std::cout<<"PASS queued delay automation, finite feedback, gate hold expiration\n";
}
