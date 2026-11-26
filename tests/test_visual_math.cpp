#include "SkellCore/VisualMath.h"
#include "../Source/chain/BasicDSP.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)
using namespace skellcore::visual;
namespace dsp = skellpreset::dsp;
int main() {
    // ---- log frequency axis
    LogFreqAxis ax;
    SKELL_REQUIRE(std::fabs(ax.toNorm(20.f)) < 1e-6f && std::fabs(ax.toNorm(20000.f) - 1.f) < 1e-6f);
    SKELL_REQUIRE(std::fabs(ax.toNorm(632.455f) - .5f) < 1e-3f);            // geometric mean of 20..20k
    for (float x : {0.f, .1f, .37f, .5f, .93f, 1.f}) SKELL_REQUIRE(std::fabs(ax.toNorm(ax.fromNorm(x)) - x) < 1e-4f);
    SKELL_REQUIRE(ax.toNorm(-5.f) == 0.f && ax.toNorm(std::nanf("")) == 0.f && ax.toNorm(1e9f) == 1.f);
    SKELL_REQUIRE(dbToNorm(-30.f, -60.f, 0.f) == .5f && dbToNorm(std::nanf(""), -60.f, 0.f) == 0.f);

    // ---- compressor law == the REAL engine (DC detector => detector level is exact)
    for (float thr : {-40.f, -24.f, -12.f}) for (float ratio : {1.f, 2.f, 4.f, 10.f}) {
        dsp::Compressor c; c.prepare(48000.); c.set(thr, ratio, .2f, 5.f, 0.f);
        for (float level = -60.f; level <= 0.f; level += 3.f) {
            c.envelope = 0;
            for (int i = 0; i < 20000; ++i) c.gain(dsp::dbToGain(level));   // settle envelope + parameter smoothing
            const float expected = compressorReductionDb(level, thr, ratio, 0.f);
            if (std::fabs(c.lastReductionDb - expected) > .02f) {
                std::fprintf(stderr, "law mismatch thr=%g ratio=%g level=%g engine=%g curve=%g\n", thr, ratio, level, c.lastReductionDb, expected);
                return 1;
            }
        }
    }
    // soft knee properties (only for engines that implement a knee)
    { const float thr = -20.f, ratio = 4.f, knee = 8.f; float prev = -1e9f;
      SKELL_REQUIRE(std::fabs(compressorReductionDb(thr - knee / 2, thr, ratio, knee)) < 1e-6f);
      SKELL_REQUIRE(std::fabs(compressorReductionDb(thr + knee / 2, thr, ratio, knee) - compressorReductionDb(thr + knee / 2, thr, ratio, 0.f)) < 1e-5f);
      for (float in = -60.f; in <= 0.f; in += .25f) {
          const float out = compressorOutDb(in, thr, ratio, 0.f, knee);
          SKELL_REQUIRE(out >= prev - 1e-6f); prev = out;                      // output never decreases with input
          SKELL_REQUIRE(compressorReductionDb(in, thr, 1.f, knee) == 0.f);         // ratio 1 never reduces, with or without knee
      } }
    SKELL_REQUIRE(compressorReductionDb(0.f, -20.f, std::nanf(""), 0.f) == 0.f);   // NaN ratio -> ratio 1 -> no reduction
    { float curve[5]; buildCompressorCurve(curve, 5, -60.f, 0.f, -30.f, 4.f, 2.f);
      SKELL_REQUIRE(std::fabs(curve[0] - (-60.f + 2.f)) < 1e-5f);                  // below threshold: unity + makeup
      SKELL_REQUIRE(std::fabs(curve[4] - (-22.5f + 2.f)) < 1e-4f);                 // 0 dB in: reduction 30*.75=22.5, out -22.5, +2 makeup
      buildCompressorCurve(nullptr, 5, 0, 0, 0, 1); buildCompressorCurve(curve, 0, 0, 0, 0, 1); }

    // ---- gate thresholds == the REAL engine's Schmitt behaviour
    { dsp::Gate g; g.prepare(48000.); g.threshold = -40.f; g.hysteresisDb = 6.f; g.holdMs = 0.f; g.attackMs = 1.f;
      const auto th = gateThresholds(g.threshold, g.hysteresisDb);
      SKELL_REQUIRE(th.openDb == -40.f && th.closeDb == -46.f);
      auto run = [&](float db, int n) { for (int i = 0; i < n; ++i) g.processGain(dsp::dbToGain(db)); };
      run(th.openDb + 10.f, 4000);  SKELL_REQUIRE(g.open);                          // above open threshold -> open
      run((th.openDb + th.closeDb) / 2.f, 20000); SKELL_REQUIRE(g.open);            // inside hysteresis band -> stays open
      run(th.closeDb - 2.f, 20000); SKELL_REQUIRE(!g.open);                         // below close threshold -> closed
      run((th.openDb + th.closeDb) / 2.f, 20000); SKELL_REQUIRE(!g.open);           // inside band again -> stays closed
      run(th.openDb + 1.f, 4000);   SKELL_REQUIRE(g.open);
      SKELL_REQUIRE(gateThresholds(-40.f, 99.f).closeDb == -52.f && gateThresholds(-40.f, -3.f).closeDb == -40.f); }

    // ---- decimation
    { std::vector<float> ramp(1000); for (int i = 0; i < 1000; ++i) ramp[(size_t)i] = float(i);
      float mn[10], mx[10]; decimateMinMax(ramp.data(), ramp.size(), mn, mx, 10);
      for (int c = 0; c < 10; ++c) { SKELL_REQUIRE(mn[c] == float(c * 100) && mx[c] == float(c * 100 + 99)); }
      float a[8], b[8]; decimateMinMax(ramp.data(), 3, a, b, 8);                      // more columns than samples
      for (int c = 0; c < 8; ++c) SKELL_REQUIRE(std::isfinite(a[c]) && std::isfinite(b[c]) && a[c] <= b[c]);
      float nan[4] = {1.f, std::numeric_limits<float>::quiet_NaN(), 2.f, std::numeric_limits<float>::infinity()};
      decimateMinMax(nan, 4, a, b, 4); for (int c = 0; c < 4; ++c) SKELL_REQUIRE(std::isfinite(a[c]) && std::isfinite(b[c]));
      decimateMinMax(nullptr, 0, a, b, 8); for (int c = 0; c < 8; ++c) SKELL_REQUIRE(a[c] == 0.f && b[c] == 0.f);
      decimateMinMax(ramp.data(), 10, nullptr, nullptr, 4); }

    // ---- measured transfer from real/other engines
    SKELL_REQUIRE(std::fabs(measureStaticTransferDb([](float x) { return x; }, -12.f, 48000.) + 12.f) < .05f);
    SKELL_REQUIRE(std::fabs(measureStaticTransferDb([](float x) { return x * .5f; }, -12.f, 48000.) + 18.02f) < .05f);
    { auto clip = [](float x) { return std::clamp(x, -.5f, .5f); };
      SKELL_REQUIRE(std::fabs(measureStaticTransferDb(clip, -20.f, 48000.) + 20.f) < .05f);   // linear region
      const float hot = measureStaticTransferDb(clip, 0.f, 48000.);
      SKELL_REQUIRE(hot < -1.f && hot > -6.f); }                                              // compressed but not silent
    SKELL_REQUIRE(measureStaticTransferDb([](float) { return 0.f; }, -12.f, 48000.) == -240.f);
    { dsp::Compressor c; c.prepare(48000.); c.set(-30.f, 4.f, 1.f, 100.f, 0.f);
      auto engine = [&](float x) { return x * c.gain(x); };
      const float o50 = measureStaticTransferDb(engine, -50.f, 48000.);
      const float o40 = measureStaticTransferDb(engine, -40.f, 48000.);
      const float o20 = measureStaticTransferDb(engine, -20.f, 48000.);
      const float o10 = measureStaticTransferDb(engine, -10.f, 48000.);
      SKELL_REQUIRE(std::fabs((o40 - o50) - 10.f) < .3f);        // below threshold: unity slope
      SKELL_REQUIRE((o10 - o20) > 0.f && (o10 - o20) < 6.f);     // above threshold: compressing, far below unity slope (10 dB)
    }
    std::puts("test_visual_math OK");
    return 0;
}
