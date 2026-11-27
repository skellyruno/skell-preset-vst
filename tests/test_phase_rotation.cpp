#include "../Source/chain/PhaseRotation.h"
#include <cstdio>
#include <random>
#include <limits>
#include <cstdlib>
using skellpreset::dsp::PhaseRotation;
static void require(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL %s\n",msg);std::exit(1);}}
int main(){
 std::mt19937 gen(59);std::uniform_real_distribution<float> dist(-.12f,.12f);
 double worst=0,match=0;int cases=0;
 for(double sr:{32000.,44100.,48000.,96000.,192000.}){
  for(float f:{0.f,.3f,.55f,.8f,1.f})for(float q:{0.f,.3f,.6f,1.f}){
   PhaseRotation a,b;a.prepare(sr);b.prepare(sr);a.set(f,q,0.f,.5f);b.set(f,q,1.f,.5f);
   double e0=0.,e1=0.;for(int n=0;n<(int)sr*2;n++){float l=dist(gen),r=dist(gen);auto y=a.process(l,r),z=b.process(l,r);if(n>sr*.25){e0+=l*l+r*r;e1+=y[0]*y[0]+y[1]*y[1];}match=std::max(match,(double)std::abs(y[0]+z[0]));require(std::isfinite(y[0])&&std::isfinite(y[1]),"finite");}
   double err=std::abs(10*std::log10(e1/e0));worst=std::max(worst,err);require(err<.10,"unity energy");++cases;
   a.reset();a.set(f,q,0.f,.5f);auto left=a.process(1.f,0.f);require(left[1]==0.f,"channel isolation");
   for(int n=0;n<40000;++n){if(n%64==0)a.set((n%97)/96.f,(n%89)/88.f,(n%128)?0.f:1.f,.5f);auto y=a.process(dist(gen),dist(gen));require(std::isfinite(y[0])&&std::isfinite(y[1])&&std::abs(y[0])<8&&std::abs(y[1])<8,"bounded automation");}
  }
 }
 require(match<1e-7,"polarity");
 std::printf("PASS: %d static configurations across five rates, energy error max %.6f dB, polarity null %.9g, channel isolation, automation stress\n",cases,worst,match);
}
