#include "../Source/chain/LiveMatchOptimizer.h"
#include <iostream>
using namespace skellpreset::liveMatch;
static Audio source(int sr,unsigned seed){Audio a;a.rate=sr;float state=0;for(int i=0;i<8*sr;++i){seed=1664525u*seed+1013904223u;const float x=(float)(seed>>8)/8388608.f-1.f;state=.65f*state+.35f*x;const double t=(double)i/sr;const float env=std::fmod(t,1.07)<.72?.22f:0.f;const float v=env*state;a.stereo.push_back(v);a.stereo.push_back(v);}return a;}
static Profile profile(const Audio& a){Analyzer z;z.prepare(a.rate);for(size_t i=0;i<a.frames()&&!z.finished();++i)z.push(&a.stereo[2*i],&a.stereo[2*i+1],1);return z.result();}
int main(){for(int sr:{16000,44100,48000,96000,192000}){auto a=source(sr,13),b=a;const size_t lag=(size_t)std::round(.779*sr);for(size_t i=0;i<a.frames();++i)for(size_t ch=0;ch<2;++ch){const size_t k=i*2+ch;b.stereo[k]=.8f*a.stereo[k]+(i>=lag?.55f*a.stereo[(i-lag)*2+ch]:0.f)+(i>=2*lag?.16f*a.stereo[(i-2*lag)*2+ch]:0.f);}
const auto e=estimateEchoEvidence(a,b,a.frames()/2);std::cout<<sr<<" paired "<<e.paired<<" delay "<<e.seconds<<" strength "<<e.strength<<"\n";if(!e.paired||std::abs(e.seconds-.779f)>.002f)return 1;
if(estimateEchoEvidence(a,source(sr,41),a.frames()/2).paired)return 2;
if(estimateEchoEvidence(a,a,a.frames()/2).seconds!=0.f)return 3;
const float target=pairedEchoRatio(a,b.stereo.data(),0,a.frames()/2,e.seconds);
if(target<.15f||target>1.f||pairedEchoRatio(a,a.stereo.data(),0,a.frames()/2,e.seconds)>.001f)return 6;
if(echoRatioError(target,target)!=0.f||echoRatioError(-1.f,target)<1.e5f)return 7;
std::atomic<bool> cancel{true};if(estimateEchoEvidence(a,b,a.frames()/2,&cancel).paired)return 4;
// Evidence must not depend on holdout PCM.
for(size_t i=a.stereo.size()/2;i<b.stereo.size();++i)b.stereo[i]=0;
if(estimateEchoEvidence(a,b,a.frames()/2).seconds!=e.seconds)return 5;
}
// Known native rack render: old fixed grid misses this long echo. Search
// must recover the actual period without selecting from the held-out phrase.
auto a=source(16000,13);const auto p=profile(a);auto rec=fit(p);rec.output=0;rec.delay={.5f,{{(.779f-.055f)/.85f,.5f,.45f,.5f}}};
Audio b;b.rate=a.rate;NativeCandidate chain(a.rate,rec);for(size_t i=0;i<a.frames();++i){const auto y=chain.process(a.stereo[2*i],a.stereo[2*i+1]);b.stereo.insert(b.stereo.end(),y.begin(),y.end());}
const auto q=profile(b);const auto found=optimize(a,p,&q,nullptr,nullptr,&b);const float chosen=.055f+.85f*found.recipe.delay.p[0];
std::cout<<"Native search: "<<found.dryHoldout<<" -> "<<found.holdoutFinal<<" delay "<<chosen<<" candidates "<<found.tested<<"\n";
if(!found.valid||found.recipe.delay.wet<=0.f||std::abs(chosen-.779f)>.002f||found.holdoutFinal>1.2f||found.tested>referenceBudget)return 8;
return 0;}
