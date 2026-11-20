# Skell-Preset real-editor Quick4 (local, no screenshot workflows)

The shared SkellCore Quick4 launcher now opts into an explicit four-scene
mode in this repo's real JUCE SkellPresetUIRenderer. The four outputs are:
(1) Obsidian Simple with original Tone/Punch/Space/Grit rotary cards at 100%,
(2) Obsidian Advanced effect editor at 100%,
(3) effect library open at 70%, and (4) MIC widget open at 70%.

The compiled editor and processor are real C++ JUCE components, not a
mockup or screenshot from an older commit. Its existing full 110-scene
deep audit remains intact when not passed --quick4.

Preferred setup: check out SkellCore and use quick4.ps1 from Windows
Developer PowerShell, or quick4.sh on Linux. Both call the persistent
workspace runner, reuse JUCE and Ninja objects on disk, validate manifest
and PNGs, and package a source-SHA-stamped portable ZIP. Full instructions:
SkellCore/docs/QUICK4_PERSISTENT_WORKSPACE.md at the selected core revision.

Direct Linux invocation from a compiled source tree:

    SkellPresetUIRenderer --output ./ui-quick4 --quick4

Direct Windows invocation from Developer PowerShell:

    .\SkellPresetUIRenderer.exe --output .\ui-quick4 --quick4

There is NO automatic GitHub screenshot capture: its dedicated
ui-screenshots.yml workflow is manual-only and must not be triggered
for routine UI work. Windows VST3 CI may **compile** the renderer as a
source-integrity check, but it must not run screenshot captures.
The Quick4 ZIP can be uploaded into a new chat if that isolated session
cannot mount the developer machine's persistent workspace.

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
