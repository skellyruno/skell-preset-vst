# November 2026 — VML & ShallowHost research for Skell-Preset v0.6

## Source inventory

The uploaded `voice-modulation-linux-main.zip` is a C++/Qt/PipeWire
Linux voice application licensed **GNU GPL v3**. It includes Chorus,
Flanger, Ring Modulation, Pitch/Formant Shifting, a curve-LFO parameter
modulator, preset folders, and an effect registry.

The uploaded `ShallowHost-main.zip` is a Rust/GPUI application with a
JUCE-based C++ VST host, licensed **MIT (Copyright 2026 Noktomezo)**.
Notable patterns: per-plugin chain identities, serialized hosted-plugin
state, incremental scanning with status feedback, a grouped searchable
catalog, animation-safe drag placeholders, and separation of message
thread graph mutations from the audio processing graph.

## Implemented in the JUCE VST3 (v0.6)

1. Three **original** realtime effect implementations
   `VocalChorus`, `VocalFlanger`, `VocalRingMod` in
   `Source/chain/ModulationFX.h`; independent 2-channel state,
   preallocated fractional-delay buffers, bounded feedback, zero
   lookahead, and no audio-thread mutex or heap calls.
2. Three rack module types **appended at indices 20, 21, 22**.
   All previous indices 0-19 are preserved. Existing APVTS parameter
   IDs s1..s16, p1..p16, slot order and state format are unchanged.
3. Three full adlib factory configurations (original IDs 0-30 retained):
   `Wide Choir Adlib` (31), `Phantom Sweep` (32) and `Alien Ring` (33).
4. Searchable, category-sorted effect library drawn inside the actual
   JUCE editor. It uses a small text search, scrollable result list,
   keyboard selection, and closes when the button is clicked again.
   It never scans plugins or touches the realtime graph while searching.
   External VST3 loading remains an explicit user-initiated action.
5. Bespoke vector-based interactive graphs and miniature rack icons for
   all three new DSP effects.
6. New DSP regression (`SkellPresetModulationFXTest`), `.skellpreset`
   roundtrip tests for module IDs 20-22, and an expanded real-editor
   headless screenshot suite (84 scenes, plus contact sheet).

## Licensing and architecture

**No GPL-3.0 VML implementation has been copied or linked.** The
algorithms above are independent implementations of standard audio
techniques; the reference was used to identify high-value features.
If future work directly ports VML GPL source, distribution obligations
must be evaluated before shipping SKELL binaries.

ShallowHost's architecture and UI behavior informed the module browser.
No Rust/GPUI UI code is embedded. The existing JUCE VST3 host retains its
explicit user-chosen file workflow; automatically scanning external
plugin DLLs inside an active DAW could hang or crash the host and requires
a separate scanner process and cache first.

**Do not imitate VML's audio-callback mutex.** Its EffectPipeline
acquires std::lock_guard while processing and tick() also acquires a
mutex. Skell-Preset continues its own preallocated, lock-free native
audio path. Chorus and flanger use creative delay taps, but **add zero
reported PDC**; musical time offsets are not equivalent to host latency.

## Verification and remaining limitations

Portable DSP executable was compiled locally with C++17, -Wall,
-Wextra, -Werror and exercised 8k/44.1k/48k/96k, silence, stereo
decorrelation, harsh automation changes and Inf inputs. The regular
GitHub Actions workflows independently compile the full plugin and
render its live JUCE editor.

Remaining: real FL Studio validation, dynamic hosted-plugin hot swapping
and latency reconfiguration, third-party plugin sandbox scanning, actual
music production CPU profiling and human vocal listening A/B.
