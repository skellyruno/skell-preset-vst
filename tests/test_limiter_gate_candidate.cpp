#include "../Source/chain/LimiterGateCandidate.h"
#include <cmath>
#include <cstdio>
#include <limits>

using skellpreset::research::LimiterGateCandidate;
int main() {
    for(double rate:{44100.,48000.,96000.,192000.}) {
        constexpr double ms=90.;
        constexpr float floor=.025f;
        const int frames=static_cast<int>(std::round(rate*ms*.001));
        LimiterGateCandidate g,split;
        g.configure(rate,ms,.01f,floor);split.configure(rate,ms,.01f,floor);
        if(g.processGain(.01f)!=1.f || split.processGain(.01f)!=1.f)return 1;
        float last=1.f;
        for(int n=0;n<2*frames;++n) {
            const float now=g.processGain(0.f);
            if(!std::isfinite(now)||now>last||now<floor)return 2;
            last=now;
        }
        if(std::abs(last-(floor+(1.f-floor)*.01f))>2.e-6f)return 3;
        for(int offset=0;offset<2*frames;offset+=127)
            for(int n=offset;n<std::min(offset+127,2*frames);++n)
                split.processGain(0.f);
        if(g.processGain(0.f)!=split.processGain(0.f))return 4;
        if(g.processGain(.25f)!=1.f)return 5;
        // One shared gain applied to both channels preserves their ratio.
        const float common=g.processGain(.001f);
        if(std::abs((.001f*common)/(-.0005f*common)+2.f)>1.e-6f)return 6;
        if(!std::isfinite(g.processGain(std::numeric_limits<float>::quiet_NaN())))return 7;
        g.configure(rate,0.,.01f,1.f);g.reset();
        if(g.processGain(0.f)!=1.f)return 8;
        std::printf("PASS %.0f Hz: release residual %.6f after %.0f ms, partition/stereo/finite checks\n",rate,last,2*ms);
    }
    return 0;
}
