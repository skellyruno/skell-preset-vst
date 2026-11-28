# Handoff 2026-11-26 — shared telemetry, presentation mode, visual geometry

Base: `skell-preset-vst` at `dcdf7e9fa3ca82c5bcbd0a50beff19b8e87a366e` (zip supplied by owner).
Environment of this work: no JUCE, no network, no cmake, no Skellay/Maximus/Soundgoodizer sources,
no SkellHardwareUI/WorldLink/AdlibFX header snapshots. **Nothing here was built as a plugin, and no
UI screenshot exists.** Everything below is JUCE-free C++17 verified with g++ 13.

## What changed (all additive except one whitespace fix)
| File | Change |
|---|---|
| `SkellCore/include/SkellCore/Telemetry.h` | NEW. `EffectTelemetry` tap, `SnapshotSlot` seqlock, `BucketRing`, `LevelMetrics`, `FreshnessWatch` |
| `SkellCore/include/SkellCore/PresentationMode.h` | NEW. Clean/Visual + Simple/Advanced state, `tierFor()` telemetry tiers |
| `SkellCore/include/SkellCore/VisualMath.h` | NEW. Log axis, compressor law, gate thresholds, min/max decimation, `measureStaticTransferDb` |
| `tests/test_telemetry.cpp`, `test_presentation_mode.cpp`, `test_visual_math.cpp` | NEW. Registered in `tests/CMakeLists.txt` (appended; `find_package(Threads)` added) |
| `Source/chain/VocalFeelSuiteDSP.h` line 201 | Whitespace only: two statements split onto two lines (GCC `-Wmisleading-indentation`). No behaviour change |

Existing `LiveAudioScope`, `LiveSpectrumCapture`, `StageClock` are untouched and still the live feeds.

## Design facts the UI must respect
* **The rack compressor has no knee.** `Compressor` in `BasicDSP.h` is a hard-knee law on a rectified
  envelope level: `GR = (level - thr) * (1 - 1/ratio)`. `compressorReductionDb(..., kneeDb=0)` equals it
  (tested against the real engine, 12 threshold/ratio combos, DC detector). Do not draw a knee for this engine.
  The x-axis of a transfer graph is the **detector envelope level**, not instantaneous input dB.
* **Gate is Schmitt**: opens at `threshold`, closes below `threshold - hysteresis` after hold (`gateThresholds()`, tested vs real `Gate`).
* **Fields not reported stay "not available"**: `gateMean == -1`, no `kFlagGrValid`/`kFlagGateValid`. Do not draw those as live meters.
* **Correlation / mid / side are windowed (300 ms)**: they take ~1 s to settle (96.4% charged at 1 s, 99.9% at 2 s). Output-side only.
* **Band energy is four band-pass probes** (120 Hz, 700 Hz, 3.5 kHz, 10 kHz, Q=1). A coarse tilt indicator, NOT a spectrum. Use `LiveSpectrumAnalyzer` for a real spectrum.
* **Rings expose N-1 entries once wrapped** (255 of 256) by design; one slot is reserved against the writer.
* Latency: input and output taps are compared sample-aligned as passed in. PDC-delayed effects need aligned buffers.
* Saturation: no theoretical curve was added. `measureStaticTransferDb` measures a static transfer from the actual engine instead.

## Verified (this session, real runs)
Full portable suite, 52 test sources (49 original + 3 new): **48 pass, 1 build-fail (`test_preset_package`, needs JUCE), 3 skipped (2 JUCE, 1 Skellay source)**
under each of `-O2`+UBSan, `-O0`+ASan+UBSan, `-O3`+UBSan, all with `-Wall -Wextra -Werror -pedantic`.
Your original tree under the same flags: 43 pass, 3 build-fail, 3 skipped (the 2 extra failures were the whitespace issue above).
New tests specifically prove:
* metrics: peak/RMS/correlation (+1, -1, 0, noise), mid/side, silence counter, NaN/Inf/mono/null/zero-length input;
* **block-size invariance, bit-exact**, of both history rings for block sizes 1, 7, 32, 127, 480, 512, 4096, 96000;
* zero heap allocations over 2000 active blocks (global `operator new` counter);
* inactive consumer publishes nothing; tap never alters audio (real `Compressor` output bit-identical with tap on/off);
* real `Compressor`: reported GR history max == engine's `lastReductionDb` max; real `Gate`: every 20 ms bucket equals the engine's own mean gain to 1e-5;
* cross-thread: 0 torn snapshots in ~400k reads; negative control (same harness, slot without seqlock) tears 44k-185k times, so the harness can detect failure;
* ring race: reverting the reserved-slot rule produced 148k-582k torn/inconsistent buckets in a lapping stress run; shipped rule produced 0 in 9M reads;
* cost: ~15-20 ns/sample active (about 0.07-0.10% of one core at 48 kHz per tap, single 2.3 GHz Xeon vCPU); inactive returns immediately.

## NOT verified
* No JUCE compile of anything, no plugin, no VST3, no host test, no screenshot. `PluginProcessor`/`PluginEditor` were not modified.
* GCC ThreadSanitizer does not support `atomic_thread_fence`, so a TSan run is not evidence for the seqlock; the torn-read harness above is.
* Cost numbers are from this sandbox CPU, not a user machine or a real host.
* CMake registration was not executed (no cmake); it follows the existing entry pattern.

## Wiring still to do (JUCE side; needs a compiler)
1. `PluginProcessor.h`: own one `skellcore::telemetry::EffectTelemetry` per rack slot (fixed array, no allocation); call `prepare()` from `prepareToPlay`.
2. `PluginProcessor.cpp` `processBlock` (line ~785) / the per-slot processing loop: after each slot, call `tap.process(inL,inR,outL,outR,n,side)`. For Compressor/Gate slots fill `SideInfo` per sample from `lastReductionDb` / `processGain()` (store into a preallocated scratch array, not a vector).
3. `PluginEditor.cpp` timer (`startTimerHz(30)`, line ~322; `timerCallback` ~1481): compute `tierFor(ViewContext)` per slot, call `setConsumerActive(needsAudioTap(tier))`, throttle draws to `updateHz(tier)`, read with `readOutput`/`readSlow`/`readFast`, wrap with `FreshnessWatch` and show a stale/idle state instead of animating old data.
4. Persist `ViewState` as UI state (not an automatable parameter) using `toString`/`...FromString`.
5. Draw: GR history from `readSlow().grDbMax`; waveform from `readFast()` min/max; transfer curve from `buildCompressorCurve(..., kneeDb=0)`; gate window from `gateThresholds()`.

## Still blocked / untouched (from the original task list)
Real UI build + screenshots; Skellay block adapter; Maximus/Soundgoodizer ports (no `.rs` sources supplied);
limiter-gate parity; stereo-separation rack effect; sidechain linking and parallel-send graph in the processor;
standalone integration. `UPDATED_AGENT_PROMPT.md`, `README_FIRST.md`, `02_Original_DSP`, `04_Visual_References` were not in the upload.
To unblock the UI: JUCE 8.0.12 source + the three pinned `skellcore-vst-template` header snapshots (SkellHardwareUI.h, WorldLink.h, AdlibFX.h).

## Possible follow-up (not done)
Skip band probes and correlation when the tier is `Compact` (meters only) to cut tap cost further.
