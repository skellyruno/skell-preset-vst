# VoxRack review and independent Skell-Preset improvements

Reference reviewed: user-supplied `voxrack-main(1).zip`, including `docs/ARCHITECTURE.md`, `apo/dsp.h`, `apo/fx.h`, and WinUI rack files.

VoxRack is a Windows microphone Audio Processing Object (system mic DSP) controlled by a WinUI application through shared memory. Skell-Preset is a JUCE VST3 inside a DAW, so it should **not** copy the APO COM registration, Windows service integration, WinUI layout, or shared-memory application state.

The VoxRack owner has explicitly authorized reusing and adapting the original DSP source in Skell-Preset. The first authorized port is VoxRack's band-limited delay feedback topology, adapted into stereo Delay and Adlib Throw (see [VoxRack DSP port](VOXRACK_DSP_PORT.md)). The earlier independent improvements still include stable ordered stages, a lightweight tracking path, De-Esser, and regression tests. VoxRack's Windows APO COM registration, background services, and shared-memory WinUI application are *not* imported into the VST3.

### Latency trade-offs

| Path | Reported processing PDC | Notes |
| --- | --- | --- |
| EQ, compression, gate, De-Esser, saturation, exciter | 0 samples | Zero-lookahead designs |
| Delay, reverb, Adlib Throw / Doubler | 0 samples | Deliberate audible wet delays/tails, not dry-path PDC |
| One oversampled SkellCore limiter | about 3 ms | Host-reported exact sample count |
| Two limiters in series | about 6 ms | Stage delays accumulate |
| Skell Live Lead | 0 samples | Limiter omitted; not a certified true-peak ceiling |
| Skell Vocal Polish | about 3 ms | Quality lookahead finishing chain |

The user's audio interface, host buffer and other plugins can add latency. A measured 0-sample plugin PDC is not a promise of zero round-trip time. Future work: latency-shift-safe graph crossfades, host A/B tests, wave-equivalence comparisons, explicit CPU profiling and actual custom FL chains matched to dry/wet WAVs.
