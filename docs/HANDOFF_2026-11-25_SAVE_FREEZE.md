# Skell Preset — handoff 2026-11-25 (save freeze fix + remaining work)

Branch: `feature/studio-dynamics-delay-ui-20261121` (PR #90, **draft**).
Previous commit: `01a9f37`. This pass is committed after it; CI must be
re-checked before claiming anything beyond local evidence.

## 1. What this pass changed (preset-browser save freeze)

The reported "clicking Save freezes the plugin" had a code-evidenced cause in
`Source/ui/PresetBrowser.cpp`: the async file-chooser callback ran the entire
write + library copy + full rescan + per-cover image decode on the message
thread, then re-scanned again, and re-enabled the buttons before the heavy
work finished (so a second click could queue a duplicate save). There was also
no distinction between overwriting a personal preset and saving a factory
sound, and no overwrite protection beyond the OS dialog.

Implemented:

- `SkellPresetAudioProcessor::makePresetDocument(Document&)` (new,
  `PluginProcessor.cpp` / `.h`): builds the snapshot on the message thread.
  `exportPreset(file)` now = `makePresetDocument` + `package::write`.
- `PresetBrowser::saveActivePreset(bool chooseName)` replaces the old
  `chooseExport()` path:
  - `SAVE` overwrites the selected personal library preset directly.
  - Factory sounds / never-saved chains fall through to the Save As chooser,
    so no factory program is ever rewritten.
  - `saveInFlight` + `chooserActive` guards make duplicate clicks impossible;
    buttons are disabled for the whole write.
- `startPresetWrite()` snapshots metadata (artist/genre/vocal type/pair and the
  current cover path), then runs only `package::write()` (JSON + ZIP
  compression + atomic replace) on `juce::Thread::launch`, with
  `MessageManager::callAsync` completion via `SafePointer`.
- `completePresetWrite()` does the single library copy, metadata carry-over,
  cover copy, one `refreshLibraryAfterChange()` and a non-modal inline status
  (`SAVED` / `SAVED...` / `EXPORTED - NO LIBRARY COPY` / `SAVE FAILED`) painted
  on the Manage Library page instead of a modal success alert.
- `copyToLibrary()` no longer rescans; `refreshLibraryAfterChange()` is the
  only rescan entry point (previously up to three scans per save).
- `scanLibrary()` now keeps a thumbnail cache keyed by cover path + mtime +
  size (`CoverCacheEntry` / `coverCache`), so unchanged covers are never
  re-decoded. `refreshArtwork()` caches the full-resolution backdrop the same
  way (`backdropSource` / `backdropModified` / `backdropLoaded`), so selecting
  the same preset repeatedly no longer decodes a 1600x1000 image.
- New `SAVE AS…` button (`saveAsButton`) beside `SAVE` on the Manage Library
  page, plus `refreshSaveControls()` (idle gating + `SAVE`/`SAVE COPY` label).
- `testBrowserLayoutForTest()` widget list grew to 21 entries and asserts the
  new button's visibility/overlap like the rest.

### Verification actually run (local)

- `cmd /c build-ui.cmd` → clean, `SkellPresetUIRenderer.exe` relinked
  2026-11-25 09:04.
- `ctest --test-dir build-local -C Release` (via the VS-bundled
  `...\CMake\bin\ctest.exe`; `ctest` is **not** on PATH) → **52/52 passed**.
- `build-local\SkellPresetUIRenderer.exe --output .\ui-r10` → **4 scenes,
  0 failures** (real editor, Quick4 only, no GitHub Action).

### NOT verified / do not claim

- No DAW test. FL Studio and any Windows VST3 host were unavailable, so the
  save path was not clicked through in a host. `--full` was not re-run.
- Background-thread `package::write()` is portable-safe (it only touches its
  own `Document` copy and the target file), but the *perceived* freeze fix
  rests on code inspection + the original symptoms, not a host repro.
- Saving an External VST3 slot still fails with the existing
  "not yet portable" message (unchanged, intentional).
- The inline status line is only painted on the Manage Library page; it is not
  a toast on the library page.

## 2. What is left to do (in brief order)

1. **Simple mode → one unified hardware panel.** Current paint is a multi-layer
   composition: `PluginEditor.cpp:1255` panel, identity band + scope
   `:1260-1294`, four 252x352 macro cards `:1295-1355`, separate master strip
   `:1357-1383`. Keep macro APVTS setup (`:125-154`) and visibility/bounds
   (`:1429-1452`) exactly as-is; simplify only the paint. Drop STEREO from the
   Simple strip when it becomes a rack effect. Update the matching hard-coded
   assertions in `getScreenshotLayoutIssues()` (`:599-627`) only for the
   geometry that actually changes. Macro labels Tone/Punch/Space/Grit must not
   be renamed or removed (Simple-only rule in `AGENTS.md`).
2. **Empty slot simplification.** Two places still over-draw:
   - `SlotEditor.cpp:1944-1967`: "EMPTY SLOT" + "PRESS + ..." + eight family
     chips. Keep one CTA title/hint, delete the family chips; footer at
     `:3732-3751` duplicates the message — make it conditional/omit for Empty.
   - `ChainPanel.cpp:247-362`: empty rows already skip the wet dial (`:320-349`)
     but still paint icon/name and the power dot. Branch `Empty` right after
     box/selection and paint a compact `EMPTY +` affordance only; preserve row
     hit behaviour and the type checks at `:578-589`.
   Empty graph gesture is already blocked (`SlotEditor.cpp:3795`,
   `:4056-4061`); do not regress that.
3. **STEREO → Advanced rack effect.** `MixerStereoSeparation` is currently a
   global post-rack stage (`PluginProcessor.cpp:1350-1364`) with the APVTS ID
   `stereoSeparation`. Planned: append a new `sN_fxExt12` choice generation
   (never extend an existing choice's range) exposing "Stereo Separation",
   reuse the same DSP in the slot path, keep `stereoSeparation` readable for
   automation, and migrate saved state so the global stage and the rack effect
   never both process. Registry (`EffectRegistry.h`, 45 entries, append-only)
   and `ModuleType` (`PluginProcessor.h:35`) both need an appended entry —
   `static_assert` guards the count.
4. **Rack polish**: compact rows/scrollbar, consistent wet/power affordances,
   no empty-row chrome once item 2 lands.
5. **Visualizer**: elapsed-time animation, cached static scenery, throttled
   repaint when the editor is hidden, and measured frame times. Current stage
   paint is in `SkellIdleDeck` (see `tests/ui_renderer.cpp` stage scenes and
   `docs/MAJOR_UI_MASTER_SIGNAL_PASS_2026-11-24.md`).
6. **Preset browser redesign** beyond the save fix: pagination/scroll polish,
   the cover-preview block at `PresetBrowser.cpp:2029-2076`, and Manage Library
   grouping. Keep every existing test helper (`testBrowserLayoutForTest`,
   `testMainVocalsPresetFilterForTest`, `testCategoryCountsForTest`, …) green.
7. **Global UI polish** (typography/spacing/state consistency) last, after the
   structural items settle.

## 3. Hard constraints (unchanged)

- Preserve 55 protected factory program identities (incl. factory programs
  43/45/46), user presets, FL Studio recall, every APVTS parameter ID, rack
  slot identity, external plugin state, macro assignments and undo/redo.
- No UI code may rename automation parameters or VST3 IDs.
- Effect/parameter additions are **append-only**. Never change an existing
  choice parameter's item list or normalization.
- No file I/O, allocation, locks or GUI work on the audio thread.
- Routine screenshots: local Quick4 only. Do not trigger screenshot GitHub
  Actions. Build with `build-ui.cmd` / `build-vst3.cmd` (no `cmake` on PATH).
- Keep PR #90 draft until host-verified.

## 4. Working-tree housekeeping

Untracked and deliberately **not** committed: every `*.png` at repo root,
`ui-*/` capture folders (incl. the new `ui-r10/`), `dist/`,
`build-*.txt`, `cmake-log.txt`, `ctest-all.txt`, private checkpoint zips and
`_r6checkpoint/` / `_r7checkpoint/`, plus the local `build-ui.cmd` /
`build-vst3.cmd` helpers. Do not `git add -A`.
