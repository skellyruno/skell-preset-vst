#include "HybridRoom.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <random>
#include <vector>
using E=skellcore::experimental::convolution::HybridRoom;
int main(){
    static_assert(E::bufferingLatency==0);
    for(double fs:{8000.,44100.,48000.,96000.,192000.,384000.}){
        E a,b;a.prepare(fs);b.prepare(fs);a.setParameters(1.2f,.3f,0.f);a.reset();
        for(int i=0;i<1024;++i){const E::Frame x{.5f*float(std::sin(i*.12)),.3f*float(std::cos(i*.33))};assert(a.processFrame(x[0],x[1])==x);}
        a.setParameters(1.2f,0.f,1.f);a.setLowCut(20);a.setEarlyLate(1,0);a.setPredelay(0);a.reset();
        // Four independent sparse convolution routes, including input HP and output LP.
        const float hp=float(-std::expm1(-6.283185307179586*20/fs));
        const float pole=float(std::exp(-6.283185307179586*std::min(.44*fs,16000.)/fs));
        const unsigned n=unsigned(fs*.12);std::vector<float> x(n);float low=0;
        for(unsigned i=0;i<n;++i){const float u=i==0?1.f:0.f;low+=hp*(u-low);x[i]=u-low;}
        for(unsigned channel=0;channel<2;++channel){a.reset();E::Frame expected{};for(unsigned i=0;i<n;++i){E::Frame er{};for(const auto& t:a.reflections())if(i>=t.delay){er[0]+=t.left[channel]*x[i-t.delay];er[1]+=t.right[channel]*x[i-t.delay];}for(unsigned c=0;c<2;++c)expected[c]=(1-pole)*er[c]+pole*expected[c];const auto y=a.processFrame(i==0&&channel==0?1.f:0.f,i==0&&channel==1?1.f:0.f);for(unsigned c=0;c<2;++c)assert(std::abs(y[c]-expected[c])<2e-6f);}}
        // Predelay shifts the entire wet response, never the dry path.
        a.setEarlyLate(.8f,.8f);a.setParameters(1.2f,.3f,1.f);a.setPredelay(0);a.reset();b.setParameters(1.2f,.3f,1.f);b.setLowCut(20);b.reset();b.setPredelay(50);
        const auto delay=unsigned(std::lround(fs*.05));std::vector<E::Frame> wet(n);for(unsigned i=0;i<n;++i)wet[i]=a.processFrame(i==0?.7f:0.f,0.f);
        for(unsigned i=0;i<n+delay;++i){const auto y=b.processFrame(i==0?.7f:0.f,0.f);assert(y==(i<delay?E::Frame{}:wet[i-delay]));}
        // Repeated automation while a long tail is live; deterministic reset and finite output.
        a.prepare(fs,1.65);b.prepare(fs,1.65);for(auto* e:{&a,&b}){e->setPredelay(0);e->setParameters(1.2f,.3f,1);e->setLowCut(90);e->reset();}std::mt19937 gen(731);std::uniform_real_distribution<float> u(-1,1);float peak=0;double tail=0;
        for(unsigned i=0;i<unsigned(fs*2);++i){if(i%127==0){const float d=.15f+7.85f*(u(gen)+1)*.5f,dam=.49f*(u(gen)+1),pre=100.f*(u(gen)+1);for(auto* e:{&a,&b}){e->setParameters(d,dam,1);e->setPredelay(pre);}
            a.setLowCut(90);b.setLowCut(90);}
            const float l=i<fs?.8f*u(gen):0.f,r=i<fs?.8f*u(gen):0.f;auto y=a.processFrame(l,r),z=b.processFrame(l,r);assert(y==z);for(auto v:y){assert(std::isfinite(v));peak=std::max(peak,std::abs(v));if(i>=fs)tail+=v*v;}}
        assert(peak<4.f&&tail>1e-6);a.reset();assert((a.processFrame(0,0)==E::Frame{}));auto y=a.processFrame(std::numeric_limits<float>::quiet_NaN(),0);assert(std::isfinite(y[0]));
        skellcore::experimental::convolution::HybridVocalRoom room;assert((room.process(.5f,.5f)==E::Frame{}));room.prepare(fs);room.set(.6f,.4f,12.f,100.f);room.reset();double e=0;for(unsigned i=0;i<unsigned(.3*fs);++i){auto v=room.process(i==0?.7f:0.f,0.f);e+=v[1]*v[1];}assert(e>1e-7);
    }
}
