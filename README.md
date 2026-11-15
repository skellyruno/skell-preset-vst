# SKELLPRESET | ONE PLUGIN. EVERY SOUND.

**2026 development build — JUCE 8 Windows VST3 only (no standalone).** SkellPreset uses SkellCore's reusable headless renderer and DSP modules. The plugin starts with a 10-slot rack. **+ ADD SLOT** appends a real processing/automation stage, up to 16 total; scroll the rack to see positions 11–16. The new physical IDs s11–s16 never replace the original ten host automation IDs.

## Effects, latency and quality

The serial rack has EQ (high-pass + parametric bell), compressor, gate with hold/hysteresis, **zero-lookahead stereo-linked De-Esser**, saturator, exciter, delay, SkellCore FDN reverb, Adlib Throw, Adlib Doubler, SkellCore oversampled lookahead limiter, and separately selectable VoxRack Room / Smooth Comp.

The **new modulation suite** adds original, zero-lookahead **Vocal Chorus**, **Vocal Flanger**, and **Ring Modulator**, in addition to Broadcast Radio, Retro Crush, Tremolo, Stereo Width and Three-Band Dynamics. Independent stereo delay buffers are preallocated in `prepareToPlay`; there is no allocation or mutex acquisition in those effects' per-sample paths. EQ and dynamics add **zero samples of host PDC**; intentional effect delays and tails are distinct from algorithmic PDC. Each lookahead limiter reserves approximately **3 ms**, so serial limiters add latency. Host-aligned dry and bypass paths and the editor's PDC readout use the current chain. SkellCore's limiter is **not independently certified to BS.1770 true-peak standards**.

**Skell Live Lead** omits the limiter for **0 samples reported PDC** during tracking. **Skell Vocal Polish** has a De-Esser plus the 3 ms limiter for mixing. Neither preset controls interface buffer, other plugins or FL Studio mixer latency. Real-world CPU use and audio quality in FL Studio still need host tests and your vocal reference WAVs.

## Advanced vocal suite (development PR #5)

