#include <SkellCore/backends/vocal/CausalMatchCompressor.h>
#include <iostream>
#include <limits>
#include <vector>
#include <cstdlib>
using skell::match::CausalMatchCompressor;
static void require(bool ok){if(!ok)std::abort();}
int main(){
 const CausalMatchCompressor::Params p{std::log(12.),std::log(90.),-24.,.75,3.,6.,std::log(.5),std::log(30.)};
 for(double rate:{32000.,44100.,48000.,96000.,192000.})for(int mode=0;mode<3;++mode)for(int kind=0;kind<3;++kind)for(int period:{1,16,64}){
  CausalMatchCompressor c;require(c.prepare(rate,mode,kind,period,p));std::vector<float> l(12000,.01f),r(l.size(),.005f);
  for(size_t i=1000;i<9000;++i){l[i]=.5f;r[i]=.25f;}
  auto a=l,b=r,u=l,v=r;c.processBlock(a.data(),b.data(),int(l.size()));c.reset();
  for(size_t i=0;i<l.size();){auto n=std::min<size_t>(37,l.size()-i);c.processBlock(u.data()+i,v.data()+i,int(n));i+=n;}
  require(a==u&&b==v);require(std::abs(a[0])>.001f);require(a[8000]/l[8000]<a[500]/l[500]);
  for(size_t i=0;i<a.size();++i)require(std::isfinite(a[i])&&a[i]==2*b[i]);
  // Causality: future samples may not affect any prefix output.
  c.reset();u=l;v=r;std::fill(u.begin()+2000,u.end(),.9f);std::fill(v.begin()+2000,v.end(),.9f);c.processBlock(u.data(),v.data(),int(u.size()));require(std::equal(a.begin(),a.begin()+2000,u.begin()));
 }
 CausalMatchCompressor c;float l=.5f,r=.2f;c.process(l,r);require(l==.5f&&r==.2f);require(!c.prepare(0,0,0,16,p));require(!c.prepare(44100,0,0,0,p));require(c.prepare(44100,0,0,16,p));l=std::numeric_limits<float>::quiet_NaN();c.process(l,r);require(std::isfinite(l)&&std::isfinite(r));
 std::cout<<"PASS 135 rate/domain/detector/interval configurations, arbitrary block parity, causal prefix, stereo link, bypass and invalid settings\n";
}
