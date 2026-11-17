#pragma once
// Small, bounded hot-path math helpers for the causal Alchemist finish.
// The logarithm/exponential identities preserve the same transfer functions;
// the rational tanh is bounded to [-4,4] and used only by the soft ceiling.
#include <algorithm>
#include <cmath>

namespace skellpreset::dsp::alchemist_fast_math {
inline constexpr float log2ToDb10 = 3.010299956639812f;
inline constexpr float dbToLog2 = 0.1660964047443681f;

inline constexpr float log2ToDb20 = 6.020599913279624f;

inline float powerDb(float power, float floor = 1.e-14f) noexcept {
    return log2ToDb10 * std::log2(power + floor);
}

inline float gainDb(float gain) noexcept {
    return log2ToDb20 * std::log2(std::max(gain, 1.e-12f));
}

inline float gainFromDb(float db) noexcept {
    return std::exp2(db * dbToLog2);
}

// [7/7] Pade tanh; max absolute error is about 1.51e-5 on [-4,4].
// Outside that interval the transfer is already effectively at its knee.
inline float tanhPade(float x) noexcept {
    x = std::clamp(x, -4.f, 4.f);
    const float x2 = x * x;
    const float x4 = x2 * x2;
    return x * (135135.f + 17325.f * x2 + 378.f * x4 + x4 * x2)
         / (135135.f + 62370.f * x2 + 3150.f * x4 + 28.f * x4 * x2);
}

inline float softCeiling(float input) noexcept {
    if (!std::isfinite(input)) return 0.f;
    constexpr float threshold = .85f;
    constexpr float range = .08f;
    const float magnitude = std::abs(input);
    if (magnitude <= threshold) return input;
    const float output = threshold
        + range * tanhPade((magnitude - threshold) / range);
    return std::copysign(output, input);
}
} // namespace skellpreset::dsp::alchemist_fast_math
