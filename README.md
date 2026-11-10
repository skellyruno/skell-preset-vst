# SKELL-PRESET | ONE PLUGIN. EVERY SOUND.

**2026 development build — real JUCE 8 Windows VST3.** Skell-Preset uses SkellCore's reusable headless renderer and DSP modules. The plugin starts with a 10-slot rack. **+ ADD SLOT** appends a real processing/automation stage, up to 16 total; scroll the rack to see positions 11–16. The new physical IDs s11–s16 never replace the original ten host automation IDs.

## Effects, latency and quality

The serial rack has EQ (high-pass + parametric bell), compressor, gate with hold/hysteresis, **zero-lookahead stereo-linked De-Esser**, saturator, exciter, delay, SkellCore FDN reverb, Adlib Throw, Adlib Doubler, and SkellCore oversampled lookahead limiter. EQ and dynamics add **zero samples of host PDC**; intentional effect delays and tails are distinct from algorithmic PDC. Each lookahead limiter reserves approximately **3 ms**, so serial limiters add latency. Host-aligned dry and bypass paths and the editor's PDC readout use the current chain. SkellCore's limiter is **not independently certified to BS.1770 true-peak standards**.

**Skell Live Lead** omits the limiter for **0 samples reported PDC** during tracking. **Skell Vocal Polish** has a De-Esser plus the 3 ms limiter for mixing. Neither preset controls interface buffer, other plugins or FL Studio mixer latency. Real-world CPU use and audio quality in FL Studio still need host tests and your vocal reference WAVs.

## Presets and compatibility

The factory bank includes Main Vocals and Adlibs; unfinished Vocoding items remain hidden. Import and export native **.skellpreset** archives. The **My Presets** bank lists locally saved/imported native preset packages for quick auditions without closing the menu. Schema-v2 archives retain all sixteen physical slots and the active chain length; schema-v1 ten-slot packages remain readable. Existing DAW state and original 10-slot parameter IDs are preserved.

**FL Studio .fst / third-party preset formats are not directly compatible.** To rebuild your particular main vocal and adlib chains, supply same-performance dry/wet WAV pairs, optional FX-only prints, the FL chain screenshots, BPM/key and plugin settings. See [Reference WAV workflow](docs/REFERENCE_WAV_WORKFLOW.md). Full WAV-driven multi-effect matching remains unfinished.

## UI and rendering

Obsidian/neon-green, Cream and Crimson skins, 70/85/100/125% resize choices, whole-row drag-reordering, a scrollable expandable rack, graphical module editors, visual bypass and preset browser. The headless renderer uses the **actual JUCE editor/processor**, not a mockup. Its matrix covers themes, sizes and modes, plus extended rack and De-Esser/Live Lead scenes.

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

A maximum of 16 slots is deliberate because the current order serialization uses 4-bit slot IDs. Click-free type/order graph swapping, seamless host latency transitions during active transport, multiband/linear-phase processing, parallel lanes, deeper pitch/formant adlibs, full MATCH, direct FL .fst translation, and real FL Studio sessions/CPU profiling remain development tasks. Screenshot success does not prove audio quality.

For the user-supplied VoxRack architecture study and why its Windows APO implementation was **not** copied into a VST3, see [VoxRack review](docs/VOXRACK_REVIEW.md).
