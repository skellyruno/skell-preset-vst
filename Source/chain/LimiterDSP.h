#pragma once
// Compatible with the actually compiled SkellCore 6a2077b limiter. Only
// append-only timing and metering are added; the default kernel is unchanged.
#include <SkellCore/Core.h>
namespace skellpreset::dsp {
class CompatibleLimiter {
public:
    // Economy: 2 interpolated phases x 8 taps. Standard: 4 x 16.
    // Neither mode is yet BS.1770 true-peak certified.
    enum class Quality { Economy, Standard };
    void prepare(double sampleRate, int channels = 2, Quality quality = Quality::Standard) {
        ready_ = false;
        rate_ = skellcore::detail::safeSampleRate(sampleRate);
        channels_ = std::clamp(channels, 1, 2);
        quality_ = quality;
        activeTaps_ = quality == Quality::Economy ? 8 : taps;
        activePhases_ = quality == Quality::Economy ? 2 : 4;
        delaySamples_ = std::max(48, static_cast<int>(rate_ * .003));
        for (auto& b : delay_) b.assign(static_cast<std::size_t>(delaySamples_) + 1, 0.f);
        for (auto& k : kernel_) k.fill(0.0);
        for (int p = 0; p < activePhases_; ++p) {
            const double offsetFraction = static_cast<double>(p) / activePhases_;
            const double centre = (activeTaps_ - 1) * .5;
            double sum = 0.0;
            auto& phaseKernel = kernel_[static_cast<std::size_t>(p)];
            for (int j = 0; j < activeTaps_; ++j) {
                const double offset = j - centre + offsetFraction;
                const double sinc = std::abs(offset) < 1.e-10 ?
                    1.0 : std::sin(skellcore::detail::pi * offset) / (skellcore::detail::pi * offset);
                const double window = .5 - .5 * std::cos(2 * skellcore::detail::pi * j / (activeTaps_ - 1));
                const auto tap = static_cast<std::size_t>(j);
                phaseKernel[tap] = sinc * window;
                sum += phaseKernel[tap];
            }
            for (int j = 0; j < activeTaps_; ++j)
                phaseKernel[static_cast<std::size_t>(j)] /= sum;
        }
        releasePole_ = skellcore::detail::pole(rate_, 70.0);
        attackPole_=0.;attackMs_=0.;releaseMs_=70.;
        reset();
        ready_ = true;
    }
    void reset() noexcept {
        for (auto& b : delay_) std::fill(b.begin(), b.end(), 0.f);
        for (auto& h : history_) h.fill(0.0);
        write_ = 0; historyWrite_ = 0; gain_ = 1.0; hold_ = 0;meterGain_=1.;
    }
    void setCeilingDb(float db) noexcept {
        const float clamped = static_cast<float>(skellcore::detail::clamp(db, -24, 0));
        if (!ceilingValid_ || std::abs(clamped-ceilingDb_) > 0.f) {
            ceilingDb_ = clamped;
            ceiling_ = std::pow(10.0, ceilingDb_ / 20.0);
            ceilingValid_ = true;
        }
    }
    // Attack is bounded by the existing 3ms lookahead. The delayed sample
    // receives a final safety clamp, so slower attack never defeats the ceiling.
    // Default values retain the original detector path bit for bit.
    void setTiming(float attackMs,float releaseMs) noexcept {
        const double attack=skellcore::detail::clamp(attackMs,0.,3.);
        const double release=skellcore::detail::clamp(releaseMs,5.,500.);
        if(attack!=attackMs_) {
            attackMs_=attack;
            attackPole_=attack>0.?skellcore::detail::pole(rate_,attack):0.;
        }
        if(release!=releaseMs_) {releaseMs_=release;releasePole_=skellcore::detail::pole(rate_,release);}
    }
    float appliedGain() const noexcept {return (float)meterGain_;}
    float gainReductionDb() const noexcept {
        return static_cast<float>(-20.*std::log10(std::max(1.e-12,gain_)));
    }
    int latencySamples() const noexcept { return ready_ ? delaySamples_ : 0; }
    bool isPrepared() const noexcept { return ready_; }
    Quality quality() const noexcept { return quality_; }
    std::size_t estimatedWorkingBytes() const noexcept {
        return 2 * (static_cast<std::size_t>(delaySamples_) + 1) * sizeof(float)
             + sizeof(history_) + sizeof(kernel_);
    }
    void processFrame(const float* input, float* output, int channels) noexcept {
        if (!input || !output) return;
        const int count = std::clamp(channels, 0, std::min(channels_, 2));
        if (count == 0) return;
        if (!ready_) {
            for (int c = 0; c < count; ++c)
                output[c] = static_cast<float>(skellcore::detail::finite(input[c]));
            return;
        }
        const int h = historyWrite_;
        double peak = 0.0;
        for (int c = 0; c < count; ++c) {
            const auto channel = static_cast<std::size_t>(c);
            const float sample = static_cast<float>(skellcore::detail::finite(input[c]));
            delay_[channel][write_] = sample;
            auto& hist = history_[channel];
            hist[static_cast<std::size_t>(h)] = sample;
            hist[static_cast<std::size_t>(h + taps)] = sample; // mirrored history
            peak = std::max(peak, std::abs(static_cast<double>(sample)));
            for (int phase = 0; phase < activePhases_; ++phase) {
                const double* coeff = kernel_[static_cast<std::size_t>(phase)].data();
                double value = 0.0;
                for (int j = 0; j < activeTaps_; ++j) {
                    const auto tap = static_cast<std::size_t>(j);
                    value += coeff[tap] * hist[static_cast<std::size_t>(h + taps - j)];
                }
                peak = std::max(peak, std::abs(value));
            }
        }
        historyWrite_ = (h + 1) & (taps - 1);
        const double desired = peak > ceiling_ ? ceiling_ / peak : 1.0;
        if (desired < gain_) { gain_ = attackPole_>0.?attackPole_*gain_+(1.-attackPole_)*desired:desired; hold_ = delaySamples_ + activeTaps_; }
        else if (hold_ > 0) --hold_;
        else gain_ = releasePole_ * gain_ + (1.0 - releasePole_) * desired;
        const std::size_t next = write_ + 1 == delay_[0].size() ? 0 : write_ + 1;
        double applied=gain_;
        if(attackPole_>0.)for(int c=0;c<count;++c) {
            const double value=std::abs(static_cast<double>(delay_[static_cast<std::size_t>(c)][next]));
            if(value>ceiling_)applied=std::min(applied,ceiling_/value);
        }
        meterGain_=applied;
        for(int c=0;c<count;++c)output[c]=static_cast<float>(delay_[static_cast<std::size_t>(c)][next]*applied);
        write_ = next;
    }
private:
    static constexpr int taps = 16;
    double rate_ = 48000.0, ceiling_ = .891250938, gain_ = 1.0, releasePole_ = .999;
    double attackPole_=0.,attackMs_=0.,releaseMs_=70.,meterGain_=1.;
    int channels_ = 2, delaySamples_ = 144, hold_ = 0, historyWrite_ = 0;
    int activeTaps_ = taps, activePhases_ = 4;
    std::size_t write_ = 0;
    bool ready_ = false, ceilingValid_ = false;
    float ceilingDb_ = -1.f;
    Quality quality_ = Quality::Standard;
    std::array<std::vector<float>, 2> delay_;
    std::array<std::array<double, taps * 2>, 2> history_ {};
    std::array<std::array<double, taps>, 4> kernel_ {};
};
}
