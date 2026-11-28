# Skell Preset — Selected improvement roadmap (2026-11-22)

Owner selected **18 of 20** proposed items: 1–8, 10–12 and 14–20. Explicitly **not selected:** 9 (instant A/B snapshots) and 13 (per-slot gain staging). Do not silently add those features.

## P0 — compatibility, sound and release gates
1. Smart input noise floor detection with user override (proposal; never enable listening/learning without opt-in).
2. Complete independent mic gate with attack, hysteresis and reduction depth. Reconcile preserved draft #94 without changing existing APVTS IDs or fixed factory indices.
3. Frequency-aware vocal-driven wet sidechain, with detector filtering and existing dry vocal preservation.
4. External sidechain host bus only after FL Studio input-bus support and automation/latency checks.
5. Dynamic vocal rider quality testing and controlled enhancements.
6. Optional true-peak protection with oversampling and measured latency/CPU.
7. Advanced independent de-esser harshness/sibilance controls, ensure crash regression is resolved.
8. Same-take reference MATCH architecture (level-aligned, privacy-safe, not a fake similarity score).

## P1 — preset navigation and creative workflow
10. Preset search and Favorites **with Artists**: dedicated Artists category, assign user-owned preset artist in a separate, atomic sidecar, artist search, Favorites category. Preserve the two-item Main Vocals factory whitelist, user preset ZIPs, old Favorites.txt and all factory IDs.
11. Non-destructive opt-in preset variation generator, never overwrite original or silently alter Alchemist/Deuce protected sounds.
12. Safe bounded undo/redo on message thread for user operations; preserve host automation identity and real-time safety.
14. Reference comparison: measured LUFS, true peak, stereo width, tonal spectrum and crest; no invented match accuracy.

## P2 — live visualization, editor and system
15. Pre/post spectral analyzer; throttle paint and ensure GUI work never blocks audio.
16. Gate/ducking gain reduction meters driven by measured DSP state.
17. Clear rack routing visualization without modifying DSP routing or saved order IDs.
18. Fine adjustment, typed values and double-click defaults (extend existing knobs consistently).
19. Multi-scale layout, labels, contrast, keyboard and focus accessibility; actual local Quick4 screenshot → visual inspect → fix → recapture.
20. Explicit CPU/latency monitor with real workload/time labels and sample rate; don't claim benchmark without measurements.

## Current sprint / verification
- Main release stream: draft PR #90; separate safety stream #91; preserved gate changes in draft #94 pending reconciliation.
- First artist work: user library artist sidecar only, not a new VST parameter or preset format. Runtime Linux Quick4 and FL Studio are unavailable in this execution environment; Windows exact-head CI plus real local Quick4 are separate gates.
- Acceptance: factory 0–54 identities, index 43/46, all old automation IDs, original My Presets and saved state unaffected. Blank ArtistTags.txt and missing files must work. Invalid labels never write; failed writes restore in-memory values. Native editor tests and Windows VST3 must pass before merge.
- **No verified implementation is claimed for other selected items** until specific tests and sound/visual evidence are recorded.
