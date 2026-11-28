# Skell Preset — pre-release polish pass, 2026-11-23

Owner target: November 24 release. This is an exact-source development batch for draft PR #90, not a release approval or a rendered screenshot. Catalog remains 39 factories: 30 Main Vocals, Digital Bones only in Adlibs, and 8 Vocoding. No personal preset files, module IDs, APVTS IDs, or audible DSP transfer functions are changed by this batch.

## 50 implemented changes

### Native effect editors and graph interactions
1. Space processors receive a four-rotary parameter deck rather than the generic horizontal fader grid.
2. Motion processors receive the same compact hardware arrangement appropriate to their continuous modulation controls.
3. Pitch processors use rotary controls for compact editing.
4. Non-EQ Tone processors use rotary controls; the EQ family keeps precision linear controls.
5. Rotary controls have editable physical-unit value fields beneath each dial.
6. Parameter-card bounds are centralized so the drawing, actual attached controls and regression checks agree.
7. Inactive/reserved control captions are dimmed instead of appearing editable.
8. The Studio Limiter gate-floor caption is hidden when MORE is closed.
9. Classic EQ drag gain now accounts for the Tone macro contribution to the displayed curve.
10. Studio EQ drag gain uses its actual 36 dB parameter range, not the eight-band EQ's 30 dB range.
11. New Studio EQ nodes and dragged nodes use the same gain-mapping helper.
12. HP/LP graph dragging changes cutoff without rewriting its unused gain parameter.
13. EQ node selection uses both axes, not just the nearest frequency.
14. An all-disabled Studio EQ cannot accidentally drag an invisible band.
15. Context clicks no longer write unrelated effect graph or MIC card parameters.
16. Slot changes finish the graph automation gesture on the original physical slot.
17. Host-automated module-type changes end a pending graph gesture.
18. Drag events stop if the target physical slot or module has changed.
19. Editor destruction balances outstanding graph gestures.
20. Native layout regression checks card containment, pairwise card separation and the expected slider style.
21. Studio EQ regression checks that drag gain matches the curve and HP/LP movement preserves gain.

### Live performer
22. Nonfinite/oversized meter input is sanitized and bounded before stage animation.
23. Stage geometry validates the right edge of the full skeleton.
24. Grave markers have subtle engraved details.
25. Low-cost gradient mist adds depth behind the performer.
26. Paired lower-leg bones improve the silhouette.
27. Articulated ankles better connect feet to shins.
28. Visible toe bones make the bare skeleton more complete.
29. Paired forearm bones add anatomical detail.
30. Elbow joints are defined more clearly.
31. Mandible movement follows measured audio energy, with no fake signal.
32. The actual waveform receives a soft glow and a less distracting silent baseline.
33. Status distinguishes LIVE, IDLE and actual master BYPASS.
34. The live lamp has bounded signal-responsive illumination.
35. Bypassed performer remains readable; the master shade and disabled controls still indicate bypass.
36. Visible performer repaints on master-mode refresh, including stopped transport.

### MIC modeling
37. Four profile cards grow from 32 to 40 logical pixels.
38. Different mic silhouettes distinguish the tonal profiles; the UI still makes no measured hardware IR claims.
39. Larger profile names and explanatory captions improve legibility.
40. Unchanged model-card states no longer trigger their own redundant repaint.
41. Source selection returns keyboard focus to the input selector.
42. Escape dismisses the input-source overlay first, rather than immediately closing the whole MIC panel.
43. Host-driven source automation updates the open source-menu selection.
44. Non-left clicks cannot select an input source.
45. The SIMULATE label and input/amount separation clarify signal flow.
46. Input and enable controls have state-appropriate tooltips.
47. Geometry regression checks actual profile height and pairwise card overlap.

### Libraries and controls
48. Module search ranks exact name, prefix and substring matches ahead of fuzzy/category-only results; the result count, category colors and focus outline are clearer.
49. Module/settings/preset/VST3 menus receive tighter click/focus handling: precise close hit targets, page navigation, responsive scale-card bounds, current-row selection after scrolling, hover reset and preservation of selected VST3 file identity through searches/rescans; stale removed VST3 entries trigger refresh.
50. Audio scope jumps to actual capture events (8 loop iterations instead of 512 for a 512-sample block), preserves host-block partition behavior, uses overflow-safe downmix, and avoids unnecessary atomic publication between capture events. Added regression coverage for variable partitions, mono, null blocks, NaN/Inf, reset and ring wrap.

## Validation evidence
- Isolated C++17 harness compiled and ran with `g++ -O2 -Wall -Wextra -Wpedantic -Werror`. The copied scope header was byte-verified by Git blob hash `ddc19d3aaca7f4858cafa98b408bd62ebc98b25e`.
- 400 randomized block partitions totaling 26,214,400 input frames matched the contiguous reference; exact decimated samples, mono, invalid/extreme samples and ring retention passed.
- Independent logical-coordinate checks passed both effect-control arrangements and 11,997 MIC panel dimensions (312–440 by 308–400).
- These isolated checks do not prove actual JUCE drawing or shipping Windows correctness.
- Prior catalog exact-head portable suite passed on `4ef72a0`. New exact-head full portable/Windows fixture, pluginval, FL Studio listening/recall and real local Quick4 inspection are required.
- Local JUCE/SkellCore/Quick4 checkout remains unavailable; GitHub clone failed DNS in this environment. Do not present mockups or old screenshots as current. Do not merge until actual native tests and release gates pass.
