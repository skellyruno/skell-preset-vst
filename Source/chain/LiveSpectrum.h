#pragma once
// Actual input/output audio spectra, never a decorative frequency display.
// The audio callback publishes atomic mono samples. The message thread
// computes Goertzel bins; no FFT plan, allocations, mutex or GUI in the callback.
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skellpreset::visual {
class LiveSpectrumCapture final {
public:
    static constexpr int capacity=1024;
    LiveSpectrumCapture() noexcept {reset();}
    void reset() noexcept {
        for(auto& value:data)value.store(0.f,std::memory_order_relaxed);
        count.store(0,std::memory_order_release);
    }
    void push(float left,float right) noexcept {
        const float l=std::isfinite(left)?left:0.f;
        const float r=std::isfinite(right)?right:0.f;
        const auto index=count.load(std::memory_order_relaxed);
        data[index%capacity].store(
            std::clamp(.5f*l+.5f*r,-8.f,8.f),std::memory_order_relaxed);
        count.store(index+1,std::memory_order_release);
    }
    void snapshot(std::array<float,capacity>& out) const noexcept {
        const auto end=count.load(std::memory_order_acquire);
        for(unsigned i=0;i<(unsigned)capacity;++i) {
            const auto history=static_cast<std::uint64_t>(capacity-i);
            const auto idx=end>=history?(end-history)%capacity:0U;
            out[(size_t)i]=end>=history
                ?data[static_cast<std::size_t>(idx)].load(std::memory_order_relaxed):0.f;
        }
    }
private:
    std::array<std::atomic<float>,capacity> data{};
    // At 48 kHz a 32-bit full-rate cursor wraps in under 25 hours.
    std::atomic<std::uint64_t> count{0};
};

class LiveSpectrumAnalyzer final {
public:
    static constexpr int bands=36;
    LiveSpectrumAnalyzer() noexcept {
        double sum=0.;
        for(int i=0;i<LiveSpectrumCapture::capacity;++i) {
            const double x=(double)i/(LiveSpectrumCapture::capacity-1);
            window[(size_t)i]=(float)(.5-.5*std::cos(twoPi*x));
            sum+=window[(size_t)i];
        }
        normalization=(float)(2./sum);
    }
    static float frequency(int band) noexcept {
        const float fraction=std::clamp((float)band/(bands-1),0.f,1.f);
        return 80.f*std::pow(112.5f,fraction); // 80Hz...9kHz
    }
    std::array<float,bands> analyze(
         const std::array<float,LiveSpectrumCapture::capacity>& samples,
         double sampleRate) const noexcept {
        std::array<float,bands> result{};
        const double sr=std::clamp(std::isfinite(sampleRate)?sampleRate:48000.,
                                   8000.,192000.);
        std::array<double,LiveSpectrumCapture::capacity> weighted{};
        bool hasEnergy=false;
        for(std::size_t i=0;i<weighted.size();++i){
            const float sample=std::isfinite(samples[i])?std::clamp(samples[i],-8.f,8.f):0.f;
            weighted[i]=static_cast<double>(sample)*window[i];
            hasEnergy=hasEnergy||weighted[i]!=0.;
        }
        if(!hasEnergy)return result;
        for(int band=0;band<bands;++band) {
            const float f=frequency(band);
            // Frequencies outside the actual band limit are unavailable,
            // not multiple copies of the same near-Nyquist value.
            if(f>sr*.45)continue;
            const double omega=twoPi*(double)f/sr;
            const double coeff=2.*std::cos(omega);
            double q1=0.,q2=0.;
            for(int i=0;i<LiveSpectrumCapture::capacity;++i) {
                const double q=weighted[(size_t)i]+coeff*q1-q2;
                q2=q1;q1=q;
            }
            const double energy=std::max(0.,q1*q1+q2*q2-coeff*q1*q2);
            result[(size_t)band]=(float)std::clamp(
                std::sqrt(energy)*normalization,0.,8.);
        }
        return result;
    }
private:
    static constexpr double twoPi=6.283185307179586476925286766559;
    std::array<float,LiveSpectrumCapture::capacity> window{};
    float normalization=0.f;
};
} // namespace skellpreset::visual
