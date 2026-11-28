#include <SkellCore/backends/vocal/CausalMatchCompressor.h>
#include "Delay.h"
#include <iostream>
#include <vector>
#include <cstdlib>
using skell::match::CausalMatchCompressor;
using namespace SKELLdelay;
#define require(ok) do { if(!(ok)){std::cerr<<"failed line "<<__LINE__<<"\n";std::abort();} } while(0)
static std::vector<float> render(int sr,int block,bool chain){
    Delay2State s; s.v=delay2Default; s.v[2]=0;s.v[3]=0;s.v[4]=192;s.v[5]=0;s.v[6]=0;s.v[7]=128;
    Delay2Engine d(s,sr,144.,block,true);
    CausalMatchCompressor c;const CausalMatchCompressor::Params p{std::log(12.),std::log(90.),-24.,.75,3.,6.,std::log(.5),std::log(30.)};
    require(c.prepare(sr,0,0,16,p));std::vector<float> l(sr),r(sr);l[0]=.125f;r[0]=.0625f;
    for(int i=0;i<sr;i+=block){const int n=std::min(block,sr-i);if(chain)c.processBlock(l.data()+i,r.data()+i,n);d.process(l.data()+i,r.data()+i,n);}
    for(int i=0;i<sr;++i)require(std::isfinite(l[i])&&std::isfinite(r[i])&&std::abs(l[i]-2*r[i])<1.3e-7f);
    const int expected=static_cast<int>(std::nearbyint(sr*60./144.));
    require(std::all_of(l.begin(),l.begin()+expected,[](float v){return v==0.f;}));require(l[expected]>.001f);
    return l;
}
int main(){for(int sr:{32000,44100,48000,96000,192000})for(bool chain:{false,true}){auto a=render(sr,64,chain);for(int block:{37,128,512})require(a==render(sr,block,chain));}std::cout<<"PASS 5 rates, 4 block sizes, Delay 2 beat timing and optional causal compressor cascade\n";}
