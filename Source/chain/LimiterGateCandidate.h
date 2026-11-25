#pragma once
#include <algorithm>
#include <cmath>

namespace skellpreset::research {
// Isolated, causal gate release law. The input is a detector peak supplied by
// the caller, not the full limiter's lookahead/peak-window/comp/saturation path.
// No production parameter IDs or signal routing are changed by this prototype.
class LimiterGateCandidate {
public:
    void configure(double rate, double releaseMs, float threshold, float floor) noexcept {
        const double hz=std::isfinite(rate)?std::clamp(rate,8000.,384000.):48000.;
        const double ms=std::isfinite(releaseMs)?std::clamp(releaseMs,0.,1000.):90.;
        const double frames=std::max(1.,std::round(hz*ms*.001));
        keep=std::exp(std::log(.01)/(2.*frames));
        threshold_=std::isfinite(threshold)?std::clamp(threshold,0.f,1.f):0.f;
        floor_=std::isfinite(floor)?std::clamp(floor,0.f,1.f):1.f;
    }
    void reset() noexcept { gain_=1.; }
    float processGain(float peak) noexcept {
        const float level=std::isfinite(peak)?std::abs(peak):0.f;
        if(level>=threshold_) gain_=1.;
        else gain_=floor_+(gain_-floor_)*keep;
        if(std::abs(gain_-floor_)<1.e-15)gain_=floor_;
        return static_cast<float>(gain_);
    }
private:
    double keep=0.,gain_=1.;
    float threshold_=0.f,floor_=1.f;
};
}
