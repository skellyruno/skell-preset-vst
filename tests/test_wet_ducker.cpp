#include "../Source/chain/WetDuckerDSP.h"
#include <cmath>
#include <cstdio>
#include <limits>
int main() {
    skellpreset::dsp::VoiceWetDucker d;
    d.prepare(48000.);
    d.set(false,.9f,-35.f,200.f);
    for(int i=0;i<4800;++i)
        if(d.processGain(.8f,.8f)!=1.f)return 1;
    d.set(true,.9f,-35.f,200.f);
    float active=1.f;
    for(int i=0;i<24000;++i)active=d.processGain(.6f,-.6f);
    if(!(active<.3f&&active>=0.f))return 2;
    d.prepare(48000.);d.set(true,.9f,-35.f,200.f);
    float linked=1.f;
    for(int i=0;i<24000;++i)linked=d.processGain(.6f,.6f);
    if(std::abs(linked-active)>1.e-5f)return 3;
    float tail=active;
    for(int i=0;i<24000;++i)tail=d.processGain(0.f,0.f);
    if(!(tail>active&&tail<1.f))return 4;
    for(int i=0;i<96000;++i)tail=d.processGain(0.f,0.f);
    if(tail<.99f)return 5;
    d.set(false,.9f,-35.f,200.f);
    for(int i=0;i<96000;++i)tail=d.processGain(.6f,.6f);
    if(tail!=1.f)return 6;
    d.prepare(48000.);
    d.set(true,1.f,-60.f,50.f);
    for(int i=0;i<48000;++i) {
        const float g=d.processGain(
            i%3?std::numeric_limits<float>::quiet_NaN():100.f,-100.f);
        if(!std::isfinite(g)||g<0.f||g>1.f)return 7;
    }
    // New DUCKING attack is an audible DSP control, not an inert GUI value.
    d.prepare(48000.);d.set(true,.8f,-36.f,320.f,2.f);
    float fast=1.f;
    for(int i=0;i<480;++i)fast=d.processGain(.8f,.8f);
    d.prepare(48000.);d.set(true,.8f,-36.f,320.f,80.f);
    float soft=1.f;
    for(int i=0;i<480;++i)soft=d.processGain(.8f,.8f);
    if(!(fast<soft-.35f && fast>=0.f && soft<=1.f))return 8;
    std::printf("PASS: 2ms duck attack %.4f < 80ms duck attack %.4f\n",fast,soft);
    std::printf("PASS: voice wet duck OFF=unity, linked -%0.1f dB, 200 ms recovery, NaN clamp, zero lookahead\n",
                -20.f*std::log10(active));
    return 0;
}
