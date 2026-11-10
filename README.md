# SKELL-PRESET | ONE PLUGIN. EVERY SOUND.

**Stage 5 / 0.5.0 development build** — all-in-one vocal-effects plugin based on the Skell VST template and pinned SkellCore v0.2. This is a real JUCE 8 plugin codebase, not a rendered design mockup. The 10-slot processing chain, edited slot parameters, UI shell, factory browser, rack gate, four audible macros and state persistence are implemented. **The full requested seven-stage feature set is not complete. Windows VST3 and Standalone targets compiled successfully in GitHub Actions at Stage 3; Stage 4 Windows VST3, Standalone and native package security tests passed in GitHub Actions run 42405769660. Neither build has been DAW-tested in FL Studio.**

## What works now

- VST3 and Standalone CMake targets, dedicated IDs `SkAu`/`SkPr`, bundle `audio.skell.preset`.
- Ten serial slots. Choose a module from the right-side dropdown or a slot's context menu, toggle it on/off, adjust four control knobs + per-slot WET, and drag a handle to reorder.
- EQ: high-pass + one peaking band (one visible draggable EQ node). Compressor: threshold, ratio, attack/release. Gate: threshold and release. Saturator, exciter, zero-lookahead soft limiter, delay and SkellCore FDN reverb.
- Input/output trim, global gate, dry/wet mix, delta, and 20ms ramped global bypass and per-slot wet/bypass transitions. Smooth delay read-head time changes over 20ms to reduce clicks from automation. Stereo and mono audio input/output. ProcessBlock reads pre-cached atomic parameter pointers and performs no dynamic allocation.
- Four *functioning* macros: **Tone** adjusts EQ boost, **Punch** adjusts compressor threshold, **Space** adjusts delay/reverb wet, **Grit** adjusts saturation drive. The macro destinations are presently fixed. Parameter IDs for future macros 5-8 already exist but are not routed.
- 24 original factory preset *settings* across Main Vocals, Adlibs and Vocoding banks. Bank and name search, art cards. Vocoding bank entries currently load effect-chain settings, **not a vocoder**.
- Screenshot-inspired navy/green vector editor, EQ display, 4 sizes (S/M/L/XL), SkellCore LookAndFeel, original six cover images.
- DAW state serialization through APVTS + ValueTree metadata for order, name, and program.
- MATCH offline **portable** harness tests latency alignment, polarity, gain, invalid/silent input handling, fractional-lag and advisory drift estimation, and a restricted EQ/compressor/delay search. The restricted synthetic test scores over 95%, but **only with known band/Q and envelope priors**; this is not a demonstrated real-vocal general MATCH engine.

## Not yet implemented

The MATCH WAV drop/capture wizard, robust cross-correlation drift correction, multistage spectral/dynamics/pitch inference, full multi-resolution optimizer/report, editable user banks, favorites, audible demo auditioning, 3-lane parallel mixer, true-peak latency/PDC, host-driven vocoder, advanced adlib stack/throws, ADAPT, dynamic macro mapping, the per-slot A/B workflow and all 19 requested advanced modules. `autoGain`, `adapt`, `laneMode`, `laneX_*` and macros 5-8 are **reserved parameters with no DSP effect**. The 24 factory names/styles are included, but some names refer to future capabilities; they do not imply the unavailable functionality is present.

We keep these inactive rather than simulating or mislabeling them. The current limiter is a basic zero-lookahead soft-clipper, not SkellCore's experimental true-peak module. The SkellCore FDN is the shared prototype, not the production MasteringEngine.

## Dependency policy / SkellCore

The application follows `skellyruno/skell-vst-template` (`CMakeLists.txt` / JUCE 8 style), and FetchContent pins SkellCore at commit:

`1f759e4f6ab08d950aaccfc2dfc0415825461ff1`

Current shared target: `SkellCore::DSP` and `SkellCore::UI`. JUCE is pinned at `8.0.12`. The GitHub repository contains the template's bundled SkellCore source. Standalone source ZIPs without that folder fetch the immutable SkellCore revision. The plugin has its **own** plugin ID. No change is made to the template repo or other SKELL plugins.

## Windows build / packaging

