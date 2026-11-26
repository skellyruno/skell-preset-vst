#include "../Source/chain/VocalAssistant.h"
#include "../Source/chain/EffectRegistry.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace skellpreset::assistant;
static void require(bool x){if(!x)std::abort();}
static_assert(skellpreset::registry::at(4).id==4 && skellpreset::registry::at(5).id==5 &&
 skellpreset::registry::at(6).id==6 && skellpreset::registry::at(20).id==20 &&
 skellpreset::registry::at(23).id==23 && skellpreset::registry::at(30).id==30 &&
 skellpreset::registry::at(45).id==45 && skellpreset::registry::at(46).id==46);
int main(){int tests=0;
 for(double sr:{22050.,44100.,48000.,96000.,192000.}){
  std::vector<float>x(std::size_t(sr*2)), y(x.size());
  for(std::size_t i=0;i<x.size();++i){x[i]=.1f*std::sin(2*3.141592653589793*220*i/sr);y[i]=-x[i];}
  auto f=analyse(x.data(),nullptr,x.size(),sr);auto g=analyse(x.data(),y.data(),x.size(),sr);
  require(f.valid&&g.valid&&std::abs(f.rmsDb-g.rmsDb)<1e-10&&g.stereoCorrelation<-.999&&f.stereoCorrelation>.999);++tests;
  auto p=recommend(f,Goal::Bright);require(p.count>=1&&p.steps[p.count-1].effectId==45&&!p.headroomWarning);++tests;
  p=recommend(f,Goal::Match);require(p.needsReference&&p.count==0);++tests;
  for(std::size_t i=0;i<x.size();++i)x[i]=.99f*std::sin(2*3.141592653589793*10000*i/sr);
  f=analyse(x.data(),nullptr,x.size(),sr);p=recommend(f,Goal::Bright);require(f.valid&&p.headroomWarning&&p.count>0);for(std::size_t i=0;i<p.count;++i)require(p.steps[i].effectId!=45);++tests;
  x[0]=std::numeric_limits<float>::quiet_NaN();require(!analyse(x.data(),nullptr,x.size(),sr).valid);++tests;
  std::fill(x.begin(),x.end(),0);require(!analyse(x.data(),nullptr,x.size(),sr).valid);++tests;
  require(!analyse(x.data(),nullptr,2,sr).valid);++tests;
  f.rmsDb=std::numeric_limits<double>::infinity();require(recommend(f,Goal::Warm).count==0);++tests;
 }
 std::cout<<tests<<" assistant checks passed\n";
}
