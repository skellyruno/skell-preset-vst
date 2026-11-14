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
- Use --quick4 for four real scenes; preserve full 110-scene deep audit.
- Preserve Simple mode Tone/Punch/Space/Grit and remove them ONLY from
  Advanced. No UI code may rename any automation parameter, factory ID,
  VST3 ID, or saved FL Studio state.
- Maintain Windows VST3, real DSP, low-latency, CPU and FL Studio gates
  independently from visual capture. Keep PRs draft until verified.
