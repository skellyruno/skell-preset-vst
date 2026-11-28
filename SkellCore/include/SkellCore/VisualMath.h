#pragma once
// Pure, allocation-free geometry for effect visualisations. UI/message
// thread or tests only. Every curve here is either (a) the exact static law
// of an engine in this repo (validated against that engine in tests) or
// (b) MEASURED from the engine by measureStaticTransfer(). Nothing is drawn
// from an assumed formula the DSP does not implement.
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace skellcore::visual {

// ---- axes
struct LogFreqAxis {
    float minHz = 20.f, maxHz = 20000.f;
    float toNorm(float hz) const noexcept {
        const float lo = std::log(minHz), hi = std::log(maxHz);
        if (!(hz > 0.f) || !std::isfinite(hz)) return 0.f;
        return std::clamp((std::log(hz) - lo) / (hi - lo), 0.f, 1.f);
    }
    float fromNorm(float x) const noexcept {
        const float lo = std::log(minHz), hi = std::log(maxHz);
        return std::exp(lo + std::clamp(x, 0.f, 1.f) * (hi - lo));
    }
};
inline float dbToNorm(float db, float loDb, float hiDb) noexcept {
    if (!std::isfinite(db) || hiDb <= loDb) return 0.f;
    return std::clamp((db - loDb) / (hiDb - loDb), 0.f, 1.f);
}

// ---- compressor static law
// kneeDb == 0 is EXACTLY the repo's skellpreset Compressor: no knee,
// reduction = (level - threshold) * (1 - 1/ratio) above threshold.
// kneeDb > 0 is the standard quadratic soft knee and must only be used for an
// engine that actually implements one (the current rack compressor does not).
inline float compressorReductionDb(float inDb, float thresholdDb, float ratio, float kneeDb = 0.f) noexcept {
    const float r = std::clamp(std::isfinite(ratio) ? ratio : 1.f, 1.f, 100.f);
    const float slope = 1.f - 1.f / r;
    const float over = inDb - thresholdDb;
    const float k = std::max(0.f, std::isfinite(kneeDb) ? kneeDb : 0.f);
    if (k <= 0.f) return over > 0.f ? over * slope : 0.f;
    if (over <= -0.5f * k) return 0.f;
    if (over >= 0.5f * k) return over * slope;
    const float t = over + 0.5f * k;
    return slope * t * t / (2.f * k);
}
inline float compressorOutDb(float inDb, float thresholdDb, float ratio, float makeupDb = 0.f, float kneeDb = 0.f) noexcept {
    return inDb - compressorReductionDb(inDb, thresholdDb, ratio, kneeDb) + makeupDb;
}
// Fills out[0..n) with output dB for inputs evenly spaced over [inLoDb,inHiDb].
inline void buildCompressorCurve(float* out, std::size_t n, float inLoDb, float inHiDb,
                                 float thresholdDb, float ratio, float makeupDb = 0.f, float kneeDb = 0.f) noexcept {
    if (!out || n == 0) return;
    for (std::size_t i = 0; i < n; ++i) {
        const float t = n > 1 ? float(i) / float(n - 1) : 0.f;
        out[i] = compressorOutDb(inLoDb + t * (inHiDb - inLoDb), thresholdDb, ratio, makeupDb, kneeDb);
    }
}

// ---- gate thresholds (Schmitt) matching skellpreset Gate:
// opens at >= thresholdDb, closes only below thresholdDb - hysteresis (and after hold).
struct GateThresholds { float openDb, closeDb; };
inline GateThresholds gateThresholds(float thresholdDb, float hysteresisDb) noexcept {
    const float h = std::clamp(std::isfinite(hysteresisDb) ? hysteresisDb : 0.f, 0.f, 12.f);
    return {thresholdDb, thresholdDb - h};
}

// ---- waveform decimation: n samples -> columns min/max pairs (no allocation)
inline void decimateMinMax(const float* src, std::size_t n, float* mins, float* maxs, std::size_t columns) noexcept {
    if (!mins || !maxs || columns == 0) return;
    for (std::size_t c = 0; c < columns; ++c) { mins[c] = 0.f; maxs[c] = 0.f; }
    if (!src || n == 0) return;
    for (std::size_t c = 0; c < columns; ++c) {
        const std::size_t a = c * n / columns;
        std::size_t b = (c + 1) * n / columns; if (b <= a) b = std::min(n, a + 1);
        float lo = src[a], hi = src[a];
        for (std::size_t i = a + 1; i < b; ++i) { lo = std::min(lo, src[i]); hi = std::max(hi, src[i]); }
        mins[c] = std::isfinite(lo) ? lo : 0.f; maxs[c] = std::isfinite(hi) ? hi : 0.f;
    }
}

// ---- MEASURED static transfer from a real engine (not realtime; tests / UI thread)
// processSample(x) is called per sample with a sine of the requested peak level.
// Returns the output level in dB (RMS ratio applied to the input level), so a
// linear engine returns inputDb. The first `settleSamples` are discarded.
template <class Fn>
float measureStaticTransferDb(Fn&& processSample, float inputPeakDb, double sampleRate,
                              double freqHz = 997.0, int settleSamples = 24000, int measureSamples = 24000) {
    const double amp = std::pow(10.0, double(inputPeakDb) / 20.0);
    const double w = 6.283185307179586 * freqHz / sampleRate;
    double ssIn = 0, ssOut = 0; long idx = 0;
    for (int i = 0; i < settleSamples + measureSamples; ++i, ++idx) {
        const float x = float(amp * std::sin(w * double(idx)));
        const float y = processSample(x);
        if (i >= settleSamples) { ssIn += double(x) * x; ssOut += double(y) * y; }
    }
    if (ssIn <= 0.0 || ssOut <= 0.0) return -240.f;
    return inputPeakDb + float(10.0 * std::log10(ssOut / ssIn));
}

} // namespace skellcore::visual
