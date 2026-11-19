# Alchemist native same-take A/B audit — 2026-11-18

**Status: experimental. No candidate is approved or promoted.** This report records a real same-input render of the current native Skell Preset chain against the user's MAIN-effects print. It does not claim exact replication or substitute for owner listening.

## Source identities

All three source files were verified against the owner-approved archive manifest on branch `feature/deuce-v25-listening-approved-20261115`, archive commit `7eb041a848ed61d242443321a1cbb9f230ff1a47`.

| Role | Original name | Verified SHA-256 |
|---|---|---|
| Input | `alchemist test vocals no main effects.wav` | `115c28f87f57fb13eaafce7e3c28a0fbaaba95c86bddaa89440b3c92d453d706` |
| MAIN wet target | `alchemist test main effects on .wav` | `e40d6abcb9b9a111c8a3be8be3445b21726c1d8b98e6e99e5a3613d6add4adbb` |
| FL Studio MAIN track preset | `MAIN.fst` | `66077b7a4e1695ab8817b6cbe74be7f098afa1fe8fee52ac4fa3e90065359424` |

The input is a **REC-processed vocal print with MAIN effects off**. It is not an untreated dry vocal. The target is the additional MAIN-chain wet print. Both files are 44.1 kHz, stereo, PCM 24-bit, 1,878,768 samples (42.602449 s).

### Hash-bound MAIN.fst slot inventory

The owner-maintained slot manifest, bound to the exact MAIN.fst SHA above, records:

1. Fruity Parametric EQ 2
2. Fruity Limiter
3. Waves CLA-76 Stereo — preset **In Your Face Vocal**, SETUP_A
4. Waves CLA-2A Stereo — preset **Vox**, SETUP_A
5. Soundgoodizer — Mode A, raw amount 169
6. Fruity Limiter

This is a hash-bound owner slot inventory; it is not a claim that scanning embedded plugin-name strings can reconstruct slot order. The embedded structure report confirms the two Waves preset names and stored non-bypassed state, but raw Waves vectors are not mapped reliably to physical knob values. Stored state does not prove that every effect was audible.

## Render method

- Plugin source branch head: `e165f3dc090a0225df3f16aa69adc4d3f032c4a0`.
- Windows CI merge/build commit: `c4bf5f79733309b571f2c20cda36af2381614ce3`.
- Renderer: `SkellPresetAlchemistRender.exe`, SHA-256 `4846e5895e36ec22138188d556c19d4428cb6dfa88aa930f6fe5299ffc4ed756`.
- The renderer called the compiled `SkellPresetAudioProcessor::processBlock` on the same full input for factories 43, 45, 47, 48, 49, 50, 51, 52 and 53. This is not an external VST3-host or FL Studio render.
- Delay analysis and the 15-second audition both use the same 19.000–34.000 s window. The target delay measured 49 samples (1.111 ms), six samples from the previous 55-sample estimate. Candidate delays were measured individually and aligned to the same input timeline.
- Every audition file is 44.1 kHz stereo, 15 seconds, PCM 24-bit FLAC. Original-level mode applies one shared safety attenuation of -1.025 dB. Level-matched mode attenuates all tracks to the quietest track (-16.611 LUFS); no track is boosted.
- The 4× interpolated peak is an estimate, not a certified BS.1770 true-peak measurement.

## Same-take results

Target: **-14.261 LUFS**, sample peak **-0.025 dBFS**, 4× peak estimate **+0.025 dBTP**.

