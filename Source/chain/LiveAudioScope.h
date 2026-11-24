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
        const std::uint64_t before=count.load(std::memory_order_relaxed);
        std::uint64_t cursor=before;
        // Jump directly to capture events. For a 512-sample block this
        // executes eight iterations, not 512, with identical sample phase.
        // The published mono trace is independent of host block splitting.
        for(int i=decimation-phase-1;i<samples;){
            const float l=std::isfinite(left[i])?left[i]:0.f;
            const float r=right?(std::isfinite(right[i])?right[i]:0.f):l;
            const float mono=.5f*l+.5f*r; // avoid overflow before downmix
            data[cursor%capacity].store(std::clamp(mono,-1.f,1.f),
                                       std::memory_order_relaxed);
            ++cursor;
            if(samples-i<=decimation)break; // also avoids signed int overflow
            i+=decimation;
        }
        phase=(phase+samples%decimation)%decimation;
        if(cursor!=before)count.store(cursor,std::memory_order_release);
    }
    template<std::size_t N>
    void snapshot(std::array<float,N>& output) const noexcept {
        static_assert(N<=capacity);
        const auto end=count.load(std::memory_order_acquire);
        for(std::size_t i=0;i<N;++i){
            const auto prior=N-i;
            const auto offset=end>=prior?end-prior:0;
            output[i]=end>=prior?data[offset%capacity].load(std::memory_order_relaxed):0.f;
        }
    }
    std::uint64_t samplesCaptured() const noexcept {return count.load(std::memory_order_acquire);}
private:
    std::array<std::atomic<float>,capacity> data{};
    std::atomic<std::uint64_t> count{0};
    int phase=0; // audio thread only
};
}
