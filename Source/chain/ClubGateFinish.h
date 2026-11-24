#pragma once
// Experimental causal counterpart to the owner-selected offline Club Gate B.
// This is a bounded soft finish after a gate, not a replacement for its detector.
#include <algorithm>
#include <array>
#include <cmath>
namespace skellpreset::dsp {
class ClubGateFinish final {
public:
 void prepare(double rate) noexcept {
  rate=std::isfinite(rate)?std::clamp(rate,8000.,192000.):44100.;
  window=static_cast<int>(std::lround(rate*.01));
  power.fill(0.);shape.fill(0.);position=0;powerSum=shapeSum=0.;
 }
 void setThresholdDb(float db) noexcept {
  threshold=std::pow(10.,std::clamp(std::isfinite(db)?double(db):-57.4,-85.,-25.)/20.);
 }
 float processGain(float left,float right) noexcept {
  const double l=std::isfinite(left)?std::clamp(double(left),-8.,8.):0.;
  const double r=std::isfinite(right)?std::clamp(double(right),-8.,8.):0.;
  const double p=(l*l+r*r)*.5;
  powerSum=std::max(0.,powerSum-power[position]+p);power[position]=p;
  const double g=std::clamp(std::sqrt(powerSum/window)/threshold,0.,1.);
  shapeSum=std::clamp(shapeSum-shape[position]+g,0.,double(window));shape[position]=g;
  if(++position==window)position=0;
  return static_cast<float>(.75+.25*shapeSum/window);
 }
private:
 std::array<double,1920> power{},shape{};
 int window=441,position=0;
 double powerSum=0.,shapeSum=0.,threshold=.00135;
};
}
