# Skell Preset rack redesign – developer handoff

Updated 2026-11-11. Read live repository HEAD and status first; this is not a test certificate.

- Source repository: https://github.com/skellyruno/skell-preset-vst
- UI work PR #3: https://github.com/skellyruno/skell-preset-vst/pull/3; stacked on the modulation DSP work in PR #2. Review/merge PR #2 first or retarget before merging PR #3.
- User direction: professional hardware-rack layout, dark midnight-black default with neon green, rounded rectangle buttons, distinct clean skull badge, compact SKELLPRESET title without a dash, readable hardware-style knobs and parameter values, orderly labels, premium module visualizers, clean preset browser with separate cover for every preset and optional custom cover for user presets.

## Changes in PR #3

- Default colour palette now midnight-black with neon green; existing Cream/Crimson skins preserved with skull-click theme cycling.
- Shared JUCE LookAndFeel renders centered hardware rotary knobs and crisp rounded-rectangle buttons, plus themed context menus.
- Original filled skull replaced with a thinner angular vector skull. This remains an in-plugin mark; the user's official shared master SKELL skull is not finalized.
- Four large Simple view macro cards show distinct labels and percentages; Advanced has a distinct macro deck underneath the plugin-specific graph with rack rails and upgraded effect labels. Rendered advanced panel height now separates graph and macro strip.
- EQ controls display physical frequency/gain/Q/low-cut units; other normalized effect params display % rather than raw 0..1. Parameter IDs, automation normalization and DSP are unchanged.
- Preset browser now contains unique deterministic procedural artwork for every factory item; custom artwork for a My Presets item is stored as a local PNG sidecar next to the .skellpreset file, and previewed from a GUI-thread cache. Does not embed artwork in .skellpreset or alter native package compatibility. Cover picker accepts PNG/JPG up to 2 MB and 4096x4096 and writes 240x180 thumbnail with juce::TemporaryFile.

## Code source audit

- Provided Voice-Changer-main ZIP: Rust/Tauri desktop app, MIT. Explore state-management/UI organization and independent DSP ideas, but do not insert incompatible Rust code in the JUCE realtime audio graph.
- Provided FLAMEEWRLD-Studio-Vocal-Chain ZIP: JUCE C++ vocal chain with multiple effects and processor/editor code; no top-level license declaration found. User states permission to reuse; if porting any DSP, record provenance, preserve correct JUCE realtime usage and verify parity. Do not blindly overwrite existing tested rack effects.

## Release validation

- A Windows VST3 test/build, native preset tests and nested hosted VST3 probe succeeded on an intermediate PR #3 commit 7c5becd. This does **not** verify the full latest head; read CI for current commit before release.
- Portable DSP regression has passed on intermediate commits. Linux headless real-JUCE screenshot jobs and latest Windows jobs must still finish and receive visual review. Inspect all available scene captures at 70, 85, 100 and 125 percent, all skins, preset browser, rack-right and module-picker.
- Real FL Studio loading, interaction, DPI and critical audio/CPU checks cannot be certified from CI alone.
