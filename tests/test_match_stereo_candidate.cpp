#include "../Source/chain/LiveMatchOptimizer.h"
#include <iostream>
using namespace skellpreset::liveMatch;
int main(){
    Audio dry;dry.rate=16000;
    for(int i=0;i<8*dry.rate;++i){const double t=i/dry.rate;const float e=std::fmod(t,1.)<.8?.12f:0.f;const float x=e*(float)std::sin(6.2831853*220*t),y=e*.16f*(float)std::sin(6.2831853*317*t);dry.stereo.push_back(x+y);dry.stereo.push_back(x-y);}
    auto profile=[](const Audio& a){Analyzer z;z.prepare(a.rate);for(size_t i=0;i<a.frames()&&!z.finished();++i)z.push(&a.stereo[2*i],&a.stereo[2*i+1],1);return z.result();};
    const auto dp=profile(dry);Recipe recipe=fit(dp);recipe.output=0;recipe.width={1.f,{{.1f,.35f,.5f,0.f}}};
    Audio ref;ref.rate=dry.rate;NativeCandidate chain(dry.rate,recipe);
    for(size_t i=0;i<dry.frames();++i){const auto y=chain.process(dry.stereo[2*i],dry.stereo[2*i+1]);ref.stereo.insert(ref.stereo.end(),y.begin(),y.end());}
    const auto rp=profile(ref);const auto found=optimize(dry,dp,&rp,nullptr,nullptr,&ref);
    std::cout<<"Width: "<<found.dryHoldout<<" -> "<<found.holdoutFinal<<" ("<<found.tested<<" candidates)\n";
    if(!found.valid||found.recipe.width.wet<=0||found.holdoutFinal>=found.dryHoldout||found.tested>referenceBudget)return 1;
    // Verify the doubler is actual native wet-only DSP, not a gain trick.
    Recipe plain;plain.output=0;plain.ratio=1;plain.highpassHz=0;Recipe doubled=plain;doubled.doubler={.9f,{{.85f,.65f,.65f,.85f}}};
    NativeCandidate a(48000,plain),b(48000,doubled);double side=0,energy=0;
    for(int i=0;i<48000;++i){const float x=.15f*(float)std::sin(6.2831853*220*i/48000.);const auto u=a.process(x,x),v=b.process(x,x);if(!std::isfinite(v[0])||!std::isfinite(v[1])||u[0]!=u[1])return 2;side+=(v[0]-v[1])*(v[0]-v[1]);energy+=v[0]*v[0]+v[1]*v[1];}
    if(side<.01*energy)return 3;
    return 0;
}
