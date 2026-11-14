#include "../Source/chain/RackRealtimePolicy.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include <cassert>
#include <cmath>
#include <limits>
// No JUCE required; test host-reported PDC policy independently of platform.
int main() {
    using namespace skellpreset;
    using namespace skellpreset::realtime;
    constexpr int limiter=3, external=12, pitch=30;
    constexpr int limiterSamples=144, pitchSamples=768;
    int alchemistLatency=0;
    for(const auto& stage: alchemist::stages)
        alchemistLatency+=stageLatency(stage.module,limiter,external,pitch,
                                        limiterSamples,0,pitchSamples);
    // All seven native Alchemist stages are zero-lookahead.
    assert(alchemistLatency==0);
    assert(stageLatency(limiter,limiter,external,pitch,144,0,768)==144);
    assert(stageLatency(external,limiter,external,pitch,144,230,768)==230);
    assert(stageLatency(pitch,limiter,external,pitch,144,0,768)==768);
    assert(stageLatency(limiter,limiter,external,pitch,-5,0,768)==0);
    assert(stageLatency(0,limiter,external,pitch,144,0,768)==0);
    // Two lookahead limiters are six milliseconds at 48kHz.
    assert(2*stageLatency(limiter,limiter,external,pitch,144,0,768)==288);
    assert(!needsLimiterWetRender(0.f,0.f));
    assert(!needsLimiterWetRender(0.f,std::numeric_limits<float>::quiet_NaN()));
    assert(needsLimiterWetRender(.01f,0.f)); // pending bypass ramp
    assert(needsLimiterWetRender(0.f,.01f)); // ramping in
    return 0;
}
