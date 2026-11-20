#include "../Source/chain/SkelldizerDSP.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#if defined(NDEBUG)
#error "Stress assertions must execute in release builds"
#endif
int main() {
    using skellpreset::dsp::Skelldizer;
    volatile double checksum=0;
    for (double rate : {44100.0,48000.0,96000.0}) {
        for (int block : {16,64,128,512,1024}) {
            std::array<Skelldizer,20> processors;
            for(auto& p:processors) {p.prepare(rate);p.set(.6f,0.f,.5f,1.f);}
            const int total=(int)rate; // 1 second of audio, 20 instances
            auto start=std::chrono::steady_clock::now();
            double maxPeak=0;
            for(int offset=0;offset<total;offset+=block) {
                for(int plugin=0;plugin<(int)processors.size();++plugin) {
                    auto& p=processors[(size_t)plugin];
                    const float amount=(offset/block)%13==0?.95f:.42f;
                    p.set(amount,((offset/block)/7)%4/3.f,.5f,1.f);
                    for(int n=offset;n<std::min(total,offset+block);++n) {
                        const float x=n%1009==0?std::numeric_limits<float>::quiet_NaN():
                                      n%701==0?std::numeric_limits<float>::infinity():
                                      .21f*(float)std::sin(6.283185307179586*290.0*n/rate);
                        const auto y=p.process(x,-x);
                        assert(std::isfinite(y[0]) && std::isfinite(y[1]));
                        assert(std::abs(y[0])<=1.001f && std::abs(y[1])<=1.001f);
                        maxPeak=std::max(maxPeak,std::abs((double)y[0]));
                        checksum+=y[0]*1.e-6;
                    }
                }
            }
            const double elapsed=std::chrono::duration<double>(
                std::chrono::steady_clock::now()-start).count();
            // Informational throughput; CI hardware varies and is not FL Studio.
            std::cout << "Skelldizer instances=20 rate=" << rate
                      << " block=" << block << " wall_seconds=" << elapsed
                      << " realtime_capacity=" << 1.0/elapsed
                      << " max_peak=" << maxPeak << '\n';
            assert(maxPeak>0);
        }
    }
    std::cout<<"checksum="<<checksum<<"\n";
}