| Factory | Candidate | Raw LUFS | Raw 4× peak est. dBTP | Spectral MAE dB | 20 ms envelope MAE dB | 200 ms envelope MAE dB |
|---:|---|---:|---:|---:|---:|---:|
| 43 | Alchemist Native Preview | -13.569 | -0.008 | 4.338 | 2.185 | 1.362 |
| 45 | Alchemist Refined | -14.620 | -0.783 | 4.334 | 2.298 | 1.336 |
| 47 | Alchemist Peak Guard | -14.622 | -0.951 | 4.333 | 2.298 | 1.333 |
| 48 | Alchemist Tone Match | -14.769 | -0.960 | **4.155** | 2.399 | 1.411 |
| 49 | Alchemist Lighter Dynamics | -13.880 | -0.926 | 4.414 | 2.301 | 1.372 |
| 50 | Alchemist Fast Compressor Blend | -14.001 | -0.927 | 4.350 | 2.277 | 1.334 |
| 51 | Alchemist Smooth Compressor Blend | -14.666 | -0.946 | 4.193 | 2.401 | 1.410 |
| 52 | Alchemist Serial Dynamics | -16.610 | -1.102 | 4.225 | 3.224 | 2.540 |
| 53 | Alchemist Low-End Contour | -16.611 | -1.112 | 4.211 | 3.255 | 2.558 |

### Interpretation and safety

- Factory 48 has the best raw spectral MAE among factories 43, 45 and 47–51, but its 20/200 ms envelope errors are worse than factory 43. It is not an overall winner.
- Factories 52 and 53 look better only in the **RMS-level-matched diagnostic** (spectral MAE 4.017 / 4.012 dB; 20 ms envelope 1.980 / 1.998 dB; 200 ms envelope 1.129 / 1.132 dB). They are about 2.35 LU quieter in their actual renders. Matching RMS would require +2.316 / +2.332 dB and produce +1.214 / +1.220 dBTP 4× estimates. Those are diagnostic calculations only; no gain is applied to the audition. Do not promote them or add simple makeup gain.
- The factory 53 low-end contour changes the first high-pass from 65 Hz to 30 Hz and neutralizes the 70 Hz / +3.39 dB bell. A supplemental mid-channel 1/3-octave Welch diagnostic shows the target-minus-candidate deficit at 31.5 / 40 / 50 Hz improving from +14.4 / +12.5 / +7.8 dB (factory 52) to +11.5 / +9.0 / +5.5 dB (factory 53). Residual 63 / 80 / 100 Hz differences are about +1.2 / +0.5 / -1.8 dB. This is an exploratory band diagnostic, not the official multiresolution score.
- The input, target and all candidates are effectively mono on this excerpt (stereo correlation approximately 1.0; side energy at the numerical floor). No stereo widening is evidenced here.
- No candidate is a confirmed sonic match. Factory 43 remains the baseline; 43, 45 and 47–53 remain available for rollback and further listening.

## Verification

- A/B tooling regression suite: **15/15 passed** on the measured run.
- Windows Release CTest: **41/41 passed**.
- Compiled real-rack stress: **1/1 passed** for 5/10/20-instance Alchemist baseline and serial-trial finite-output/latency stress.
- Nested JUCE VST3 host probe passed: plugin load, transport forwarding, stereo processing, PDC and state.
- Native factory recall/audio/PDC/state smoke passed for Alchemist trials and existing Deuce recall.
- Windows VST3 package: artifact #14478248598, build-info commit `c4bf5f79733309b571f2c20cda36af2381614ce3`, binary SHA-256 `26f94ae7f8e80228f84d1d3fd13485197e992e8303b041104c5c605ffc052430`.
- Same-take A/B artifact: [workflow run #44413463282](https://github.com/skellyruno/skell-preset-vst/actions/runs/44413463282), [artifact #14480245187](https://github.com/skellyruno/skell-preset-vst/actions/runs/44413463282/artifacts/14480245187).
- The existing player loaded all 22 embedded audio elements in Chromium; synchronized play, track switching and level-mode switching worked without console errors. This PR also fixes the metrics table's horizontal overflow and rounds displayed cells to three decimals; full precision remains in metrics.json.

**Not verified:** FL Studio runtime/session save-reload, automation recall in a real FL Studio project, owner listening acceptance, and a truly untreated dry Alchemist vocal. No exact match claim is made.

## Next priority

Tune serial dynamics/crest management in a new append-only trial. Do not add +2.3 dB makeup after the limiter. Test controlled pre-limiter drive or a safer wet/threshold blend, keep the same 19–34 s source window, and require the candidate to approach the target loudness without exceeding the -1 dBTP preview ceiling estimate or worsening the short/medium envelope. Preserve all current factories and the empty-rack startup.
