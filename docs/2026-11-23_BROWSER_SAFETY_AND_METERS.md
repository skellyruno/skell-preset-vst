# Skell Preset — preset browser safety and accurate meter polish

2026-11-23. Continuation of the existing draft PR #90 from exact passing
Windows VST3 source `76a6d168`. No new factory presets, DSP algorithms,
latency, APVTS IDs, VST3 identities, user-file deletions or state migrations.

## Root-cause fixes

1. **Critical preset card colour-palette overflow:** the prior four-item
   `std::array<juce::Colour,4>` was indexed using `seed%5`. Alchemist
   seed 44 and many other cover seeds could read index 4 out of range.
   Preset cards now use a single bounded `artworkPaletteIndexForTest()`
   helper, exercised by native UI regressions for seeds 0, 3, 4, 9, 44
   and 45 (the tests check that the same helper used by paint is safe).
   This fixes an actual out-of-bounds source bug, not a cosmetic concern.
2. **Edit Details click-through:** clicking the background of the Edit
   Details page could also trigger the hidden library `mouseDown` card
   logic and audition another preset. The preset card hit predicate now
   strictly rejects that page; the library card painter returns before
   rendering behind Edit Details controls.
3. **Incorrect preset card hit targets:** row gaps, right-hand outside
   margin and right-clicks are no longer interpreted as load requests.
   Actual clickable regions now match the 56px-tall visible sound cards,
   including the independent Favorite star action. Pure native UI tests
   assert inside/outside edges and the Edit Details exclusion.
4. **Preset card material polish:** card faces use a restrained
   top-left to bottom-right smoked-metal gradient and inner highlight,
   without image file access during paint or extra audio-thread work.

## Accurate, restrained global meter response

- Existing simple-mode IN/OUT hardware meters now display real sample
  peak **in a -60..0 dBFS bar scale**, rather than raw linear `0..1`
  amplitude divided into 12 steps. Quiet vocals at -40 dBFS show four
  bars rather than appearing dead.
- Attack immediately shows a newly encountered peak. Release uses a
  measured-input-only exponential UI follower (0.84 per 30 Hz tick),
  smoothing visual decay without delaying, filtering or changing DSP.
- Last segments turn amber near 0dB; the top segment turns red at
  overload, with a targeted repaint when clip status changes even
  without a change in the number of lit segments.
- Values from NaN/Inf or hostile host input are sanitized on the
  message thread. No new timers, audio-thread allocations or locks.
- Native UI smoke tests exercise silent/-60/-40/-20/0dB scaling,
  transient attack, monotonic decay, overload and nonfinite guards.

## Required verification

- Re-run exact-source Windows VST3 build, 38 portable tests, native UI
  smoke, CPU diagnostics, preset ZIP, and factory recall checks.
- A green build does not prove the latest UI look: local Quick4 must
  capture and inspect current-source real JUCE output (not mocks and
  not routine GitHub Action screenshots). Check card highlights,
  Edit Details, simple meters, Crimson/Obsidian, Digital Bones,
  main mic performer, rack scrolling, 70/85/100/125% layouts.
- FL Studio recall, real host CPU and recorded vocal A/B remain
  independent unverified gates. Draft PR #90 remains unmerged.
