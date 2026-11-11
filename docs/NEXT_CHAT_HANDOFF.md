# SKELL-PRESET — development handoff for a new conversation

Updated: 2026-11-11. **Always inspect the current `main` HEAD, open PRs and
workflow runs before editing; this handoff is a starting point, not a
substitute for checking the live repository.**

## Resume this project

Repository: https://github.com/skellyruno/skell-preset-vst

Tell the next developer/assistant: *Continue the SKELL-PRESET JUCE 8 VST3
project from current GitHub main, preserving existing host automation,
preset compatibility and current branding. Read README.md and this handoff,
review the recent commits/PRs, make targeted UI/DSP improvements, build and
test after each meaningful change, and only merge validated work.*

## Architecture and current features

- JUCE 8.0.12, Windows VST3/Standalone targets, bundled SkellCore,
  actual-editor headless screenshot renderer and portable C++ DSP tests.
- Serial rack starts with 10 physical slots and can expand to 16; original
  host parameter/slot IDs remain stable. Slot order serialization uses
  4-bit IDs, so 16 is a hard compatibility limit for now.
- Simple four-macro and Advanced rack views, dockable rack, keyboard-accessible
  searchable effect picker, vector module visuals, skull-controlled
  Obsidian/Neon, Cream and Crimson themes, 70/85/100/125% scales.
- A shared SkellCore first-run tour can be replayed from Help. It deliberately
  does **not** switch audio modes or automate parameters.
- A factory bank of 34 original-ID presets, native .skellpreset import/export
  including v1 (10-slot) and v2 (16-slot) compatibility.
- Original/ported-with-permission DSP: EQ, dynamics, de-esser, gate, VoxRack
  Room/Comp and filtered feedback delay, delay/reverb, creative vocal FX,
  Radio/Crusher/Tremolo/Width/3-band dynamics, Chorus/Flanger/Ring Mod.
  VML concepts were independently reimplemented; GPL VML source was not copied.
- No claims of certified true-peak limiting, complete FL .fst import,
  full reference-WAV MATCH, or true through-zero flanging.

## Latest realtime-DSP work

Pull request: https://github.com/skellyruno/skell-preset-vst/pull/2

- Optimizes chorus oscillator computation and per-voice delay reads without
  changing host parameter IDs, serialized state, preset bank or declared PDC.
- Guards denormal sample tails and non-finite sample-rate input.
- Adds original-direct-sine chorus parity, buffer wraparound and host-input
  regression tests at 8, 44.1, 48 and 96 kHz.
- Portable C++ test job passed on the PR; Windows VST3 and screenshot workflows
  were still running when this document was initially created. **Check current
  PR/run status before assuming merge or final success.**

## Verification / release gates

```sh
cmake -S . -B build-tests -DSKELL_PRESET_PORTABLE_ONLY=ON
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

The `windows-vst3.yml` workflow builds Windows JUCE VST3, runs CTest,
a nested VST3 host probe and packages an installable artifact.
The `ui-screenshots.yml` workflow uses the **real** PluginEditor/Processor
to capture and verify its expected editor screenshot matrix.

Still pending for production: FL Studio/Reaper sessions, critical listening
with real vocal WAVs, burst CPU/denormal/automation profiling at low buffer
sizes, click-free graph/order swaps and live PDC transitions, complete MATCH
workflow, parallel lanes, enhanced pitch/formant effects, and release-grade
external VST3-host validation beyond the synthetic fixture.

## Safe continuation rules

Keep old parameter IDs and .skellpreset schema compatibility; don't silently
replace existing effect types. Don't introduce heap allocation, locks, filesystem
operations or GUI work into the audio callback. Keep third-party license
provenance documented. Prefer small, tested commits and inspect screenshots
before judging the UI. Build/tests alone do not establish commercial-quality
sound. Don't claim a Windows or DAW test passed until its logs confirm that.
