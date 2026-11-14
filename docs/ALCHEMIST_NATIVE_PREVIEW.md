# Alchemist — Native Preview v0.1

## Purpose and status

**EXPERIMENTAL / NOT APPROVED FOR SONIC MATCH CLAIMS**

A complete, editable Skell Preset factory chain built from six existing rack processors plus one new **causal match-finishing native DSP module**. Added at program index **43** (44th program), appended after the existing 43 programs. Search **Alchemist** in the Main Vocals bank. Each stage remains independently automatable, bypassable and reorderable; the original plugin manufacturer/code is unchanged.

The matching work in the owner-provided `Skelly_Sound_Matching_v0_7_Complete.zip` is **offline**, includes a noncausal phase-matching ancestry, and is **not** an implementation of real-time CLA-76, CLA-2A, Fruity Parametric EQ 2 or Soundgoodizer. That toolkit remains untouched. Its measured 0.9790 waveform correlation on a held-out timeline section of one Alchemist take **must not** be attributed to this native chain. The native chain has not yet been A/B measured against that recording.

## Evidence used

- `MAIN.fst` identifies CLA-76 preset **In Your Face Vocal** and CLA-2A preset **Vox**, without exposing trustworthy mappings to every plug-in parameter.
- Independent screenshots show Fruity Parametric EQ 2 response curves, Waves dynamics and Soundgoodizer **Mode A**. The two EQ screenshots might be views of one EQ: exact number of instances and exact mixer order remain unverified.
- The v0.5 offline envelope fitting study estimated fast threshold **-34.76 dBFS**, approximate ratio **7:1**, slower threshold **-19.46 dBFS** and ratio **1.196:1**. These are fitting hypotheses, not readings of Waves hardware/software knobs.
- A final Fruity Limiter ON/OFF test gave an extremely small difference for the specific reference take (correlation ~0.9999999997; RMS difference about -111 dBFS). This native preview intentionally omits the final limiter, avoiding unnecessary lookahead latency. Other source recordings might engage limiting.
- The v0.7 offline trial used post-processing of a prior render, not a ready-to-port VST3 processing graph. The protected v0.3 champion and every newer experiment are unchanged.

## Seven serial native modules

| Slot | Native processor | Purpose | Caveat |
|---|---|---|---|
| 1 | Classic EQ | Approx. 65 Hz high-pass | Recreates low-cut role, not exact Fruity slope |
| 2 | 8-Band EQ | Conservative bell/notch placements + independent Q | Listening-start approximation |
| 3 | Compressor | Fast CLA-76 *role*: -34.76dB, 7:1, 5ms/90ms, 35% wet | Different detector/topology from Waves |
| 4 | VoxRack Smooth Comp | Slower CLA-2A *role*: -19.46dB, 1.196:1, 160ms, 70% wet | Different gain cell/program dependence |
| 5 | Three-Band Dynamics | Multiband component of Soundgoodizer *role*, 26% wet | Not Image-Line Maximus |
| 6 | Saturator | Restrained harmonic color, 17% wet | Not Soundgoodizer proprietary algorithm |
| 7 | Alchemist Match Finish | Causal adaptation of archived v0.7 cubic harmonic + 20/120/280ms envelope model | Not the offline v0.4–v0.7 phase fitter, and not yet a full-chain sound match |

The normalized stage values live in `Source/chain/AlchemistNativePreset.h` and are the **sole source of truth** for this embedded factory program.

### The first actual DSP transfer from v0.7

`Source/chain/AlchemistMatchDSP.h` implements bounded cubic harmonic shaping with the measured v0.7 coefficient **-0.017994**, reference level **0.3012775**, normalization **+0.0775 dB**, and slow/fast envelope fit weights **-0.123735 / -0.35**. The slow envelope center is **-19.878 dBFS** from the preserved 1–18s training window of the previous matched audio. All coefficients are saved as normalized per-slot controls; no WAV or reference file is bundled.

This native version uses sample-by-sample causal one-pole RMS followers and 15ms causal control smoothing, replacing the original block analysis and symmetric Gaussian smoothing. It has **zero lookahead and zero added plugin latency**, is stereo linked, and does not allocate on the audio thread. It is **not** the offline phase-matching / Soundgoodizer / Waves stage.

#### Limited preliminary isolation test (not a VST3 A/B)

The current causal math was independently simulated on the previously processed **v0.6B** recording from the archived matching package. Its 19–35s validation metrics against the level-matched reference were:

| Metric | Prior v0.6B | Causal finish simulation | Offline v0.7 experiment |
|---|---:|---:|---:|
| Zero-lag waveform correlation | 0.9785 | 0.9791 | 0.9790 |
| 20ms active-envelope MAE, dB | 1.001 | 0.965 | 0.968 |
| 200ms active-envelope MAE, dB | 0.656 | 0.637 | 0.646 |
| Spectral-band MAE, dB | 0.379 | 0.379 | 0.378 |

The model was evaluated on the same archival performance used during development; it is **not** independently validated on other vocals, and the first six VST3 processors have not recreated the v0.6B input to this finishing stage. These figures cannot be marketed as the current plugin's match score. Match Finish source and tests are included, and native VST3/FL Studio tests must still pass.
 No original reference WAV, screenshot, commercial plugin data or proprietary state is bundled into the distributable.

## Regression and listening gates

1. Portable `SkellPresetAlchemistNativeTest` checks append-only factory IDs, exact stage order, bounded parameters and representative processing at 44.1/48/96 kHz. **Source test added; inspect CI results before claiming a pass.**
2. Full Windows VST3 compile and plugin host fixture must pass.
3. Open the preset in FL Studio; verify slot order, bypass, wet controls, safe project save/reopen, no unexpected extra limiter, mono/stereo consistency and maximum peak.
4. Render supplied **dry** Alchemist vocal through this exact VST3 chain and compare to the original **main effects** reference, not the level-matched v0.7 candidate alone. Measure aligned waveform correlation, spectral error, 20ms/200ms envelope error and true peak. Equalize audition loudness and keep the protected v0.3/v0.7 files unchanged.
5. Refine only from real A/B measurements, not from visually inferred proprietary knob settings.

## Integration constraints

- Do not change factory program indices 0–42, module type IDs or existing APVTS parameter IDs.
- No Skell Studio process, Python, external vendor VST3 or reference-audio file is needed at runtime.
- Use an explicitly **preview** identity and leave the shipped factory preset intact until native-audio A/B is measured.
- A separate `.skellpreset` package can be imported into existing Skell Preset versions supporting all seven module IDs, including new ID 31, and schema v2, even before the built-in preset ships.
