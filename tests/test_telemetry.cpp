#include "SkellCore/Telemetry.h"
#include "SkellCore/VisualMath.h"
#include "../Source/chain/BasicDSP.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <vector>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)

// ---- allocation counter: proves the audio-thread path never touches the heap
static std::atomic<long> g_allocs{0};
static std::atomic<bool> g_count{false};
void* operator new(std::size_t n) { if (g_count.load()) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace skellcore::telemetry;
namespace dsp = skellpreset::dsp;
constexpr double SR = 48000.0, TWO_PI = 6.283185307179586;

struct Sig { std::vector<float> l, r; };
static Sig sine(double hz, double amp, int n, bool antiPhase = false, bool rightSilent = false) {
    Sig s; s.l.resize((size_t)n); s.r.resize((size_t)n);
    for (int i = 0; i < n; ++i) {
        const float v = float(amp * std::sin(TWO_PI * hz * i / SR));
        s.l[(size_t)i] = v; s.r[(size_t)i] = rightSilent ? 0.f : (antiPhase ? -v : v);
    }
    return s;
}
// Runs a signal through the tap in blocks of `block` (input == output, pass-through).
static void feed(EffectTelemetry& t, const Sig& s, int block, const SideInfo* side = nullptr) {
    const int n = (int)s.l.size();
    for (int i = 0; i < n; i += block) {
        const int c = std::min(block, n - i);
        t.process(s.l.data() + i, s.r.data() + i, s.l.data() + i, s.r.data() + i, c, side ? *side : SideInfo{});
    }
}
static EffectTelemetry* make() { auto* t = new EffectTelemetry(); t->prepare(SR); t->setConsumerActive(true); return t; }
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// ---- helpers for the cross-thread test
static LevelMetrics allSame(std::uint32_t k) {
    LevelMetrics m; const std::uint32_t v = k & 0xFFFFFF; const float f = float(v);
    m.peakL = m.peakR = m.rmsL = m.rmsR = m.correlation = m.midMs = m.sideMs = f;
    m.bandMs[0] = m.bandMs[1] = m.bandMs[2] = m.bandMs[3] = f; m.grDbMax = m.gateMean = f;
    m.flags = v; m.silentSamples = v; m.endSampleLo = v; m.endSampleHi = v; m.blockSamples = v; return m;
}
static bool allSameConsistent(const LevelMetrics& m) {
    const float f = float(m.flags);
    return m.peakL == f && m.peakR == f && m.rmsL == f && m.rmsR == f && m.correlation == f && m.midMs == f && m.sideMs == f
        && m.bandMs[0] == f && m.bandMs[1] == f && m.bandMs[2] == f && m.bandMs[3] == f && m.grDbMax == f && m.gateMean == f
        && m.silentSamples == m.flags && m.endSampleLo == m.flags && m.endSampleHi == m.flags && m.blockSamples == m.flags;
}
// Deliberately WRONG slot (relaxed words, no sequence counter): negative control only.
struct NaiveSlot {
    static constexpr std::size_t words = sizeof(LevelMetrics) / 4;
    std::atomic<std::uint32_t> w[words]; std::atomic<bool> written{false};
    NaiveSlot() { for (auto& x : w) x.store(0); }
    void store(const LevelMetrics& m) { std::uint32_t raw[words]; std::memcpy(raw, &m, sizeof m); for (std::size_t i = 0; i < words; ++i) w[i].store(raw[i], std::memory_order_relaxed); written.store(true); }
    LevelMetrics load() const { std::uint32_t raw[words]; for (std::size_t i = 0; i < words; ++i) raw[i] = w[i].load(std::memory_order_relaxed); LevelMetrics m; std::memcpy(&m, raw, sizeof m); return m; }
};

int main() {
    // ============ 1. metric correctness
    { auto* t = make(); feed(*t, sine(997, .5, 48000), 512);
      { LevelMetrics w; SKELL_REQUIRE(t->readOutput(w));                    // 1 s in: the 300 ms window is only 96.4% charged
        SKELL_REQUIRE(near(w.midMs, .125f * (1.f - std::exp(-1.f / .3f)), 2e-4f)); }
      feed(*t, sine(997, .5, 48000), 512);                                  // 2 s in: steady state
      LevelMetrics in, out; SKELL_REQUIRE(t->readInput(in) && t->readOutput(out));
      SKELL_REQUIRE(near(out.peakL, .5f, .002f) && near(out.peakR, .5f, .002f) && near(in.peakL, .5f, .002f));
      SKELL_REQUIRE(near(out.rmsL, .35355f, .004f));
      SKELL_REQUIRE(out.correlation > .999f);
      SKELL_REQUIRE(out.sideMs < 1e-9f && near(out.midMs, .125f, .001f));
      SKELL_REQUIRE(out.blockSamples == 48000u % 512u || out.blockSamples == 512u);
      SKELL_REQUIRE(out.endSample() == 96000u);
      SKELL_REQUIRE(!(out.flags & kFlagGrValid) && !(out.flags & kFlagGateValid) && out.gateMean == -1.f); // nothing reported => not available
      delete t; }
    { auto* t = make(); feed(*t, sine(997, .5, 96000, true), 512);          // R = -L, 2 s
      LevelMetrics o; SKELL_REQUIRE(t->readOutput(o));
      SKELL_REQUIRE(o.correlation < -.999f && o.midMs < 1e-9f && near(o.sideMs, .125f, .001f)); delete t; }
    { auto* t = make(); feed(*t, sine(997, .5, 96000, false, true), 512);   // right silent, 2 s
      LevelMetrics o; SKELL_REQUIRE(t->readOutput(o));
      SKELL_REQUIRE(o.correlation == 0.f && o.peakR == 0.f && near(o.midMs, o.sideMs, 1e-6f)); delete t; }
    { auto* t = make(); Sig s; s.l.resize(48000); s.r.resize(48000); unsigned a = 12345u, b = 987654u;
      for (size_t i = 0; i < 48000; ++i) { a = a * 1664525u + 1013904223u; b = b * 22695477u + 1u;
          s.l[i] = (float(a >> 8) / 8388608.f - 1.f) * .3f; s.r[i] = (float(b >> 8) / 8388608.f - 1.f) * .3f; }
      feed(*t, s, 256); LevelMetrics o; SKELL_REQUIRE(t->readOutput(o)); SKELL_REQUIRE(std::fabs(o.correlation) < .1f); delete t; }

    // ============ 2. band probes: argmax lands on the probe nearest the tone
    { const double tones[4] = {120, 700, 3500, 10000};
      for (int k = 0; k < 4; ++k) { auto* t = make(); feed(*t, sine(tones[k], .4, 24000), 480);
          LevelMetrics o; SKELL_REQUIRE(t->readOutput(o)); int best = 0;
          for (int j = 1; j < 4; ++j) if (o.bandMs[j] > o.bandMs[best]) best = j;
          SKELL_REQUIRE(best == k); SKELL_REQUIRE(o.bandMs[k] > 3.f * o.bandMs[(k + 1) % 4]);
          SKELL_REQUIRE(o.bandMs[k] > .5f * (.4f * .4f / 2.f) / 2.f);   // probe passes roughly the mid energy of the tone
          delete t; } }

    // ============ 3. silence + hostile input
    { auto* t = make(); Sig z; z.l.assign(48000, 0.f); z.r.assign(48000, 0.f); feed(*t, z, 480);
      LevelMetrics o; SKELL_REQUIRE(t->readOutput(o));
      SKELL_REQUIRE(o.isSilent(SR) && o.silentSamples == 48000u && (o.flags & kFlagSilent));
      SKELL_REQUIRE(o.correlation == 0.f && o.midMs == 0.f && o.bandMs[0] == 0.f);
      feed(*t, sine(997, .5, 480), 480); SKELL_REQUIRE(t->readOutput(o) && o.silentSamples == 0u && !o.isSilent(SR)); delete t; }
    { auto* t = make(); Sig s = sine(997, .5, 1000); const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
      s.l[10] = nan; s.r[20] = inf; s.l[30] = -inf;
      feed(*t, s, 1000); LevelMetrics in, o; SKELL_REQUIRE(t->readInput(in) && t->readOutput(o));
      SKELL_REQUIRE((o.flags & kFlagNonFinite) && (in.flags & kFlagNonFinite));
      for (float v : {o.peakL, o.peakR, o.rmsL, o.rmsR, o.correlation, o.midMs, o.sideMs, o.bandMs[0], o.bandMs[1], o.bandMs[2], o.bandMs[3]}) SKELL_REQUIRE(std::isfinite(v));
      SideInfo bad; bad.hasGrBlock = true; bad.grDbBlock = nan; bad.gateBlock = inf; t->process(s.l.data(), s.r.data(), s.l.data(), s.r.data(), 100, bad);
      SKELL_REQUIRE(t->readOutput(o) && std::isfinite(o.grDbMax) && std::isfinite(o.gateMean));
      t->process(nullptr, nullptr, nullptr, nullptr, 100); t->process(s.l.data(), nullptr, s.l.data(), nullptr, 0);   // null / empty must be harmless
      t->process(s.l.data(), nullptr, s.l.data(), nullptr, 64);                                                       // mono path
      delete t; }

    // ============ 4. inactive consumer = no work, no publication
    { EffectTelemetry t; t.prepare(SR); SKELL_REQUIRE(!t.consumerActive());
      const auto base = t.publishSequence();                                   // prepare() publishes one zeroed snapshot
      feed(t, sine(997, .5, 9600), 512); SKELL_REQUIRE(t.publishSequence() == base);
      Bucket b[4]; SKELL_REQUIRE(t.readFast(b, 4) == 0 && t.readSlow(b, 4) == 0);
      LevelMetrics o; SKELL_REQUIRE(t.readOutput(o) && o.endSample() == 0 && o.peakL == 0.f);
      t.setConsumerActive(true); feed(t, sine(997, .5, 9600), 512); SKELL_REQUIRE(t.publishSequence() > base && t.readFast(b, 4) == 4);
      const auto seq = t.publishSequence(); t.setConsumerActive(false); feed(t, sine(997, .5, 9600), 512); SKELL_REQUIRE(t.publishSequence() == seq); }

    // ============ 5. block-size invariance of the time-based history (bit exact)
    { const int N = 96000; Sig s = sine(220, .6, N); for (int i = 0; i < N; ++i) { const float env = (i / 8000) % 2 ? .15f : 1.f; s.l[(size_t)i] *= env; s.r[(size_t)i] *= env; }
      std::vector<float> gr((size_t)N), gate((size_t)N);
      for (int i = 0; i < N; ++i) { gr[(size_t)i] = std::fabs(s.l[(size_t)i]) * 12.f; gate[(size_t)i] = (i / 8000) % 2 ? 0.f : 1.f; }
      std::vector<Bucket> refFast, refSlow; std::uint64_t refEnd = 0;
      for (int block : {1, 7, 32, 127, 480, 512, 4096, 96000}) {
          auto* t = make();
          for (int i = 0; i < N; i += block) { const int c = std::min(block, N - i); SideInfo si; si.grDbPerSample = gr.data() + i; si.gatePerSample = gate.data() + i;
              t->process(s.l.data() + i, s.r.data() + i, s.l.data() + i, s.r.data() + i, c, si); }
          std::vector<Bucket> f(256), sl(256); const auto nf = t->readFast(f.data(), 256), ns = t->readSlow(sl.data(), 256); f.resize(nf); sl.resize(ns);
          LevelMetrics o; SKELL_REQUIRE(t->readOutput(o)); 
          // fast ring has wrapped (400 buckets written) so N-1 = 255 are readable; slow ring holds 100
          if (block == 1) { refFast = f; refSlow = sl; refEnd = o.endSample(); SKELL_REQUIRE(nf == 255 && ns == 100); }
          else { SKELL_REQUIRE(o.endSample() == refEnd && f.size() == refFast.size() && sl.size() == refSlow.size());
                 SKELL_REQUIRE(std::memcmp(f.data(), refFast.data(), f.size() * sizeof(Bucket)) == 0);
                 SKELL_REQUIRE(std::memcmp(sl.data(), refSlow.data(), sl.size() * sizeof(Bucket)) == 0); }
          delete t; }
      SKELL_REQUIRE(refEnd == (std::uint64_t)N);
      bool sawGate1 = false, sawGate0 = false; for (auto& b : refSlow) { if (b.gateMean > .99f) sawGate1 = true; if (b.gateMean < .01f && b.gateMean >= 0.f) sawGate0 = true; }
      SKELL_REQUIRE(sawGate1 && sawGate0); }

    // ============ 6. REAL Compressor: reported GR == engine's GR, audio untouched by tap
    { auto runComp = [&](bool tapOn, std::vector<float>& outAudio, std::vector<float>& grOut, EffectTelemetry* t) {
          dsp::Compressor c; c.prepare(SR); c.set(-24.f, 4.f, 2.f, 120.f, 0.f);
          Sig s = sine(997, .5, 48000); outAudio.resize(48000); grOut.resize(48000);
          std::vector<float> ol(512), orr(512);
          for (int i = 0; i < 48000; i += 512) { const int n = std::min(512, 48000 - i);
              for (int k = 0; k < n; ++k) { const float x = s.l[(size_t)(i + k)]; const float g = c.gain(x);
                  ol[(size_t)k] = x * g; orr[(size_t)k] = s.r[(size_t)(i + k)] * g; grOut[(size_t)(i + k)] = c.lastReductionDb; outAudio[(size_t)(i + k)] = ol[(size_t)k]; }
              if (tapOn) { SideInfo si; si.grDbPerSample = grOut.data() + i; t->process(s.l.data() + i, s.r.data() + i, ol.data(), orr.data(), n, si); } } };
      std::vector<float> a0, g0, a1, g1; EffectTelemetry* t = make();
      runComp(false, a0, g0, nullptr); runComp(true, a1, g1, t);
      SKELL_REQUIRE(a0.size() == a1.size() && std::memcmp(a0.data(), a1.data(), a0.size() * sizeof(float)) == 0);   // bit-identical audio
      std::vector<Bucket> h(256); const auto n = t->readSlow(h.data(), 256); SKELL_REQUIRE(n == 50);
      float histMax = 0; for (size_t i = 0; i < n; ++i) histMax = std::max(histMax, h[i].grDbMax);
      float engMax = 0; for (float v : g1) engMax = std::max(engMax, v);
      SKELL_REQUIRE(histMax == engMax && engMax > 3.f);                                                           // real, non-trivial reduction
      LevelMetrics o; SKELL_REQUIRE(t->readOutput(o) && (o.flags & kFlagGrValid) && o.grDbMax > 3.f);
      SKELL_REQUIRE(o.rmsL < .35355f * .9f);                                                                      // output meter shows the actual level drop
      // bucket min/max of output must be smaller than input over the settled tail
      SKELL_REQUIRE(h[n - 1].outMax < h[n - 1].inMax * .9f);
      delete t; }

    // ============ 7. REAL Gate: reported gate gain tracks the engine
    { dsp::Gate g; g.prepare(SR); g.threshold = -45.f; g.holdMs = 20.f; g.attackMs = 2.f; g.release = 60.f;
      const int N = 48000; Sig s = sine(997, .3, N); for (int i = 12000; i < 36000; ++i) { s.l[(size_t)i] = 0.f; s.r[(size_t)i] = 0.f; }
      std::vector<float> gain((size_t)N), ol((size_t)N), orr((size_t)N);
      for (int i = 0; i < N; ++i) { gain[(size_t)i] = g.processGain(s.l[(size_t)i]); ol[(size_t)i] = s.l[(size_t)i] * gain[(size_t)i]; orr[(size_t)i] = s.r[(size_t)i] * gain[(size_t)i]; }
      auto* t = make(); for (int i = 0; i < N; i += 480) { SideInfo si; si.gatePerSample = gain.data() + i; t->process(s.l.data() + i, s.r.data() + i, ol.data() + i, orr.data() + i, 480, si); }
      std::vector<Bucket> h(256); const auto n = t->readSlow(h.data(), 256); SKELL_REQUIRE(n == 50);
      // timeline: burst 0-250 ms | silence 250-750 ms | burst 750-1000 ms
      // Exact: every bucket must equal the mean of the ENGINE's own gain over those samples.
      for (size_t b = 0; b < n; ++b) { double m = 0; for (size_t i = b * 960; i < (b + 1) * 960; ++i) m += gain[i];
          if (std::fabs(h[b].gateMean - float(m / 960.0)) > 1e-5f) { std::fprintf(stderr, "bucket %zu telemetry=%g engine=%g\n", b, h[b].gateMean, m / 960.0); return 1; } }
      SKELL_REQUIRE(h[5].gateMean > .9f);          // 100-120 ms: burst, gate open
      SKELL_REQUIRE(h[14].gateMean < .95f && h[14].gateMean > h[20].gateMean && h[20].gateMean > h[25].gateMean);  // genuine release tail decays monotonically
      SKELL_REQUIRE(h[30].gateMean >= 0.f && h[30].gateMean < .01f);   // 600 ms: closed, and reported (not n/a)
      SKELL_REQUIRE(h[45].gateMean > .9f);         // 900 ms: second burst, gate reopened
      LevelMetrics o; SKELL_REQUIRE(t->readOutput(o) && (o.flags & kFlagGateValid) && !(o.flags & kFlagGrValid)); delete t; }

    // ============ 8. audio path never allocates
    { auto* t = make(); Sig s = sine(440, .5, 512); std::vector<float> gr(512, 3.f), ga(512, .5f); SideInfo si; si.grDbPerSample = gr.data(); si.gatePerSample = ga.data();
      t->process(s.l.data(), s.r.data(), s.l.data(), s.r.data(), 512, si);               // warm
      g_allocs = 0; g_count = true;
      for (int i = 0; i < 2000; ++i) t->process(s.l.data(), s.r.data(), s.l.data(), s.r.data(), 512, si);
      t->setConsumerActive(false); t->process(s.l.data(), s.r.data(), s.l.data(), s.r.data(), 512);
      g_count = false; SKELL_REQUIRE(g_allocs.load() == 0); delete t; }

    // ============ 9. sample-rate / parameter extremes
    for (double sr : {std::numeric_limits<double>::quiet_NaN(), 1.0, 8000.0, 44100.0, 96000.0, 192000.0, 1e12}) {
        EffectTelemetry t; t.prepare(sr); t.setConsumerActive(true); Sig s = sine(1000, .5, 4096);
        for (int i = 0; i < 4096; i += 256) t.process(s.l.data() + i, s.r.data() + i, s.l.data() + i, s.r.data() + i, 256);
        LevelMetrics o; SKELL_REQUIRE(t.readOutput(o) && std::isfinite(o.correlation) && std::isfinite(o.bandMs[3]));
        SKELL_REQUIRE(t.sampleRate() >= 8000.0 && t.sampleRate() <= 384000.0 && t.fastBucketSeconds() > 0 && t.slowBucketSeconds() > t.fastBucketSeconds() * .99);
    }
    { EffectTelemetry t; t.prepare(SR, -5.f, 1e9f); SKELL_REQUIRE(t.fastBucketSeconds() >= .001 - 1e-9 && t.slowBucketSeconds() <= .5 + 1e-9); }

    // ============ 10. cross-thread: no torn snapshots, no torn / non-consecutive buckets
    { SnapshotSlot<LevelMetrics> slot; BucketRing<64> ring; std::atomic<bool> stop{false};
      std::thread writer([&] { for (std::uint32_t k = 1; !stop.load(std::memory_order_relaxed); ++k) {
              slot.store(allSame(k)); const float b = float(k); ring.push(Bucket{b, b, b, b, b, b}); } });
      while (slot.sequence() == 0) std::this_thread::yield();      // reader must not run before the first publish (default state is not "all equal")
      long torn = 0, okReads = 0, tornBuckets = 0, gaps = 0, bucketReads = 0; Bucket buf[64];
      for (int iter = 0; iter < 300000; ++iter) {
          LevelMetrics m; if (slot.load(m)) { ++okReads; if (!allSameConsistent(m)) ++torn; }
          if (iter % 8 == 0) { const auto n = ring.copyLatest(buf, 64); ++bucketReads;
              for (size_t i = 0; i < n; ++i) { const float v = buf[i].inMin; if (!(buf[i].inMax == v && buf[i].outMin == v && buf[i].outMax == v && buf[i].grDbMax == v && buf[i].gateMean == v)) ++tornBuckets;
                  if (i && buf[i].inMin != buf[i - 1].inMin + 1.f) ++gaps; } } }
      stop = true; writer.join();
      std::fprintf(stderr, "  [cross-thread] snapshot reads ok=%ld torn=%ld | bucket reads=%ld torn=%ld nonconsecutive=%ld\n", okReads, torn, bucketReads, tornBuckets, gaps);
      SKELL_REQUIRE(torn == 0 && tornBuckets == 0 && gaps == 0); }
    // The racing reader is deliberately bounded and may miss nearly every
    // publication on a busy runner. Separately guarantee 2048 distinct
    // cross-thread publications are observed, without weakening integrity.
    { SnapshotSlot<LevelMetrics> slot; BucketRing<64> ring;
      std::atomic<unsigned> ready{0},ack{0};
      std::thread writer([&]{for(unsigned k=1;k<=2048;++k){
          slot.store(allSame(k));const float b=float(k);ring.push(Bucket{b,b,b,b,b,b});
          ready.store(k,std::memory_order_release);
          while(ack.load(std::memory_order_acquire)!=k)std::this_thread::yield();
      }});
      bool valid=true;Bucket buf[64];
      for(unsigned k=1;k<=2048;++k){
          while(ready.load(std::memory_order_acquire)!=k)std::this_thread::yield();
          LevelMetrics m;const auto n=ring.copyLatest(buf,64);
          valid=valid && slot.load(m) && allSameConsistent(m) && slot.sequence()==2*k && n>0;
          if(n)valid=valid && buf[n-1].inMin==float(k) && buf[n-1].gateMean==float(k);
          ack.store(k,std::memory_order_release);
      }
      writer.join();SKELL_REQUIRE(valid);
      std::puts("  [scheduled handoff] 2048 distinct cross-thread snapshots and bucket publications verified"); }
    // Negative control: identical harness on a slot WITHOUT the sequence counter. It should tear,
    // which shows the harness above is capable of catching a broken implementation. Informational only
    // (tearing is timing dependent and must not make the suite flaky on a single-core CI machine).
    { NaiveSlot naive; std::atomic<bool> stop{false}; std::thread writer([&] { for (std::uint32_t k = 1; !stop.load(std::memory_order_relaxed); ++k) naive.store(allSame(k)); });
      while (!naive.written.load()) std::this_thread::yield();
      long torn = 0; for (int iter = 0; iter < 300000; ++iter) { LevelMetrics m = naive.load(); if (!allSameConsistent(m)) ++torn; }
      stop = true; writer.join();
      std::fprintf(stderr, "  [negative control] naive slot without seqlock: torn reads=%ld of 300000 (expected > 0)\n", torn); }

    // ============ 11. freshness / stale detection
    { FreshnessWatch w(300.0); SKELL_REQUIRE(w.state() == FreshnessWatch::State::NeverLive);
      SKELL_REQUIRE(w.update(0, 0) == FreshnessWatch::State::NeverLive);
      SKELL_REQUIRE(w.update(1000, 0) == FreshnessWatch::State::NeverLive);       // never published: not "stale", not "live"
      SKELL_REQUIRE(w.update(1010, 2) == FreshnessWatch::State::Live);
      SKELL_REQUIRE(w.update(1200, 2) == FreshnessWatch::State::Live);
      SKELL_REQUIRE(w.update(1400, 2) == FreshnessWatch::State::Stale);           // 390 ms without a publish
      SKELL_REQUIRE(w.update(1405, 4) == FreshnessWatch::State::Live);            // resumes
      SKELL_REQUIRE(w.update(std::nan(""), 4) == FreshnessWatch::State::Live); }  // bad clock ignored
    std::puts("test_telemetry OK");
    return 0;
}
