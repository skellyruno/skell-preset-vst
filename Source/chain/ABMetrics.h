#pragma once
// Portable, offline-only, synchronized A/B diagnostics. This code is never
// called on Skell Preset's real-time audio thread. It intentionally reports
// sample-aligned RMS, peak, and correlation, NOT LUFS or perceptual similarity.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace skellpreset::analysis {
struct ComparisonSummary {
    std::uint64_t samples=0;
    std::uint64_t rejected=0;
    bool hasSignal=false;
    double candidateMinusReferenceRmsDb=0.;
    double candidatePeakDbfs=0.;
    double referencePeakDbfs=0.;
    double sampleCorrelation=0.;
    double levelMatchedNrmse=0.;
};
class ComparisonAccumulator final {
public:
    void push(float candidate,float reference) noexcept {
        if(!std::isfinite(candidate)||!std::isfinite(reference)) {
            ++rejected_; return;
        }
        const double a=reference,b=candidate;
        referenceEnergy_+=a*a;
        candidateEnergy_+=b*b;
        crossEnergy_+=a*b;
        referencePeak_=std::max(referencePeak_,std::abs(a));
        candidatePeak_=std::max(candidatePeak_,std::abs(b));
        ++samples_;
    }
    ComparisonSummary result() const noexcept {
        ComparisonSummary out{};
        out.samples=samples_;
        out.rejected=rejected_;
        out.referencePeakDbfs=20.*std::log10(std::max(1.e-30,referencePeak_));
        out.candidatePeakDbfs=20.*std::log10(std::max(1.e-30,candidatePeak_));
        if(samples_==0||referenceEnergy_<=1.e-20||candidateEnergy_<=1.e-20)
            return out;
        out.hasSignal=true;
        out.candidateMinusReferenceRmsDb=10.*std::log10(
            candidateEnergy_/referenceEnergy_);
        const double denominator=std::sqrt(referenceEnergy_)
                                *std::sqrt(candidateEnergy_);
        out.sampleCorrelation=std::clamp(crossEnergy_/denominator,-1.,1.);
        // RMS-only gain match preserves signal polarity: an inverted result
        // correctly has a large error rather than silently flipping polarity.
        out.levelMatchedNrmse=std::sqrt(std::max(0.,2.-2.*out.sampleCorrelation));
        return out;
    }
private:
    std::uint64_t samples_=0,rejected_=0;
    double referenceEnergy_=0.,candidateEnergy_=0.,crossEnergy_=0.;
    double referencePeak_=0.,candidatePeak_=0.;
};
} // namespace skellpreset::analysis
