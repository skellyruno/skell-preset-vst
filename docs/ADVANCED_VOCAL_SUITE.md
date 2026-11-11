# Skell Preset — advanced vocal suite integration and release gates

**Status:** Source implementation in PR #5 (feature branch), not a published
release. This branch must not be marketed as a production-ready FL Studio VST3
until its latest Windows build, headless UI screenshots, native preset tests,
and real host tests pass.

## Source and design choices

- User-authorized FLAMEEWRLD Studio Vocal Chain (JUCE C++):
  serial FET/opto dynamics, reactive resonance control, vocal-processing
  ergonomics. Implemented as original portable Skell C++ DSP with bounded
  parameters and stereo-linked detection; no unverifiable 1:1 commercial
  hardware emulation claims.
- User-authorized Auralis Voice Changer (Rust, MIT): eight-band spectral EQ
  concept and 1024-point FFT phase-vocoder pitch/formant. DSP has been adapted
  to self-contained C++17 engines, not a Rust/Tauri runtime. Preserve the
  Auralis MIT notice in docs/third_party.
- Previous PR #4 provides Vocal Rider, Ducking Ping-Pong and Vocal Vibrato.
  Previous PR #3 provides the midnight-black rack UI, real editor screenshot
  suite, custom preset artwork and animated vector module visualizers.

## New native rack effects

### 8-Band EQ (ID 26)
- Eight independent bell bands in log-spaced regions, each with automatable
  frequency, gain (-15 to +15 dB) and independent bandwidth (Q 0.4–8).
- Band frequency and gain reuse the existing 16 generic slot parameters.
  Q values are appended as 8 extra host parameters per slot.
- Clicking any of the eight nodes selects its band and rebinds the main four
  hardware knobs to that band pair; an additional Q slider controls the
  selected band's actual coefficient. DSP and editor draw the same eight
  cascaded filters, with parameter coefficient interpolation.
- The effect is more capable than the legacy one-bell EQ but retains the
  old EQ as a distinct option for old projects. Q is not invented from a graph.
- Potential ringing / gain interactions require listening tests under stacked
  extreme boosts and cuts.

### FET + Opto (ID 27)
- Stereo-linked, fast peak compressor into slower RMS leveling plus a parallel
  saturation control. Threshold / ratio / FET speed / parallel.
- Not a licensed recreation of any named compressor. No assumed auto-makeup.

### Harshness Tamer (ID 28)
- Dynamic peak reduction driven by presence-band excess and threshold.
  Frequency / threshold / maximum cut / release.
- Not a multiband neural Soothe clone. Compare with de-esser on real vocals.

### Adaptive Noise Control (ID 29)
- Stereo-linked adaptive noise floor attenuation with threshold, strength,
  hold and release. Zero-added PDC; no model inference.
- This is **not neural noise suppression**. The source project's RNNoise
  path uses an external Rust `nnnoiseless` dependency and 48 kHz, 480-sample
  frames. Before adding neural mode require verified licensed model/runtime,
  48 kHz resampling, latency reporting, CPU and stereo testing.

### Studio Pitch + Formant (ID 30, experimental)
- Stereo 1024-point Hann FFT phase vocoder, hop 256, with independent
  pitch and formant semitone values and adjustable ranges.
- **768-sample host PDC** (16 ms at 48 kHz) plus aligned rack wet/dry
  and byte-exact delayed neutral passthrough. This is a creative studio effect;
  **not** an autotune system, realtime key tracker or verified transparent
  formant shifter. Tone, transients and stereo coherence require listening
  validation at extremes.
- Not enabled in legacy factory presets or the low-latency Live Lead chain.
  The new Formant Sketch factory preset is explicitly a studio sound.

## Factory program compatibility

- Existing IDs 0–36 are unchanged. New IDs:
  37 Eight-Band Air, 38 Opto Punch, 39 Smooth Edge,
  40 Noise Guard, 41 Formant Sketch.
- Existing rack type IDs 0–25 are unchanged, and the old 23-choice
  `sN_type` automation range is untouched.
- Prior `sN_fxExt` range (None/Rider/Duck Echo/Vibrato) stays unchanged.
  The 26–29 effects use a separately appended `sN_fxExt2` choice.
  Studio pitch uses separately appended `sN_fxExt3`.
- All existing `sN_p1..p16` remain stable. Q uses appended
  `sN_eqQ1..eqQ8`, with neutral defaults.
- Native `.skellpreset` **schema remains v2**, permits type IDs up to 30
  and optional 8-value `eqQ` arrays; imports older v1/v2 with neutral Q.
  Bounds and transactional validation reject malformed data before state
  mutation. External VST3 state is still not exportable to native v2 ZIPs.

## UI and graphics

- Midnight-black / neon-green rack, other skins and skull-click theme cycling
  from prior PR #3 remain.
- Eight-band graph shows the actual combined eight-filter response, eight
  interactive nodes, selected-pair knob labels, physical Hz/dB readouts and
  Q control. New dynamics, resonance, noise and studio pitch each have
  distinct vector displays and compact rack icon.
- Important UI fix: JUCE APVTS slider attachments override custom numeric
  formatting. `SlotEditor::applyKnobReadouts()` reapplies proper Hz/kHz,
  dB, semitones, percentages and ms **after** attachment creation/rebinding,
  including EQ page switches.
- A headless real-editor screenshot matrix checks 100 scenes, multiple skins,
  compact sizes and modules. A successful older screenshot matrix does not
  verify the latest head.

## CI and manual validation

Portable tests:
`SkellPresetAdvancedVocalDSPTest` (8/44.1/48/96/192 kHz and nonfinite),
`SkellPresetSpectralPitchFormantTest` (known sine, octave shift, neutral
768-sample delay, nonfinite automation), native preset round-trip,
and prior modules' existing DSP suites.

Windows tests:
MSVC /W4 /WX build, native preset package tests, nested VST3 fixture
host probe, binary packaging. CI logs should be checked on **latest head**,
not merely an earlier successful intermediate commit.

Manual release gates in FL Studio:
load/reload old projects and new presets, host automation compatibility,
real stereo listening including 164-BPM vocals, extreme gain and frequency
sweeps, screen capture and interactive changes at 70/85/100/125% sizes,
CPU on lower-end hardware, zero NaNs, denormals, clicks, correct PDC and
project persistence. A host plugin scanner may require rescanning after
parameter-count changes.

**Do not merge PR #5 until its upstream PR #4, #3 and #2 are reconciled
and the complete test matrix is green.**
