#include "../Source/chain/LiveSpectrum.h"
#include "../Source/chain/SlotVisualCapture.h"
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
    // Selected-slot capture must never display another effect's data.
    SlotVisualCapture selected;
    SlotVisualCapture::Frame frame;
    std::array<float,1024> l{},r{};
    l.fill(.25f);r.fill(-.25f);
    selected.request(2);
    if(selected.beginBlock()!=2 || selected.read(2,frame))return 30;
    selected.captureInput(l.data(),r.data(),1024);
    if(selected.read(2,frame))return 35; // do not expose an incomplete pair
    selected.captureOutput(l.data(),r.data(),1024);
    if(!selected.read(2,frame) || std::abs(frame.outputRms-.25f)>1.e-6f ||
       std::abs(frame.sideRms-.25f)>1.e-6f || frame.output[1023]!=0.f ||
       std::abs(frame.correlation+1.f)>1.e-6f || frame.mid[0]!=0.f || frame.side[0]!=.25f ||
       frame.leftRms!=.25f || frame.rightRms!=.25f)return 31;
    selected.request(4);
    if(selected.read(2,frame) || selected.read(4,frame) || selected.beginBlock()!=4)return 32;
    r.fill(.25f);selected.captureInput(l.data(),r.data(),1024);selected.captureOutput(l.data(),r.data(),1024);
    if(!selected.read(4,frame) || frame.sideRms!=0.f || frame.output[1023]!=.25f ||
       frame.correlation!=1.f || frame.mid[0]!=.25f || frame.side[0]!=0.f)return 33;
    l.fill(0.f);r.fill(0.f);selected.captureInput(l.data(),r.data(),1024);selected.captureOutput(l.data(),r.data(),1024);
    if(!selected.read(4,frame) || frame.correlation!=0.f || frame.outputRms!=0.f)return 36;
    selected.request(-1);
    if(selected.read(4,frame) || selected.beginBlock()!=-1)return 34;
    std::puts("PASS: real spectrum, attenuation, silence, invalid samples, extreme values, six rates, Nyquist limits, selected slot capture and stereo RMS");
    PitchHistory history;history.reset();
    PitchHistory::Samples pitch{};
    for(double rate:{44100.,48000.,96000.,192000.}){
        history.reset();
        for(int block=0;block<160;++block){
            for(int n=0;n<1024;++n){const double time=(block*1024+n)/rate;l[(size_t)n]=(float)(.25*std::sin(twoPi*110.*time)+.08*std::sin(twoPi*220.*time));}
            history.push(l.data(),l.data(),1024,rate);
        }
        float sr=0.f;if(!history.snapshot(pitch,sr))return 40;
        const auto reading=estimatePitch(pitch,sr);
        if(std::abs(reading.hz-110.f)>1.f || reading.confidence<.8f || std::abs(reading.midi-45.f)>.15f)return 41;
    }
    pitch.fill(0.f);if(estimatePitch(pitch,12000.f).hz!=0.f)return 42;
    history.reset();history.push(l.data(),l.data(),1024,std::numeric_limits<double>::quiet_NaN());
    float rejectedRate=0.f;if(history.snapshot(pitch,rejectedRate))return 45;
    unsigned rng=12345u;for(auto& value:pitch){rng=1664525u*rng+1013904223u;value=(float)(rng>>8)/8388608.f-1.f;}
    if(estimatePitch(pitch,12000.f).hz!=0.f)return 46;
    if(saturatorCore(0.f,2.f,1.f,1.2f)!=0.f || std::abs(saturatorCore(.5f,2.f,1.f,1.2f)-std::tanh(1.f)/1.2f)>1.e-6f)return 43;
    TailReading tail;tail.update(.1f,.2f,.02f);tail.update(0.f,.1f,.02f);tail.update(0.f,.01f,.1f);
    if(!tail.active || std::abs(tail.dropDb+20.f)>.01f || std::abs(tail.elapsed-.12f)>.001f)return 44;
    tail.update(0.f,0.f,.1f);const float finishedAt=tail.elapsed;
    tail.update(0.f,0.f,.1f);if(!tail.finished || tail.elapsed!=finishedAt)return 47;
    tail.update(.1f,.1f,.02f);if(tail.active || tail.finished)return 48;
    std::puts("PASS: pitch history at four rates, silence/noise rejection, exact drive core and observed tail drop");
    return 0;
}
