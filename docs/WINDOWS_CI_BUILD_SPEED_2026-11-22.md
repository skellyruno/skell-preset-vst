# Skell-Preset Windows CI: early test bundle and safe dependency caching

Updated 2026-11-22 on draft PR #90. Keep all release gates independent from convenience previews.

## Why previous Windows CI failed

- CI 45080670481 compiled core plugin code, then failed compiling `SkellPresetUIRenderer` because `tests/ui_renderer.cpp` used `vocalFeelRecipes` without including `VocalFeelRecipes.h`.
- The resulting undeclared identifier, bad recipe reference, subscript, setter-signature and lambda errors are secondary to the missing header. Fix the include; never silence warnings or remove renderer tests.
- The previous serial dependency was: portable CTests → all Windows targets (including the expensive full editor renderer) → package/security and real JUCE smoke → VST3 artifact upload. This made a renderer-only compile failure prevent access to an otherwise compiled plugin.

## New workflow stages

1. Portable CTests remain required before Windows; PR concurrency still cancels only older pull-request runs.
2. Restore immutable JUCE/SkellCore FetchContent downloads and helper builds from `build/_deps` via `actions/cache@v4`; dependency keys are tied to CMake configuration, with fallback reuse. CMake still checks pinned dependency revisions. Neither user source, VST3 shipping artifacts nor full CMake build outputs are cached.
3. Compile shipping `SkellPreset_VST3` first with up to 3 MSBuild workers (previously 2); preserve /W4 /WX. If memory or toolchain constraints appear, reduce back to 2 rather than masking warnings.
4. Normalize and strictly verify the actual shipping VST3 metadata, copy the VST3 + exact-commit SHA256 build-info, and upload `Skell-Preset-Windows-VST3-PREVIEW`. This artifact is **test-only, not host-verified, not a release**.
5. Compile remaining native tools/editor/fixtures; execute *all* original package security, rack stress, nested-VST3 host, studio audio, JUCE UI and factory/preset state smoke checks.
6. Only after all full validations succeed upload the original `Skell-Preset-Windows-VST3` artifact and diagnostic renderer. No production `main` merge or release is performed.

## Validation evidence

- Exact-head source compile must be verified by Windows GitHub Actions; workflow source edits or portable success alone do not prove the new preview or validated VST3 exists.
- Genuine local Quick4 screenshot visual inspection/recapture, real FL Studio project & automation restore, pluginval and musical A/B are separate gates and are **not** certified by Windows CI alone.
- Record exact workflow run, source head and signed/hashed binary identity before user-facing install instructions.
