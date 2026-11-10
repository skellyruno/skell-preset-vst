# Real JUCE headless UI renderer

This harness creates actual `SkellPresetAudioProcessor` and `SkellPresetAudioProcessorEditor`
objects. The editor uses the same `Canvas`, rack, preset browser, knobs, fonts, and
JUCE theme code as the VST3 build. Screenshots are JUCE `createComponentSnapshot()`
images, **not** HTML mocks or synthetic redesigns.

It captures 48 PNGs: **Obsidian**, **Cream**, and **Crimson** × **Advanced**, **Preset Browser**, **Simple**, and **Bypass** × **70%, 85%, 100%, and 125%**. `contact-sheet.png`
shows Advanced and Preset Browser at 70% for all three themes.

## View screenshots in GitHub Actions

Open **Actions → Skell-Preset / Headless JUCE UI screenshots** on the repository,
run **workflow_dispatch**, and download the **Skell-Preset-UI-Screenshots**
artifact. A push changing the UI or renderer also triggers captures automatically.
The build fails if controls overlap the top bar, exceed canvas bounds, the expected
mode doesn't appear, the Bypass overlay is absent, a preset selector fails its
open/close callback test, a PNG is blank, or any file has unexpected dimensions.

## Local Linux setup

Install CMake, Ninja, Git, Xvfb, Xauth, ALSA/JACK dev packages and the Linux X11,
fontconfig, freetype, OpenGL and GTK3 developer packages. CMake fetches pinned
JUCE 8.0.12 and the pinned SkellCore revision (unless `JUCE_PATH` and
`SKELLCORE_PATH` are supplied).

```bash
cmake -S . -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSKELL_PRESET_BUILD_UI_RENDERER=ON -DSKELL_PRESET_PACKAGE_TESTS=OFF
cmake --build build-ui --target SkellPresetUIRenderer --parallel 2
xvfb-run -a -s '-screen 0 1920x1080x24' \
  ./build-ui/SkellPresetUIRenderer_artefacts/Release/SkellPresetUIRenderer \
  --output ui-screenshots
```

If the generated executable is in a different folder, locate it with
`find build-ui -type f -name SkellPresetUIRenderer -executable`.

## Windows

The same CMake target can run in an interactive Windows desktop session, without
FL Studio or any DAW. Build in Visual Studio, then run
`SkellPresetUIRenderer.exe --output ui-screenshots` from a shell. Windows does not
need Xvfb; it **does** need an available window station/GUI message manager.

## Scope and limitations

- "Headless" means **no human-visible desktop**. On Linux, Xvfb supplies the
  required native display server; JUCE GUI initialization is not truly display-free.
- Layout assertions cover visible element bounds and top-bar intersections, and
  interaction callbacks for preset open/close and Simple/Advanced/Bypass.
  Image content checks detect blank output. Pixel-perfect comparison, text
  truncation, drag/drop behavior, and actual FL Studio rendering require
  additional testing. Font rasterization differs between operating systems.
- A screenshot proves the component painted, **not** that processing, MIDI,
  audio latency, or CPU load works in a host. Portable DSP tests remain separate.
- UI automation and preset changes are performed on the message thread; no
  audio callback runs while capturing.

## Reusable SkellCore capture engine

The screenshot loop now calls `skellcore::ui::headless::render()` from
`<SkellCore/HeadlessUIRenderer.h>`. Skell-Preset keeps a bundled SkellCore
snapshot for compatibility with its audio modules; its renderer header is
identical to the upstream shared library at
[`skell-vst-template/SkellCore`](https://github.com/skellyruno/skell-vst-template/tree/main/SkellCore).
Other projects should instead pin an immutable SkellCore commit and link
`SkellCore::UIRenderer`. Plugin-specific scene choices and interaction
assertions remain in `tests/ui_renderer.cpp`, not in the shared capture engine.