New native rack modules include a stereo-linked **Vocal Rider**, tempo-synced **Ducking Ping-Pong** echoes and **Vocal Vibrato** (source-review PR #4); plus an **8-band parametric EQ** (independent frequency, gain and Q per band), serial **FET + Opto** compression, **Harshness Tamer**, and **Adaptive Noise Control**. Each has an effect icon, keyboard-searchable entry and dedicated parameter-driven vector display. New rotary and master-strip readouts use physical Hz, dB, %, ratio or time labels.

An optional, **experimental Studio Pitch + Formant** uses a stereo 1024-point FFT / 256-sample hop with **768-sample reported host delay compensation**. The 768-sample delay equates to 16 ms at 48 kHz. This is not a live autotune or a production-validated transparent formant processor. The Adaptive Noise module is **not neural RNNoise** and has no claim of trained noise suppression. Keep studio pitch out of live tracking chains until actual voice listening and CPU/PDC checks have passed.

Old `sN_type` effect choice automation is preserved exactly. New effects are selected through separately appended `fxExt`, `fxExt2`, and `fxExt3` choices; independent EQ-Q host parameters are appended. Native schema-v2 presets can roundtrip Q arrays, while older files import with safe neutral Q. New factory indices 34–41 follow the preserved original 0–33.

Read the [DSP integration audit](docs/ZIP_DSP_INTEGRATION_AUDIT.md) and [advanced suite release gates](docs/ADVANCED_VOCAL_SUITE.md) for provenance, verification status, and unfinished model/host work. **PR #5 is stacked on #4, #3 and #2**; do not merge out of order or interpret passing portable tests as a released VST3.

## Clean startup, rack wheel and MATCH readiness

New Windows VST3 instances start with ten visible **empty/off slots** (up to
sixteen available) and **zero reported rack PDC**. This changes only the new
instance startup—not the factory presets, recalled audio chains, automation
IDs or saved FL Studio projects. Selecting the Alchemist factory preset
still loads all seven native stages. A new effect chosen for an empty slot
is enabled immediately; replacing an intentionally bypassed effect preserves
its bypass state.

The top-right power control bypasses the rack without clearing its state.
Simple mode retains its original **Tone / Punch / Space / Grit** cards,
rotary knobs, live waveform and whole-number percentage readouts. The four
macro sliders and their entire bottom deck are **removed only from Advanced**,
giving the effect editor a full-height work area. The original published
`macro1..macro4` parameter IDs and their DSP mappings are unchanged for
saved sessions. The three **Input / Mix / Output** master controls stay
as bottom-right rotary knobs in both views with unchanged gain and dry/wet
automation. REC WAV stays hidden, with its historical capture code retained
for compatibility.

Wheel up/down **over an effect row** moves that physical effect earlier/later
in the processing order; scroll over empty rack space or the rail to browse
large chains. Right-click offers Edit, Choose Effect, Move Up/Down,
Enable/Bypass and Clear. Double-click opens the searchable **movable** effect
picker: drag its title to reposition without changing anything in the audio
graph. The rack's top-right microphone icon opens a smaller **draggable MIC**
widget. The new one-jaw vector skull continues to cycle the saved themes.
The tagline is **ONE PLUGIN. EVERY PRESET.**

The offline reference-audio alignment in `Source/match/MatchCore.h` now
compares **overlap-normalized correlation**, avoiding high-energy or short
overlap offsets dominating the candidate search. Test fixtures cover
delayed/inverted signals, level changes, silent inputs and noisy,
transient vocal-like audio. This is groundwork for comparing a processed
plugin recording to a reference; it is **not yet complete automatic
multistage plugin matching**. The MATCH UI still describes that limitation.

## 322 DAW-informed workflow (optional, no processor replacement)

The source review of 322.BOOTH added a **canonical searchable effect registry**,
**real post-FX live waveform** (no fake/random animation), optional host-fed
audio capture infrastructure (no longer shown as REC WAV), and the new
native **Studio Six** factory chain at index 42. All 322 Web Audio effects
already have Skell native equivalents, so **none of the original simpler or
advanced Skell DSP modules were overwritten**. FL Studio supplies all audio
and owns microphone permissions, track arming, playlist transport and
session recording; Skell's WAV capture is an optional post-rack print only.

See [322 DAW comparison and recording limitations](docs/322_DAW_SKELL_INTEGRATION.md).

## Presets and compatibility

The factory bank preserves its first 34 Main Vocals/Adlibs program IDs including **Wide Choir Adlib**, **Phantom Sweep**, **Alien Ring**, **VoxRack Velvet**, and **VoxRack Chamber**. New vocal-suite factory entries extend the bank to 43 programs without shifting historical indices. Unfinished Vocoding items remain hidden. Import and export native **.skellpreset** archives. The **My Presets** bank lists locally saved/imported native preset packages for quick auditions without closing the menu. Schema-v2 archives retain all sixteen physical slots and the active chain length; schema-v1 ten-slot packages remain readable. Existing DAW state and original 10-slot parameter IDs are preserved.

**FL Studio .fst / third-party preset formats are not directly compatible.** To rebuild your particular main vocal and adlib chains, supply same-performance dry/wet WAV pairs, optional FX-only prints, the FL chain screenshots, BPM/key and plugin settings. See [Reference WAV workflow](docs/REFERENCE_WAV_WORKFLOW.md). Full WAV-driven multi-effect matching remains unfinished.

## UI and rendering

Obsidian/neon-green, Cream and Crimson skins, 70/85/100/125% resize choices, wheel-up/down or drag reordering of effects, a scrollable expandable rack, movable MIC and effect picker widgets, three footer master knobs, graphical module editors, visual bypass and preset browser. A **native searchable effect library** replaces the default module selector; search by instrument/effect category, use Up/Down/Enter or click a row, and click the selector again to close. Creative modulation modules have bespoke parameter-driven vector displays (not fake FFT activity). The headless renderer uses the **actual JUCE editor/processor**, not a mockup. Its matrix covers themes, sizes and modes, plus extended rack and De-Esser/Live Lead scenes.

See [UI renderer instructions](docs/UI_RENDERER.md). The screenshot and Windows VST3 workflows upload artifacts to [GitHub Actions](https://github.com/skellyruno/skell-preset-vst/actions).

## Build and tests

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Portable DSP regression tests (do not require JUCE):

```bash
cmake -S . -B build-tests -DSKELL_PRESET_PORTABLE_ONLY=ON
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

Project plugin code/manufacturer and automation IDs remain unchanged. JUCE is pinned to 8.0.12. The repository contains its bundled SkellCore snapshot and the shared headless renderer API. Test modules are never configured or allocated in the audio callback.

## Unfinished / release gates

A maximum of 16 slots is deliberate because the current order serialization uses 4-bit slot IDs. Click-free type/order graph swapping, seamless host latency transitions during active transport, multiband **dynamics**/linear-phase EQ, parallel lanes, verified low-latency pitch correction, full MATCH, direct FL .fst translation, neural noise suppression with a licensed model, and real FL Studio listening/CPU profiling remain development tasks. Screenshot success does not prove audio quality.

VoxRack is now owner-authorized source for carefully selected DSP ports. Its feedback-filtering design is adapted into the stereo Delay and Adlib Throw with zero added PDC. See [VoxRack DSP Port](docs/VOXRACK_DSP_PORT.md) for implementation and regression tests. Windows APO registration and UI hosting code remain separate; see [VoxRack Review](docs/VOXRACK_REVIEW.md).

## Source studies and licensing

- [2026-11-11 source review and design decisions](docs/VML_SHALLOWHOST_REVIEW.md) — Voice Modulation for Linux (GPL-3.0) and ShallowHost (MIT), including what was implemented, intentionally not copied and known limitations. VML's GPL code was **not incorporated** into SKELL source; the modulation DSP is original.
- Previous owner-provided local-mic-effects and audio-mixer project reviews remain at [Imported Effects](docs/IMPORTED_EFFECTS.md).

## Alchemist native WAV renderer (diagnostic executable)

The optional `SkellPresetAlchemistRender.exe` is an **offline test runner**, not a
second VST3 or a substitute for FL Studio. Build with
`-DSKELL_PRESET_ALCHEMIST_RENDERER=ON`. It calls the actual compiled
`SkellPresetAudioProcessor::processBlock` and factory Alchemist chain.

```powershell
.\SkellPresetAlchemistRender.exe --smoke
.\SkellPresetAlchemistRender.exe --smoke-mono
.\SkellPresetAlchemistRender.exe --stress
.\SkellPresetAlchemistRender.exe ".\dry-vocal.wav" ".\alchemist-preview.wav"
.\SkellPresetAlchemistRender.exe --compare ".\alchemist-preview.wav" ".\original-FL-wet.wav"
```

Input is **mono or stereo WAV, 8–192 kHz**. Mono is intentionally duplicated
into the two input channels before running the native stereo chain. Output is
stereo **32-bit floating-point WAV** at the original sample rate; the runner
refuses to overwrite an existing output. Keep the source and your original
FL Studio processed reference untouched. The `--smoke-mono` test generates
temporary mono audio, renders it through Alchemist and checks that the
resulting stereo WAV is readable, finite and non-silent.

The new `--compare CANDIDATE.wav REFERENCE.wav` reports **the first 15 seconds**
of synchronized candidate-vs-original audio: signed RMS difference in dB,
peak dBFS on each file, signed sample correlation, and normalized RMS error
after RMS-only gain matching (without changing either file). Inputs must use
the **same original take, same sample rate and matching sample start**;
otherwise a low correlation does not mean the tonal match is bad. This is a
diagnostic tool, **not true-peak, LUFS, blind listening or perceptual scoring**.

The `--stress` mode measures 5/10/20 serialized full-processor instances at
44.1/48/96 kHz and 64/128/256-sample buffer sizes. It checks output finiteness,
level bounds and host-reported latency and prints DSP callback timings. CI runner
numbers are **not** proof of performance on the user's PC. Alchemist's native
rack should report **0 samples added latency**; vintage lookahead limiters and
spectral pitch are separate, deliberately nonzero-latency algorithms. Actual
A/B matching to the owner-supplied FL Studio reference still requires synchronized,
loudness-matched recordings and host-level tests.

## Persistent local Quick4 UI capture

The source of truth for screenshots is the **compiled current JUCE editor**.
SkellCore now provides persistent quick4.ps1 and quick4.sh launchers
(see skellcore-vst-template/docs/QUICK4_PERSISTENT_WORKSPACE.md).
The source, pinned JUCE and incremental Ninja build cache live under
~/Skelly/Quick4 by default; no need to reclone on a fresh chat. Skell Preset
has an opt-in --quick4 mode with four real views (Simple, Advanced, effect
library and MIC), and preserves the original full renderer test matrix.

Each local capture ZIP is named with the exact source Git SHA and includes
manifest and verification details so a different chat can inspect it
without claiming stale UI screenshots are current. Screenshot workflows
are manual-only. Windows VST3 CI compiles (but never runs) the UI renderer
as an additional source integrity gate; this does not prove FL Studio
compatibility, visual perfection or audio sound quality.

## Studio DSP direct integration and GUI finish

Skell Preset's native **Studio Chorus** now contains a C++17 translation of
Skell Studio's three-LFO, two-band crossover modulation DSP, replacing the
prior native Vocal Chorus engine inside its existing module slot. The rest
of the rack, the original 4 chorus automation knobs and global wet remain. Studio's default dry-plus-chorus output is preserved
within the native slot (not forced wet-only); rack wet crossfades that
complete effect against the original signal.
Reference: Lemon Studio `crates/os-engine/src/effects/chorus.rs`, Copyright
(C) 2026 ItsAlphaNeon, licensed **GPL-3.0-or-later**. Skell Preset's combined
build will require compliant GPL-3.0-or-later source distribution if released.
The existing SkellCore/third-party licenses still apply.

The shared visual treatment has a restrained gradient finish and fine
machined panel highlights. Simple-mode Tone/Punch/Space/Grit remain in the
approved positions, Advanced still omits the macro strip, and the footer
master knobs and MIC widget keep their existing interactions. Renderer/build,
real Windows FL Studio use, CPU, and side-by-side sound A/B are separate gates.
