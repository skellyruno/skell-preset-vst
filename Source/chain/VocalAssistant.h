#pragma once
// Independent offline recommendation engine. Preview only: no host access,
// no effect mutations, no cloud calls and no recording. Call on worker thread.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
namespace skellpreset::assistant {
enum class Goal { Clean, Bright, Warm, Spacious, Deep, Match };
enum class Need { Noise, Dynamics, Sibilance, Air, Colour, Space, Formant, Headroom, Reference };
struct Features {
 bool valid=false; double peakDb=-120, rmsDb=-120, quietDb=-120, crestDb=0, dynamicDb=0, airDb=-120, presenceDb=-120, stereoCorrelation=0;
};
inline double db(double power) noexcept { return 10*std::log10(std::max(power,1e-12)); }
inline Features analyse(const float* l,const float* r,std::size_t n,double sr) {
 Features f;
 if(!l||!std::isfinite(sr)||sr<22050||sr>192000||n<sr||n>60*sr)return f;
 const auto block=std::size_t(std::lround(sr*.02));
 // Bilinear Butterworth high-pass probes, consistent cutoffs across rates.
 struct HighPass { double b0,b1,b2,a1,a2,z1=0,z2=0;
  HighPass(double hz,double rate) {const double w=2*3.141592653589793*hz/rate,c=std::cos(w),a=std::sin(w)/std::sqrt(2.),d=1+a;b0=(1+c)/(2*d);b1=-(1+c)/d;b2=b0;a1=-2*c/d;a2=(1-a)/d;}
  double process(double x){double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
 };
 std::array<HighPass,2> lo{HighPass(5500,sr),HighPass(5500,sr)},hi{HighPass(9000,sr),HighPass(9000,sr)};
 std::vector<double> levels; levels.reserve(n/block);
 double peak=0,ss=0,ls=0,rs=0,lr=0,air=0,pres=0,part=0;
 for(std::size_t i=0;i<n;++i){
  const double x=l[i],y=r?r[i]:x;
  if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>16||std::abs(y)>16)return Features{};
  const double q=(x*x+y*y)*.5;ss+=q;part+=q;ls+=x*x;rs+=y*y;lr+=x*y;peak=std::max(peak,std::max(std::abs(x),std::abs(y)));
  const std::array<double,2> z{x,y};for(unsigned c=0;c<2;++c){const double u=lo[c].process(z[c]),v=hi[c].process(z[c]);pres+=u*u*.5;air+=v*v*.5;}
  if((i+1)%block==0){levels.push_back(db(part/block));part=0;}
 }
 if(peak<1e-5||ss/n<1e-10||levels.empty())return f;
 std::sort(levels.begin(),levels.end());auto quant=[&](double p){return levels[std::size_t(p*(levels.size()-1))];};
 f.valid=true;f.peakDb=db(peak*peak);f.rmsDb=db(ss/n);f.crestDb=f.peakDb-f.rmsDb;f.quietDb=quant(.1);f.dynamicDb=quant(.9)-quant(.1);
 f.airDb=db(air/std::max(ss,1e-12));f.presenceDb=db(pres/std::max(ss,1e-12));f.stereoCorrelation=std::clamp(lr/std::max(std::sqrt(ls*rs),1e-12),-1.,1.);
 return f;
}
struct Suggestion { int effectId; Need need; const char* reason; };
struct Plan { std::array<Suggestion,8> steps{}; std::size_t count=0; bool needsReference=false; bool headroomWarning=false; };
inline Plan recommend(const Features& f,Goal goal) noexcept {
 Plan p; if(!f.valid||!std::isfinite(f.peakDb)||!std::isfinite(f.rmsDb)||!std::isfinite(f.crestDb)||!std::isfinite(f.dynamicDb)||!std::isfinite(f.quietDb)||!std::isfinite(f.airDb)||!std::isfinite(f.presenceDb)||!std::isfinite(f.stereoCorrelation))return p;
 auto add=[&](int id,Need need,const char* reason){if(p.count<p.steps.size())p.steps[p.count++]={id,need,reason};};
 p.headroomWarning=f.peakDb>-1;
 if(goal==Goal::Match){p.needsReference=true;return p;} // no guessed match chain
 if(f.quietDb>-55&&f.dynamicDb>12)add(4,Need::Noise,"Inspect quiet passages before auditioning a gate; quiet percentile is not a measured noise floor.");
 if(f.dynamicDb>18||f.crestDb>16)add(23,Need::Dynamics,"Audition gentle vocal riding for level variation.");
 if(f.airDb>-16)add(46,Need::Sibilance,"High-band energy is elevated; audition split-band de-essing, verify consonants.");
 if(goal==Goal::Bright&&f.airDb<-19&&!p.headroomWarning)add(45,Need::Air,"Audition restrained air enhancement; keep sibilance and headroom under review.");
 if(goal==Goal::Warm&&!p.headroomWarning)add(5,Need::Colour,"Audition subtle harmonic colour; compare level matched.");
 if(goal==Goal::Deep)add(30,Need::Formant,"Audition independent formant shift; verify pitch and reported latency.");
 if(goal==Goal::Spacious){add(20,Need::Space,"Audition chorus only if additional width is wanted; check mono.");add(6,Need::Space,"Audition reverb separately from direct tone.");}
 return p;
}
} // namespace skellpreset::assistant
