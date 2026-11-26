#include "../Source/chain/VocalPreviewAnalysis.h"
#include <cstdio>
#include <limits>
using namespace skellpreset::audition;
int main(){
    Metrics a,b;
    for(int i=0;i<1000;++i){if(!a.add(.5f,.5f)||!b.add(1.f,-1.f))return 1;}
    if(a.rms()!=.5||a.correlation()!=1.||b.correlation()!=-1.||b.overSamples!=0||a.crestDb()!=0.)return 2;
    const auto m=match(a,b);if(!m.valid||std::abs(a.rms()*m.dry-b.rms()*m.wet)>1.e-12)return 3;
    if(m.dry>1.||m.wet>1.||std::max(a.peak*m.dry,b.peak*m.wet)>.891251)return 4;
    Metrics s;if(match(s,b).valid||s.add(std::numeric_limits<float>::quiet_NaN(),0.f))return 5;
    s.add(2.f,-2.f);s.add(0.f,0.f);if(s.overSamples!=2||s.activeFrames!=1||s.activeRms()!=2.)return 6;
    if(std::abs(s.crestDb()-3.0102999566)>1.e-8||std::abs(s.sideDb()-3.0102999566)>1.e-8)return 7;
    for(unsigned n=1;n<=1280;++n)for(unsigned d=0;d<16;++d){const float x=variation(n,d);if(x<-1.f||x>1.f||x!=variation(n,d))return 8;}
    if(variation(1,0)==variation(2,0))return 9;
    std::puts("PASS: RMS, active RMS, crest, stereo correlation/side, clip counts, nonfinite rejection, peak-safe A/B and 1280 deterministic seeds");
}
