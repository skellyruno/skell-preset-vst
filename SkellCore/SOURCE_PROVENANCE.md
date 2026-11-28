# SkellCore source provenance (2026-11-07)

Original production DSP preserved in SkellCore. Imported source hashes are listed below for reproducible review.

| Upstream component | Original GitHub blob SHA |
| --- | --- |
| SkellyTune RealtimeTuner.hpp | `b3e118fd76533f74b19a4e0c28b1e5f34c403dd2` |
| SkellyTune DelayContract.hpp | `5b0e15ed405f5ab8a2a95779d7e9bc690459382c` |
| SkellyTune CausalCycleSynth.hpp | `ac7877b86414c0f53623f4a021575649aeee6732` |
| SkellyTune FormantEnvelopeGuard.hpp | `cef1e05f61136c5d636e4b82b6e196d7698ebde0` |
| DeEsser PhraseAwareDetector.h | `7337cfcb1ec299128336f7d24fbae4965af6fe2f` |
| Skelly Master MasteringEngine.h | `a2b2264815e07647422864bcdd75a17b63695e50` |

- SkellyTune real-time pitch and related DSP: `skellyruno/skelltune-mini`, `dsp/*.hpp` at commit around v3.3. License: MIT (copied in `licenses/SkellyTune-MIT.txt`). The backend is *not* a new pitch algorithm; it adapts this implementation.
- Phrase-aware de-esser: `skellyruno/skell-deesser-vst`, `Source/dsp/PhraseAwareDetector.h`. The file is copied with only the enclosing namespace changed from global to `skellcore::deesser` to avoid collisions.
- Mastering engine: `skellyruno/skell-master-vst`, `Source/DSP/MasteringEngine.h`. Copied with an enclosing namespace change from `skelly` to `skellcore::master`, avoiding symbol conflicts with SkellyTune.

When mixing dependencies from other authors or plugins, verify their redistribution and commercial-use terms *before import*. Do not assume all GitHub repos are permissively licensed.

Known quality limits:
- The prototype `TruePeakLimiter` is **not** certified true-peak or BS.1770 compliant.
- The production mastering DSP expects a separate oversampler.
- The production pitch source may have variable source age for voiced synthesis; the delay contract describes the fixed dry/transient host-compensated path, not time-varying source grain age.
- The header-only `SkellyTuneBackend` is currently realtime profile; no experimental cycle-synthesis mode is enabled by the adapter.

## Independent hybrid room snapshot (2026-11-28)
Exact SkellCore e828c825e074b47259aba6d339f078ecae79de45, SkellCore/include/SkellCore/experimental/research/convolution/HybridRoom.h, blob efc2c36e691c41fd90104f8193d1d45b660086a5. Original independent Skell DSP, no vendor implementation or IR. Explicit SKELL_IR_REVERB_RESEARCH=ON audition build only; default OFF. No APVTS/module/factory changes. See docs/HYBRID_REVERB_20261128.md.
