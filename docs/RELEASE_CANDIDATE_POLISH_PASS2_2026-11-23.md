# Second pre-release polish pass — 2026-11-23

## Scope
Native JUCE source changes for the existing draft PR #90. The 39 factory programs (including Digital Bones, the only Adlib), module identifiers and APVTS parameter identifiers are unchanged. This pass changes presentation, control handling and visual analysis, not audible effect transfer functions. The following 50 tracked items include bug fixes, visual changes and regression improvements; they are not 50 separate new features.

### EQ and native effect faces
1. Share EQ filter-menu geometry between paint and hit testing.
2. Exclude the three-pixel gutters from filter selection.
3. Require a click inside the actual Remove Node card.
4. Ignore non-left-button EQ menu clicks.
5. Highlight hovered filter rows.
6. Distinguish selected filter shape from keyboard focus.
7. Add arrow, Home, End and Enter/Space navigation with Escape dismissal.
8. Regression-check all six hit targets at three menu widths and their boundaries.
9. Close stale EQ menus on slot changes.
10. Prevent stale filter-type callbacks after module replacement or band removal.
11. Prevent stale remove callbacks from altering another module or disabled band.
12. Resolve right-click EQ targets in both frequency and gain dimensions and reject distant clicks.
13. Dismiss an open node menu on click-away without also changing the graph.
14. Track extended p5–p16 controls in the graph refresh snapshot.
15. Evaluate EQ/hosted-processor changes before the peak-only repaint fast path.
16. Sanitize nonfinite slot-peak input.
17. Stop VST3 scanning and dismiss overlays through their proper cleanup paths.

### Delay and multi-page editing
18. Display the actual selected note division on Studio Delay's tempo-synced face.
19. Distinguish host-tempo and free-time modes.
20. Represent the real dry-level setting in the echo envelope.
21. Suppress echo envelopes when stereo mode is OFF.
22. Use alternating stereo colors only for ping-pong.
23. Clearly label disabled echoes, synced echoes and free-time echoes.
24. Make graph drags read-only on Studio Delay's secondary pages and Alchemist Match's calibration/reference pages, rather than altering hidden p1/p2.
25. Label those read-only graph states explicitly.

### Rack behavior
26. Replace colliding rack visual-state encoding with disjoint physical-ID/type/power/wet fields.
27. Bind rack drag-and-drop to the actual physical effect ID, not the original mutable row.
28. Balance pending wet gestures when the rack is hidden or destroyed.
29. Finish the wet gesture when its effect becomes empty.
30. Reset pointer drag and wheel state when the rack is hidden.
31. Ignore nonfinite wet values and viewport wheel deltas.
32. Use actual rack context-menu card geometry, excluding two-pixel gaps.
33. Disable invalid Move, Bypass and Clear actions in context menus.
34. Clearly distinguish destructive Clear/Remove actions.
35. Close the rack context menu if its target slot disappears.
36. Navigate rack context actions by keyboard without allowing underlying rack reorder shortcuts.
37. Give the rack focus when opening its menu.
38. Balance host automation when double-click resetting wet.
39. Avoid toggling rack power twice during a double-click.
40. Extend native rack hit-target regression for menu boundaries.

### Performer and libraries
41. Cache a single sanitized audio frame for the performer rather than resampling in paint.
42. Smooth real activity with bounded attack/release and regression-check nonfinite input.
43. Add central stage spotlight and more layered gravestone silhouettes/bases.
44. Clear Simple waveform/IDLE status and decay main meters when the transport stops sending audio.
45. Stop the skull-icon animation timer when not hovered or visible.
46. Reset preset sort/filter hover and accumulated scroll; clear stale focus after removing a favorite.
47. Reset MIC drag/resize state on dismissal; ignore non-left mouse actions and dismiss the source menu without beginning a panel drag.

### Actual spectrum and validation
48. Widen the full-rate spectrum capture cursor to 64 bits, avoiding the 32-bit rollover after roughly 25 hours at 48 kHz, and downmix without intermediate overflow.
49. Sanitize and window the input once per analyzer frame, skip silent frames and leave unsupported high-frequency bins empty instead of duplicating a clamped bin.
50. Keep spectrum regression assertions active under NDEBUG and extend coverage for silence, NaN, extreme inputs, six sample rates, Nyquist limits and reset.

## Evidence
- Exact LiveSpectrum.h blob 1056ed97993a6acee3967368fe37bbd81d43564a and test blob ee9f332ddfb2d3e485e585152e13becffdc1bf12 were copied into a standalone local test directory and verified using git hash-object.
- Ran g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror against the exact test and header; PASS: real spectrum, attenuation, silence, invalid samples, extreme values, six rates, Nyquist limits and reset.
- Previous revision 03cd2f3 passed its portable suite and advanced to remaining Windows native fixture compilation. This does not certify the new revision.
- Full exact-head Windows build, native interaction tests, pluginval, FL Studio and listening checks remain release gates. No real Quick4 capture: local JUCE/SkellCore unavailable; git checkout failed DNS. No mockups or claim of pixel inspection.
