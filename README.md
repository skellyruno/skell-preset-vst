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

## 322 DAW-informed workflow (optional, no processor replacement)

The source review of 322.BOOTH added a **canonical searchable effect registry**,
**real post-FX live waveform** (no fake/random animation), a host-fed **REC WAV**
button that writes 24-bit stereo files on a background thread, and the new
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

Obsidian/neon-green, Cream and Crimson skins, 70/85/100/125% resize choices, whole-row drag-reordering, a scrollable expandable rack, graphical module editors, visual bypass and preset browser. A **native searchable effect library** replaces the default module selector; search by instrument/effect category, use Up/Down/Enter or click a row, and click the selector again to close. Creative modulation modules have bespoke parameter-driven vector displays (not fake FFT activity). The headless renderer uses the **actual JUCE editor/processor**, not a mockup. Its matrix covers themes, sizes and modes, plus extended rack and De-Esser/Live Lead scenes.

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
