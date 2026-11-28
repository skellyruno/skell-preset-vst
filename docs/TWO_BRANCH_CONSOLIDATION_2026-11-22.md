# Skell Preset: two active development branches
Date: 2026-11-22

## Active work (maximum two)
1. **Plugin release candidate:** `feature/studio-dynamics-delay-ui-20261121`, draft #90. Consolidates the complete earlier #92 AdlibFX/Studio DSP/UI plus new opt-in vocal wet-return ducking. Do not merge to main until exact-head portable + Windows VST3 + compiled JUCE/audio tests, real Quick4 screenshots and FL Studio recall/listening gates pass.
2. **Repository administration safeguards:** `audit/repo-safeguards-inventory-20261122`, draft #91. Its exact-head portable + Windows CI passed; review before merging. **main branch protection remains OFF** and requires an administrator action in issue #93.

## Preservation of other work
- **#92:** Its full Git history is included as the **second parent** of a non-rebased merge commit in #90. Keep the original branch/head and PR available as historical reference even after closing the superseded draft.
- **#8:** Shared SkellCore AdlibFX CHOP, MOTION, FILTER and SATURATE were reconciled as append-only module IDs 39–42 with actual DSP, native UI and portable package support in #90. The old PR and branch remain available for source comparison; do not delete.
- **#89:** Factory 55 Alchemist A/B remains an experimental measured trial. Preserve the original PR/branch and audition artifacts; do not promote Factory 55 or silently overwrite Factory 54.
- Earlier 41 closed-unmerged PR refs remain under archive review, and original factory 55 program identities, original 43 host ordering, presets, APVTS IDs and FL Studio state must survive.

## Latest performance evidence (Linux GitHub Actions)
- Studio Delay, 48 kHz, two channels, direct per-sample DSP with no JUCE/DAW callback:
  - 8 independent active instances x one audio second = **105.994 ms total wall**, **13.25 ms per instance**, **18.20 MiB** estimated delay buffer storage.
  - 16 independent active instances x one audio second = **213.728 ms total wall**, **13.36 ms per instance**, **36.41 MiB** estimated delay buffer storage.
- Numbers are **kernel diagnostics on GitHub CI**: not FL Studio DSP %, not a VST3 realtime/host callback benchmark, not a comparison against old DSP (no identical-hardware baseline).
- Source: [workflow 45027817983](https://github.com/skellyruno/skell-preset-vst/actions/runs/45027817983), portable-tests job. Audio/CPU improvement not yet established by A/B.

## New wet-return sidechain
- User-selectable **ON, DEPTH, THRESHOLD, RELEASE** per physical slot; OFF for all factory and legacy imports. New host IDs are appended (not renumbered).
- Applies to Reverb, VoxRack Room, Delay, Studio Delay, Adlib Throw and Ducking Ping Pong. Linked envelope listens to the **pre-rack dry vocal**; no secondary host bus or additional lookahead/PDC.
- Studio Delay attenuates its internal echo gain, preserving the independent DRY setting. Other native effects blend their output's difference from the dry signal.
- New `.skellpreset` fields remain optional in schema 2. Invalid fields reject entire import.
- **Unverified:** real FL Studio sidechain host routing (not implemented), real Quick4 screenshots, listener taste test and CPU across 5/10/20 complete VST3 instances.

## Merge safeguards
Never merge unverified audio/UI changes into main, force push, delete historical branches, renumber host automation, or release a Windows build without native GUI/audio testing. 

## Ring-buffer indexing CPU optimization — Nov 22
- Replaced runtime-modulo operations in per-sample Studio Delay read/write paths with equivalent conditional ring wrapping. No ring sizes, tap interpolation, feedback paths, UI parameters, state or PDC changed.
- **Standalone C++17 exact header**, 15-second 44.1k stereo owner-provided voice: 661,500 stereo frames; unducked and 12ms ducked output raw float PCM are **bitwise identical** before and after (verified with binary `cmp`).
- Same local Linux container, same `g++ -O2 -std=c++17 -DNDEBUG`, `steady_clock`, 5 warm trial rounds, 48k stereo, 8 and 16 engines, one second per engine. 8 instances: median **12.859→10.977 ms** (~**14.6%** less). 16 instances: median **26.264→23.000 ms** (~**12.4%** less). This is a **DSP kernel benchmark only**, not a Windows FL Studio real-time CPU measurement.
- Existing portable Studio Delay regressions exercise first-echo sample accuracy, six-second beat sync across ring wraps, stereo feedback, safety/finite output. **New exact-head Windows VST3, JUCE compiled audio and UI checks must pass**; do not merge on standalone performance results alone.
