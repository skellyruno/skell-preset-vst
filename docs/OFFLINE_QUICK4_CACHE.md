# Offline Quick4 source cache

The ChatGPT local environment may not resolve github.com. This source-only workflow packages the exact plugin commit and the pinned JUCE/SkellCore/Skellay dependencies. It does not compile, execute, or capture screenshots. Source provenance is recorded in PLUGIN_COMMIT and DEPENDENCIES. The existing Windows and portable workflows remain independent. Run only on explicit dispatch or a change to the cache workflow itself.

Extract the archive locally and configure with explicit source directories:
```sh
cmake -S "$ROOT/plugin" -B "$ROOT/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSKELL_PRESET_BUILD_UI_RENDERER=ON \
  -DJUCE_PATH="$ROOT/juce" -DSKELLCORE_PATH="$ROOT/core/SkellCore" \
  -DFETCHCONTENT_SOURCE_DIR_SKELLGUIDEDTOURHEADERS="$ROOT/guided" \
  -DFETCHCONTENT_SOURCE_DIR_SKELLWORLDPRESENCEHEADERS="$ROOT/presence" \
  -DFETCHCONTENT_SOURCE_DIR_SKELLADLIBFXHEADERS="$ROOT/adlib" \
  -DSKELLAY_SOURCE_DIR="$ROOT/skellay" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$ROOT/build" --target SkellPresetUIRenderer
xvfb-run -a "$ROOT/build/SkellPresetUIRenderer" --output "$ROOT/captures"
```

Check the manifest and inspect actual images. A successfully transferred archive is not a successful UI build. Use the shared Quick4 launcher when the persistent checkout is available.
