#pragma once
#include "LiveVocalMatch.h"
#include <SkellCore/backends/tune/RealtimeTuner.hpp>
#include <SkellCore/backends/tune/ScaleMask.hpp>
namespace skellpreset::liveMatch {
// Worker-only scale-note evidence. Relative major/minor have the same note
// mask, so never guess a tonic from a monophonic vocal. Sparse or conflicting
// phrases abstain; callers must ask for confirmation before tuning.
struct PitchEvidence {std::uint16_t mask=0;float coverage=0,margin=0;int distinct=0;};
inline PitchEvidence inferPitchNotes(const Audio& a,size_t begin,size_t end,const std::atomic<bool>* cancel=nullptr) noexcept {
    if(!std::isfinite(a.rate)||a.rate<8000||a.rate>192000||a.stereo.size()%2||end>a.frames()||begin>=end||end-begin>a.rate*20.)return {};
    skelly::CausalPitchTracker tracker;tracker.prepare(a.rate);
    std::array<double,12> counts{};int previous=-1;double stable=0.;const double hop=tracker.hopHostSamples()/a.rate;
    for(size_t i=begin;i<end;++i){if((i&2047u)==0&&cancel&&cancel->load(std::memory_order_relaxed))return {};
        const float l=a.stereo[i*2],r=a.stereo[i*2+1];if(!std::isfinite(l)||!std::isfinite(r))return {};
        skelly::PitchEstimate e;if(!tracker.push(.5f*(l+r),e))continue;
        if(!e.valid||e.confidence<.84f||!std::isfinite(e.hertz)||e.hertz<=0){previous=-1;stable=0;continue;}
        const double midi=69.+12.*std::log2(e.hertz/440.);const int note=(int)std::lround(midi);
        if(std::abs(midi-note)>.43){previous=-1;stable=0;continue;}
        if(note!=previous){previous=note;stable=0;continue;}stable+=hop;
        if(stable>=.065)counts[(size_t)((note%12+12)%12)]+=hop*e.confidence;
    }
    PitchEvidence result;double total=0;for(auto& c:counts){if(c<.09)c=0;else ++result.distinct;total+=c;}
    if(total<1.2||result.distinct<5)return result;
    // Relative major/minor share a mask: evaluate 12 distinct diatonic sets.
    double first=-1,second=-1;std::uint16_t winner=0;
    for(int root=0;root<12;++root){const auto mask=skelly::scaleMask(root,1);double covered=0;for(int k=0;k<12;++k)if(mask&(1u<<k))covered+=counts[(size_t)k];const double score=covered/total;
        if(score>first){second=first;first=score;winner=mask;}else if(score>second)second=score;}
    result.coverage=(float)first;result.margin=(float)(first-second);
    if(first>=.94&&first-second>=.055)result.mask=winner;
    return result;
}
inline PitchEvidence consensusPitchNotes(const Audio& a,const std::atomic<bool>* cancel=nullptr) noexcept {
    if(a.frames()<a.rate*5.9||a.frames()>a.rate*20.+1)return {};
    const auto first=inferPitchNotes(a,0,a.frames()/2,cancel),second=inferPitchNotes(a,a.frames()/2,a.frames(),cancel);
    if(!first.mask||first.mask!=second.mask||(cancel&&cancel->load(std::memory_order_relaxed)))return {};
    return {first.mask,std::min(first.coverage,second.coverage),std::min(first.margin,second.margin),std::min(first.distinct,second.distinct)};
}
}
