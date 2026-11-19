# Alchemist — Native Preview v0.1

## Purpose and status

**EXPERIMENTAL / NOT APPROVED FOR SONIC MATCH CLAIMS**

A complete, editable Skell Preset factory chain built from six existing rack processors plus one new **causal match-finishing native DSP module**. Added at program index **43** (44th program), appended after the existing 43 programs. Search **Alchemist** in the Main Vocals bank. Each stage remains independently automatable, bypassable and reorderable; the original plugin manufacturer/code is unchanged.

The matching work in the owner-provided `Skelly_Sound_Matching_v0_7_Complete.zip` is **offline**, includes a noncausal phase-matching ancestry, and is **not** an implementation of real-time CLA-76, CLA-2A, Fruity Parametric EQ 2 or Soundgoodizer. That toolkit remains untouched. Its measured 0.9790 waveform correlation on a held-out timeline section of one Alchemist take **must not** be attributed to this native chain. A native same-take A/B was completed on 2026-11-18 using the compiled processor renderer: the REC-processed input was compared with the MAIN-effects print over 19–34 s of the same 42.602 s performance. This is not an external VST3-host or FL Studio session render, and it does not establish a sonic match or owner listening approval. See [the dated same-take report](ALCHEMIST_SAME_TAKE_AB_2026-11-18.md).

## Evidence used

- `MAIN.fst` identifies CLA-76 preset **In Your Face Vocal** and CLA-2A preset **Vox**, without exposing trustworthy mappings to every plug-in parameter.
- The owner-maintained slot manifest bound to `MAIN.fst` SHA-256 `66077b7a4e1695ab8817b6cbe74be7f098afa1fe8fee52ac4fa3e90065359424` lists one Fruity Parametric EQ 2 → Fruity Limiter → Waves CLA-76 Stereo (In Your Face Vocal, SETUP_A) → Waves CLA-2A Stereo (Vox, SETUP_A) → Soundgoodizer Mode A (raw amount 169) → Fruity Limiter. This is a hash-bound owner slot inventory, not a claim that raw plugin-name strings were parsed as independent slots. The embedded report confirms the two Waves preset names and non-bypassed stored state, but their raw vectors are not reliably mapped to physical controls; stored state does not prove every effect was audible.
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
 No original reference WAV, screenshot, commercial plugin data or proprietary state is bundled into the VST3 distributable. The separate CI audition artifact contains only the 15-second comparison excerpts.

## Measured same-take tonal mismatch and experimental revision v0.3

The user's **REC-processed Alchemist input with MAIN effects off** (`alchemist test vocals no main effects.wav`; not untreated dry vocal) and **MAIN-effects print** (`alchemist test main effects on .wav`) were compared in 44.1 kHz stereo as power-spectrum density ratios. These *source-relative* differences are not normalized EQ presets:

| Frequency band | Original processed vs dry, power dB |
|---|---:|
| 40–80 Hz | +2.3 |
| 80–150 Hz | -0.1 |
| 150–300 Hz | -2.3 |
| 300–600 Hz | +0.1 |
| 600–1200 Hz | -1.1 |
| 1200–2400 Hz | +1.2 |
| 2400–4500 Hz | +4.5 |
| 4500–8000 Hz | **+10.2** |
| 8000–14000 Hz | **+12.5** |

The first native Skell preset incorrectly attenuated the high bands, making it especially dull relative to the reference. Version 0.3 uses a deliberately limited/regularised eight-bell biquad fit, constrained to **+11 dB maximum high-band boost** rather than attempting the unrestricted solution. Its native bell profile is:

- 70 Hz +3.39 dB, Q1.40
- 158 Hz -2.97 dB, Q0.66
- 320 Hz +3.30 dB, Q2.37
- 660 Hz -1.31 dB, Q1.54
- 1380 Hz +0.26 dB, Q1.02
- 2930 Hz -1.64 dB, Q0.68
- 5100 Hz +7.77 dB, Q0.65
- 12000 Hz +11.00 dB, Q0.65

**Interpretation:** these are analysis-derived native coefficient targets, **not** recovered Fruity Parametric EQ 2 settings. The measured ratio includes Waves / Soundgoodizer distortion, envelope shaping and level changes, so forcing the whole wet/dry spectral difference through EQ alone can over-brighten or overdrive some vocals. In particular, the upper bands must be measured after the downstream saturator/comp and in a true same-input VST3 export. The first listening test was poor, so this is explicitly an **experimental second tonal hypothesis**, not a validated match. `SkellPresetAlchemistReferenceToneTest` keeps the bright high-band response from accidentally reverting to negative treble gains.

## Regression and listening gates

1. Windows Release CTest on merge commit `c4bf5f79733309b571f2c20cda36af2381614ce3` passed **41/41**; the compiled 5/10/20-instance real-rack stress test passed **1/1**.
2. The owner-archive A/B workflow passed its **15/15** Python tooling tests, verified the input/target/FST hashes, and rendered factories 43, 45 and 47–53. The Windows VST3 host probe and native factory recall/audio/PDC/state smoke checks passed.
3. The A/B renderer calls the compiled `SkellPresetAudioProcessor::processBlock`; it is not a hosted-VST3 or FL Studio render. The Windows VST3 artifact and renderer are separate build outputs.
4. FL Studio open/save/reopen, project automation recall and owner listening approval remain outstanding. No factory has been promoted.
5. Refine only from same-input measurements and actual listening; do not infer proprietary Waves knob positions from raw vectors.

