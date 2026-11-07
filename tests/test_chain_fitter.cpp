#include "../Source/match/ChainFitter.h"
#include <iostream>
#include <random>
#include <cassert>
int main(){
    constexpr int rate=16000,n=rate;
    std::vector<float> dry(n);
    std::minstd_rand generator(99);
    std::uniform_real_distribution<float> noise(-1,1);
    // Synthetic pseudo-voiced excitation with changing pitch and level envelope.
    double phase=0;
    for(int i=0;i<n;i++){
        const float t=i/(float)rate;
        const float hz=170.f+26.f*std::sin(2*skellpreset::dsp::pi*t*1.2f);
        phase+=2*skellpreset::dsp::pi*hz/rate;
        const float level=t<.17f?.10f:t<.37f?.40f:t<.58f?.21f:t<.79f?.7f:.14f;
        dry[(size_t)i]=level*(float)(.64*std::sin(phase)+.23*std::sin(2*phase)+.09*std::sin(3*phase)+.14f*noise(generator));
    }
    skellpreset::match::ThreeModuleSettings truth;
    truth.eqFreq=1200;truth.eqGainDb=5.;truth.eqQ=1.;truth.thresholdDb=-20.;truth.ratio=3.2f;
    truth.attackMs=8.;truth.releaseMs=100.;truth.delaySamples=3200;
    truth.feedback=.28f;truth.delayMix=.38f;
    const auto wet=skellpreset::match::renderKnownChain(dry,rate,truth);
    auto guess=truth;
    guess.eqGainDb=1.5f;guess.thresholdDb=-16;guess.ratio=2.3f;
    guess.delaySamples=3000;guess.feedback=.2f;guess.delayMix=.26f;
    const auto fit=skellpreset::match::fitKnownChain(dry,wet,rate,guess,9);
    std::cout<<"MATCH restricted 3-module score="<<fit.score<<" evaluations="<<fit.evaluations
             <<" EQ gain="<<fit.settings.eqGainDb<<" threshold="<<fit.settings.thresholdDb
             <<" ratio="<<fit.settings.ratio<<" delay samples="<<fit.settings.delaySamples
             <<" feedback="<<fit.settings.feedback<<" mix="<<fit.settings.delayMix<<"\n";
    assert(fit.score>95.f);
}
