# Skell Preset — STEREO separation handoff (2026-11-25)

Status: **implemented, built, tested, committed and pushed** as `284934d` to
`feature/studio-dynamics-delay-ui-20261121` (PR #90, still draft).
CI run `45597656942` on that commit: **success** (portable-tests and
windows-vst3 both green). The earlier `32a0729` run `45526096512` also passed.

Remaining gaps are listed under "NOT done" at the end.

## What shipped in this working tree

### Native DSP
- `Source/chain/MixerStereoSeparation.h` — new, namespace
  `skellpreset::dsp::MixerStereoSeparation`. This is the supplied Stereo
  Enhancer separation matrix from the private R6 checkpoint, not a generic
  widener:
  - `s = raw/128`, `a = 1 - s/2`, `b = a*s`, `L' = a*L + b*R`, `R' = a*R + b*L`
  - raw range `-96..+96` preserved exactly (negative widens, positive merges)
  - 10 ms **sample-count** smoothing (`lround(rate*.01)`), neutral init,
    exact neutral bypass (`a==1 && b==0` early-continue, no multiply)
  - no delay, no latency, no phase inversion, no added side signal
  - helpers `matrixA/matrixB/peakGainBound/neutral/rampLength`
- `Source/PluginProcessor.cpp`
  - parameter `stereoSeparation` appended **last** in the layout
    (`AudioParameterFloat`, range -96..96 step 1, default 0)
  - `rawGlobal.stereoSeparation` pointer wired in the ctor
  - prepared in `prepareToPlay` right after `micInputGate` and snapped to the
    current parameter value
  - applied in `processBlock` **after** the full rack and the dry/wet mix,
    **before** master output gain and the power/bypass blend, and before
    metering/capture — so any level it adds is visible, not hidden behind the
    fader
  - `loadFactoryPreset` resets separation to 0 for every program
  - export writes it, import restores it
  - legacy host state: `stereoSeparation` added to the `GateLegacyDefault`
    table so pre-R6 DAW snapshots load neutral instead of inheriting values
  - `addProgramIdentityListeners` includes the new ID
- `Source/chain/PresetPackage.h/.cpp` — `Document::stereoSeparation` (int,
  default 0), written under `globals.stereoSeparation`, strict
  `decodeInteger(-96,96)` on read. Absent in older archives → neutral.

### UI
- `Source/ui/SkellStereoSeparationKnob.h` — new `SkellStereoSeparationKnob`
  (real `juce::Slider`, `RotaryHorizontalVerticalDrag`). Matches the existing
  macro/master hardware style: engraved ticks, recessed bezel, satin cap,
  bone-white pointer. Bipolar: the lit arc grows **out of the centre detent**,
  so neutral paints no arc at all. `setRotaryParameters(1.2π, 2.8π)`.
  Double-click returns to 0. Readout = percentage + direction
  (`0% NEUTRAL` / `72% WIDE` / `100% MONO`).
  **`applyReadoutFormat()` must be called after the SliderAttachment**,
  because the attachment replaces `textFromValueFunction` with the host one.
- `Source/PluginEditor.cpp`
  - `stereoSeparation` member + `SliderAttachment` + `applyReadoutFormat()`
  - shown only in Simple mode alongside INPUT/MIX/OUTPUT
  - master strip re-laid out from 3 × 362 px to **4 × 272 px** cells:
    dials at x = 58 / 330 / 602 / 874, y = 570, 74 × 74;
    readouts at `140 + i*272, 584, 150, 30`
  - paint: labels `INPUT / MIX / OUTPUT / STEREO`, units `dB / % / dB / ""`
    (STEREO has no stacked unit — the readout carries `%` and direction)
  - `getScreenshotLayoutIssues()` assertions for the 4th control
    (size, style, no overlap with OUTPUT, no overlap with its readout,
    readout freshness)
  - **important fix**: the readout loop previously walked
    `masterValues.size()` against a 3-entry slider array — that
    out-of-bounds read was a hard crash in the renderer

### Tests
- `tests/test_stereo_separation.cpp` — JUCE-free module test, wired in
  `tests/CMakeLists.txt` as `SkellPresetStereoSeparationTest`. Covers the
  published law at every raw step, identity at 0, clamping, neutral null,
  settled matrix match, mono symmetry, mid/side response, block-size identity,
  zero latency, ramp monotonicity, peak bound, repeated engage/disengage.
- `tests/render_alchemist_native.cpp` — new `--stereo-separation` mode driving
  the **real processor** at 44.1/48/96/192 kHz. Registered as ctest
  `SkellPresetStereoSeparationProcessor`.
- `tests/test_preset_package.cpp` — round-trip assertion for
  `stereoSeparation == -43`, plus legacy default `== 0`.
- `tests/ui_renderer.cpp` — `--extras` now also captures
  `obsidian-stereo-wide-100.png` (raw -96) and `obsidian-stereo-mono-100.png`
  (raw +96) through the real editor.

### Build
- `CMakeLists.txt`: `/utf-8` added for `SkellPreset` and `SkellPresetUIRenderer`.
  Without it MSVC decodes BOM-less UTF-8 sources as the system code page and
  the `★`/`☆`/`·`/`▼` literals reach the UI as mojibake (`â...`).
  This was a **real rendering bug**, confirmed in a capture, not a font issue.
- `CMakeLists.txt`: `/STACK:8388608` + `/utf-8` for `SkellPresetAlchemistRender`.
  The full processor on the default 1 MiB stack faulted before the first check.

## Verification actually run

- `SkellPresetStereoSeparationTest` → all checks passed
- `SkellPresetPackageTests` → passed
- `SkellPresetAlchemistRender --stereo-separation` → PASS at all four rates
- `ctest --test-dir build-local -C Release` → **52/52 passed**
- `SkellPresetUIRenderer --output ui-r9` → **4 scenes, 0 failures**
- `SkellPresetUIRenderer --output ui-full --full` → **128 scenes, 0 failures**
- VST3 built: `build-local\SkellPreset_artefacts\Release\VST3\Skell-Preset.vst3`
  - binary size 5,888,512 bytes
  - SHA-256 `9788FC7181F10901EC8258FE2223A3937CA1EB1797959E898D546C322257C642`
  - zipped to `dist\Skell-Preset-vst3-revamp.zip` (2,506,958 bytes)
- Visual: `crop-stereo-A1.png` (neutral), `crop-wide-B2.png` (`100% WIDE`),
  `crop-mono-B2.png` (`100% MONO`) — arc grows from centre detent, pointer
  tracks, no text collision in the strip.

## NOT done — do these next

1. **The STEREO knob is in the Simple master strip, not "next to MIC".**
   No rotary MIC knob exists in this build — MIC is a 31×29 icon button in the
   rack header (`ChainPanel.cpp:236`, `micRegion {164,9,31,29}`) that toggles
   `micPanel`. Confirm with the owner whether they meant
   (a) alongside that header button, (b) inside the Mic Modeling effect face,
   or (c) the master-strip placement used here. If the header is wanted, the
   rack header has ~28 px of free space — not enough for a 74 px dial at the
   same scale, so the header layout needs rework first.
2. **Numerical parity against real FL Studio is NOT established.** No
   Fruity Stereo Enhancer render or FL mixer separation capture was available.
   What is proven is that the implementation matches the *supplied R6 law*
   exactly. Do not claim FL parity.
3. **"Restoring verified FL Studio mixer stereo separation values"** is not
   implemented. There is no verified source of those values, so nothing was
   invented. Factory programs all load neutral by design. If verified values
   arrive, add them per-preset.
4. **52 protected program identities**: the test only asserts every factory
   program resets separation to neutral; it does not re-assert the full
   55/52-ID ledger. Verify the protected-identity test still covers the count
   the owner expects (`test_factory_catalog_history.cpp`).
5. **Supplied DSP is private.** `MixerStereoSeparation.h` is now in the repo.
   Confirm distribution rights before any public release; the algorithm is
   the supplied enhancer law, so treat it as private until cleared.
6. **Do not commit** `SKELLY_SOUND_MATCH_R7_PRIVATE_NOV24.zip`,
   `_r6checkpoint/`, `_r7checkpoint/`, any `*.png`, `ui-*/`, `dist/`,
   `build-*.txt`, `cmake-log.txt`, `ctest-all.txt`, `build-ui.cmd`,
   `build-vst3.cmd`. All remain untracked.
7. **Save a new private checkpoint to the Library** (R8) containing this
   tree, since the R6 zip on the Desktop is a 0-byte placeholder and the real
   payload was recovered from `Downloads\SKELLY_SOUND_MATCH_R7_PRIVATE_NOV24.zip`.
8. **Not validated in a DAW.** No FL Studio, no Windows VST3 host load of the
   new binary. `--stereo-separation` exercises `processBlock` directly.
9. **Performer/graveyard work from the previous handoff** (commit `32a0729`)
    is still in this tree and unverified by CI.

## Cleanup notes

- The processor's master smoothing (`inputSmooth/outputSmooth/mixSmooth`,
  20 ms) is intentionally block-rate dependent. Do **not** write a
  block-size-invariance assertion against whole-plugin output; it fails for
  that reason alone. Test the module, not the whole plugin.
- `monoSource` in `processBlock` is derived from
  `getTotalNumInputChannels()==1` (declared bus layout), not from the buffer
  content. A 2-channel buffer of identical sides is *not* treated as mono —
  use `setPlayConfigDetails(1, 2, rate, block)` in tests.
