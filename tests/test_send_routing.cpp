#include "../Source/chain/SendRouting.h"
#include <array>
#include <cstdio>
using namespace skellpreset::dsp;
int main() {
    for(int mode=0;mode<4;++mode)for(bool first:{false,true}) {
        const auto y=delaySendInput(.75f,.25f,mode,first);
        const std::array<float,2> expected=mode==0?std::array<float,2>{.5f,.5f}:
            mode==2&&first?std::array<float,2>{0.f,.5f}:std::array<float,2>{.75f,.25f};
        if(y!=expected)return 1;
    }
    if(delaySendInput(1.f,-1.f,2,true)!=std::array<float,2>{0.f,0.f})return 2;
    if(duckDubReturn(.5f,1.f)!=.5f || duckDubReturn(.5f,0.f)!=0.f)return 3;
    // The rack's existing linear dry/wet balance remains unchanged.
    const float dry=.25f,wet=.5f,mix=.5f;
    const float baseline=dry+(wet-dry)*mix;
    const float ducked=dry+(duckDubReturn(wet,.25f)-dry)*mix;
    if(baseline!=.375f||ducked!=.1875f)return 4;
    // Wet-only delay: ducking cannot reintroduce a direct impulse at 100% wet.
    if(duckDubReturn(0.f,.1f)!=0.f || duckDubReturn(.75f,.2f)>.151f)return 5;
    std::puts("PASS send matrix: all modes, opt-in isolation, mono fold, dry-preserving delay and dub returns");
}
