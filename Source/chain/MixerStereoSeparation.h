#pragma once
#include <algorithm>
#include <cmath>

// Skell Preset: native stereo separation for the final output stage.
//
// Mixer-style candidate: preserve the mid signal and scale only the side.
// Keep the published raw range, but map its full travel to the endpoints.
//
//     s  = raw / 96             (-96..+96, host range unchanged)
//     a  = 1 - s/2
//     b  = s/2
//     L' = a*L + b*R
//     R' = a*R + b*L
//
// Negative raw widens, positive raw merges toward mono, raw 0 is exactly
// neutral. The module introduces no delay, no chorus and
// no generated side signal. Full right is a true averaged mono signal.
//
// Smoothing is sample-count based over 10 ms, initialized neutral so a fresh
// instance (or a prepared-but-unset one) passes audio through unchanged.
namespace skellpreset::dsp {
class MixerStereoSeparation {
public:
    static constexpr int minimumRaw = -96;
    static constexpr int maximumRaw = 96;

    static float matrixA(int raw) noexcept {
        return 1.f - static_cast<float>(clampRaw(raw)) * (1.f / 192.f);
    }
    static float matrixB(int raw) noexcept {
        return 1.f - matrixA(raw);
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
        stepA = 0.f;
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
    }
    void process(float* left, float* right, int samples) noexcept {
        if (left == nullptr || right == nullptr || samples <= 0) return;
        for (int i = 0; i < samples; ++i) {
            if (remaining != 0) {
                if (--remaining != 0) {
                    a += stepA;
                    b = 1.f - a;
                } else {
                    a = targetA;
                    b = targetB;
                }
            }
            // Exact neutral bypass: no multiply, no rounding, no drift.
            if (a == 1.f && b == 0.f) continue;
            const float x = left[i];
            const float y = right[i];
            if (x == y) continue; // exact mono-source transparency at every setting
            if (a == .5f && b == .5f) {
                const float mid = .5f*x + .5f*y;
                left[i] = right[i] = mid;
                continue;
            }
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
    float stepA = 0.f;
    int rampSamples = 441, remaining = 0;
};
} // namespace skellpreset::dsp
