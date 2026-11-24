#include "../Source/chain/RackRealtimePolicy.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include <cstdio>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)
#include <cmath>
#include <limits>
// No JUCE required; test host-reported PDC policy independently of platform.
int main() {
    using namespace skellpreset;
    using namespace skellpreset::realtime;
    // Full, shortened and reordered racks must only configure active IDs.
    std::uint64_t identity=0;
    for(int id=0;id<16;++id)identity|=(std::uint64_t)id<<(4*id);
    SKELL_REQUIRE(activeSlotMask(identity,0,16)==0);
    SKELL_REQUIRE(activeSlotMask(identity,7,16)==0x7fU);
    SKELL_REQUIRE(activeSlotMask(identity,16,16)==0xffffU);
    SKELL_REQUIRE(activeSlotMask(identity,20,16)==0xffffU); // clamp corrupt count
    SKELL_REQUIRE(activeSlotMask(identity,16,-1)==0);
    SKELL_REQUIRE(activeSlotMask(identity,-4,16)==0);
    SKELL_REQUIRE(activeSlotMask(identity,16,0)==0);
    SKELL_REQUIRE(activeSlotMask(identity,20,100)==0xffffU);
    SKELL_REQUIRE(activeSlotMask(identity,16,3)==7U);
    SKELL_REQUIRE(activeSlotMask(0,16,16)==1U);
    SKELL_REQUIRE(activeSlotMask(0xffffffffffffffffULL,16,8)==0);
    const std::uint64_t reorder=std::uint64_t{9} |
            (std::uint64_t{2}<<4) | (std::uint64_t{7}<<8);
    SKELL_REQUIRE(activeSlotMask(reorder,3,16)==((1u<<9)|(1u<<2)|(1u<<7)));
    // Slots become eligible for lazy coefficient configuration on expansion.
    SKELL_REQUIRE((activeSlotMask(identity,8,16)&(1u<<8))==0);
    SKELL_REQUIRE((activeSlotMask(identity,9,16)&(1u<<8))!=0);
    constexpr int limiter=3, external=12, pitch=30;
    constexpr int limiterSamples=144, pitchSamples=768;
    int alchemistLatency=0;
    for(const auto& stage: alchemist::stages)
        alchemistLatency+=stageLatency(stage.module,limiter,external,pitch,
                                        limiterSamples,0,pitchSamples);
    // All seven native Alchemist stages are zero-lookahead.
    SKELL_REQUIRE(alchemistLatency==0);
    SKELL_REQUIRE(stageLatency(limiter,limiter,external,pitch,144,0,768)==144);
    SKELL_REQUIRE(stageLatency(external,limiter,external,pitch,144,230,768)==230);
    SKELL_REQUIRE(stageLatency(pitch,limiter,external,pitch,144,0,768)==768);
    SKELL_REQUIRE(stageLatency(limiter,limiter,external,pitch,-5,0,768)==0);
    SKELL_REQUIRE(stageLatency(0,limiter,external,pitch,144,0,768)==0);
    // Two lookahead limiters are six milliseconds at 48kHz.
    SKELL_REQUIRE(2*stageLatency(limiter,limiter,external,pitch,144,0,768)==288);
    SKELL_REQUIRE(!needsLimiterWetRender(0.f,0.f));
    SKELL_REQUIRE(!needsLimiterWetRender(0.f,std::numeric_limits<float>::quiet_NaN()));
    SKELL_REQUIRE(needsLimiterWetRender(.01f,0.f)); // pending bypass ramp
    SKELL_REQUIRE(needsLimiterWetRender(0.f,.01f)); // ramping in
    return 0;
}
