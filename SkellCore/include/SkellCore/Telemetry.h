#pragma once
// SkellCore shared telemetry foundation.
//
// Purpose: give every effect face / rack row a source of MEASURED signal
// activity (levels, gain reduction, gate state, stereo field, coarse band
// energy, history) that the UI can draw. Nothing here is synthesised: if an
// effect does not report a quantity the field stays at its documented "not
// available" value and the UI must not draw it as a live meter.
//
// Thread contract
//   * Audio thread: prepare()-free hot path = EffectTelemetry::process().
//     No allocation, locks, syscalls, file I/O, GUI calls or FFTs. Cost is
//     O(samples) with a few flops per sample, and exactly zero (early return)
//     while no UI consumer is active.
//   * UI thread: read*() calls, setConsumerActive(). Reads never block the
//     audio thread (seqlock / per-element atomics, bounded retries).
//   * prepare() allocates nothing either (all storage is in-object), but it
//     resets state and must not be called concurrently with process().
//   * process() takes only const sample pointers: telemetry cannot alter audio.
//
// All published quantities are linear amplitude / mean-square unless a field
// name says Db. Latency: input and output taps are compared sample-aligned as
// given; if an effect has latency the caller must pass PDC-aligned buffers or
// accept a visible offset (do not hide it).
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace skellcore::telemetry {

// ---------------------------------------------------------------- utilities
inline float toDb(float linear, float floorDb = -120.f) noexcept {
    if (!(linear > 0.f) || !std::isfinite(linear)) return floorDb;
    return std::max(floorDb, 20.f * std::log10(linear));
}
inline float meanSquareToDb(float ms, float floorDb = -120.f) noexcept {
    if (!(ms > 0.f) || !std::isfinite(ms)) return floorDb;
    return std::max(floorDb, 10.f * std::log10(ms));
}

// ------------------------------------------------------------ seqlock slot
// Single writer, any number of readers. Payload is stored as relaxed 32-bit
// atomics so there is no data race (no UB under TSan) and no lock.
template <class T>
class SnapshotSlot {
    static_assert(std::is_trivially_copyable_v<T>, "POD payload required");
    static_assert(sizeof(T) % 4 == 0, "payload must be a multiple of 4 bytes");
    static constexpr std::size_t words = sizeof(T) / 4;
public:
    SnapshotSlot() noexcept { T zero{}; store(zero); seq_.store(0, std::memory_order_relaxed); }
    // Writer thread only.
    void store(const T& value) noexcept {
        std::uint32_t raw[words];
        std::memcpy(raw, &value, sizeof(T));
        const auto s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);          // odd = writing
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < words; ++i) data_[i].store(raw[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);          // even = stable
    }
    // Any reader. Returns false if a consistent copy could not be obtained
    // within the bounded retry budget (caller keeps its previous value).
    bool load(T& out) const noexcept {
        for (int attempt = 0; attempt < 8; ++attempt) {
            const auto s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1u) continue;
            std::uint32_t raw[words];
            for (std::size_t i = 0; i < words; ++i) raw[i] = data_[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_relaxed) == s1) { std::memcpy(&out, raw, sizeof(T)); return true; }
        }
        return false;
    }
    std::uint32_t sequence() const noexcept { return seq_.load(std::memory_order_acquire); }
private:
    std::array<std::atomic<std::uint32_t>, words> data_{};
    std::atomic<std::uint32_t> seq_{0};
};

// ------------------------------------------------------------ level metrics
enum MetricFlags : std::uint32_t {
    kFlagNonFinite = 1u << 0,   // NaN/Inf seen in this block (treated as 0 for metrics)
    kFlagSilent    = 1u << 1,   // block peak at or below silence floor
    kFlagGrValid   = 1u << 2,   // effect reported gain reduction for this block
    kFlagGateValid = 1u << 3    // effect reported gate state for this block
};