On Windows 11 with Visual Studio 2022 or 2026, CMake 3.22+, Git and internet access:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Alternatively download/clone JUCE 8.0.12 locally and pass `-DJUCE_PATH=C:\path\to\JUCE`. If SkellCore already lives on disk, pass `-DSKELLCORE_PATH=C:\path\to\skell-vst-template\SkellCore` to build without that network FetchContent. The CMake-generated `.vst3` is normally under `build/SkellPreset_artefacts/Release/VST3/`, the Standalone under `build/SkellPreset_artefacts/Release/Standalone/`. The actual exact folder depends on generator and JUCE version.

The source is already uploaded to [`skellyruno/skell-preset-vst`](https://github.com/skellyruno/skell-preset-vst); use **Actions → Skell-Preset | Windows VST3 → Run workflow**. The workflow runs portable tests first, then compiles VST3 and Standalone on `windows-latest`, and provides a VST3 `.zip` artifact.

## macOS build

With Xcode Command Line Tools, CMake 3.22+ and internet access:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

JUCE's Standalone format supports Apple Silicon and Intel depending on the CMake architecture. VST3 remains the primary packaged target.

## Portable test harness (no JUCE needed)

```sh
cmake -S . -B build-test -DSKELL_PRESET_PORTABLE_ONLY=ON
cmake --build build-test --parallel 2
ctest --test-dir build-test --output-on-failure
```

1. `SkellPresetDSPTest`: finite EQ/compression/echo/limiter output at 44.1, 48 and 96 kHz.
2. `SkellPresetMatchTest`: alignment, polarity inversion and gain recovery of a synthetic same-performance pair.
3. `SkellPresetRestrictedChainFitTest`: constrained recovery of a known Biquad + Compressor + Delay render; the synthetic example score is >95 and reports estimated settings. Real-vocal field tests are **still needed**.
4. `SkellPresetAutomationTest`: 48 kHz rapidly automated delay-time changes, finite stereo output, bounded feedback, and repeatable reset.

## Audio routing / parameters

`Source/PluginProcessor.cpp` owns all APVTS parameters and serializes the host's state. Each of the 10 persistent processor objects is prepared once in `prepareToPlay`. The chain ordering is a **single atomic packed integer** (4 bits per slot), allowing a reorder from the UI without locks or intermediate duplicate slots on the audio thread. Slot module `type`, `on`, `wet`, and `p1`–`p16` are stable host parameter IDs; the current module gives each generic parameter its context. All 16 are retained even if a module uses fewer controls.

Every processing callback reads its pre-cached atomics once per block and traverses the ordered slots. Input, output, mix and bypass are linearly smoothed over 20ms. All current modules report zero lookahead/host latency; the FDN and delay produce time effects but do not require delaying the dry path for PDC. This does **not** fulfill stage-2's dynamic latency management yet.

### Preset state and native package schema

Current factory presets are defined in `Source/chain/FactoryPresets.h`. Project/host persistence uses a ValueTree snapshot containing fixed APVTS parameters plus `slotOrder`, `presetName` and `program` properties.

The working Stage 4 `.skellpreset` ZIP contains exactly one `preset.json` (`schemaVersion: 1`). Save and Import buttons in the browser use `PresetPackage`, validating and applying identity, 10 ordered slots, 16 normalized parameters each, macro values, and selected global controls. See *Stage 4: native .skellpreset import/export* below for security limits. The future full schema may include `{tags,input_profile,loudness,match_report}` and optional cover, IR, samples and notes, subject to explicit version migration and asset validation. This is distinct from the host's DAW-state blob.

### Macros

The present stage uses fixed mappings:

| Macro | Effect |
|---|---|
| Tone (`macro1`) | all active EQ slots, peak gain offset ±4 dB |
| Punch (`macro2`) | all active compressors, threshold offset ±6 dB |
| Space (`macro3`) | all active delays/reverbs, wet factor 0.5–1.5 (clamped) |
| Grit (`macro4`) | all active saturators, drive offset ±5 dB |

Later a ValueTree `MacroMap` should map `{slot_id,param_id,min,max,curve,invert}` targets, smoothing per parameter and reflecting effective mappings in the editor. Do not ship the placeholder `macro5`…`macro8` as if they already work.

## Seven-stage implementation plan

1. **This archive**: parameter layout, theme, real serial audio chain and a subset of DSP modules, 24 factory signatures, browsing shell, restricted MATCH tests.
2. Race-free graph hot-swap and 20ms equal-power crossfade between graphs, accurate PDC and aligned dry/bypass, delayed cleanup, host regression tests.
3. Port and test existing SkellCore and SKELL plugin engines rather than recreating superficially equivalent implementations; add remaining modules with true display/latency contracts.
4. Implement secure zip preset/bank packages with import/export, editable metadata, cover art and preview audio, user bank storage and test migrations.
5. Mutable macro maps, bidirectional automation/parameter UI, smoothing and authoring UI.
6. Full MATCH: audio pair import, offset and drift, EQ/dynamics, nonlinear and time FX, pitch/formant, global optimizer, robust confidence report and real fixture benchmarks.
7. Adlib tools, MIDI vocoder, 3-lane graph, ADAPT, gain-matched preview, skins, CPU/load profiling, A/B tests in FL Studio/Reaper and release readiness.

## Assets and licensing

Six original placeholder covers and an original skull symbol are in `assets/`. The screenshot is used only as a layout reference, not embedded. The plugin uses system sans fonts (OS fallback), not redistributed font files. Use original or licensed recordings for future preview/matching packs. SkellCore third-party backend licensing remains owned/declared in the pinned upstream dependency.


## Stage 4: native .skellpreset import/export

The factory browser now has **IMPORT** and **SAVE PRESET** actions for native files. A `.skellpreset` is a ZIP archive containing exactly one UTF-8 `preset.json` document. The Stage 4 schema (`schemaVersion: 1`) stores the preset name, author, bank, 10 module slots in their signal order, per-slot enabled state/wet/16 parameters, 8 macro values, and global input/output/mix/mode controls. There is **no external plugin preset loading** or third-party asset redistribution.

**Security:** The reader never extracts files, rejects any archive with extra entries, unexpected names or symlinks, caps compressed archives at 4 MiB and the single uncompressed JSON entry at 256 KiB, and requires in-range finite control values, valid unique slot IDs, and an exact version. Invalid packages are rejected transactionally before changes reach the live chain. Export uses a temporary file and atomic replacement so a failed save doesn't clobber the old file.

**Known limits:** This is a small, deterministic Stage 4 package *subset* with only `preset.json`; covers, IR WAVs, samples and multi-preset banks are not implemented yet. Preset import applies parameter changes on the UI thread but the current fixed-slot architecture has not implemented module-graph hot swaps or latency reconciliation. The `.skellpreset` roundtrip and malicious ZIP test executable is registered as `NativePresetPackage` in JUCE CTest builds.

## Stage 5 — Compact UI, real rack gate, first adlib DSP, PDC (November 2026)

- Default editor at **70%** (818 × 482), preserving the 1168 × 688 internal layout. Available scales are 70/85/100/125% with a fixed aspect ratio.
- Preset selector now opens and closes on second click. Escape, outside clicks and mode changes dismiss the browser. Vocoding mode and factory bank entries are hidden, not deleted: legacy mode parameter indices and factory program IDs remain unchanged.
- The old top-bar Noise Gate is removed; **Gate is a normal bypassable rack module**. Obsolete `globalGate` remains as an inactive parameter ID for backwards host compatibility.
- Adds **Adlib Throw**, a phrase-end replay using SkellCore PhraseTracker + delay, and **Adlib Doubler**, a stereo micro-delay with decorrelated modulation. Factory Adlibs instantiate both. These are not formant shifters or MIDI triggers.
- SkellCore's experimental lookahead TruePeakLimiter replaces zero-lookahead soft clipping. Each Limiter reserves its fixed lookahead, even when bypassed. Per-limiter wet/dry, master Mix and Bypass paths delay-match this latency; the processor reports the sum to the host on structural edits and state load. Not BS.1770-certified.
- All limiter, PDC, throw and doubler buffers are preallocated in prepareToPlay. Structural module type changes are **not realtime automatable** in this version; editing a chain may require host realignment. Parallel lanes and crossfaded graph swaps remain unfinished.
- Five portable tests include sample-exact dry/bypass PDC and finite stereo doubler output. Real Windows VST3 compilation and FL Studio runtime test must still validate these changes.
- Advanced formant/pitch adlibs, MIDI throw triggering, full MATCH reconstruction and independent 3-lane PDC are future work.
