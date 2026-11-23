# Skell Preset — dark luxury polish and four-section preset library

2026-11-23. Continues the **existing** draft PR #90. This revision is
UI-only; no oscillator, audio process block, latency, APVTS parameter,
program number, factory preset file, plug-in ID or saved state format changes.

## User-visible work

1. **Retired the separate Ad-libs preset browser section**. The library now
   shows Main Vocals, My Presets, Artists, Favorites. This hides historical
   factory Adlibs entries from browsing, including Favorites and search,
   but **does not delete or renumber** those factory programs. Old FL Studio
   projects can continue to recall their original IDs.
2. **Digital Bones is retained** as the alternative effect modulation
   visualization. It uses real control values and the measured FX peak;
   it was NOT the removed modulation orb. Do not remove it.
3. Existing personal preset files, user-created categories and
   manually assigned `Ad-libs` genre metadata are preserved: users can
   still find those **personal** sounds under My Presets, Favorites or
   their existing genre filters. No file migration or deletion.
4. Obsidian and Crimson share a less harsh high-contrast palette with
   charcoal, bone-white text and measured neon accents. Legacy Cream
   theme enum 1 remains read-compatible, rendered as Obsidian.
5. UI panels use softer border contrast, satin-black top lighting and
   a thin lower shadow rather than thick repeated neon outlines.
6. Simple-mode hardware knobs receive tactile scale marks, a recessed
   metal bezel, hover/drag-state accents and visible keyboard focus.
   Their JUCE slider controls, values and host automation gestures are
   untouched.
7. Rack effect cards use a subtle directional gradient and fine
   illuminated edge for the selected physical effect. The wet knob,
   scroll rail, wheel-to-reorder behavior and power switch retain the
   same hit boxes and parameter IDs.
8. Fix exact-source Windows MSVC /WX compile blocker:
   `SkellIdleDeck::paint` no longer declares unused `height`,
   retaining the same full-body skeleton/microphone and live scope.

## Test/review gates

- The native UI regression `testMainVocalsPresetFilterForTest()`
  verifies the 2 approved Main Vocal factory choices, a 4-section
  browser, exclusion of the historical Adlibs factory programs, and
  continued availability of personal preset genres, artists and
  favorites without writing user files.
- Keyboard page navigation, sorting, layout, cached category counts,
  Undo/Redo and real DSP regression suites remain in the Windows
  workflow. Native UI smoke **must** pass on the exact commit before
  distribution; prior portable success is not sufficient.
- Quick4 requires a real local build of the current JUCE editor and
  genuine screenshots for visual inspection; a CI PNG or generated
  concept image cannot substitute. Capture both themes and small/large
  scales, verify the microphone performer, Digital Bones, rack hit areas,
  browser tab removal, and selected/disabled/hover states.
- Preserve user presets, legacy program identities and FL Studio recall.
  Never merge draft PR #90 without verification.
