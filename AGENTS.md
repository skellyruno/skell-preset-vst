# Skell-Preset development agent rules

- Follow ../SkellCore/AGENTS.md where available and the shared SkellWorld/AGENTS.md.
- Real current UI means the actual JUCE SkellPresetAudioProcessorEditor
  compiled from a named exact Git commit. No mockup images or prior commits
  may be presented as current.
- Routine screenshots use LOCAL Quick4 only; do not trigger screenshot
  GitHub Actions. Windows VST3 CI may compile renderer source but must not
  capture images.
- Shared launcher: skellcore-vst-template/quick4.ps1 (Windows) or quick4.sh
  (Linux), feeding the persistent Quick4 workspace. See
  docs/LOCAL_UI_SCREENSHOTS.md and docs/UI_RENDERER.md.
- The renderer's default invocation captures the four Quick4 scenes; use `--output <folder>` to choose the destination and `--full` only for the complete deep visual audit. Do not pass an unsupported `--quick4` switch.
- Owner direction 2026-11-26: one rack workspace with a moon-triggered stage drawer for Tone/Punch/Space/Grit and Input/Mix/Output. Retain legacy mode values and all automation IDs/VST3 IDs; never change audio during UI navigation.
- Owner-approved pre-release exception (2026-11-23): remove 16 unwanted factory
  Adlibs and compact host program indexes; retain Digital Bones, all other
  factory recipes, personal files, DSP module IDs and APVTS parameter IDs.
- Maintain Windows VST3, real DSP, low-latency, CPU and FL Studio gates
  independently from visual capture. Keep PRs draft until verified.

- Latest owner correction (supersedes prior layout): Power is the rightmost header control, after Settings. Remove the dedicated Live Stage button. The wordmark returns to the stage; presets sit immediately beside undo/redo. Keep the logo theme switch separate. Verify the exact order at all UI scales.

- Empty effect slots use the native category chooser and existing searchable library; browsing must never change the selected slot until a real effect is chosen. Do not restore the single floating plus on an empty face. Sidechain must remain a direct signal/target/link workflow; no decorative cable maze.

- Owner correction: clean header with preset, undo, redo, Settings and Power aligned to the right. No header latency/signal-path readouts. Wordmark sits close to tagline; version under the final ET. Rack removal is an X, not a bin. No separate stage output peak/readout or waveform rail; actual audio can light the stage. Keep the moon arrow. Use the owner-provided comp-live, eq-live, sat-live and reverb-live references for measured effect visual layouts, not prerecorded graph animations.

- Owner correction: move the live input/output peak meters into the header gap between branding and presets, with a shared dBFS scale. Remove the footer meters and their rail. Keep header latency/signal-path readouts removed.

- Owner correction: header peak meters have no numeric labels or dBFS text; keep measured IN/OUT bars, with a tight four-pixel gap between preset, undo, redo, Settings and Power. Default browser opens aligned under preset controls, clear of the rack; retain manual browser positioning and either rack dock. Anatomical reference: OpenStax Anatomy & Physiology 2e, sections 7.1 and 8.1; original procedural contours, no downloaded art.
