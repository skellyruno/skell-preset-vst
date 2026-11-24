#include "../Source/chain/LiveSpectrum.h"
#include <array>
#include <limits>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)
#include <cmath>
#include <cstdio>
using namespace skellpreset::visual;
constexpr double twoPi=6.28318530717958647;
int main() {
    LiveSpectrumCapture in,out;
    std::array<float,LiveSpectrumCapture::capacity> input{},output{};
    in.snapshot(input);
    for(float v:input)SKELL_REQUIRE(v==0.f);
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
    SKELL_REQUIRE(analyzer.frequency(peak)>830.f&&analyzer.frequency(peak)<1200.f);
    SKELL_REQUIRE(a[(size_t)peak]>.2f&&b[(size_t)peak]>.09f);
    SKELL_REQUIRE(b[(size_t)peak]<a[(size_t)peak]*.55f);
    SKELL_REQUIRE(b[(size_t)peak]>a[(size_t)peak]*.45f);
    in.push(NAN,NAN);out.push(INFINITY,INFINITY);
    in.snapshot(input);out.snapshot(output);
    for(float v:input)SKELL_REQUIRE(std::isfinite(v));
    for(float v:output)SKELL_REQUIRE(std::isfinite(v));
    in.reset();in.snapshot(input);
    for(float v:input)SKELL_REQUIRE(v==0.f);
    const auto silent=analyzer.analyze(input,48000.);
    for(float v:silent)SKELL_REQUIRE(v==0.f);
    input.fill(std::numeric_limits<float>::quiet_NaN());
    for(float v:analyzer.analyze(input,48000.))SKELL_REQUIRE(v==0.f);
    input.fill(std::numeric_limits<float>::max());
    for(float v:analyzer.analyze(input,48000.))SKELL_REQUIRE(std::isfinite(v)&&v>=0.f&&v<=8.f);
    for(int n=0;n<LiveSpectrumCapture::capacity;++n)
        input[(size_t)n]=.4f*std::sin((float)(twoPi*1000.*n/8000.));
    const auto lowRate=analyzer.analyze(input,8000.);
    for(int band=0;band<LiveSpectrumAnalyzer::bands;++band)
        if(analyzer.frequency(band)>3600.f)SKELL_REQUIRE(lowRate[(size_t)band]==0.f);
    for(double sr:{8000.,16000.,44100.,48000.,96000.,192000.}){
        for(int n=0;n<LiveSpectrumCapture::capacity;++n)
            input[(size_t)n]=.4f*std::sin((float)(twoPi*1000.*n/sr));
        const auto bins=analyzer.analyze(input,sr);
        for(float v:bins)SKELL_REQUIRE(std::isfinite(v)&&v>=0.f&&v<=8.f);
    }
    in.push(std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());
    in.snapshot(input);SKELL_REQUIRE(input.back()==0.f);
    std::puts("PASS: real spectrum, attenuation, silence, invalid samples, extreme values, six rates, Nyquist limits and reset");
    return 0;
}
