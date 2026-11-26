#pragma once
// Exact physical-to-normalized mappings for existing Skell Preset controls.
// These do not translate arbitrary research filters into native effects.
#include <cmath>
#include <optional>
namespace skellpreset::assistant {
enum class Control { ChorusRate, ChorusDepth, ChorusVoices, ChorusSpread,
 FlangerRate, FlangerDepth, FlangerFeedback, FlangerSpread,
 DelayMs, DelayFeedback, DelayCutHz, DeEsserHz, DeEsserThreshold, DeEsserRange };
struct Mapping { int type, parameter; float value; };
inline std::optional<Mapping> mapControl(Control c,double value) noexcept {
 if(!std::isfinite(value))return {};
 int id=0,p=0;double lo=0,hi=1,n=0;bool log=false;
 switch(c){
 case Control::ChorusRate:id=20;p=0;lo=.08;hi=3.98;break;
 case Control::ChorusDepth:id=20;p=1;lo=1;hi=11;break;
 case Control::ChorusVoices:id=20;p=2;lo=2;hi=5;break;
 case Control::ChorusSpread:id=20;p=3;break;
 case Control::FlangerRate:id=21;p=0;lo=.05;hi=5;break;
 case Control::FlangerDepth:id=21;p=1;lo=.3;hi=9.3;break;
 case Control::FlangerFeedback:id=21;p=2;lo=-.82;hi=.82;break;
 case Control::FlangerSpread:id=21;p=3;break;
 case Control::DelayMs:id=7;p=0;lo=55;hi=905;break;
 case Control::DelayFeedback:id=7;p=1;hi=.8;break;
 case Control::DelayCutHz:id=7;p=2;lo=500;hi=18000;break;
 case Control::DeEsserHz:id=46;p=0;lo=2000;hi=16000;log=true;break;
 case Control::DeEsserThreshold:id=46;p=1;lo=-80;hi=0;break;
 case Control::DeEsserRange:id=46;p=2;hi=24;break;
 default:return {};
 }
 if(value<lo||value>hi)return {};
 n=log?std::log(value/lo)/std::log(hi/lo):(value-lo)/(hi-lo);
 return Mapping{id,p,float(n)};
}
struct Edit { int slot,type,parameter; float before,after; };
inline bool validEdit(const Edit& e) noexcept {
 if(e.slot<0||e.slot>=16||e.parameter<0||!std::isfinite(e.before)||!std::isfinite(e.after)||e.before<0||e.before>1||e.after<0||e.after>1)return false;
 if(std::abs(e.before-std::round(e.before*1000.f)/1000.f)>1e-6f||std::abs(e.after-std::round(e.after*1000.f)/1000.f)>1e-6f)return false;
 return ((e.type==20||e.type==21)&&e.parameter<4)||((e.type==7||e.type==46)&&e.parameter<3);
}
inline std::optional<Edit> propose(int slot,int currentType,float current,Control c,double requested) noexcept {
 auto m=mapControl(c,requested);if(!m||m->type!=currentType)return {};
 // APVTS slot parameters use a .001 interval. Bind undo to the snapped value.
 const float snapped=std::round(m->value*1000.f)/1000.f;
 Edit e{slot,currentType,m->parameter,current,snapped};if(!validEdit(e))return {};return e;
}
inline bool canApply(const Edit& e,bool custom,bool visible,int type,float current,bool undo=false) noexcept {
 return validEdit(e)&&custom&&visible&&type==e.type&&std::isfinite(current)&&std::abs(current-(undo?e.after:e.before))<=1e-6f;
}
} // namespace skellpreset::assistant
