#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "De-esser test assertions must remain enabled in Release"
#endif
using namespace skellpreset::dsp;

static void check(double rate) {
    SplitDeEsser neutral, reduced;
    neutral.prepare(rate);reduced.prepare(rate);
    neutral.set(6000.f,-35.f,0.f,80.f);
    reduced.set(6000.f,-35.f,18.f,80.f);
    double error=0,rawHi=0,wetHi=0,rawLow=0,wetLow=0;
    for(int n=0;n<(int)(rate*.8);++n) {
        const float high=.55f*std::sin((float)(2*pi*8000.*n/rate));
        const float low=.12f*std::sin((float)(2*pi*170.*n/rate));
        const float input=high+low;
        const auto dry=neutral.process(input,input);
        const auto wet=reduced.process(input,input);
        assert(std::isfinite(dry[0])&&std::isfinite(wet[0]));
        error=std::max(error,(double)std::abs(dry[0]-input));
        if(n>(int)(rate*.3)) {
            rawHi+=high*input;wetHi+=high*wet[0];
            rawLow+=low*input;wetLow+=low*wet[0];
        }
    }
    assert(error<2.e-5); // zero reduction reconstructs the input
    assert(std::abs(wetHi)<std::abs(rawHi)*.83); // sibilance attenuated
    assert(std::abs(wetLow)>std::abs(rawLow)*.72); // vocal fundamentals remain
    const auto bad=reduced.process(std::numeric_limits<float>::quiet_NaN(),
                                   std::numeric_limits<float>::infinity());
    assert(std::isfinite(bad[0])&&std::isfinite(bad[1]));
    reduced.reset();
    for(int i=0;i<(int)(rate*.3);++i) {
        const auto y=reduced.process(0,0);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
    }
}
int main(){for(double rate:{44100.,48000.,96000.})check(rate);
    std::cout<<"PASS zero-latency split de-essing, unity and sibilance attenuation\n";}
