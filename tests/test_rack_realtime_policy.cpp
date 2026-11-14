#include "../Source/chain/RackRealtimePolicy.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include <cassert>
#include <cmath>
#include <limits>
// No JUCE required; test host-reported PDC policy independently of platform.
int main() {
    using namespace skellpreset;
    using namespace skellpreset::realtime;
    // Full, shortened and reordered racks must only configure active IDs.
    std::uint64_t identity=0;
    for(int id=0;id<16;++id)identity|=(std::uint64_t)id<<(4*id);
    assert(activeSlotMask(identity,0,16)==0);
    assert(activeSlotMask(identity,7,16)==0x7fU);
    assert(activeSlotMask(identity,16,16)==0xffffU);
    assert(activeSlotMask(identity,20,16)==0xffffU); // clamp corrupt count
    const std::uint64_t reorder=std::uint64_t{9} |
            (std::uint64_t{2}<<4) | (std::uint64_t{7}<<8);
    assert(activeSlotMask(reorder,3,16)==((1u<<9)|(1u<<2)|(1u<<7)));
    // Slots become eligible for lazy coefficient configuration on expansion.
    assert((activeSlotMask(identity,8,16)&(1u<<8))==0);
    assert((activeSlotMask(identity,9,16)&(1u<<8))!=0);
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
