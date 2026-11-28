# Skell Preset — screenshot follow-up and performer refinement

2026-11-23. Continuation of draft PR #90; no new plugin or replacement UI.
This commit is intentionally separated from the initial screenshot layout fix
so its real Windows compilation and native UI smoke run against the exact head.

## Real screenshot: second visible defect fixed

The user's Studio Parametric EQ (Plain) screenshot shows the tiny upper-right
Q/width value as clipped `WIDTH 0...`. It was produced by
`eqQSlider.textFromValueFunction` returning `"WIDTH "+value` while the
actual JUCE TextBox was 59 pixels wide. **BAND N WIDTH** is already printed
above. The original slider now displays the same **real normalized width**
as a short numeric value (e.g. `0.63`), without the redundant prefix.
Eight-Band Q also uses its nearby Q label and a short numeric value.
Neither parameter mappings, ranges, host automation nor text parsing were
changed. New native regressions verify round-trip numeric editing for both.

## Premium performer without fake movement

Refined the full-body native JUCE skeleton with angular stage-coat silhouettes,
a fabric/metallic tonal plane, a more sculpted skull forehead and cheekbone,
and controlled edge lighting keyed to **real captured audio energy**.
The skull microphone theme icon and the real stage microphone stay unchanged.
The character does **not** pretend to animate or advise users: an interactive
chain-building assistant is a separate future design, not implemented here.

## MIC min-size layout correctness

At the minimum user-resizable 312×308 pixel panel, the original compact
TRIM text box extended to y=294 while the descriptive footer started y=292.
Repositioned TRIM four/six pixels upward. The popup remains movable and
resizable; threshold/release remain visible in basic gate mode, and all six
gate controls remain in MORE. The original APVTS assignments and settings
persist. Native smoke now tests actual child bounds/overlaps at 312×308,
324×320 and 440×400 for MIC and both gate modes, restoring page/size.

## Test robustness with real user libraries

The direct Main Vocals > ADLIBS section includes factory Adlibs and existing
personally tagged Ad-libs sounds. Its test formerly assumed a pristine library
with zero personal Adlibs and incorrectly demanded the total list equal the
factory-only count. It now verifies the factory subset **and** validates all
additional personal entries by their unchanged genre metadata.

## Independent release gates

Require exact-current-head 38/38 portable DSP regressions, native Windows
VST3 + UI smoke, real host projects/presets, and CPU/offline WAV checks.
Real local Quick4 screenshots remain unverified until the editor is built
with the pinned JUCE/SkellCore sources and genuine captures are inspected
and repeated after defects. Do not replace those with simulated artwork.
Never merge unverified draft work. Preserve factory program IDs, state,
APVTS, VST3 identity, pluginval and DAW project recall.
