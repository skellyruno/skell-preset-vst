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
    // Nonstationary broadband reference with silent sections, dynamic
    // transients and moderate independent noise should still align sample-
    // accurately. This matters before MATCH attempts full plugin fitting.
    constexpr int m=24576;
    std::vector<float> vocal(m,0.f),processed(m+180,0.f);
    unsigned int seed=0x92f6ca35u;
    auto noise=[&]() {
        seed=seed*1664525u+1013904223u;
        return (float)((seed>>8)&0xffffu)/32767.5f-1.f;
    };
    for(int i=0;i<m;++i) {
        const float gate=(i>1100 && i<10500)||(i>12500 && i<20900)?1.f:0.f;
        const float envelope=gate*(.4f+.55f*std::abs(std::sin(i*.00043f)));
        vocal[(size_t)i]=envelope*(.23f*std::sin(i*.093f)+
                                    .13f*std::sin(i*.057f)+.11f*noise());
    }
    for(int i=0;i<m;++i)
        processed[(size_t)i+117]=vocal[(size_t)i]*.72f+
            .009f*noise(); // independent recorder noise
    const auto robust=skellpreset::match::analyse(vocal,processed,190);
    assert(robust.lagSamples==117);
    assert(!robust.inverted);
    assert(std::abs(robust.gainDb+2.85335f)<.4f);
    assert(robust.confidence>.85f);
    // Nearly uncorrelated references should not produce a confident result.
    for(auto& x:processed)x=.14f*noise();
    const auto unrelated=skellpreset::match::analyse(vocal,processed,190);
    assert(unrelated.confidence<.2f);
    std::cout<<"PASS MATCH preliminary alignment, polarity, level and invalid-input guards\n";
}
