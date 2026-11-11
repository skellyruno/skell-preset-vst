// Portable phase-vocoder regression: known sine, separate pitch and formant,
// finite sample sweeps. Fixed 768-sample host latency is validated separately.
#include "../Source/chain/SpectralPitchFormant.h"
#include <array>
#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <limits>
using skellpreset::dsp::SpectralPitchFormant;
constexpr float pi=3.14159265358979323846f;

static float frequency(const std::vector<float>& x,double sr){
    int crossings=0;
    for(size_t i=x.size()/2+1;i<x.size();++i)
        if(x[i-1]<=0.f && x[i]>0.f)++crossings;
    return (float)(crossings*sr/(x.size()-x.size()/2));
}
static std::vector<float> render(double sr,float hz,
                                 float pitch,float formant){
    SpectralPitchFormant dsp;
    dsp.prepare(sr);
    dsp.set(pitch,formant,1.f,1.f);
    std::vector<float> out;
    out.reserve((size_t)(sr*.9));
    for(int i=0;i<(int)(sr*.9);++i){
        const float x=.2f*std::sin(2.f*pi*hz*(float)i/(float)sr);
        const auto y=dsp.process(x,x);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        assert(std::abs(y[0])<6.f && std::abs(y[1])<6.f);
        assert(std::abs(y[0]-y[1])<.0005f);
        out.push_back(y[0]);
    }
    return out;
}
int main(){
    static_assert(SpectralPitchFormant::latencySamples==768,"Unexpected phase vocoder PDC");
    {
        SpectralPitchFormant identity;
        identity.prepare(48000.);
        identity.set(.5f,.5f,1.f,1.f);
        for(int i=0;i<2500;++i) {
            const auto y=identity.process(i==0?.4f:0.f,i==0?-.35f:0.f);
            assert(std::abs(y[0]-(i==768?.4f:0.f))<1e-7f);
            assert(std::abs(y[1]-(i==768?-.35f:0.f))<1e-7f);
        }
    }
    for(double sr:{8000.,44100.,48000.,96000.}){
        auto neutral=render(sr,310.f,.5f,.5f);
        assert(std::abs(frequency(neutral,sr)-310.f)<35.f);
        float peak=0.f;
        for(size_t i=neutral.size()/2;i<neutral.size();++i)
            peak=std::max(peak,std::abs(neutral[i]));
        assert(peak>.01f && peak<3.f);
        auto oct=render(sr,220.f,1.f,.5f);
        assert(std::abs(frequency(oct,sr)-440.f)<70.f);
        // Formant shift is independent: a sine's fundamental should remain.
        auto shiftedFormant=render(sr,310.f,.5f,1.f);
        assert(std::abs(frequency(shiftedFormant,sr)-310.f)<40.f);
    }
    SpectralPitchFormant dsp;
    dsp.prepare(48000.);
    for(int i=0;i<48000;++i){
        dsp.set((float)(i%1000)/999.f,(float)((i*19)%1000)/999.f,
                (float)(i%500)/499.f,.7f);
        const float input=i%10==0?0.3f:-.2f;
        const auto y=dsp.process(input,input);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        assert(std::abs(y[0])<6.f&&std::abs(y[1])<6.f);
    }
    dsp.prepare(48000.);
    dsp.set(std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),1.f,1.f);
    for(int i=0;i<4000;++i){
        const auto y=dsp.process(0.f,0.f);
        assert(y[0]==0.f&&y[1]==0.f);
    }
    return 0;
}
