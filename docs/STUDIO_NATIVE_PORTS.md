# Studio DSP integration: EQ / limiter / compressor / distortion

Source repo: `skellyruno/skell-studio` (Lemon Studio GPL-3.0-or-later lineage).
Skell Preset foundation: v0.7 premium UI at PR #39. These are **additive,
opt-in modules**; existing factory sounds and FL Studio project state must be
unchanged when no new module is chosen.

| New immutable module ID | Name | Algorithm | Added PDC |
|---|---|---|---|
| 34 | Studio Parametric EQ (Plain) | 7-band plain coefficients; SmoothedBiquad and cache | 0 |
| 35 | Studio Limiter Hybrid | Studio RMS compressor and gate; existing SkellCore TruePeakLimiter | existing limiter lookahead |
| 36 | Studio Compressor | Studio-style ratio/threshold/soft knee and stereo RMS detector | 0 |
| 37 | Studio Fast Dist | Original type-A/type-B 8192-point curve with 16-bit quantized table | 0 |

## Comparison and safety
- Keep the native **8-band EQ** with independent Q and high/low cuts;
  it has more bands and is not overwritten by the Studio 7-band plain option.
- Keep original **SkellCore TruePeakLimiter**, with the exact current PDC
  reporting and local dry path, as the last stage of the Studio hybrid.
  Hybrid is not a source-identical port of the complete Studio limiter.
- Use isolated CTest vectors for Studio's original Fast Dist/FL test samples,
  neutral EQ, finite output, compressor reduction, gate closing and release.
- Do not claim that one EQ/limiter is sonically superior based only on compiling.
  Matched same-take A/B and CPU/latency measurements are required.
- No sample-thread allocation, unbounded work or locks; the distortion's
  tables are initialized during prepare, not during process.
- **Known limit**: Studio EQ HQ+/LIN not ported; its upper six gain/frequency
  controls need a dedicated 7-band editor pass for discoverability.

Module selection uses new `sN_fxExt7` choices. Every previous effect choice
parameter, VST3 identifier, factory index and native ZIP schema is preserved.