## Integration constraints

- Do not change factory program indices 0–42, module type IDs or existing APVTS parameter IDs.
- No Skell Studio process, Python, external vendor VST3 or reference-audio file is needed at runtime.
- Use an explicitly **preview** identity and leave the shipped factory preset intact until FL Studio session validation and owner listening acceptance; measured A/B alone is not approval.
- A separate `.skellpreset` package can be imported into existing Skell Preset versions supporting all seven module IDs, including new ID 31, and schema v2, even before the built-in preset ships.


## Optional factory 47 — Alchemist Peak Guard (Trial)

Factory 47 is a separate, append-only safety comparison. It copies the opt-in
factory 45 Alchemist Refined chain and appends the existing rack Limiter
(module ID 3) after Match Finish. The normalized ceiling control is 0.89,
which maps through the existing limiter law to approximately -0.97 dB; drive
is zero and the limiter is fully wet. The existing SkellCore Standard limiter
uses four interpolated phases and 16 taps, with approximately 3 ms host PDC
(132 samples at 44.1 kHz; 144 at 48 kHz).

This variant does **not** replace factory 43 or 45, does not alter the default
empty rack, and does not claim BS.1770-certified true-peak compliance. It is
a safety-oriented listening candidate, not a proven closer sound match. The
factory recall test checks the appended module, ceiling, state roundtrip and
PDC restoration when switching back to the original Alchemist. The same-take WAV comparison has now been rendered; the FL Studio session check and owner listening acceptance remain required before promotion.


## Optional factory 48 — Alchemist Tone Match (Trial)

Factory 48 is a measured, append-only tonal correction on top of factory 47.
The 2026-11-18 same-take render uses the 19–34 s audition window, not the full
42.602 s. Factory 48 changes three existing eight-band EQ gains: 2.93 kHz
+1.0 dB, 5.10 kHz -0.6 dB and 12.0 kHz -1.1 dB. It retains the factory 47
final interpolated-peak limiter and its approximately 3 ms PDC. See the dated
same-take report for measured spectral and envelope results.

This remains a **test candidate**, not an approved sound champion. In the
2026-11-18 19–34 s render, factory 48 measured 4.155 dB raw multiresolution
spectral MAE versus 4.338 dB for factory 43, the best raw spectral score among
factories 43, 45 and 47–51. Its 20 ms envelope MAE (2.399 dB) and 200 ms
envelope MAE (1.411 dB) were both worse than factory 43 (2.185 / 1.362 dB),
and its loudness was 0.508 LU below the FL target. It is not an overall winner;
factories 43, 45 and 47 remain unchanged for rollback. See
[the complete same-take report](ALCHEMIST_SAME_TAKE_AB_2026-11-18.md).


## Optional factory 53 — Alchemist Low-End Contour (Trial)

Factory 53 is an append-only experiment based on the 19–34 s same-take
comparison. Factory 52 was 8.419 dB low at 20–60 Hz and 4.212 dB high at
60–120 Hz relative to the MAIN wet target. The current native contour uses a
65 Hz high-pass and a 70 Hz / +3.39 dB / Q 1.40 bell. Factory 53 retains
Factory 52's serial FET/RMS dynamics, Skelldizer, saturation, Match Finish and
final peak guard, but changes only the first high-pass to 30 Hz and the 70 Hz
bell to 0 dB.

This is a measured corrective hypothesis, not a recovered proprietary EQ2
setting. The hash-bound owner slot manifest identifies one EQ2 and two Fruity
Limiters; numeric EQ2 controls are still not decoded. The 2026-11-18 render
measured factory 53 at -16.611 LUFS, 4.211 dB raw spectral MAE, 3.255 dB
20 ms envelope MAE and 2.558 dB 200 ms envelope MAE. Factory 52 measured
-16.610 LUFS, 4.225 / 3.224 / 2.540 dB respectively. The low-end contour
reduces the sub-50 Hz deficit versus factory 52 but does not resolve the
63–100 Hz mismatch. RMS-level matching would require +2.332 dB and estimates
+1.220 dBTP; that gain is diagnostic only and is not applied to the preview.
No existing factory is overwritten, and factory 53 is not promoted. See
[the complete same-take report](ALCHEMIST_SAME_TAKE_AB_2026-11-18.md).


## Same-take A/B evidence — 2026-11-18

The exact source identities, hash-bound MAIN.fst slot inventory, nine factory measurements, headroom analysis, CI results and outstanding FL Studio/listening gates are recorded in [ALCHEMIST_SAME_TAKE_AB_2026-11-18.md](ALCHEMIST_SAME_TAKE_AB_2026-11-18.md). The native same-take renderer uses the compiled processor's `processBlock`; it is not a hosted VST3 or FL Studio render.
