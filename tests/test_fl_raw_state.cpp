#include "../Source/chain/FlRawState.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
using namespace skell::state;
int main(){
    std::size_t count=0;
    auto check=[&](bool ok){++count;if(!ok){std::cerr<<"failed check "<<count<<'\n';std::exit(1);}};
    for(std::size_t i=0;i<18;++i)for(int v=limiterMin[i];v<=limiterMax[i];++v){auto n=normalize(v,limiterMin[i],limiterMax[i]);check(n&&denormalize(*n,limiterMin[i],limiterMax[i])==v);}
    for(std::size_t i=0;i<6;++i)for(int v=compressorMin[i];v<=compressorMax[i];++v){auto n=normalize(v,compressorMin[i],compressorMax[i]);check(n&&denormalize(*n,compressorMin[i],compressorMax[i])==v);}
    check(!denormalize(std::numeric_limits<float>::quiet_NaN(),0,10));check(!denormalize(std::numeric_limits<float>::infinity(),0,10));check(!normalize(-1,0,10));check(!denormalize(1.1f,0,10));
    Bytes l(169,0xab);put32(l,0,7);for(std::size_t i=0;i<18;++i)put32(l,4+4*i,limiterMin[i]);put32(l,164,3);
    auto s=decode(Kind::limiter,l);check(s&&s->sidechain==3&&s->exactCeiling);check(encode(*s)==l);
    s->controls[0]=1234;s->exactCeiling=false;s->sidechain=8;auto out=encode(*s);auto expected=l;put32(expected,4,1234);put32(expected,164,8);expected[168]&=~1u;check(out==expected);s->controls[4]=0;check(!encode(*s));
    for(std::size_t n=0;n<l.size();++n)check(!decode(Kind::limiter,Bytes(l.begin(),l.begin()+n)));
    put32(l,0,8);check(!decode(Kind::limiter,l));
    for(int ver=1;ver<=3;++ver){Bytes b(28+4*(ver-1),0);put32(b,0,ver);for(std::size_t i=0;i<6;++i)put32(b,4+4*i,compressorMin[i]);if(ver>=2)put32(b,28,0x1235);if(ver==3)put32(b,32,4);auto c=decode(Kind::compressor,b);check(c&&encode(*c)==b);if(ver>=2){c->reportLatency=false;auto e=encode(*c);check(e&&read32(*e,28)==0x1234);}b.push_back(0);check(!decode(Kind::compressor,b));}
    for(auto k:{Kind::multiband,Kind::eq2,Kind::stereo,Kind::softClipper}){Bytes b(k==Kind::multiband?124:k==Kind::eq2?354:k==Kind::stereo?24:8,0);if(k==Kind::eq2)put32(b,0,8);if(k==Kind::softClipper)put32(b,0,1);auto a=decode(k,b);check(a&&encode(*a)==b);++a->controls[0];check(!encode(*a));}
    std::cout<<"PASS "<<count<<" checks\n";
}
