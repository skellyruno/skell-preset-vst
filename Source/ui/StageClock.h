#pragma once
#include <algorithm>
#include <cmath>

namespace skellpreset::stage {
// Message-thread elapsed clock. Reset the time anchor when hidden, not the
// pose. A stalled message loop must not cause a large catch-up jump.
class StageClock {
public:
    void pause() noexcept { previous=-1.; }
    double tick(double nowMs,float activity) noexcept {
        if(!std::isfinite(nowMs)) { pause();return phase; }
        const double elapsed=previous<0.?0.:std::clamp((nowMs-previous)*.001,0.,.100);
        previous=nowMs;
        const double energy=std::isfinite(activity)?std::clamp(static_cast<double>(activity),0.,1.):0.;
        phase+=elapsed*(1.65+3.90*energy);
        return phase;
    }
private:
    double previous=-1.,phase=0.;
};
}
