#pragma once
// Stereo-linked, causal, allocation-free MIC Noise Gate. No added PDC.
#include <algorithm>
#include <cmath>
namespace skellpreset::dsp {
class MicNoiseGate final {
public:
 void prepare(double sampleRate) noexcept {
  sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
  detector=0.;gain=0.;holdRemaining=0;open=false;set(-40.f,4.f,60.f,85.f,4.f);
 }
 void set(float thresholdDb,float attackMs,float holdMs,float releaseMs,float hysteresisDb) noexcept {
  thresholdDb=finiteClamp(thresholdDb,-75.f,-15.f,-40.f);
  attackMs=finiteClamp(attackMs,.5f,25.f,4.f);
  holdMs=finiteClamp(holdMs,0.f,250.f,60.f);
  releaseMs=finiteClamp(releaseMs,20.f,600.f,85.f);
  hysteresisDb=finiteClamp(hysteresisDb,0.f,12.f,4.f);
  openThreshold=std::pow(10.,thresholdDb/20.);
  closeThreshold=std::pow(10.,(thresholdDb-hysteresisDb)/20.);
  attackCoefficient=1.-std::exp(-1./(sr*.001*attackMs));
  releaseCoefficient=1.-std::exp(-1./(sr*.001*releaseMs));
  detectorAttack=1.-std::exp(-1./(sr*.001*2.));
  detectorRelease=1.-std::exp(-1./(sr*.001*35.));
  holdSamples=static_cast<int>(sr*.001*holdMs);
 }
 float processGain(float left,float right) noexcept {
  const double l=std::isfinite(left)?std::abs((double)left):0.;
  const double r=std::isfinite(right)?std::abs((double)right):0.;
  const double peak=std::min(8.,std::max(l,r));
  detector+=(peak-detector)*(peak>detector?detectorAttack:detectorRelease);
  if(detector<1.e-24)detector=0.;
  if(detector>=openThreshold){open=true;holdRemaining=holdSamples;}
  else {if(holdRemaining>0)--holdRemaining;
        if(open&&holdRemaining==0&&detector<closeThreshold)open=false;}
  const double target=open?1.:0.;
  gain+=(target-gain)*(target>gain?attackCoefficient:releaseCoefficient);
  if(gain<1.e-20)gain=0.;
  return static_cast<float>(std::clamp(gain,0.,1.));
 }
 bool isOpen() const noexcept{return open;}
private:
 static float finiteClamp(float x,float lo,float hi,float fallback) noexcept {
  return std::isfinite(x)?std::clamp(x,lo,hi):fallback;
 }
 double sr=48000.,detector=0.,gain=0.;
 double openThreshold=.01,closeThreshold=.006;
 double attackCoefficient=.005,releaseCoefficient=.00025;
 double detectorAttack=.01,detectorRelease=.001;
 int holdSamples=0,holdRemaining=0;bool open=false;
};
}