struct LevelMetrics {
    float peakL = 0, peakR = 0;       // linear block peak
    float rmsL = 0, rmsR = 0;         // linear block RMS
    float correlation = 0;            // windowed (~300 ms) L/R correlation, -1..1; 0 if silent/mono-less
    float midMs = 0, sideMs = 0;      // windowed mean-square of (L+R)/2 and (L-R)/2
    float bandMs[4] = {0, 0, 0, 0};   // block mean-square of the output mid signal at four band-pass probes:
                                      // 120 Hz, 700 Hz, 3.5 kHz, 10 kHz (RBJ band-pass, Q=1). Coarse tilt, not an analyzer.
    float grDbMax = 0;                // max gain reduction in block (>=0). Valid only with kFlagGrValid
    float gateMean = -1;              // mean gate gain 0..1, -1 = not applicable
    std::uint32_t silentSamples = 0;  // consecutive samples at/below silence floor (saturating)
    std::uint32_t flags = 0;
    std::uint32_t endSampleLo = 0, endSampleHi = 0; // running sample counter at end of block
    std::uint32_t blockSamples = 0;
    std::uint64_t endSample() const noexcept { return (std::uint64_t(endSampleHi) << 32) | endSampleLo; }
    bool isSilent(double sampleRate, double seconds = 0.5) const noexcept {
        return double(silentSamples) >= sampleRate * seconds;
    }
};
static_assert(std::is_trivially_copyable_v<LevelMetrics>);

// Per-effect optional side information measured by the effect itself.
// Per-sample arrays are preferred (history is then block-size invariant);
// scalars are used when the array pointer is null. Pass nothing for effects
// that have no gain reduction / gate: the fields stay "not available".
struct SideInfo {
    const float* grDbPerSample = nullptr;   // >=0 dB reduction, length = block samples
    const float* gatePerSample = nullptr;   // 0..1 gate gain, length = block samples
    float grDbBlock = 0.f;
    bool  hasGrBlock = false;
    float gateBlock = -1.f;                 // <0 = not available
};

// -------------------------------------------------------------- bucket ring
struct Bucket {
    float inMin = 0, inMax = 0;     // mono-mix extremes over the bucket (input tap)
    float outMin = 0, outMax = 0;   // mono-mix extremes (output tap)
    float grDbMax = 0;              // max reported gain reduction in bucket
    float gateMean = -1;            // mean gate gain, -1 = not reported
};
static_assert(std::is_trivially_copyable_v<Bucket> && sizeof(Bucket) == 24);

template <std::size_t N>
class BucketRing {
public:
    static constexpr std::size_t capacity = N;
    void reset() noexcept { count_.store(0, std::memory_order_release); }
    // Writer thread only.
    void push(const Bucket& b) noexcept {
        const auto c = count_.load(std::memory_order_relaxed);
        auto& e = items_[c % N];
        e.f[0].store(b.inMin, std::memory_order_relaxed);  e.f[1].store(b.inMax, std::memory_order_relaxed);
        e.f[2].store(b.outMin, std::memory_order_relaxed); e.f[3].store(b.outMax, std::memory_order_relaxed);
        e.f[4].store(b.grDbMax, std::memory_order_relaxed); e.f[5].store(b.gateMean, std::memory_order_relaxed);
        count_.store(c + 1, std::memory_order_release);
    }
    std::uint64_t published() const noexcept { return count_.load(std::memory_order_acquire); }
    // Reader: copies up to maxCount most recent buckets, oldest first.
    // Entries the writer lapped during the copy are dropped (never torn).
    // Once the ring has wrapped at most N-1 buckets are readable: the slot the
    // writer may be overwriting right now is never exposed.
    std::size_t copyLatest(Bucket* out, std::size_t maxCount) const noexcept {
        if (!out || maxCount == 0) return 0;
        const auto end = count_.load(std::memory_order_acquire);
        const std::size_t avail = static_cast<std::size_t>(std::min<std::uint64_t>(end, N));
        const std::size_t n = std::min(avail, maxCount);
        const std::uint64_t first = end - n;
        for (std::size_t i = 0; i < n; ++i) {
            const auto& e = items_[(first + i) % N];
            out[i] = Bucket{e.f[0].load(std::memory_order_relaxed), e.f[1].load(std::memory_order_relaxed),
                            e.f[2].load(std::memory_order_relaxed), e.f[3].load(std::memory_order_relaxed),
                            e.f[4].load(std::memory_order_relaxed), e.f[5].load(std::memory_order_relaxed)};
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        const auto end2 = count_.load(std::memory_order_acquire);
        // The writer fills slot (count % N) BEFORE bumping count, so the oldest
        // slot may already be mid-overwrite: reserve one slot.
        const std::uint64_t validFrom = end2 >= N ? end2 - N + 1 : 0;
        std::size_t drop = 0;
        while (drop < n && first + drop < validFrom) ++drop;
        if (drop) { for (std::size_t i = drop; i < n; ++i) out[i - drop] = out[i]; }
        return n - drop;
    }
private:
    struct Entry { std::array<std::atomic<float>, 6> f{}; };
    std::array<Entry, N> items_{};
    std::atomic<std::uint64_t> count_{0};
};

// ----------------------------------------------------------- the effect tap
class EffectTelemetry {
public:
    static constexpr std::size_t kFastBuckets = 256; // waveform-ish, ~5 ms each  => ~1.3 s
    static constexpr std::size_t kSlowBuckets = 256; // history,       ~20 ms each => ~5.1 s
    static constexpr float kSilenceFloor = 3.1622777e-5f; // -90 dBFS

