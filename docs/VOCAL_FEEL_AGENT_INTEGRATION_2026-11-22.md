# Vocal Feel agent → Skell Preset: complete useful feature integration (2026-11-22)

## Actual user-supplied source and provenance

- Supplied ZIP: `Vocal-Tune-LIke-JJP-main.zip`; 580,304 bytes, 24 entries, ZIP CRC/integrity **PASS**.
- Original archive SHA-256: `46ac357bd3b24f9fcd0884466bee6f5cc394f5ceed2256d20e62c11673f1272a`.
- Its README calls the JUCE plugin **“Vocal Feel by ROHIT JI”**, and describes `SENS > TONE EQ > DYNAMIC DE-ESSER > OPTO COMP > MAIN + MAGIC / SPACE / ATTACK / ATTITUDE / PRESENCE > MASTER`.
- It supplies an original screenshot, JUCE DSP and editor, eleven style presets, Windows Inno Setup packaging and an automatic GitHub release workflow. No LICENSE file is included; this import uses **independent Skell-native implementation of concepts**, not verbatim import of its engine or use of its binary/brand assets.

## Shipped to draft PR #90 (not a merged or verified release)

| Feature | Skell implementation |
|---|---|
| Existing 3-color opto compressor | **Vocal Feel Opto**, appended rack ID **43**, with `sN_fxExt10` choice. FLAT/WARM/TUBE and MALE/MALE 2/FEMALE profiles; Amount/Parallel controls use the existing `sN_p1..p4` IDs. Actual stereo gain-reduction report |
| Complete source-like vocal chain | **Vocal Feel Suite**, appended rack ID **44**, with `sN_fxExt11` choice. Source Sens, 3-band voice-dependent EQ, high-band dynamic de-ess, opto comp, native 5-return mix, Main/Master controls |
| MAGIC | Bright compress/double via prepared stereo fractional delays |
| SPACE | Voice-timed slap with feedback + native stereo short room/plate-style diffusion; no new audio-thread allocations |
| ATTACK | Linked fast/slow speech transient enhancement |
| ATTITUDE | Saturated, band-limited, heavily compressed midrange return |
| PRESENCE | Voice-profiled articulated air/presence return |
| All original style identities | Exactly eleven *source-name* recipes including Rap Power, Female Air, Slapback Retro, Big Chorus Vocal, Deep Male and the other six. Styles are editable real APVTS values; Sens/Master preserved while choosing a style |
| 16-control Suite editor | Four native pages, custom dark/green keyboard-accessible styles popup, parameter readouts in physical units, direct automation gestures on 5 return mixers |
| Real audio feedback | Stereo-linked opto/deess gain reductions and per-return RMS from realtime DSP atomics; native effect graph does not invent levels |
| Analyzer | Real **pre-rack input vs post-master output** stereo mono-sum samples captured atomically. GUI-only 36-band Goertzel spectral analysis from 80 Hz to 9 kHz, with computed *exact native EQ filter-response* overlay and measured DE-ESS cutoff marker. Spectrum / Return Mix view toggle; max 10Hz GUI update, zero audio callback FFT/GUI work |
| Backward compatibility | Existing factory IDs 0..54, module IDs 0..42, automation `sN_p1..p16`, old `fxExt` through `fxExt9`, Windows VST3 identity and old FL Studio states stay unchanged in meaning; new selectors append. Loading an old state without new selectors explicitly restores them to OFF |
| DSP safety | Clip-bounded finite stereo outputs, denormal protection, unmodified source when Amount/Parallel 0 in Opto, source Suite unity at neutral/no-return default, sample-rate reprepare, preallocated audio delay & reverb, nominal 0 additional algorithmic samples PDC |
| Repeatable validation | Portable suite tests at 44.1/48/96 kHz (all five returns, all voice profiles), eleven recipe identity tests, registry choices, spectrum 1kHz/6dB capture, native JUCE processBlock/state roundtrip and UI interaction/geometry smoke. New **actual** full Quick4 renderer scenes for every Suite page, style popup, spectrum view |

## Status and evidence, not overclaims

- **Portable validated:** 38/38 CTests passed on the earlier committed source `77eaf9bd6`, including real Suite DSP, recipes, Opto and spectrum tests. The latest UI tonal-curve change still needs its exact-head CI conclusion.
- **Windows VST3 and native JUCE UI smoke:** must finish at the exact release candidate commit. Do not redistribute old passing VST3 as current. Keep the PR draft and never merge unverified changes.
- **Not visually approved:** GitHub workflow explicitly may not capture UI frames. Run local SkellCore Quick4 on Windows/Linux, then **open and inspect** current-source PNGs at 70, 85, 100, 125%, all themes, four Suite pages, styles, popup, spectrum, 2-page browser, MIC, Settings, effect pickers. Fix defects and recapture. Do not substitute the separate original agent screenshot or ImageGen mockups.
- **Not sonically approved:** no owner vocal same-take reference WAV A/B against the original agent plugin, old Skell Preset, and new Suite. Also unverified FL Studio project automation recall, CPU and pluginval/latency at 44.1/48/96 in an actual host.
- **Concepts intentionally not copied as-is:** other agent's destructive auto-release workflow (deletes old GitHub Releases), its installer/bundle identity, proprietary-looking branding or original screenshot; separate nine-mode reverb implementation in `ReverbDSP.h` not ported because this Suite uses the preexisting Skell native room circuit. The user requested *all useful features*, not duplicated vendors/artifacts or knowingly destructive release behavior.

## Next objective quality gates

1. Resolve exact-head native compile warnings/errors and run real Windows VST3/native UI/host-state suite tests; download and SHA-verify the VST3 artifact.
2. Real Quick4 screenshots and popup inspection, fix/recapture before presenting an approved UI image.
3. Same-vocal level-aligned A/B: sibilant consonants, transients, distortion aliasing and pitch-independent timbre; reject changes with no audible benefit.
4. Measure 1/5/10/20-instance CPU, peak/NaN/denormal rate, true latency and oversampling needs. Perform FL Studio/project/preset recall and pluginval before merging.
