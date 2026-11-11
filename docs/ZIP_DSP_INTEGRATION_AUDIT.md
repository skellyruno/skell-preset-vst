# Imported vocal DSP integration audit — 2026-11-11

This is a source-code audit of both user-supplied archives. It records what was
**actually integrated** and what must not be represented as finished. The user
confirmed permission to reuse the code. Existing Skell JUCE, rack ordering,
slot count, state and native presets are kept; no ZIP is blindly copied over
the current product.

## Inspected sources

1. `Voice-Changer-main.zip` (Auralis: Rust/Tauri, MIT license embedded at
   `Voice-Changer-main/LICENSE`). Native DSP in `src-tauri/src/dsp/`:
   `biquad.rs`, `dynamics.rs`, `effects.rs`, `pitch.rs`, `noise.rs`,
   `chain.rs`, `params.rs`, and `presets.rs`; audio streaming and testing
   in `audio/`. Rust code is **not** linked into this C++ plugin.
2. `FLAMEEWRLD-Studio-Vocal-Chain-master(1).zip` (JUCE C++). DSP in
   `Source/DSP/StudioDspModules.h`, `Source/DSP/YinPitchEngine.h`,
   processor and editor in `Source/PluginProcessor.*` and
   `Source/PluginEditor.*`. The ZIP does not include a top-level LICENSE
   declaration; the user states they own/have permission to reuse the code.
   The source is a reference, not a verified measurement of equivalence to
   the commercial devices named in its README.

## Ported now

| New module | Origin | Skell controls | Implementation |
| --- | --- | --- | --- |
| Vocal Rider (#23) | FLAMEEWRLD `VocalRiderAndPreamp`, Auralis level-smoothing architecture | Amount, Target (-26..-12 dB envelope), Range (3..15 dB), Warmth | Stereo-linked 80 ms envelope, bounded gain, asymmetric 24/140 ms gain slew, silence floor, optional mild harmonic colour; never claims EBU LUFS calibration |
| Ducking Ping-Pong (#24) | FLAMEEWRLD `DuckingStereoDelay`, Auralis delay-state / wet-dry separation | Division (1/16, 1/8, dotted eighth, 1/4), Feedback, Ducking, Tone | Host BPM snapshot once per block, 120 BPM fallback, 24 ms read-tap crossfade, stereo cross-feedback, filtered echo, voice-envelope duck; preallocate rings in prepare |
| Vocal Vibrato (#25) | Auralis `Colour` vibrato | Rate, Depth, Stereo, Shape | Original C++ stereo fractional-delay implementation, 8 ms nominal tap, sine/triangle shape and stereo phase spread, preallocated memory |

All effects use the original 16 physical rack slots, their existing 16 generic
parameters and existing wet/bypass attachments. Existing effect IDs 0–22 and
factory preset IDs 0–33 remain at their original indices. New presets are
appended as IDs 34–36: `Rider Ready`, `Ducked Echo`, `Skell Flutter`.

**Important compatibility qualification:** appending options to a JUCE
AudioParameterChoice may change the *normalized* parameter value associated
with an old choice index in host automation, even though text identities,
integer indices and preset state are stable. Host automation projects using
slot-type choice automation therefore need an FL Studio migration/regression
test before release. We must not label all automation compatibility certified
until this is resolved or verified.

All these effects are implemented in the standalone C++17
`Source/chain/ImportedVocalFX.h`; they do not allocate, lock, log, call the
filesystem, or call JUCE within their per-sample processing routines.
JUCE remains responsible for I/O, host transport and automation snapshots.
The new effects do not add plugin delay compensation; the wet-only vibrato
intrinsically uses a modulated ~8 ms short delay as part of its sound.

## Rejected / deferred after inspection

- **Auralis RNNoise** uses `nnnoiseless` with 480-sample/48 kHz frames (10 ms)
  and a third-party model, which needs a licensed redistributable binary,
  resampling and host PDC contracts; not pasted into a zero-PDC native rack.
- **Auralis FFT pitch/formant shifting** uses 1024 FFT / 256 hop (768 sample,
  ~16 ms at 48 kHz) and is mono-first. A new port would need proper stereo
  phase coherence, pitch validation, and dynamically safe PDC. Prefer future
  SkellCore/SkellTune integration rather than promising 0 ms AutoTune.
- **Auralis 8-band parametric EQ** offers a valuable upgrade versus Skell's
  current one-band bell/high-cut model, but needs new validated frequency/Q/
  gain mapping and a real multi-band GUI rather than repurposing one band's
  curve as if it had 8 independent controls.
- **FLAMEEWRLD YIN Auto-Key / formant synth** makes aggressive latency and
  formant claims not established by measurements in these ZIPs; needs
  dedicated known-pitch benchmarks and transition tests before shipping.
- **FLAMEEWRLD sub-octave grain harmonizer** requires pitch/phase stability,
  correct delay/PDC and vocal benchmark listening. Do not drop its fixed grain
  code into the master rack unvalidated.
- **FLAMEEWRLD serial FET/Opto + NY smash** can become a distinct
  dynamics module once gain staging, makeup behaviour, transient response and
  CPU are validated against the already-present compressor/VOXRACK smooth comp.
- **FLAMEEWRLD dynamic harshness tamer / split-band de-esser** overlaps
  existing Skell de-essing and should be benchmarked on actual sibilance,
  without claiming commercial Soothe/DS1 equivalence.
- **FLAMEEWRLD oversampled saturator, through-zero flanger and plate**
  require explicit oversampling filters, group delay, stable coefficient
  modulation and truthful latency reporting; retain existing tested Skell
  effects until these pass comparisons.
- **Auralis microphone device routing, WASAPI loopback, virtual cables,
  recording and mixer tray controls** belong to a standalone recording app,
  not an in-host VST3. UI ideas (preset A/B audition, test clip in memory,
  search, reliable indicators) remain candidates.

## Verification gate

Portable regression: `tests/test_imported_vocal_fx.cpp` checks
8/44.1/48/96/192 kHz, unity bypass, voice leveling, no boosted noise,
tempo fractions/fallback, impulse echoes, decay/cross-feedback, vibrato
fractional-delay, nonfinite input, and bounded output.

Editor verification: actual-editor screenshot matrix includes three new
module faces plus all three new factory chains. Check 70/85/100/125% scales,
all skins, rack left/right and menu. Windows VST3 CI and FL Studio manual
tests remain separate gates.

**No claim of zero CPU cost, commercial-model emulation, true-peak safety,
certified formant shifting or production validation is made here.**
