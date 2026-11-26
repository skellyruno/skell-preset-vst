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

- Owner correction: header must not place global Power next to Settings. Keep preset / power / Live Stage / undo / redo / settings in that order, with no large gap from Stage to Undo. The current chosen position is between preset and Stage. Verify visually at all scales; do not enforce the obsolete Power/Settings adjacency.
