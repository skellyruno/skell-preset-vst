# Skell Preset — native UI redesign: skeletal microphone performer

Date: 2026-11-23 | Continues the existing draft PR #90. This is native JUCE UI work, not a generated concept image.

## User-requested design, implemented in source

- Replace the previous theme-switch skull with a small skeletal vocalist visibly **holding a studio microphone**. The rack's independent MIC model + noise gate trigger remains unchanged.
- Replace the flat head/rib doodle with a full standing, asymmetrical skeletal performer (sculpted forehead and face planes, two orbital cavities, mandible/teeth, vertebrae, clavicles, scapulae, six rib pairs, pelvis, upper/lower legs, arms and gripping hand). Lighting and waveform use real post-rack peak/captured waveform activity, not fabricated visualizer data.
- Remove decorative concentric home rings and the legacy modulation orb. The modulation toggle now exposes **Digital Bones**, a 13-vertebrae/four-control view using real normalized parameters and measured slot peak. Keep genuine EQ node dots/markers needed for editing; do not substitute inoperable artwork.
- More individual effect family styling across all native editors: Dynamics/Space/Motion/Tone/Pitch/Vocal accents, consistent integrated parameter cards, bespoke chart background. Former generic radial charts now have distinct four-control family designs. **Studio Fast Dist** uses a UI-only copy of the same native lookup function to draw actual dry vs processed transfer. Audio thread and host IDs remain untouched.
- Replace the hidden effect-title return-to-Home trigger. The only targeted effect-header action is a small visible **VISUAL** button, and the only Canvas click-away areas are genuinely empty gaps between the rack and editor or the thin empty strips above/below the panels. Clicking an effect, the rack's dead space, a knob or graph does not change pages. Overlays remain dismissal-first.
- Make Undo/Redo native vector curved arrows with hover/focus and dimmed-disabled states, not variable font glyphs.
- Retire the Cream **display/theme choice**. Cycle only Obsidian (saved enum 0) and Crimson (2). Leave legacy persisted ID=1 reserved and render it as Obsidian; no change to host state shape, automation, presets, VST3 identity or factory IDs.
- Replace retired theme/old orb full-renderer scene names with two actual dark theme IDs and Digital Bones. Keep four Quick4 fast scenes and dynamic full matrix scene count, avoid an impossible old fixed 110 total.

## Boundaries and verification

- All graphics, popup behavior and stage capture are message-thread/UI-only. Existing DSP, plugin IDs, published automation, all 55 factory presets, Alchemist 43 / Deuce 46 identities, original presets and FL Studio project compatibility are not changed by this batch.
- In the exact-head Windows Actions workflow require portable tests, VST3 compilation, metadata, native UI smoke, preset import/export and native host validation. Do not claim a verified current VST3 until artifact exists for this head. A PREVIEW artifact is not release verified.
- On a local machine with cached JUCE/SkellCore, run the documented Quick4 local actual-editor capture (not CI-generated screenshots), visually inspect Home + effects + Digital Bones + Obsidian/Crimson + rack left/right and screenshot scale at 70/85/100/125%; fix defects and recapture.
- Real FL Studio project/preset restore, pluginval and recorded vocal DSP listening tests remain independent release gates.

## Specific UI regression assertions

- `SkellIdleDeck::testStageGeometryForTest()` tests 320×240, 580×390, 906×535, 1140×680, 2000×1000 plus an invalid tiny size.
- `Canvas::testEmptyGapReturnRegions()` covers both rack docking positions, top/bottom lanes and excluded effect/rack/header locations.
- `SlotEditor::testHomeReturnHitZonesForTest()` checks that the VISUAL header target cannot collide with graph, rotary controls or bypass.
- Renderer checks 2 actual skins by original persisted IDs (0 and 2), no Cream screenshot, and 4 fast Quick4 scenes.
