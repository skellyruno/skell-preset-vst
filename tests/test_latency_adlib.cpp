#include "../Source/chain/BasicDSP.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <cstring>
#include <limits>
using namespace skellpreset::dsp;

// Compare against the previous modulo-based indexing with the same ring size.
// Explicit failure checks stay active in Release builds (unlike assert).
static bool verifySharedPdc() {
    std::uint32_t random=0x193a2b7cu;
    auto next=[&](){random=random*1664525u+1013904223u;return random;};
    auto blend=[](float dry,float wet,float amount){
        return amount>=1.f?wet:amount<=0.f?dry:dry+(wet-dry)*amount;
    };
    for(int maximum:{1,64,511,960,16384}) {
        StereoSampleDelay shared;shared.prepare(maximum);
        const size_t capacity=(size_t)maximum+2;
        std::array<std::vector<float>,2> reference;
        for(auto& line:reference)line.assign(capacity,0.f);
        size_t head=0;
        for(int n=0;n<75000;++n) {
            if(n%23717==0) {
                shared.reset();head=0;
                for(auto& line:reference)std::fill(line.begin(),line.end(),0.f);
            }
            const float l=n%89==0?std::numeric_limits<float>::quiet_NaN():
                (float)(int(next()%2001)-1000)*.001f;
            const float r=n%137==0?std::numeric_limits<float>::infinity():
                (float)(int(next()%2001)-1000)*.001f;
            const int delay=n%7==0?-1:n%7==1?maximum+7:n%7==2?0:int(next()%unsigned(maximum+1));
            reference[0][head]=std::isfinite(l)?l:0.f;
            reference[1][head]=std::isfinite(r)?r:0.f;
            const size_t d=(size_t)std::clamp(delay,0,maximum);
            const size_t read=(head+capacity-d)%capacity;
            const std::array<float,2> old{reference[0][read],reference[1][read]};
            head=(head+1)%capacity;
            const auto now=shared.process(l,r,delay);
            const float mix=n%4==0?0.f:n%4==1?1.f:.43f;
            const float power=n%5==0?0.f:n%5==1?1.f:.71f;
            for(size_t c=0;c<2;++c) {
                const float wet=c?.35f:-.22f;
                const float a=blend(old[c],wet,mix),b=blend(now[c],wet,mix);
                const float finalOld=(n%2?a-old[c]:a)*.83f;
                const float finalNew=(n%2?b-now[c]:b)*.83f;
                const float outputOld=blend(old[c],finalOld,power);
                const float outputNew=blend(now[c],finalNew,power);
                if(std::memcmp(&old[c],&now[c],sizeof(float))!=0 ||
                    std::memcmp(&outputOld,&outputNew,sizeof(float))!=0)return false;
            }
        }
    }
    return true;
}

int main() {
    if(!verifySharedPdc()){std::cerr<<"FAIL shared PDC parity\n";return 1;}
    StereoSampleDelay dry, bypass;
    dry.prepare(384);bypass.prepare(384);
    const int types[]{1,3,2,3,0,0,0,0,0,0};
    const int summed=seriesLatencySamples(types,10,3,144);
    assert(summed==288);
    for(int n=0;n<600;++n){
        const auto a=dry.process(n==0?1.f:0.f,n==0?-.5f:0.f,summed);
        const auto b=bypass.process(n==0?1.f:0.f,n==0?-.5f:0.f,summed);
        assert(a==b);
        assert(a[0]==(n==summed?1.f:0.f));
        assert(a[1]==(n==summed?-.5f:0.f));
    }
    dry.reset();
    for(int n=0;n<20;++n){auto a=dry.process(0,0,summed);assert(a[0]==0&&a[1]==0);}
    AdlibDoubler stack;stack.prepare(48000);
    for(int n=0;n<48000;++n){
        const float x=.2f*std::sin((float)(2*pi*220.*n/48000.));
        const auto y=stack.process(x,x,.8f,.5f,.55f,.7f);
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
        assert(std::abs(y[0])<=.25f&&std::abs(y[1])<=.25f);
    }
    stack.reset();
    for(int n=0;n<8000;++n){auto o=stack.process(0,0,.5f,.5f,.5f,.5f);assert(o[0]==0&&o[1]==0);}
    std::cout<<"PASS sample-exact dry/bypass PDC and bounded stereo adlib doubler\n";
}
