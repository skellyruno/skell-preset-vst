#include "../Source/chain/DynamicsPeakHistory.h"
#include <cassert>
#include <limits>
#include <iostream>
int main(){using namespace skell::meter;
    assert(axis(0.f)==0.f);assert(std::abs(axis(1.f)-.5f)<1e-7f);assert(axis(4.f)==1.f);
    assert(axis(std::numeric_limits<float>::infinity())==0.f);assert(axis(std::numeric_limits<float>::quiet_NaN())==0.f);
    History<3> h;assert(!h.at(0));assert(h.advance(12.,true)==1);assert(!h.at(0)->started);
    h.feed(.8f,.4f);assert(h.advance(12.,true)==1);assert(h.at(0)->started&&h.at(0)->peakRatio==.5f);
    assert(h.advance(12.,true)==1);assert(h.at(0)->peakRatio==.5f&&h.at(0)->input==0.f);
    h.feed(.5f,1.f);assert(h.advance(12.,true)==1);assert(h.size()==3&&h.at(0)->peakRatio==2.f&&!h.at(3));
    h.feed(1.f,0.f);assert(h.advance(100.,false)==0);assert(h.advance(12.,true)==1);assert(h.at(0)->peakRatio==2.f);
    assert(h.advance(std::numeric_limits<double>::quiet_NaN(),true)==0);h.feed(std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN());assert(h.advance(12.,true)==1);assert(h.at(0)->input==0.f);
    h.clear();assert(h.size()==0&&!h.at(0));assert(h.advance(100.,true)==8);assert(h.size()==3&&!h.at(0)->started);
    std::cout<<"PASS history axis, timing, wrap, stop, silence, invalid-input and reset\n";
}
