# Club Gate B — owner-selected offline target, native experimental port

Owner approved B ("b wins lets put it in") in Sound Match session ec3659f808d5f4e07046f328. Exact identity club_gate-nov23-excerpt-gate-40_0.25, URI SHA256 d5615054f281a521f6ece8a6a463e4df3a4314b3e848ede7fffdf6a0c107cec6. A remains rollback.

B is a post-gate offline 10 ms centered RMS and smoothed gain stage. The threshold was -40 dB relative to the training excerpt's 98th-percentile envelope, with at most 25% attenuation. The exact centered algorithm requires future samples; this trial instead uses a trailing 10 ms RMS window and trailing 10 ms smoothing (fixed-size rings, linked channels, no added PDC, no audio-thread allocation). Default absolute threshold measured around -57.4 dBFS on the unnormalized source print; preview maps to the existing gate threshold minus 11.4 dB. This is an approximation, not an exact B port or fully calibrated adaptive threshold.

Local portable g++ -O2 -std=c++17 -Wall -Wextra -Werror harness passed at 44.1/48/96/192 kHz (silence, loud gain, release, stereo linked determinism, finite input guards). Real 44.1 kHz Club excerpt, same diagnostic method:
- historical offline A 0.168047 / 0.197552; B 0.136056 / 0.197552.
- causal finish on A, level matched 0.154070 / 0.197552.
- standalone existing native gate DSP on actual RAW 0.435413 / 0.197552; plus causal finish 0.400170 / 0.197552; estimated peak -6.50 dBTP.
The raw-input native approximation still falls short of offline B. No native VST3 render, UI screenshot, FL Studio or CPU certification here. No audio committed.

An opt-in test build can configure -DSKELL_PRESET_EXPERIMENTAL_CLUB_GATE_FINISH=ON. Default OFF preserves existing gate behavior, all IDs, presets, and host states. The option intentionally changes the MIC gate's audio only in that test build. Do not distribute it as stable. Before enabling in production, improve onset matching on raw input, measure CPU and test Windows VST3, recall, pluginval, and FL Studio. Separator should use a separately validated optional cleanup stage, not change the separation model. Keep this PR draft.
