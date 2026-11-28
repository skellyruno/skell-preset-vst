// Research gate only: no live rack insertion or preset/automation changes.
// Exercise the existing MIT SkellyTune backend with an explicitly selected note.
#include <SkellCore/backends/tune/RealtimeTuner.hpp>
#include <SkellCore/backends/tune/ScaleMask.hpp>
#include "../Source/chain/MatchPitchEvidence.h"
#include <iostream>
#include <vector>
#include <cmath>
int main(){
    for(double rate:{44100.,48000.,96000.,192000.}){
        skelly::RealtimeTuner tuner;tuner.prepare(rate,2);
        skelly::Parameters settings;settings.pitchClasses=1u<<9; // A only: known fixture, not guessed key
        settings.speedMs=25.f;settings.amount=1.f;settings.mix=1.f;
        settings.experimentalCycleSynthesis=false;tuner.setParameters(settings);
        const int latency=tuner.latencySamples();
        if(latency!=skelly::delayContract(rate,false).reportedSamples)return 1;
        const int n=(int)(rate*2.5);std::vector<float> input(n),output(n),right(n);
        const double inputHz=440.*std::pow(2.,30./1200.);
        for(int i=0;i<n;++i)input[i]=(float)(.20*std::sin(2*skelly::kPi*inputHz*i/rate)+.03*std::sin(4*skelly::kPi*inputHz*i/rate));
        for(int i=0;i<n;i+=256){const float* in[2]={input.data()+i,input.data()+i};float* out[2]={output.data()+i,right.data()+i};tuner.process(in,out,std::min(256,n-i),2);}
        double first=-1,last=-1;int periods=0;
        for(int i=(int)(rate*1.25);i<n;++i){if(!std::isfinite(output[i])||!std::isfinite(right[i]))return 2;
            if(output[i-1]<=0&&output[i]>0){const double crossing=i-1-output[i-1]/(output[i]-output[i-1]);if(first<0)first=crossing;else{last=crossing;++periods;}}}
        if(periods<100||last<=first)return 3;
        const double hz=periods*rate/(last-first),error=1200*std::log2(hz/440.);
        std::cout<<rate<<" Hz: input +30 cents, output "<<error<<" cents, reported delay "<<latency<<" samples\n";
        if(std::abs(error)>10||tuner.currentTargetNote()!=69)return 4;
        tuner.reset();settings.mix=0.f;tuner.setParameters(settings);
        std::fill(input.begin(),input.end(),0.f);input[17]=.25f;
        const float* in[2]={input.data(),input.data()};float* out[2]={output.data(),right.data()};
        tuner.process(in,out,latency+64,2);
        for(int i=0;i<latency+64;++i){const float expected=i==latency+17?.25f:0.f;if(std::abs(output[i]-expected)>1.e-6f||std::abs(right[i]-expected)>1.e-6f)return 5;}
    }
    // Two independent melodic phrases must agree on a note mask. A single
    // sustained note cannot determine the key, and relative keys stay ambiguous.
    using namespace skellpreset::liveMatch;
    Audio melody;melody.rate=16000;
    constexpr int notes[]={62,64,66,67,69,71,73};
    for(int phrase=0;phrase<2;++phrase)for(int note:notes){const double hz=440.*std::pow(2.,(note-69)/12.);for(int i=0;i<melody.rate*.54;++i){const double t=i/melody.rate;const float x=t<.48?.13f*(float)(std::sin(2*skelly::kPi*hz*t)+.15*std::sin(4*skelly::kPi*hz*t)):0.f;melody.stereo.push_back(x);melody.stereo.push_back(x);}}
    const auto evidence=consensusPitchNotes(melody);std::cout<<"Scale notes mask "<<evidence.mask<<" coverage "<<evidence.coverage<<" margin "<<evidence.margin<<" distinct "<<evidence.distinct<<"\n";
    if(evidence.mask!=skelly::scaleMask(2,1))return 6;
    Audio sparse;sparse.rate=16000;for(int i=0;i<8*sparse.rate;++i){const float x=.13f*(float)std::sin(2*skelly::kPi*440*i/sparse.rate);sparse.stereo.push_back(x);sparse.stereo.push_back(x);}
    if(consensusPitchNotes(sparse).mask)return 7;
    std::atomic<bool> stop{true};if(consensusPitchNotes(melody,&stop).mask)return 8;
    return 0;
}
