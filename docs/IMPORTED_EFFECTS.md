# New native voice and adlib effects — source review

The requested archives were inspected:
- **local-mic-effects-main (1).zip**: JUCE-based effects, including Radio,
  Bitcrusher, Tremolo, Stereo Width, Ring Modulation, formant/pitch, and Echo.
  The provided archive has no explicit license, so the implementation is
  independently authored from the feature behavior rather than copying code.
- **audio-mixer-main.zip**: Electron/React/Web Audio app, **MIT license**,
  with three separate frequency-band compressors, a spectrum view, routing,
  native presets, and optional RNNoise AudioWorklet.

Skell-Preset now has native VST3 modules **Broadcast Radio, Retro Crush,
Tremolo, Stereo Width, Three-Band Dynamics**. The three-band design is
adapted from audio-mixer's MIT-licensed multiband strategy using new C++
DSP with complementary 1-pole low/mid/high crossover splits. The MIT
license notice remains in the source ZIP; a copy of its terms is retained in
this repository at docs/THIRD_PARTY_AUDIO_MIXER_MIT.txt.

All five modules use the existing 16 physical rack slots and four automation
controls per module without changing existing module numbers, slot IDs or
audio I/O. Module states are per-instance and audio processing requires no
heap allocations, network, microphone access, worker threads or file writes.

**VoxRack source:** The earlier owner-authorized band-limited feedback code
continues to power native Delay and Adlib Throw. Two new factory chains
combine this VoxRack feedback behavior with the independently authored
Broadcast Radio and Retro Crush effects. These are combinations of
existing effects, not claims of a separately copied VoxRack Radio DSP engine.

**Intentionally excluded:** RNNoise and pitch correction from the supplied
apps are not silently included. They require their own licensed binaries,
real measured latency and CPU budgets, tests, and safe realtime integration.
Likewise Electron/WebAudio routing and microphone capture cannot run inside
FL Studio's native VST3 audio callback.

## DSP controls

| Module | P1 | P2 | P3 | P4 | Added PDC |
| --- | --- | --- | --- | --- | --- |
| Broadcast Radio | Low Cut | High Cut | Nasal | Drive | 0 |
| Retro Crush | Bit Depth | Sample Rate | Drive | Dither | 0 |
| Tremolo | Rate | Depth | Shape | Stereo | 0 |
| Stereo Width | Width | Mono Bass Freq | Balance | Low Mono Amount | 0 |
| Three-Band Dynamics | Low Compression | Mid Compression | High Compression | Release | 0 |

Native presets use the existing standard .skellpreset schema 2, keeping
older exported presets valid. The new modules are appended after External VST3
in the module index enum. Unsupported media, proprietary RNNoise worklets
and third-party plugin binaries are never embedded.

See \`tests/test_vocal_effects.cpp\` for stereo, parameter extremes, NaN/Inf,
silence, unity and crossover-reconstruction checks.