    // Not realtime. Resets all state.
    void prepare(double sampleRate, float fastMs = 5.f, float slowMs = 20.f) noexcept {
        rate_ = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
        fastLen_ = static_cast<std::uint32_t>(std::max(1.0, std::round(rate_ * std::clamp(fastMs, 1.f, 100.f) * 0.001)));
        slowLen_ = static_cast<std::uint32_t>(std::max(1.0, std::round(rate_ * std::clamp(slowMs, 5.f, 500.f) * 0.001)));
        winDecayPerSample_ = std::exp(-1.0 / (rate_ * 0.300));
        static constexpr double centers[4] = {120.0, 700.0, 3500.0, 10000.0};
        for (int k = 0; k < 4; ++k) bp_[k].design(rate_, centers[k], 1.0);
        reset();
    }
    void reset() noexcept {
        sumLL_ = sumRR_ = sumLR_ = sumMM_ = sumSS_ = 0.0;
        for (auto& b : bp_) b.clear();
        silent_ = 0; samplePos_ = 0;
        fast_ = BucketAcc{}; slow_ = BucketAcc{};
        fastRing_.reset(); slowRing_.reset();
        inSlot_.store(LevelMetrics{}); outSlot_.store(LevelMetrics{});
    }

    // ---- UI thread
    // When false the audio callback returns immediately (hidden / closed /
    // non-selected / scrolled-out views must not cost audio-thread time).
    void setConsumerActive(bool on) noexcept { active_.store(on, std::memory_order_release); }
    bool consumerActive() const noexcept { return active_.load(std::memory_order_acquire); }
    bool readInput(LevelMetrics& m) const noexcept { return inSlot_.load(m); }
    bool readOutput(LevelMetrics& m) const noexcept { return outSlot_.load(m); }
    std::uint32_t publishSequence() const noexcept { return outSlot_.sequence(); }
    std::size_t readFast(Bucket* out, std::size_t maxCount) const noexcept { return fastRing_.copyLatest(out, maxCount); }
    std::size_t readSlow(Bucket* out, std::size_t maxCount) const noexcept { return slowRing_.copyLatest(out, maxCount); }
    double sampleRate() const noexcept { return rate_; }
    double fastBucketSeconds() const noexcept { return fastLen_ / rate_; }
    double slowBucketSeconds() const noexcept { return slowLen_ / rate_; }

