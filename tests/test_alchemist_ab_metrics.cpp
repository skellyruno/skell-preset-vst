#include "../Source/chain/ABMetrics.h"
#include <cassert>
#include <cmath>
#include <limits>
#if defined(NDEBUG)
#error "A/B metric assertions must remain enabled in Release"
#endif
int main() {
    using skellpreset::analysis::ComparisonAccumulator;
    ComparisonAccumulator identical,louder,inverted,invalid,silent;
    for(int i=0;i<4800;++i) {
        const float x=.13f*(float)std::sin(i*.071);
        identical.push(x,x);
        louder.push(2.f*x,x);
        inverted.push(-x,x);
        silent.push(0.f,x);
    }
    invalid.push(std::numeric_limits<float>::quiet_NaN(),.1f);
    invalid.push(.1f,std::numeric_limits<float>::infinity());
    invalid.push(.1f,.1f);
    const auto a=identical.result();
    assert(a.hasSignal&&a.samples==4800&&a.rejected==0);
    assert(std::abs(a.candidateMinusReferenceRmsDb)<1.e-8);
    assert(a.sampleCorrelation>.999999999);
    assert(a.levelMatchedNrmse<1.e-6);
    const auto b=louder.result();
    assert(b.hasSignal);
    assert(std::abs(b.candidateMinusReferenceRmsDb-6.020599913)<1.e-5);
    assert(b.sampleCorrelation>.999999999);
    assert(b.levelMatchedNrmse<1.e-6);
    const auto c=inverted.result();
    assert(c.hasSignal);
    assert(c.sampleCorrelation<-.999999999);
    assert(c.levelMatchedNrmse>1.999999);
    const auto d=invalid.result();
    assert(d.samples==1&&d.rejected==2&&d.hasSignal);
    assert(!silent.result().hasSignal);
    ComparisonAccumulator empty;
    assert(!empty.result().hasSignal);
    return 0;
}
