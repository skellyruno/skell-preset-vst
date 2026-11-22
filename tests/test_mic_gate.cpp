#include "../Source/chain/MicGateDSP.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using skellpreset::dsp::MicNoiseGate;
int main() {
 int count=0;
 for(double sr:{44100.,48000.,96000.}) {
  MicNoiseGate g;g.prepare(sr);g.set(-40.f,4.f,60.f,85.f,4.f);
  for(int n=0;n<sr/2;n++)if(g.processGain(0,0)!=0.f)return 1;
  ++count;float v=0;
  for(int n=0;n<sr/2;n++)v=g.processGain(.12f,-.12f);
  if(v<.98f||!g.isOpen())return 2;
  ++count;
  for(int n=0;n<sr;n++)v=g.processGain(0,0);
  if(v>.00005f||g.isOpen())return 3;
  ++count;
  g.set(NAN,INFINITY,-5,9999,NAN);v=g.processGain(NAN,INFINITY);
  if(!std::isfinite(v)||v<0||v>1)return 4;
  ++count;
 }
 MicNoiseGate a,b;a.prepare(44100);b.prepare(44100);
 for(int n=0;n<100000;n++) {
  const float x=(n<12000||(n>24000&&n<60000))?.14f:0.f;
  if(a.processGain(x,-x)!=b.processGain(x,-x))return 5;
 }
 std::cout<<"MIC gate "<<count+1<<" portable assertions passed\n";
 return 0;
}
