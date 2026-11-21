# Skell Studio → Skell Preset integration review (2026-11-12)

## Code and license reality

Skell Studio's imported Lemon Studio engine is largely **Rust**. Relevant reference locations:

- `skell-studio/crates/os-engine/src/effects/eq.rs` (7-band parametric EQ state and modes)
- `skell-studio/crates/os-engine/src/effects/eq/design.rs` (filter design, shelves and cut filters)
- `skell-studio/crates/os-engine/src/effects/eq/equalizer.rs` (filter execution)
- `skell-studio/crates/os-engine/src/effects/dynamics.rs` (dynamics)
- `skell-studio/crates/os-engine/src/effects/reverb.rs` (reverb)
- `skell-studio/crates/os-app/factory-presets/Lemon Parametric EQ/Default.ospreset` (example text preset)

These inspected files identify themselves as Copyright (C) 2026 ItsAlphaNeon / **GPL-3.0-or-later**, and Skell Studio's root LICENSE is GPLv3. Do not assume this code may be copied into a closed-source Skell Preset VST3. Determine the product's distribution license and third-party obligations first. This branch contains **no ported Lemon source**; its new filters are independent C++ implementations inside Skell Preset's existing DSP.

## Already usable together

Skell Preset's existing **External VST3** slot hosts installed third-party VST3 effects in-process. It is not a CLAP-to-VST3 translator and does not load Skell Studio's Rust *internal* effects. Skell Studio's inbuilt effect catalogue is not itself a collection of Windows VST3 binaries.

Skell Preset now includes 8 parametric peaking bands with independent frequency, gain and Q, plus high-pass/low-pass controls. Band selection, graph editing and automation are integrated with the existing 16-slot rack, without replacing old module IDs.

## Preparing for supplied preset recreations

1. Inventory the source preset (screenshots, plugin names and parameter values, plugin order, wet/dry, and sample rate); keep a copy of the input.
2. Map processors to native Skell Preset modules where supported. Do not pretend that an internal Skell Studio plugin has been loaded inside a VST3 slot.
3. For a Skell Studio `.ospreset` EQ text preset, parse and validate `OSPRESET 1`, format and band parameters; frequency follows `20 Hz × 1000^normalized`. The seven Studio bands can be mapped by *frequency* to eight Skell Preset regions only approximately; Studio's low/high shelves, per-band filter types, steepness, HQ and linear-phase modes have no exact equivalent in this Preset EQ revision. Report these mismatches instead of silently changing the tone.
4. Save a separate `.skellpreset` and a per-module equivalence report. Test with sine sweeps, vocal/reference audio, loudness-matched A/B, host automation and phase/latency measurements.
5. Later, if legally and technically appropriate, extract shared **Skell-owned** DSP behind a versioned C++ ABI or SkellCore; no direct Rust/JUCE binary linking is required to do useful independent implementations.

## Audio/DSP acceptance gates

- Every exposed control changes the real processing coefficients, not just the graph.
- The graph shows the exact summed filter frequency response.
- All filters stay finite for silence, denormals, pathological automation and supported sample rates.
- User-saved files roundtrip every new EQ control; earlier `.skellpreset` files load with cut filters neutral.
- Windows VST3 build, host load and GUI screenshots must pass before claiming a ready installer.
