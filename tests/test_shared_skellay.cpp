#include "SkellayEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Real shared engine contract. Compare independently prepared consumers of
// the same immutable native code at the same settings and block cadence.
// No production rack audio path changes until state/controls are migrated.
int main() {
    for(auto model:{skellay::Model::classic,skellay::Model::modern,skellay::Model::bank}) {
        const auto state=skellay::defaults(model);
        if(!skellay::validate(state))return 1;
        for(int rate:{44100,48000,96000}) {
            constexpr int block=128;
            auto a=skellay::prepare(state,rate,120.,block,60.);
            auto b=skellay::prepare(state,rate,120.,block,60.);
            if(!a||!b)return 2;
            auto controls=skellay::liveControls(state);
            if(!skellay::validLiveControls(controls))return 3;
            std::vector<float> l(rate),r(rate),ll(rate),rr(rate);
            double energy=0.;
            for(int i=0;i<rate;++i) {
                l[i]=ll[i]=i<rate/10?.1f*std::sin(i*.047f):0.f;
                r[i]=rr[i]=i<rate/10?.08f*std::cos(i*.071f):0.f;
            }
            for(int at=0;at<rate;at+=block) {
                const int n=std::min(block,rate-at);
                if(at>=rate/4){a->updateTempo(100.);b->updateTempo(100.);}
                if(!a->updateRealtimeControls(controls)||!b->updateRealtimeControls(controls))return 4;
                if(!a->process(l.data()+at,r.data()+at,n)||!b->process(ll.data()+at,rr.data()+at,n))return 5;
                for(int i=at;i<at+n;++i) {
                    if(!std::isfinite(l[i])||!std::isfinite(r[i]))return 6;
                    energy+=std::abs(l[i])+std::abs(r[i]);
                }
            }
            if(l!=ll||r!=rr||energy<=0.)return 7;
            std::printf("PASS Skellay shared model=%d rate=%d same-source consumer parity\n",int(model),rate);
        }
    }
    return 0;
}
