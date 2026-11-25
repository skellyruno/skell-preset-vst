#pragma once
#include <algorithm>
#include <cmath>

// Skell Preset: native stereo separation for the final output stage.
//
// The transfer law is the Stereo Enhancer separation matrix from the private
// R6 checkpoint (original StereoFx.cpp/.h), not a generic stereo widener:
//
//     s  = raw / 128            (raw is the supplied -96..+96 range)
//     a  = 1 - s/2
//     b  = a * s
//     L' = a*L + b*R
//     R' = a*R + b*L
//
// Negative raw widens, positive raw merges toward mono, raw 0 is exactly
// neutral. The module introduces no delay, no chorus, no phase inversion and
// no side signal: a mono input stays bit-exactly mono at every position.
//
// Smoothing is sample-count based over 10 ms, initialized neutral so a fresh
// instance (or a prepared-but-unset one) passes audio through unchanged.
namespace skellpreset::dsp {
class MixerStereoSeparation {
public:
    static constexpr int minimumRaw = -96;
    static constexpr int maximumRaw = 96;

    static float matrixA(int raw) noexcept {
        return 1.f - static_cast<float>(clampRaw(raw)) * (1.f / 256.f);
    }
    static float matrixB(int raw) noexcept {
        const float a = matrixA(raw);
        return a * (static_cast<float>(clampRaw(raw)) * (1.f / 128.f));
    }
    // Worst-case broadband peak gain of the matrix, used to audit headroom:
    // |L'| <= (|a|+|b|) * max(|L|,|R|). Neutral is exactly 1.
    static float peakGainBound(int raw) noexcept {
        return std::abs(matrixA(raw)) + std::abs(matrixB(raw));
    }

    void prepare(double sampleRate) noexcept {
        const double rate = std::isfinite(sampleRate) ? sampleRate : 44100.;
        rampSamples = std::max(1, static_cast<int>(
            std::lround(std::clamp(rate, 8000., 384000.) * .01)));
        reset();
    }
    void reset() noexcept {
        a = targetA;
        b = targetB;
        remaining = 0;
        stepA = stepB = 0.f;
    }
    void setRaw(int raw) noexcept {
        const float na = matrixA(raw);
        const float nb = matrixB(raw);
        // Repeating the destination must not restart the ramp. When the target
        // is unchanged the in-flight 10 ms glide simply keeps running.
        if (na == targetA && nb == targetB) return;
        targetA = na;
        targetB = nb;
        remaining = rampSamples;
        stepA = (targetA - a) / static_cast<float>(remaining);
        stepB = (targetB - b) / static_cast<float>(remaining);
    }
    void process(float* left, float* right, int samples) noexcept {
        if (left == nullptr || right == nullptr || samples <= 0) return;
        for (int i = 0; i < samples; ++i) {
            if (remaining != 0) {
                if (--remaining != 0) {
                    a += stepA;
                    b += stepB;
                } else {
                    a = targetA;
                    b = targetB;
                }
            }
            // Exact neutral bypass: no multiply, no rounding, no drift.
            if (a == 1.f && b == 0.f) continue;
            const float x = left[i];
            const float y = right[i];
            left[i] = a * x + b * y;
            right[i] = a * y + b * x;
        }
    }
    bool neutral() const noexcept {
        return a == 1.f && b == 0.f && targetA == 1.f && targetB == 0.f;
    }
    int rampLength() const noexcept { return rampSamples; }

private:
    static int clampRaw(int raw) noexcept {
        return std::clamp(raw, minimumRaw, maximumRaw);
    }
    float a = 1.f, b = 0.f;
    float targetA = 1.f, targetB = 0.f;
    float stepA = 0.f, stepB = 0.f;
    int rampSamples = 441, remaining = 0;
};
} // namespace skellpreset::dsp
