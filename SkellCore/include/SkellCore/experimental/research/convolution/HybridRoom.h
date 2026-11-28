#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace skellcore::experimental::convolution {
// Independent sparse true-stereo early IR + orthogonal eight-line late FDN.
// No commercial IR or algorithm. Prepare/reset off the audio thread. All setters
// and processFrame have a single owner; no fixed buffering/lookahead latency.
class HybridRoom {
public:
    using Frame = std::array<float,2>;
    static constexpr unsigned bufferingLatency = 0;
    struct Reflection { unsigned delay; Frame left; Frame right; }; // output rows LL/LR, RL/RR
    void prepare(double fs, double size=1.) {
        ready_=false;
        rate_=bound(fs,8000.,384000.,48000.);size=bound(size,.5,2.,1.);
        constexpr double seconds[]{.0037,.0059,.0081,.0113,.0147,.0189,.0233,.0287,.0341,.0397,.0461,.0533};
        constexpr float matrix[12][4]={{.21f,.035f,.022f,.17f},{.05f,.11f,-.10f,.045f},{-.09f,.025f,.032f,.084f},{.07f,-.075f,.065f,-.06f},{.06f,.034f,-.047f,.07f},{-.05f,.057f,.055f,-.04f},{.045f,.036f,-.038f,.046f},{.04f,-.035f,.032f,-.04f},{-.035f,.028f,.026f,.034f},{.031f,.023f,-.024f,.03f},{.022f,-.019f,.024f,-.022f},{-.018f,.016f,.017f,.019f}};
        for(unsigned i=0;i<reflections_.size();++i){const auto& m=matrix[i];reflections_[i]={unsigned(std::lround(rate_*size*seconds[i])),{m[0],m[1]},{m[2],m[3]}};}
        early_.assign(reflections_.back().delay+2,{});
        pre_.assign(unsigned(std::ceil(rate_*.2))+2,{});
        constexpr double tailSeconds[]{.0311,.0379,.0417,.0473,.0539,.0611,.0677,.0739};
        for(unsigned j=0;j<lines_.size();++j)lines_[j].assign(std::max(17u,unsigned(std::lround(rate_*size*tailSeconds[j]))|1u),0.f);
        step_=float(-std::expm1(-1./(.025*rate_)));fadeStep_=float(1./(.02*rate_));
        setParameters(decay_,damping_,targetWet_);setLowCut(lowCut_);setPredelay(preMs_);reset();ready_=true;
    }
    void reset() noexcept {
        for(auto& line:lines_)std::fill(line.begin(),line.end(),0.f);
        std::fill(pre_.begin(),pre_.end(),Frame{});std::fill(early_.begin(),early_.end(),Frame{});
        started_=false;pos_.fill(0);state_.fill(0.f);low_={};earlyLow_={};prePos_=earlyPos_=0;
        preOld_=preNow_=preTarget_;fade_=1.f;feedback_=targetFeedback_;pole_=targetPole_;hp_=targetHp_;wet_=targetWet_;
        earlyGain_=targetEarly_;lateGain_=targetLate_;
    }
    void setParameters(float seconds,float damping,float wet) noexcept {
        seconds=float(bound(seconds,.15,20.,2.5));damping=float(bound(damping,0.,.98,.3));
        targetWet_=float(bound(wet,0.,1.,1.));
        if(seconds!=decay_||!ready_){decay_=seconds;for(unsigned j=0;j<lines_.size();++j)targetFeedback_[j]=float(std::exp(-6.907755278982137*lines_[j].size()/(rate_*decay_)));}
        if(damping!=damping_||!ready_){damping_=damping;const double hz=std::min(.44*rate_,500.*std::pow(32.,1.-damping_));targetPole_=float(std::exp(-6.283185307179586*hz/rate_));}
        if(!started_){feedback_=targetFeedback_;pole_=targetPole_;wet_=targetWet_;}
    }
    void setPredelay(float ms) noexcept {preMs_=float(bound(ms,0.,200.,0.));preTarget_=unsigned(std::lround(rate_*preMs_*.001));if(!started_){preOld_=preNow_=preTarget_;fade_=1.f;}}
    void setLowCut(float hz) noexcept {hz=float(bound(hz,20.,std::min(2000.,rate_*.4),90.));if(hz!=lowCut_||!ready_){lowCut_=hz;targetHp_=float(-std::expm1(-6.283185307179586*hz/rate_));}if(!started_)hp_=targetHp_;}
    void setEarlyLate(float early,float late) noexcept {targetEarly_=float(bound(early,0.,1.,.8));targetLate_=float(bound(late,0.,1.,.8));if(!started_){earlyGain_=targetEarly_;lateGain_=targetLate_;}}
    const std::array<Reflection,12>& reflections() const noexcept {return reflections_;}
    std::size_t historyBytes() const noexcept {std::size_t n=(early_.size()+pre_.size())*sizeof(Frame);for(const auto& l:lines_)n+=l.size()*sizeof(float);return n;}
    Frame processFrame(float left,float right) noexcept {
        const Frame in{safe(left),safe(right)};if(!ready_)return in;started_=true;
        const auto slew=[this](float& v,float t){v+=step_*(t-v);};
        slew(pole_,targetPole_);slew(hp_,targetHp_);slew(wet_,targetWet_);slew(earlyGain_,targetEarly_);slew(lateGain_,targetLate_);
        if(fade_>=1.f&&preNow_!=preTarget_){preOld_=preNow_;preNow_=preTarget_;fade_=0.f;}
        for(unsigned c=0;c<2;++c){low_[c]=flush(low_[c]+hp_*(in[c]-low_[c]));pre_[prePos_][c]=in[c]-low_[c];}
        const auto readPre=[this](unsigned d)->Frame {return pre_[(prePos_+pre_.size()-d)%pre_.size()];};
        const auto a=readPre(preOld_),b=readPre(preNow_);
        const Frame x{a[0]+fade_*(b[0]-a[0]),a[1]+fade_*(b[1]-a[1])};
        if(++prePos_==pre_.size())prePos_=0;
        fade_=std::min(1.f,fade_+fadeStep_);
        early_[earlyPos_]=x;Frame er{};
        for(const auto& t:reflections_){const auto j=earlyPos_>=t.delay?earlyPos_-t.delay:earlyPos_+early_.size()-t.delay;const auto v=early_[j];er[0]+=t.left[0]*v[0]+t.left[1]*v[1];er[1]+=t.right[0]*v[0]+t.right[1]*v[1];}
        if(++earlyPos_==early_.size())earlyPos_=0;
        for(unsigned c=0;c<2;++c)earlyLow_[c]=flush((1.f-pole_)*er[c]+pole_*earlyLow_[c]);
        std::array<float,8> z{},h{};
        for(unsigned j=0;j<8;++j){z[j]=lines_[j][pos_[j]];state_[j]=flush((1.f-pole_)*z[j]+pole_*state_[j]);h[j]=state_[j];slew(feedback_[j],targetFeedback_[j]);}
        // Normalized Hadamard is orthogonal. Per-line loss and damping are contractive.
        for(unsigned stride=1;stride<8;stride*=2)for(unsigned base=0;base<8;base+=2*stride)for(unsigned j=0;j<stride;++j){const auto u=h[base+j],v=h[base+j+stride];h[base+j]=u+v;h[base+j+stride]=u-v;}
        constexpr float norm=.3535533905932738f;
        const float l=x[0]+.35f*earlyLow_[0],r=x[1]+.35f*earlyLow_[1];
        const float drive[8]{l,r,l,-r,r,l,-r,l};
        for(unsigned j=0;j<8;++j){lines_[j][pos_[j]]=flush(.16f*drive[j]+norm*h[j]*feedback_[j]);if(++pos_[j]==lines_[j].size())pos_[j]=0;}
        const Frame tail{norm*(z[0]+z[1]-z[2]+z[3]-z[4]+z[5]-z[6]-z[7]),norm*(z[0]-z[1]+z[2]+z[3]+z[4]-z[5]-z[6]-z[7])};
        if(wet_==0.f)return in;
        return {(1.f-wet_)*in[0]+wet_*(earlyGain_*earlyLow_[0]+lateGain_*tail[0]),(1.f-wet_)*in[1]+wet_*(earlyGain_*earlyLow_[1]+lateGain_*tail[1])};
    }
private:
    static double bound(double x,double lo,double hi,double fallback) noexcept {return std::isfinite(x)?std::clamp(x,lo,hi):fallback;}
    static float safe(float x) noexcept {return std::isfinite(x)?std::clamp(x,-8.f,8.f):0.f;}
    static float flush(float x) noexcept {return std::abs(x)<1e-20f?0.f:x;}
    double rate_=48000.;bool ready_=false,started_=false;float decay_=2.5f,damping_=.3f,preMs_=0.f,lowCut_=90.f;
    float step_=.001f,fadeStep_=.001f,fade_=1.f,wet_=1.f,targetWet_=1.f,pole_=0.f,targetPole_=0.f,hp_=0.f,targetHp_=0.f,earlyGain_=.8f,targetEarly_=.8f,lateGain_=.8f,targetLate_=.8f;
    unsigned preOld_=0,preNow_=0,preTarget_=0;std::size_t prePos_=0,earlyPos_=0;
    Frame low_{},earlyLow_{};std::vector<Frame> pre_,early_;std::array<Reflection,12> reflections_{};
    std::array<std::vector<float>,8> lines_;std::array<std::size_t,8> pos_{};std::array<float,8> state_{},feedback_{},targetFeedback_{};
};
// Same public wet-only controls as the existing room slot. Original mapping,
// not a vendor calibration. Select only in an explicitly opted-in test build.
class HybridVocalRoom {
public:
    void prepare(double fs){room_.prepare(fs,.85);set(.55f,.5f,0.f,120.f);room_.reset();ready_=true;}
    void reset() noexcept {room_.reset();}
    void set(float size,float damp,float pre,float lowCut) noexcept {size=std::isfinite(size)?std::clamp(size,0.f,1.f):.55f;damp=std::isfinite(damp)?std::clamp(damp,0.f,1.f):.5f;room_.setParameters(.3f+5.7f*size,damp*.95f,1.f);room_.setPredelay(pre);room_.setLowCut(lowCut);}
    HybridRoom::Frame process(float l,float r) noexcept {return ready_?room_.processFrame(l,r):HybridRoom::Frame{};}
private: HybridRoom room_;bool ready_=false;
};
}
