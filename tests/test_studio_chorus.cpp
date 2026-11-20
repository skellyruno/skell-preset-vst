// SPDX-License-Identifier: GPL-3.0-or-later
// Port regression of Lemon Studio's published chorus sample vectors,
// and Skell Preset automation/CPU stability. Never use stock FL binaries.
#include "../Source/chain/StudioChorusDSP.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <algorithm>
#include <iostream>
#if defined(NDEBUG)
#error "Release regression must retain assertions"
#endif
using skellpreset::studio::Chorus;
static float fromBits(std::uint32_t bits) {
    float value=0.f;
    static_assert(sizeof(bits)==sizeof(value));
    std::memcpy(&value,&bits,sizeof(value));
    return value;
}
static void sourceVectors(const Chorus::Values& settings,
                          const std::array<std::uint32_t,10>& left,
                          const std::array<std::uint32_t,10>& right,
                          float tolerance) {
    Chorus chorus;
    chorus.prepare(44100);
    chorus.setRaw(settings);
    const int indices[]{0,1,2,3,100,500,1000,1999,2500,2999};
    int at=0;
    for(int n=0;n<3000;++n) {
        const float l=(float)(std::sin((double)n*.05)*.5);
        const float r=(float)((double)((n*37)%101)/101.-.5);
        const auto result=chorus.process(l,r);
        for(float x:result)assert(std::isfinite(x));
        if(at<10 && n==indices[at]) {
            const auto wantedL=fromBits(left[(size_t)at]);
            const auto wantedR=fromBits(right[(size_t)at]);
            // Cross-language code generation may differ in the last ulps.
            assert(std::abs(result[0]-wantedL)<tolerance);
            assert(std::abs(result[1]-wantedR)<tolerance);
            ++at;
        }
    }
    assert(at==10);
}
int main() {
    sourceVectors(Chorus::defaults,
        {0x00000000,0x3c8f4cdf,0x3d0f1f06,0x3d563c08,
         0xbeabd6d8,0xbd3dbd4e,0x3e9eed91,0x3e38b7de,
         0x3db8df10,0xbd736430},
        {0xbeb33333,0xbdbf9eac,0x3e26c7bb,0xbe8fb700,
         0x3dbf9eac,0xbe6dc020,0xbe4b70db,0xbe4c49ac,
         0x3eddf864,0x3e2152f2},.003f);
    sourceVectors({40,4375,149,3229,0,781,587,0,1024,65,4531,491},
        {0,0,0,0,0,0xbf1f4597,0xbf203e3e,0xbedf6dbc,
         0xbe895881,0xbe1486a3},
        {0,0,0,0,0,0xbd0f1774,0xbda0039d,0xbe06f623,
         0x3da7bf2c,0x3da0b044},.005f);

    for(double rate:{8000.,44100.,48000.,96000.,192000.}) {
        Chorus chorus;
        chorus.prepare(rate);
        chorus.set(.5f,5.f,3.f,.5f);
        auto raw=chorus.raw();
        assert(raw[11]==0); // Keep Studio's direct vocal with chorus wet.
        chorus.reset();
        const auto onset=chorus.process(.25f,-.25f);
        // Before delay taps can contain signal, the original Studio chorus
        // produces 70%-level direct audio rather than replacing it with silence.
        assert(std::abs(onset[0]-.175f)<.00001f);
        assert(std::abs(onset[1]+.175f)<.00001f);
        double energy=0.,maxAbs=0.;
        for(int n=0;n<40000;++n) {
            if(n%137==0)chorus.set(.2f+(n%7)*.32f,
                                   2.f+(n%6),2.f+(n%4),.15f*(n%5));
            const float l=n%53==0?.30f:(float)(.04*std::sin(n*.027));
            const float r=n%31==0?-.31f:(float)(.09*std::sin(n*.034));
            const auto y=chorus.process(l,r);
            for(float v:y) {
                assert(std::isfinite(v));
                assert(std::abs(v)<16.f);
                energy+=v*v;
                maxAbs=std::max(maxAbs,(double)std::abs(v));
            }
        }
        assert(energy>1. && maxAbs>.005);
        chorus.reset();
        for(int i=0;i<40000;++i) {
            const auto y=chorus.process(0.f,0.f);
            for(float v:y)assert(std::isfinite(v));
        }
        chorus.setRaw(raw);
        auto y=chorus.process(std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity());
        assert(std::isfinite(y[0])&&std::isfinite(y[1]));
    }
    std::cout<<"PASS: source sample vectors, Studio crossover chorus and 8-192kHz automation/stability\n";
    return 0;
}
