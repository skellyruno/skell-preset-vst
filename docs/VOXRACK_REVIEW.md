# VoxRack review and independent Skell-Preset improvements

Reference reviewed: user-supplied `voxrack-main(1).zip`, including `docs/ARCHITECTURE.md`, `apo/dsp.h`, `apo/fx.h`, and WinUI rack files.

VoxRack is a Windows microphone Audio Processing Object (system mic DSP) controlled by a WinUI application through shared memory. Skell-Preset is a JUCE VST3 inside a DAW, so it should **not** copy the APO COM registration, Windows service integration, WinUI layout, or shared-memory application state.

The ZIP has no clear general redistribution license. **No VoxRack code was copied**. Useful engineering ideas implemented independently include stable ordered stages, block-rate control reads, a lightweight tracking path, per-effect bypass and tail behavior, an additional original split-band De-Esser, and DSP safety tests.

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
