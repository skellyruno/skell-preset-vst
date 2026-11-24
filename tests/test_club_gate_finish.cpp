#include "../Source/chain/ClubGateFinish.h"
#include <iostream>
#include <cmath>
using skellpreset::dsp::ClubGateFinish;
int main(){
 for(double sr:{44100.,48000.,96000.,192000.}){
  ClubGateFinish a,b;a.prepare(sr);b.prepare(sr);a.setThresholdDb(-57.4f);b.setThresholdDb(-57.4f);
  for(int i=0;i<sr*.15;i++)if(a.processGain(0,0)!=.75f)return 1;
  a.prepare(sr);b.prepare(sr);float g=0;
  for(int i=0;i<sr*.2;i++){g=a.processGain(.1f,-.1f);if(g!=b.processGain(.1f,-.1f)||g<.75||g>1)return 2;}
  if(g!=1.f)return 3;
  for(int i=0;i<sr*.1;i++)g=a.processGain(0,0);
  if(std::abs(g-.75f)>1e-5)return 4;
  g=a.processGain(NAN,INFINITY);if(!std::isfinite(g)||g<.75||g>1)return 5;
 }
 std::cout<<"Club finish portable: 4 sample rates, silence/loud/release/stereo/determinism/nonfinite passed\n";return 0;
}
