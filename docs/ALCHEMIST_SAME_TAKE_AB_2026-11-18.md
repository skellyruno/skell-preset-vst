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
2. Fruity Parametric EQ 2
3. Fruity Limiter
4. Waves CLA-76 Stereo — preset **In Your Face Vocal**, SETUP_A
5. Waves CLA-2A Stereo — preset **Vox**, SETUP_A
6. Soundgoodizer — Mode A, raw amount 169
7. Fruity Limiter

The hash-verified FST contains **seven** stored stages, including two distinct native Fruity Parametric EQ 2 instances and two Fruity Limiters. The embedded structure report enumerates the two Waves chunks and confirms their preset names and stored non-bypassed state; native Fruity slots are visible in the FST's plugin sequence. Raw Waves vectors are not mapped reliably to physical knob values. Stored state does not prove that every effect was audible. The previous six-slot manifest omitted the second EQ2; this follow-up corrects the manifest, expected chain constant and regression test.

## Render method

- Plugin source branch head: `e165f3dc090a0225df3f16aa69adc4d3f032c4a0`.
- Windows CI merge/build commit: `c4bf5f79733309b571f2c20cda36af2381614ce3`.
- Renderer: `SkellPresetAlchemistRender.exe`, SHA-256 `4846e5895e36ec22138188d556c19d4428cb6dfa88aa930f6fe5299ffc4ed756`.
- The renderer called the compiled `SkellPresetAudioProcessor::processBlock` on the same full input for factories 43, 45, 47, 48, 49, 50, 51, 52 and 53. This is not an external VST3-host or FL Studio render.
- Delay analysis and the 15-second audition both use the same 19.000–34.000 s window. The target delay measured 49 samples (1.111 ms). The command-line expected-delay default is now 49 samples for this exact 19–34 s owner passage; 55 samples remains only a synthetic unit-test offset and is not the measured target delay. Candidate delays were measured individually and aligned to the same input timeline.
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


## Factory 54 exact-head follow-up — 2026-11-19

The exact-head workflow run [#44459852840](https://github.com/skellyruno/skell-preset-vst/actions/runs/44459852840) rendered ten candidates (43, 45, 47–54) against the same 19–34 s owner passage. The run passed portable CTest **32/32** plus four WAV-inspector Python tests, Windows CTest **41/41**, compiled full-rack stress **1/1**, nested external VST3 load/transport/stereo/PDC/state, native factory recall/audio/PDC/state, and the same-take A/B tooling/render steps.

| Factory | Candidate | Raw LUFS | Delta vs target | 4× peak estimate dBTP | Spectral MAE dB | 20 ms envelope MAE dB | 200 ms envelope MAE dB |
|---:|---|---:|---:|---:|---:|---:|---:|
| 43 | Alchemist Native Preview | -13.569 | +0.693 LU | -0.008 | 4.338 | 2.185 | 1.362 |
| 48 | Alchemist Tone Match | -14.769 | -0.508 LU | -0.960 | **4.155** | 2.399 | 1.411 |
| 53 | Alchemist Low-End Contour | -16.611 | -2.350 LU | -1.112 | 4.211 | 3.255 | 2.558 |
| 54 | Alchemist Serial Dynamics Blend | -14.591 | -0.330 LU | -0.944 | 4.213 | 2.318 | **1.336** |

Factory 54 improves the 20/200 ms envelope errors over Factory 48 by about 0.081/0.076 dB, but its spectral MAE is 0.058 dB worse. It remains 0.330 LU quieter than the target. It is a trade-off, not an overall winner; no makeup gain was added and no factory was promoted. Factory 43 remains the protected baseline and Factory 48 the best measured spectral candidate.

The audition package uses a shared **-1.025 dB** safety attenuation and level-matches to the quietest candidate (**-16.611 LUFS**) by attenuation only. Factory 54's 4× peak estimate is -0.944 dBTP before the shared attenuation. These 4× values are estimates, not certified BS.1770 true peaks.

- Windows VST3 artifact: [#14500336411](https://github.com/skellyruno/skell-preset-vst/actions/runs/44459852840/artifacts/14500336411)
- Same-take A/B artifact: [#14500551533](https://github.com/skellyruno/skell-preset-vst/actions/runs/44459852840/artifacts/14500551533)

**Artifact caveat:** the audio renders and measurements are valid, but the A/B artifact's chain metadata still used the old six-slot fixture and omitted the second EQ2. This PR fixes that reporting/validation defect; the regenerated exact-head A/B run must supersede the old chain metadata. The A/B renderer calls the native processor's processBlock; the separate Windows job proves external VST3 loading and audio, but neither is an FL Studio session render.

Owner listening, FL Studio project/session save-reload and real automation validation remain outstanding. Factory 54 is not approved.

## Next priority

Tune serial dynamics/crest management in a new append-only trial. Do not add +2.3 dB makeup after the limiter. Test controlled pre-limiter drive or a safer wet/threshold blend, keep the same 19–34 s source window, and require the candidate to approach the target loudness without exceeding the -1 dBTP preview ceiling estimate or worsening the short/medium envelope. Preserve all current factories and the empty-rack startup.
