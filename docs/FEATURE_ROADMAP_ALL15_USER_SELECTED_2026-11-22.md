# Skell Preset — all 15 selected upgrades (2026-11-22)

The owner selected **all 15** improvements listed in this conversation. This document supplements the *earlier, independent* `FEATURE_ROADMAP_SELECTED_2026-11-22.md` (18 of 20 other proposals); do not silently invalidate that earlier selection. PR #90 is a **draft** implementation stream. Existing factory identities, 55+ program entries, automation IDs, saved VST3 states, FL Studio project compatibility, and Simple Input/Mix/Output are nonnegotiable.

## Ordered work, acceptance criteria and status

| ID | Upgrade | State and practical release acceptance |
| --- | --- | --- |
| 1 | Unique UI for each effect | **Partially implemented:** unified one-face layout and named controls; create/inspect distinct graphics for all native FX with actual Quick4 screenshots, including popups |
| 2 | Directly interactive effect graphs | **Partial:** EQ gestures exist; verify direct EQ/compression/delay/modulation controls map to real APVTS and never introduce fake graphics |
| 3 | Genuine FL Studio external sidechain input | **Not implemented:** prototype optional input bus, reject incompatible host arrangements, verify actual FL Studio routing / zero unintended PDC. Distinct from existing internal wet ducking |
| 4 | More natural microphone noise gate | **Partial:** independent micGateOn and simple Threshold/Release/Reduction; real vocal A/B, chattering/click/soft-syllable tests required. Do not copy GPL'd Skell Studio code without licensing review |
| 5 | Mic-model sound accuracy and UI | **Partial:** real model panel already exists; measure response/level/proximity, listening A/B; keep old preset state |
| 6 | Real compressor gain reduction meter | **Not implemented:** measure DSP GR atomically and draw with rate-limited UI updates; no fake values |
| 7 | Stereo correlation/mono-safe controls | **Partial:** StereoWidth DSP exists, correlation/Mid-Side and mono compatibility tests not complete |
| 8 | Improved de-esser | **Partial:** DeEsser exists; de-esser listen mode, sideband target and sibilance tests required; fix related editor crash before release |
| 9 | Selectable nonlinear oversampling | **Not implemented:** implement and compare 1x/2x/4x CPU, aliasing, latency, true peak, serialization |
| 10 | Inline reverb/delay ducking controls | **Partial:** dry-vocal-keyed wet ducking already works; popup is tested; move inline after proper UI/FL checks |
| 11 | Auto loudness-matched FX audition | **Not implemented:** opt-in compensated A/B, avoid pumping, independent actual output level measurement |
| 12 | Correct and improved Simple macros | **Partial:** Tone/Punch/Space/Grit and confirmed Input/Mix/Output retained exclusively in Simple; validate real DSP macro mapping and existing automation |
| 13 | **Undo/Redo at top** | **First implementation pushed:** bounded 12-state native preset load/import history; Ctrl+Z/Ctrl+Y/Ctrl+Shift+Z. Does **not** yet claim arbitrary knob gestures, rack reorder, or third-party hosted VST3 undo. CI and real UI validation required. Next: safe command transactions for rack operations and gesture commits |
| 14 | **Prettier and simpler preset browser** | **First implementation pushed:** default seven-bank browse view has uncluttered Import/Save/Edit Details; secondary details page reveals artist, manual genre, and cover. No factory or user preset data renamed/deleted. Both layouts checked in native JUCE geometry tests. Actual screenshot aesthetic sign-off still required |
| 15 | Performance/test/release instrumentation | **Partial:** native/portable tests and DSP-only Studio Delay benchmarks exist. Add real CPU/UI frame timing, pluginval, 44.1/48/96k tests, FL Studio preset recall and actual Quick4 visual inspection |

## Actual UI audit gates — no mockups

1. Build the **exact commit** and run portable, Windows x64 VST3, and native JUCE UI smoke.
2. Capture **real local Quick4** at 70, 85, 100, 125% and themes; inspect Simple, Advanced, bypass, input/mix/output, all native effects, MIC model + basic/advanced gate, preset browser default + Edit Details, help, settings, effect library narrow/wide, installed VST3 browser, Studio EQ filter node, wet sidechain and host editor overlay.
3. Visually inspect each screenshot for overlap, clipping, wrong contrast, readability, alignment, scaling, art bleed, hover/focus, and responsive behavior. Fix source, then **recapture and inspect**. Native geometry tests alone are **not** screenshot sign-off.
4. Use real audio reference files, host automation, FL Studio recall, pluginval, latency/CPU and adversarial DSP runs before stable release. Keep PR draft, never merge unverified work.

## This pass (code, not user-visible release claim)

- Fixed the effect picker name/category collision at its 340px minimum. Added geometry regressions for the effect picker, installed VST3 popup, settings, EQ node menu, wet duck panel, and both preset-browser pages.
- Reduced static Settings repaint work; retained native per-slot button/parameter control semantics.
- Added native JUCE smoke that enumerates all built-in effect types (except external VST3 and the legacy mic slot, which have separate tests) and checks their inline controls and popup bounds.
- All code is on draft PR #90, never a merge. Native screenshot inspection is **blocked** unless local Quick4 Windows/Linux access becomes available; a CI compile alone does not count.