    // ---- audio thread
    // inR/outR may be null (mono). Buffers are only read.
    void process(const float* inL, const float* inR, const float* outL, const float* outR,
                 int numSamples, const SideInfo& side = {}) noexcept {
        if (!active_.load(std::memory_order_acquire)) return;
        if (numSamples <= 0 || !inL || !outL) return;
        const auto n = static_cast<std::uint32_t>(numSamples);

        double outSumGate = 0.0; float grMax = 0.f; bool gotGate = false, gotGr = false;
        double bandMs[4] = {0, 0, 0, 0};
        float pkLi = 0, pkRi = 0, pkLo = 0, pkRo = 0;
        double ssLi = 0, ssRi = 0, ssLo = 0, ssRo = 0;
        bool nfIn = false, nfOut = false;

        const double decay = winDecayPerSample_;
        double wLL = sumLL_, wRR = sumRR_, wLR = sumLR_, wMM = sumMM_, wSS = sumSS_;

        for (std::uint32_t i = 0; i < n; ++i) {
            const float li = fin(inL[i], nfIn);
            const float ri = inR ? fin(inR[i], nfIn) : li;
            const float lo = fin(outL[i], nfOut);
            const float ro = outR ? fin(outR[i], nfOut) : lo;
            pkLi = std::max(pkLi, std::fabs(li)); pkRi = std::max(pkRi, std::fabs(ri));
            pkLo = std::max(pkLo, std::fabs(lo)); pkRo = std::max(pkRo, std::fabs(ro));
            ssLi += double(li) * li; ssRi += double(ri) * ri;
            ssLo += double(lo) * lo; ssRo += double(ro) * ro;
            // Stereo field and band energy are reported for the OUTPUT (what the user hears).
            const double mid = 0.5 * (double(lo) + ro), sd = 0.5 * (double(lo) - ro);
            wLL = wLL * decay + double(lo) * lo; wRR = wRR * decay + double(ro) * ro; wLR = wLR * decay + double(lo) * ro;
            wMM = wMM * decay + mid * mid;       wSS = wSS * decay + sd * sd;
            for (int k = 0; k < 4; ++k) { const double y = bp_[k].tick(mid); bandMs[k] += y * y; }

            float gr = 0.f, gate = -1.f;
            if (side.grDbPerSample) { gr = std::max(0.f, finOnly(side.grDbPerSample[i])); gotGr = true; }
            else if (side.hasGrBlock) { gr = std::max(0.f, finOnly(side.grDbBlock)); gotGr = true; }
            if (side.gatePerSample) { gate = std::clamp(finOnly(side.gatePerSample[i]), 0.f, 1.f); gotGate = true; }
            else if (side.gateBlock >= 0.f) { gate = std::clamp(finOnly(side.gateBlock), 0.f, 1.f); gotGate = true; }
            grMax = std::max(grMax, gr);
            if (gate >= 0.f) outSumGate += gate;

            const float mi = 0.5f * (li + ri), mo = 0.5f * (lo + ro);
            accumulate(fast_, fastLen_, mi, mo, gr, gate, fastRing_);
            accumulate(slow_, slowLen_, mi, mo, gr, gate, slowRing_);
        }
        sumLL_ = wLL; sumRR_ = wRR; sumLR_ = wLR; sumMM_ = wMM; sumSS_ = wSS;
        for (auto& b : bp_) b.flush(); // keep denormals out of the filter state

        samplePos_ += n;
        const double inv = 1.0 / double(n);
        const bool blockSilent = std::max({pkLi, pkRi, pkLo, pkRo}) <= kSilenceFloor;
        silent_ = blockSilent ? (silent_ > 0xFFFFFFFFu - n ? 0xFFFFFFFFu : silent_ + n) : 0u;

        LevelMetrics a, b;
        a.peakL = pkLi; a.peakR = pkRi;
        a.rmsL = float(std::sqrt(ssLi * inv)); a.rmsR = float(std::sqrt(ssRi * inv));
        a.silentSamples = silent_; a.flags = (nfIn ? kFlagNonFinite : 0u) | (blockSilent ? kFlagSilent : 0u);
        fillPos(a, n);

        b.peakL = pkLo; b.peakR = pkRo;
        b.rmsL = float(std::sqrt(ssLo * inv)); b.rmsR = float(std::sqrt(ssRo * inv));
        const double denom = std::sqrt(wLL * wRR);
        b.correlation = (denom > 1e-18) ? float(std::clamp(wLR / denom, -1.0, 1.0)) : 0.f;
        const double winN = 1.0 / (1.0 - decay);           // effective window length in samples
        b.midMs = float(wMM / winN); b.sideMs = float(wSS / winN);
        for (int k = 0; k < 4; ++k) b.bandMs[k] = float(bandMs[k] * inv);
        b.grDbMax = gotGr ? grMax : 0.f;
        b.gateMean = gotGate ? float(outSumGate * inv) : -1.f;
        b.silentSamples = silent_;
        b.flags = (nfOut ? kFlagNonFinite : 0u) | (blockSilent ? kFlagSilent : 0u)
                | (gotGr ? kFlagGrValid : 0u) | (gotGate ? kFlagGateValid : 0u);
        fillPos(b, n);

        inSlot_.store(a);
        outSlot_.store(b); // output last: publishSequence() tracks it
    }

private:
    struct BucketAcc {
        std::uint32_t count = 0;
        float inMin = 0, inMax = 0, outMin = 0, outMax = 0, gr = 0;
        double gateSum = 0; std::uint32_t gateN = 0; bool first = true;
    };
    static float finOnly(float v) noexcept { return std::isfinite(v) ? v : 0.f; }
    static float fin(float v, bool& nonFinite) noexcept {
        if (std::isfinite(v)) return v;
        nonFinite = true; return 0.f;
    }
    // RBJ band-pass (constant 0 dB peak gain), transposed direct form II, double precision.
    struct BandProbe {
        double b0 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void design(double rate, double hz, double q) noexcept {
            const double f = std::min(hz, rate * 0.45);
            const double w = 6.283185307179586 * f / rate, alpha = std::sin(w) / (2.0 * q), a0 = 1.0 + alpha;
            b0 = alpha / a0; a1 = -2.0 * std::cos(w) / a0; a2 = (1.0 - alpha) / a0;
        }
        void clear() noexcept { z1 = z2 = 0.0; }
        double tick(double x) noexcept {
            const double y = b0 * x + z1;
            z1 = z2 - a1 * y; z2 = -b0 * x - a2 * y;
            return y;
        }
        void flush() noexcept { if (std::fabs(z1) < 1e-30) z1 = 0.0; if (std::fabs(z2) < 1e-30) z2 = 0.0; }
    };
    void fillPos(LevelMetrics& m, std::uint32_t n) const noexcept {
        m.endSampleLo = static_cast<std::uint32_t>(samplePos_ & 0xFFFFFFFFull);
        m.endSampleHi = static_cast<std::uint32_t>(samplePos_ >> 32);
        m.blockSamples = n;
    }
    template <std::size_t N>
    static void accumulate(BucketAcc& a, std::uint32_t len, float mi, float mo, float gr, float gate,
                           BucketRing<N>& ring) noexcept {
        if (a.first) { a.inMin = a.inMax = mi; a.outMin = a.outMax = mo; a.first = false; }
        else {
            a.inMin = std::min(a.inMin, mi); a.inMax = std::max(a.inMax, mi);
            a.outMin = std::min(a.outMin, mo); a.outMax = std::max(a.outMax, mo);
        }
        a.gr = std::max(a.gr, gr);
        if (gate >= 0.f) { a.gateSum += gate; ++a.gateN; }
        if (++a.count >= len) {
            ring.push(Bucket{a.inMin, a.inMax, a.outMin, a.outMax, a.gr,
                             a.gateN ? float(a.gateSum / a.gateN) : -1.f});
            a = BucketAcc{};
        }
    }

