# Vocal Feel agent source — native Skell Preset integration (2026-11-22)

Input: user-uploaded `Vocal-Tune-LIke-JJP-main.zip`; contains JUCE `Vocal Feel` attributed in its README to **ROHIT JI**, plus DSP, a Screenshot, eleven presets, Inno Setup scripts and a 32/64-bit GitHub release workflow. Archive integrity: **PASS** (24 entries). No explicit LICENSE file was present. The user explicitly requested using this agent's project in Skell Preset, while experimental changes remain in draft PR #90.

## Implemented experiment (not a factory-preset rewrite)

- **Vocal Feel Opto**, ID **43**: separate, opt-in native dynamics slot built with Skell's own portable DSP code. It takes reference ideas from the supplied 3-colour opto and male/male2/female voice detector profiling; not a byte-for-byte or direct JUCE DSP source transplant.
- The existing published effects 0–42 and factory preset catalog remain in their original indices. An **append-only** `sN_fxExt10` host parameter selects the new effect without renormalizing `fxExt9` or previous IDs. A legacy host state missing `fxExt10` explicitly restores OFF. Existing 16 per-slot `sN_p1..p4` parameters map to **Amount / Colour (FLAT, WARM, TUBE) / Voice (MALE, MALE 2, FEMALE) / Parallel**.
- The new stereo-linked processor uses a bounded high-pass detector, smooth RMS and dual-release opto envelope, conservative level make-up, optional Warm/Tube coloration, and exact unity when Amount or Parallel is zero. It allocates no memory, grabs no locks, and does no I/O in the audio callback; reports **0 samples of algorithmic latency**.
- Actual per-slot compressor **gain reduction in dB** is published to one atomic after processing and displayed in the native slot graph, updated only on a quantized change. A separate portable CTest covers 44.1/48/96 kHz, all colours and voices, finite stereo processing, bypass and wet blend, and a 10-instance DSP-only wall-clock benchmark.
- *Important release caveats:* No user-supplied real vocal same-take A/B or measured FL Studio latency/CPU yet. A passing synthetic regression is not a sonic verdict. Screenshot/UI geometry smoke and Windows VST3 still must pass at the exact commit. Never claim the uploaded project's entire five-chain engine, analyser, de-esser, or original third-party installer was imported.

## Source evaluation / next experiments

- Worth exploring later: full five-return chain mixer (MAGIC/SPACE/ATTACK/ATTITUDE/PRESENCE), live de-ess/opto meters, manual voice-dependent tonal EQ centres, and true separate de-ess split. Make every control real and test with original vocals, preserve or explicitly migrate host state.
- **Do not import** the other project's release action: it loops through all existing GitHub Releases and deletes them before creating `v1.0.0`, which violates Skell's preservation rules. Keep only useful DSP design ideas.
- The uploaded project's own screenshot is evidence of *that project only*, not a screenshot of the updated Skell Preset editor. Use real current Quick4 / Windows capture and inspect every popup before visual sign-off.
