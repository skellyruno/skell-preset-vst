#pragma once
// Central rack latency / bypass-cost policy; no JUCE dependency.
// Preserve the historical rule: an inserted lookahead limiter reserves PDC
// even at wet=0, so automation does not silently shift the dry signal.
#include <algorithm>
#include <cmath>
namespace skellpreset::realtime {
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
