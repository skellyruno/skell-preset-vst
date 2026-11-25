# DAW-native hosting feasibility — independent pilot

## Motivation / field report
The owner confirms FloopHole can run directly in FL Studio but all their ordinary third-party VST3s still fail inside Skell-Preset. Passing owned-fixture CI does not verify installed products. Do not characterize this as repaired or VST3-hosting-ready.

## What FloopHole actually does (v1.2.0 source reviewed)
- `FLPluginScanner_win32.cpp` discovers user-installed `Plugins/Fruity/Effects/<Name>/<Name>_x64.dll`, falling back to named/unique x64 DLLs.
- `FLPluginInstance_win32.cpp` loads installed effect DLLs via `LoadLibraryExW`, resolves `CreatePlugInstance`, supplies Fruity SDK host callbacks, state, sample rate, transport, editor parent, and renders audio with the native function table.
- `VST3Plugin_win32.cpp` exposes each discovered supported FL native effect as a VST3 class. Its `InitDll` calls plugin discovery and initialization.
- The direct render implementation uses `std::recursive_mutex` around `processAudio`. Do **not** transplant that path into Skell-Preset's real-time callback.
- MIT applies to FloopHole's own code. Native Image-Line DLLs/presets/resources remain licensed separately and are **not** distributed.
- It is **not** a universal DAW-native plugin ABI. Reason Rack Extensions, Logic built-in effects, Ableton devices, Pro Tools AAX, etc. require separate documented plugin formats/hosting paths. Windows VST3 is already JUCE-based; CLAP, if desired, requires a separate compatible host.

## Safe first milestone (this commit)
A separate Windows `SkellPluginDiagnostic.exe` scans/instantiates an ordinary VST3 and renders four stereo buffers **in its own process**, logging the failing stage and exact loader error. The included PowerShell script sets a 25-second timeout and terminates a hung probe. The diagnostic is NOT a low-latency audio bridge.
A **read-only FL native discovery** helper verifies the directory structure and enumerates a bounded inventory of effect DLLs without loading any proprietary binaries. A synthetic fixture is checked in Windows CI. It does not require a functional FL installation to compile; discovery requires licensed local FL Studio files. No generator/instrument hosting is implemented.

## Next engineering gates before any direct FL-native support
1. Run diagnostic against at least two of the user's failing ordinary VST3 effects and collect the first failing stage and exact error, not merely the fact a dialog did not open.
2. Prove standalone FL native effect discovery, then isolated worker instantiation using carefully reviewed FloopHole-derived host callbacks and proper license notices.
3. Implement versioned out-of-process IPC for bounded stereo blocks, parameter changes, tempo, full state and watchdog recovery; account for at least one block of latency and PDC before live monitoring.
4. Benchmark real-time buffer underruns and lock contention and verify the first native effect in a disposable FL Studio session. Only then implement a trusted Skell rack adapter and optional native editor.
5. Keep missing-plugin recall as explicit and fail-safe; never replace a saved plugin class with a different module. Preserve factory, personal, automation, and legacy DAW state.
6. UI and stage work is on separate draft PR #90; keep this experimental plugin-host branch draft and unmerged.

The old in-process FloopHole guard remains enabled. Direct FL native support is **not yet implemented**.
