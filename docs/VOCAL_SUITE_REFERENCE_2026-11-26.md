# Vocal suite reference integration — 2026-11-26

Reference: owner-supplied vocal-processing-suite-main.zip, commit marker a3ed718027721df08f7b1888c5fea65961cdb970; Famous Beats vocal-presets README and source review.

Implemented: original native one-click starter chains (previous batch); isolated native audio-file preview in SkellPresetAlchemistRender; saved native preset or starter profile, streaming mono/stereo reader, stereo float WAV, reported latency removal, optional 0–10 second tail, finite checks and before/after RMS and sample peak. No private audio stored in Git. No external VST3 or new factory/program/automation identities. Uses existing Skell DSP only. Do not claim third-party tuner, RNNoise, or Avalon hardware emulation.

## CLI (local native renderer build)
```
SkellPresetAlchemistRender --preview-starter 2 "C:/vocals/dry.wav" "C:/vocals/trap-preview.wav" 2
SkellPresetAlchemistRender --preview-preset "C:/presets/My.skellpreset" "C:/vocals/dry.wav" "C:/vocals/wet.wav" 2
```
Profiles 1..5 correspond to the existing builder order. Default tail zero; explicit tail extends output. Input max ten minutes; 8–192 kHz; no intentional overwrite; audio never uploaded. No embedded waveform preview/export UI yet. Offline native processing is not a real host certification. Host tempo falls back to the existing processor default; no imported DAW tempo automation. RMS and peak are not perceptual loudness.

## Audit and follow-ups
- Reference file preview, A/B and DSP regression strategy useful. Implement GUI preview/playback/export separately with isolated owned worker lifetime and explicit audio input; avoid synchronous rendering in DAW editor.
- Reference midpoint two-sample saturation is not a measured anti-aliasing solution. Existing approved drive kept. Need measured aliasing tests before an opt-in quality mode.
- Reference compressor updates one envelope across channels and calculates coefficients per sample; do not replace linked native dynamics. Reference limiter precedes reverb, so no whole-chain output ceiling guarantee.
- Reference gate uses instantaneous threshold; preserve current attack/hold/hysteresis design.
- RNNoise: evaluate upstream model licensing, 48 kHz resampling, measured latency and clean-vocal preservation independently. No imported neural weights.
- Use only our SkellyTune engine for future key/scale correction, preserving approved sounds and automation. Current starter chains have no tuning module.
- Dedicated preamp/air, distinct room algorithms, graphical file preview, safe level-matched A/B and audio-driven recommendation are next steps, not completed.
- Uploaded suite lacks a top-level license; no literal source/art assets copied. No dependency upgrade required for this independent implementation.
