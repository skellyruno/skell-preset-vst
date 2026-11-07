#include "../Source/match/MatchCore.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
int main() {
    constexpr int n=16000;
    std::vector<float> dry(n),wet(n+128);
    for(int i=0;i<n;i++) {
        auto t=i/16000.0;
        dry[(size_t)i]=(float)(.35*std::sin(2*skellpreset::dsp::pi*180*t)+.18*std::sin(2*skellpreset::dsp::pi*340*t))* (float)(.5+.5*std::sin(2*skellpreset::dsp::pi*2.1*t));
        wet[(size_t)i+73]=.7f*dry[(size_t)i];
    }
    const auto r=skellpreset::match::analyse(dry,wet,120);
    std::cout<<"MATCH alignment lag="<<r.lagSamples<<" gainDb="<<r.gainDb<<" score="<<r.score<<"\n";
    assert(r.lagSamples==73);assert(std::abs(r.gainDb+3.09804f)<.02f);assert(r.score>99.9f);
    assert(std::abs(r.fractionalLagSamples-73.0)<.5);
    assert(!r.driftSuspected);
    // Polarity inversion must be detected, not mistaken for failure.
    for(auto &x:wet)x=-x;
    const auto inv=skellpreset::match::analyse(dry,wet,120);
    assert(inv.inverted && inv.lagSamples==73 && inv.score>99.9f);
    const auto silence=skellpreset::match::analyse(std::vector<float>(256,0.f),std::vector<float>(256,0.f));
    assert(silence.score==0.f&&silence.confidence==0.f);
    auto invalid=dry;invalid[3]=std::numeric_limits<float>::quiet_NaN();
    const auto broken=skellpreset::match::analyse(invalid,wet,120);
    assert(broken.score==0.f&&broken.confidence==0.f);
    std::cout<<"PASS MATCH preliminary alignment, polarity, level and invalid-input guards\n";
}
