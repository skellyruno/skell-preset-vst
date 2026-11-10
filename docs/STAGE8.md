# Skell-Preset 0.5.1 / Stage 8: automation reliability and UI performance

This stage intentionally keeps the plugin manufacturer/code (`SkAu`/`SkPr`), existing APVTS IDs, the 10-slot serialized chain, and factory program indices unchanged.

## DSP
- Introduced a fixed-capacity `SmoothedBiquad`: coefficient transitions advance once per stereo sample/frame, do not allocate, and preserve separate left/right filter histories. EQ and high-pass coefficients ramp over roughly 12 ms following automation instead of jumping on each block.
- Compressor threshold, ratio and makeup gain are de-zippered with a 12 ms parameter pole. Attack/release envelope behaviour is unchanged.
- Invalid detector inputs (NaN/Inf) are sanitized in the compressor; invalid delay/doubler inputs are prevented from contaminating their persistent feedback buffers.
- Reported-latency policy remains unchanged. Existing dry/wet and bypass PDC tests are retained.

## UI
- EQ, compressor, gate and delay visualizations use relevant X axes; dB/delay plots no longer accidentally display the 20 Hz to 20 kHz frequency ruler.
- The slot graph repaints only when its type/primary parameters/macros change; the chain repaints only on visible state/meter changes, rather than continuously at idle.
- The I/O meters repaint only when their coarse display levels change.
- Window scale is restored from `uiScale` (default 70%), with the selector following manual resize changes.

## Verification
- Seven portable tests at 44.1/48/96 kHz, including a rapid 16-sample automation fixture with injected NaNs/Infs, finite-value checks, output bound and sample-discontinuity guard, and sample-accurate PDC fixtures.
- Portable CMake Release uses explicitly enabled `assert()` so optimised tests cannot silently skip assertions.
- Re-ran tests with GCC/Clang-style `-Wall -Wextra -Wpedantic -Werror` and ASan/UBSan; all seven passed locally.
- The 2-second synthetic stress fixture measures native CPU time but is **not** an FL Studio session profiling claim and does not measure the full VST3.
- Windows JUCE/VST3 compilation and host-level UI checks are separate validation gates; check GitHub Actions before calling the binary ready.

## Still outstanding
- Safe click-free *structural* graph swaps, including type/latency transitions and full parallel-lane PDC.
- True multislot EQ, full vocal MATCH of recorded FL dry/wet WAVs, formant/harmoniser adlib DSP and full sample-library management.
- FL Studio load/unload, cross-host automation and actual UI screenshots at all scales, CPU profiling and golden audio comparisons on real vocals.

## Compiler-warning cleanup (Stage 8.1)
- Explicitly convert module choice to float when forwarding to APVTS (`C4244`).
- Rename the preset-card waveform Y coordinate to avoid shadowing the card position (`C4456`).
- Change float-buffer initialization in shared SkellCore FDN reset from integer `0` to float `0.0f` (STL `C4244`).
- Enable MSVC `/WX` for Skell-Preset's production target so new project compilation warnings fail CI.

## UI safety / CI integration (Stage 8.2)
- Native preset file-chooser callbacks use a `SafePointer` so closing the editor while a chooser is open cannot dereference an obsolete `PresetBrowser`.
- Prevent simultaneous native chooser launches by disabling Import/Export during a dialog.
- SkellCore's consumer-project CI runs core tests and sanitizers; the quickstart plugin *generator* test stays with its template repo, because Skell-Preset deliberately replaces the demo processor.
- GitHub Actions Node 24 runner compatibility: `checkout@v5`, `upload-artifact@v5`.
