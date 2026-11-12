#pragma once
// Lock-free single-audio-producer / message-thread-reader mini scope.
// 322 DAW uses Web Audio AnalyserNode, which cannot be embedded in a JUCE
// VST3. This native equivalent samples the ACTUAL processed host signal.
// No locks, heap work, filesystem, or UI calls from the audio callback.
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace skellpreset::visual {
class LiveAudioScope {
public:
    static constexpr unsigned capacity=256;
    static constexpr int decimation=64;
    LiveAudioScope() noexcept {reset();}
    void reset() noexcept {
        for(auto& sample:data)sample.store(0.f,std::memory_order_relaxed);
        count.store(0,std::memory_order_release);phase=0;
    }
    void push(const float* left,const float* right,int samples) noexcept {
        if(!left||samples<=0)return;
        unsigned cursor=count.load(std::memory_order_relaxed);
        for(int i=0;i<samples;++i) {
            if(++phase>=decimation){
                phase=0;
                const float l=std::isfinite(left[i])?left[i]:0.f;
                const float r=right?(std::isfinite(right[i])?right[i]:0.f):l;
                data[cursor%capacity].store(std::clamp(.5f*(l+r),-1.f,1.f),
                                           std::memory_order_relaxed);
                ++cursor;
            }
        }
        count.store(cursor,std::memory_order_release);
    }
    template<std::size_t N>
    void snapshot(std::array<float,N>& output) const noexcept {
        static_assert(N<=capacity);
        const auto end=count.load(std::memory_order_acquire);
        for(unsigned i=0;i<N;++i){
            const auto prior=N-i;
            const auto offset=end>=prior?end-prior:0;
            output[i]=end>=prior?data[offset%capacity].load(std::memory_order_relaxed):0.f;
        }
    }
    unsigned samplesCaptured() const noexcept {return count.load(std::memory_order_acquire);}
private:
    std::array<std::atomic<float>,capacity> data{};
    std::atomic<unsigned> count{0};
    int phase=0; // audio thread only
};
}
