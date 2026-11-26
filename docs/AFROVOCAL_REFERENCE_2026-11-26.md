# Vocal workflow reference pass — 2026-11-26

Sources: Famous Beats vocal-presets, AfroVocal-Presets README, processor and JUCE safety research; owner-uploaded Vocal Processing Suite README, FilePreview, GainStage and ProcessorTests. No third-party source/artwork copied; no foreign pitch algorithm. Preserve five original starters and factory IDs.

Implemented:
1. Afro Silk Lead. 2. Amapiano Close. 3. Highlife Harmony. 4. Afro R&B Velvet. 5. Dancehall Rhythm.
6. Bounded deterministic variation engine. 7. Gentle intent. 8. Forward intent. 9. Spacious intent. 10. Intimate intent. 11. Nested chooser (640 variations without host program expansion). 12. Reproducible profile:variant CLI selection.
13. Explicit offline host tempo. 14. Source-window wet RMS separate from tail. 15. Tail-only measurements. 16. Active-frame RMS (sample amplitude threshold, not VAD/LUFS). 17. Over-full-scale sample count. 18. Crest factor. 19. Stereo correlation and side RMS. 20. Attenuation-only matched audition trim recommendation with peak headroom including tail. No automatic gain changes to rendered audio.

CLI: --preview-starter 6:33 INPUT NEW_OUTPUT.wav 2 105
The default variant is zero; variant 1–64 is a reproducible coordinated starting point, not AI or a matched artist sound. Default tempo 120 BPM, optional 40–260. Keep actual DAW tempo when playing live.

The upload's actual waveform/player/export UI is a valuable next step, but it must render on an isolated owned worker instead of repeatedly doing synchronous work in the editor. A/B trims are recommendations, not automatically rendered pairs. Neural denoise, dedicated preamp, measured oversampling, room algorithms, and our own SkellyTune integration remain open. No copying unlicensed preset definitions or model assets. No new DSP algorithms in this batch; all profiles use existing native modules.
