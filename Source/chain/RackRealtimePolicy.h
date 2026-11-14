#pragma once
// Central rack latency / bypass-cost policy; no JUCE dependency.
// Preserve the historical rule: an inserted lookahead limiter reserves PDC
// even at wet=0, so automation does not silently shift the dry signal.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace skellpreset::realtime {
inline std::uint32_t activeSlotMask(std::uint64_t packedOrder,int activeCount,
                                    int capacity) noexcept {
    // Packed order stores sixteen 4-bit physical slot IDs at most.
    const int count=std::clamp(activeCount,0,std::min(capacity,16));
    std::uint32_t mask=0;
    for(int pos=0;pos<count;++pos) {
        const auto id=(unsigned)((packedOrder>>(4*pos))&0x0fU);
        if(id<(unsigned)capacity)mask|=(std::uint32_t{1}<<id);
    }
    return mask;
}
inline int stageLatency(int module,int limiterModule,int externalModule,int pitchModule,
                        int limiterSamples,int externalSamples,int pitchSamples) noexcept {
    if(module==limiterModule)return std::max(0,limiterSamples);
    if(module==externalModule)return std::max(0,externalSamples);
    if(module==pitchModule)return std::max(0,pitchSamples);
    return 0;
}
inline bool needsLimiterWetRender(float currentWet,float targetWet) noexcept {
    // When dry, delay the dry path for PDC but avoid expensive oversampled
    // peak detection. When a fade is underway, process until it completes.
    return (std::isfinite(currentWet) && currentWet>0.f) ||
           (std::isfinite(targetWet) && targetWet>0.f);
}
}
