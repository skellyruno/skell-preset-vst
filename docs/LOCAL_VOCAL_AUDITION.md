# Local listening pair export

The native renderer now accepts --audition-starter PROFILE[:VARIANT] INPUT NEW_FOLDER [TAIL_SECONDS] [BPM] and --audition-preset PRESET.skellpreset INPUT NEW_FOLDER [TAIL_SECONDS] [BPM].

Example: SkellPresetAlchemistRender --audition-starter 18:33 dry.wav pluggnb-audition 2 140

Outputs raw wet render, equal-duration A-dry.wav and B-processed.wav (32-bit floating stereo, dry zero-padded for tail), listen.html with actual measured waveform peaks and same-position A/B switching. Match uses aligned source-window RMS, attenuation only, common trim for <= -1 dBFS sample peak including tail. Silence/extreme >12dB mismatch rejected. No true peak/LUFS claim or seamless crossfade claim. Local HTML uses relative filenames, not uploads. Directory staging avoids intentional overwrite; source remains unchanged. The native regression checks matched levels, frame counts, file creation and rejection of existing destination. Browser playback and embedded DAW-editor preview are separate unverified items.
