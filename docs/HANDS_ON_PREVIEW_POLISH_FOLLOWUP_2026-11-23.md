# Follow-up to hands-on preview feedback — 2026-11-23

This is an incremental native editor pass after a300f82. No factory preset or DSP algorithm change in this pass. No new release approval.

- Header tagline sits immediately beneath the shortened wordmark rectangle; v.01 coordinates unchanged.
- Removed the redundant low-opacity cover overlay drawn above every editor control; covers remain in the preset library.
- Preset, MIC, settings and module-picker overlays now dismiss incompatible overlays before opening. Switching workspace or power also closes transient overlays.
- Settings retain the compact footprint, use consistent hardware action-button typography, include Guide replay and synchronize the size dropdown after host resizing without replacing an active popup.
- Settings dismissal closes its own popup.
- Guide no longer describes removed SkellWorld presence, old gate hold control or invisible targets.
- Undo/redo restores the real selected physical rack slot and Empty Rack home state; native interaction regression covers empty load, undo and redo.
- All spatial effects use a distinct larger-primary/smaller-secondary control bank. Dynamics keep split control decks; tone, motion and EQ retain appropriate layouts. Text boxes resize with knob bounds, not only on slider-style changes.
- Studio Delay shows dry/echo amounts, suppresses zero-energy echo shapes, and visibly distinguishes OFF mode.
- Adlib graph caption explicitly describes settings response rather than advertising an unsupported XY drag.
- Slot peak, GR, de-ess, suite return and spectrum displays share the actual host-audio freshness state. Avoid spectrum refresh when stopped and avoid refreshing an obscured effect editor while the live performer is shown.

Evidence: independent coordinate audit against the 45 registry types and source formulas passed 180 control placements (containment, height and pairwise separation). This is not an actual screenshot check. Prior a300f82 portable tests passed; Windows native compile was still active at the last check. Local JUCE/SkellCore/Quick4 lookup failed and GitHub clone failed DNS. New exact-head Windows build, native interaction fixtures, audio listening and local Quick4 inspection remain required. Preserve the prior workflow so its full macro/latency checks can complete.
