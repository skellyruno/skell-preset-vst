#include "../Source/chain/LiveSpectrum.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace skellpreset::visual;
constexpr double twoPi=6.28318530717958647;
int main() {
    LiveSpectrumCapture in,out;
    std::array<float,LiveSpectrumCapture::capacity> input{},output{};
    in.snapshot(input);
    for(float v:input)assert(v==0.f);
    for(int n=0;n<5000;++n){
        const float sample=.42f*std::sin((float)(twoPi*1000.*n/48000.));
        in.push(sample,sample);
        out.push(sample*.5f,sample*.5f);
    }
    in.snapshot(input);out.snapshot(output);
    LiveSpectrumAnalyzer analyzer;
    const auto a=analyzer.analyze(input,48000.);
    const auto b=analyzer.analyze(output,48000.);
    int peak=0;
    for(int i=1;i<LiveSpectrumAnalyzer::bands;++i)
        if(a[(size_t)i]>a[(size_t)peak])peak=i;
    assert(analyzer.frequency(peak)>830.f&&analyzer.frequency(peak)<1200.f);
    assert(a[(size_t)peak]>.2f&&b[(size_t)peak]>.09f);
    assert(b[(size_t)peak]<a[(size_t)peak]*.55f);
    assert(b[(size_t)peak]>a[(size_t)peak]*.45f);
    in.push(NAN,NAN);out.push(INFINITY,INFINITY);
    in.snapshot(input);out.snapshot(output);
    for(float v:input)assert(std::isfinite(v));
    for(float v:output)assert(std::isfinite(v));
    in.reset();in.snapshot(input);
    for(float v:input)assert(v==0.f);
    std::puts("PASS: lock-free input/output actual audio, 36 log bins, 1kHz tone, 6dB attenuation, finite reset");
    return 0;
}