    double rate_ = 48000.0, winDecayPerSample_ = 0.9999;
    std::uint32_t fastLen_ = 240, slowLen_ = 960, silent_ = 0;
    std::uint64_t samplePos_ = 0;
    BandProbe bp_[4];
    double sumLL_ = 0, sumRR_ = 0, sumLR_ = 0, sumMM_ = 0, sumSS_ = 0;
    BucketAcc fast_, slow_;
    BucketRing<kFastBuckets> fastRing_;
    BucketRing<kSlowBuckets> slowRing_;
    SnapshotSlot<LevelMetrics> inSlot_, outSlot_;
    std::atomic<bool> active_{false};
};

// ------------------------------------------------- UI-side freshness helper
// "Stale" = the audio thread has stopped publishing (transport stopped,
// effect bypassed by host, plugin suspended). The UI must then fade/hold the
// display and say so instead of animating old data as if it were live.
class FreshnessWatch {
public:
    enum class State { NeverLive, Live, Stale };
    explicit FreshnessWatch(double staleAfterMs = 300.0) noexcept : staleAfterMs_(staleAfterMs) {}
    State update(double nowMs, std::uint32_t publishSequence) noexcept {
        if (!std::isfinite(nowMs)) return state_;
        if (!seen_) { seen_ = true; lastSeq_ = publishSequence; lastChangeMs_ = nowMs; return state_ = State::NeverLive; }
        if (publishSequence != lastSeq_) { lastSeq_ = publishSequence; lastChangeMs_ = nowMs; everLive_ = true; return state_ = State::Live; }
        if (!everLive_) return state_ = State::NeverLive;
        return state_ = (nowMs - lastChangeMs_ > staleAfterMs_) ? State::Stale : State::Live;
    }
    State state() const noexcept { return state_; }
private:
    double staleAfterMs_, lastChangeMs_ = 0; std::uint32_t lastSeq_ = 0;
    bool seen_ = false, everLive_ = false; State state_ = State::NeverLive;
};

} // namespace skellcore::telemetry
