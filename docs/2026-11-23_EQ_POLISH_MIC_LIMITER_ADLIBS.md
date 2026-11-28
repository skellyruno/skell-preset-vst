# Skell Preset — EQ layout + compact workflows (2026-11-23)

Continuation on draft PR #90, sourced from the green Windows VST3 at
`028bf5fbcc300255e07f2deefb0b201a5e7b7fba`. This is a native JUCE
editing pass in the SAME plugin, not a new web/plugin mockup.

## Visible changes

- **All effect headers:** the duplicated effect-name/detail block previously
  painted at x47/y103 and x47/y128 crossed the top tabs, EQ band selectors,
  Q/width control and optional sidechain panel (confirmed in the user-supplied
  Studio Parametric EQ screenshot). The first heading at y29 remains.
  Auxiliary help now uses the dedicated y383..400 metadata strip, below the
  real graph and above the editable parameter cards. Fix applies to all
  effect types, not only EQ.
- **Actual EQ:** keep 8 band tabs, interactive nodes, add/enable/filter-type
  menus, Q, and automation IDs unchanged; the extra title no longer covers
  those. Existing custom family panels and Digital Bones remain available.
- **Limiter:** default to two primary knobs on each studio-limiter page and
  the preserved LIMIT/DYNAMICS page selector. A new UI-only MORE/LESS toggle
  exposes the two remaining knobs and ninth gate-floor control when requested.
  Every existing p1..p9 attachment, automation name, serialized value,
  factory preset and DSP calculation remains unchanged. UI smoke checks
  collapse, expand, host-editability and restoration.
- **Mic popup:** default 324×320 with drag-to-move and resize still working.
  Model page keeps source selection, four model flavors, Amount and Trim in a
  cleaner arrangement. Noise Gate defaults to Threshold and Release and
  exposes all six original controls via MORE; nothing is reset or deleted.
  Native UI tests exercise its compact default, advanced visibility and
  control bounds.
- **Preset navigation:** sidebar now has Main Vocals, nested indented ADLIBS
  below it, My Presets, Artists and Favorites. There is no extra top-level
  bank and no Adlibs option in the genre FILTER dropdown; personal Ad-libs
  tags remain editable in Edit Details. Old factory Adlibs program IDs and
  saved user presets remain unchanged. Existing Adlibs factory and personally
  tagged sounds are discoverable in the direct submenu; factory favorites
  are browsable if starred.
- **Header:** smaller vector Undo/Redo 32×30 and `v.01` just under the T in
  PRESET. Actual binary version unchanged.
- **Skelly theme icon:** unchanged as requested. The full-body skeleton
  performer and its real audio-responsive scope remain; no generated mockups.

## Tests and safety

- Native tests assert never-overlapping effect metadata/graph/limiter page
  controls, 2-knob collapsed/4-knob extended limiter with original p1..p9
  mapping, mic basic/advanced controls, four top-level preset sections with
  directly nested factory and personal Adlibs, library hover/keyboard/layout,
  preserved genre metadata and history control sizes.
- 38 portable DSP tests and CPU/latency diagnostics must be rerun on exact
  head. Build native Windows VST3 and run the actual JUCE UI/editor smoke.
- Do not pretend a build is visually approved without LOCAL Quick4
  current-source screenshots and inspection. Check 70/85/100/125% scale
  and both Obsidian and Crimson; verify the screenshot's EQ header overlap
  is gone, other effect pages have clear tabs, popup is compact/mobile,
  and limiter reveals advanced controls.
- No factory files, DAW states, APVTS IDs, VST3 IDs or original projects
  altered. FL Studio install/recall and real vocal A/B remain independent
  release gates. Never merge this draft PR without verification.
