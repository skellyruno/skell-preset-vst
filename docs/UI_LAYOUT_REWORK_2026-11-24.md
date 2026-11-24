# Actual control layout rework — draft PR #90

Owner feedback: previous effects looked unchanged; Input/Mix/Output must remain Simple-only, and their value readouts must update when turned. This supersedes the previous Advanced visibility decision.

- Master readouts: live JUCE labels adjacent to the real rotary sliders; post-attachment value callback + timer fallback, explicit visibility/position/format assertions. Simple-only on paint, refresh, resize and bypass workspace. No DSP or automation changes.
- Native effects: remove stale duplicate card drawing, replace identical small 2x2 sliders for non-EQ effects with four full-height instrument panels. EQ keeps larger precision pairs, dynamics uses two precision faders + two rotary controls, motion/space/tone/pitch use hardware rotary banks, vocal uses dedicated full-height controls. Editable value boxes are larger. Existing graph/page gestures and IDs retained. Layout assertions use new real bounds and text boxes.
- Browser: artwork grid at category roots, 2 or 3 responsive columns, shared paint/hit-test bounds, scrolling and keyboard capacity, count badges, bigger typography, divider. Existing leaf preset list, favorites and manual labels stay intact.

JUCE 8.0.12 git ls-remote attempted this turn: DNS resolution failed. No local render/visual verification is claimed. Exact-head Windows CI and local Quick4 screenshots remain required. Previous compiled preview 30d27c8 is superseded, not a verified final build.
