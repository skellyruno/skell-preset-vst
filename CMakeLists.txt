cmake_minimum_required(VERSION 3.22)
project(SkellPreset VERSION 0.7.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SKELL_PRESET_PORTABLE_ONLY "Build portable DSP and MATCH tests without JUCE" OFF)
enable_testing()
option(SKELL_PRESET_TEST_SHARED_SKELLAY "Compile and test the real shared Skellay DSP" OFF)
if(SKELL_PRESET_TEST_SHARED_SKELLAY)
    include(cmake/SharedSkellay.cmake)
endif()
add_subdirectory(tests)
if(SKELL_PRESET_PORTABLE_ONLY)
    return()
endif()

include(FetchContent)
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.12
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Immutable SkellCore v0.2 template revision. No changing upstream main branch.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SkellCore/CMakeLists.txt")
    # Template checkout: use the bundled SkellCore directly (no duplicate fetch).
    add_subdirectory(SkellCore)
elseif(DEFINED SKELLCORE_PATH AND EXISTS "${SKELLCORE_PATH}/CMakeLists.txt")
    add_subdirectory("${SKELLCORE_PATH}" SkellCoreLocal)
else()
    FetchContent_Declare(SkellCoreDependency
      GIT_REPOSITORY https://github.com/skellyruno/skell-vst-template.git
      GIT_TAG 1f759e4f6ab08d950aaccfc2dfc0415825461ff1
      SOURCE_SUBDIR SkellCore)
    FetchContent_MakeAvailable(SkellCoreDependency)
endif()


# GuidedTour and HardwareIconButton come from the same pinned SkellCore
# revision. Fetch headers only: the existing bundled SkellCore targets are
# preserved so duplicate CMake targets and DSP changes are impossible.
FetchContent_Declare(SkellGuidedTourHeaders
    GIT_REPOSITORY https://github.com/skellyruno/skellcore-vst-template.git
    GIT_TAG 6a2077b3275819dcc1166f8c5a4a04a5f3558caf
    GIT_SHALLOW FALSE
    SOURCE_SUBDIR _guided_tour_headers_only)
FetchContent_MakeAvailable(SkellGuidedTourHeaders)

# Optional SkellWorld presence uses a separately pinned headers-only SkellCore
# snapshot. It adds no audio target, APVTS parameter, or audio-thread work.
FetchContent_Declare(SkellWorldPresenceHeaders
    GIT_REPOSITORY https://github.com/skellyruno/skellcore-vst-template.git
    GIT_TAG c4ae09c896fa84279d3cc42f19925f97a7e5c124
    GIT_SHALLOW FALSE
    SOURCE_SUBDIR _world_presence_headers_only)
FetchContent_MakeAvailable(SkellWorldPresenceHeaders)

# Shared JUCE-free AdlibFX effect header at immutable tested revision.
# Include headers only; do not configure duplicate SkellCore audio targets.
FetchContent_Declare(SkellAdlibFXHeaders
    GIT_REPOSITORY https://github.com/skellyruno/skellcore-vst-template.git
    GIT_TAG cf2d182a1c4b1b45f97253f5e1b17dd91343aeaa
    GIT_SHALLOW FALSE
    SOURCE_SUBDIR _adlib_effect_headers_only)
FetchContent_MakeAvailable(SkellAdlibFXHeaders)

juce_add_plugin(SkellPreset
    COMPANY_NAME "Skell Audio"
    BUNDLE_ID "audio.skell.preset"
    PLUGIN_MANUFACTURER_CODE SkAu
    PLUGIN_CODE SkPr
    FORMATS VST3
    PRODUCT_NAME "Skell-Preset"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(SkellPreset)

# Regenerate original placeholder PNGs on clean source checkouts. No Pillow required.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/bg_panel.png")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_assets.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE SKELL_ASSET_RESULT)
    if(NOT SKELL_ASSET_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate the SKELL original placeholder assets")
    endif()
endif()

juce_add_binary_data(SkellPresetAssets
    SOURCES
    assets/bg_panel.png
    assets/logo_skull.png
    assets/cover_0.png
    assets/cover_1.png
    assets/cover_2.png
    assets/cover_3.png
    assets/cover_4.png
    assets/cover_5.png)

# Original SkellHub / SkellyMotion branding. The exact owner-supplied
# archive is an offline input, validated before embedding. Public source
# checkouts without that input retain a native, clearly distinct fallback.
set(SKELL_HUB_ART_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/hub" CACHE PATH "Original SkellHub artwork directory")
if(EXISTS "${SKELL_HUB_ART_DIR}/skelly-radar.png" AND EXISTS "${SKELL_HUB_ART_DIR}/skelly-skull.png")
    file(SHA256 "${SKELL_HUB_ART_DIR}/skelly-radar.png" SKELL_RADAR_SHA)
    file(SHA256 "${SKELL_HUB_ART_DIR}/skelly-skull.png" SKELL_SKULL_SHA)
    if(NOT SKELL_RADAR_SHA STREQUAL "993ee9c5254542a397b0e4cf3207df7444b281c38b9127d42df7b19ec8c724d8" OR
       NOT SKELL_SKULL_SHA STREQUAL "9faacd04ea4daf5086e1779d945d3285c0ab4da29b3da0e6bb9c87a44798cf8a")
        message(FATAL_ERROR "SkellHub artwork differs from the approved SkellyMotion source")
    endif()
    juce_add_binary_data(SkellHubMatchArt HEADER_NAME SkellHubMatchArtData.h NAMESPACE SkellHubMatchArtData
        SOURCES "${SKELL_HUB_ART_DIR}/skelly-radar.png" "${SKELL_HUB_ART_DIR}/skelly-skull.png")
    target_link_libraries(SkellPreset PRIVATE SkellHubMatchArt)
    target_compile_definitions(SkellPreset PRIVATE SKELL_MATCH_HUB_ART=1)
else()
    message(STATUS "SkellHub source artwork unavailable: use tools/install_hub_match_art.py before branded builds")
endif()

# Private build input generated by tools/prepare_artist_bank.py. Never commit the
# payload; public builds intentionally omit the private artist catalog.
set(SKELL_ARTIST_BANK_FILE "$ENV{SKELL_ARTIST_BANK_FILE}" CACHE FILEPATH "Private native artist bank JSON")
option(SKELL_REQUIRE_ARTIST_BANK "Fail if a private artist build has no bank" OFF)
if(SKELL_ARTIST_BANK_FILE)
    if(NOT EXISTS "${SKELL_ARTIST_BANK_FILE}")
        message(FATAL_ERROR "Private artist bank input not found")
    endif()
    configure_file("${SKELL_ARTIST_BANK_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/skell_artist_bank.json" COPYONLY)
    juce_add_binary_data(SkellArtistBankAssets HEADER_NAME SkellArtistBankData.h NAMESPACE SkellArtistBankData
        SOURCES "${CMAKE_CURRENT_BINARY_DIR}/skell_artist_bank.json")
    target_link_libraries(SkellPreset PRIVATE SkellArtistBankAssets)
    target_compile_definitions(SkellPreset PRIVATE SKELL_EMBEDDED_ARTIST_BANK=1)
elseif(SKELL_REQUIRE_ARTIST_BANK)
    message(FATAL_ERROR "Private artist build requires SKELL_ARTIST_BANK_FILE")
endif()

target_sources(SkellPreset PRIVATE
    Source/chain/ArtistBank.h Source/chain/ArtistBank.cpp
    Source/PluginProcessor.h Source/PluginProcessor.cpp
    Source/PluginEditor.h Source/PluginEditor.cpp
    Source/chain/BasicDSP.h Source/chain/StudioChorusDSP.h Source/chain/StudioNativeDSP.h Source/chain/VocalFeelOptoDSP.h Source/chain/VocalFeelSuiteDSP.h Source/chain/ModulationFX.h Source/chain/ImportedVocalFX.h Source/chain/AdvancedVocalDSP.h Source/chain/SpectralPitchFormant.h Source/chain/EffectRegistry.h Source/chain/LiveAudioScope.h Source/chain/LiveSpectrum.h Source/chain/ClipRecorder.h Source/chain/VocalEffects.h Source/chain/VoxRackFX.h Source/chain/FactoryPresets.h Source/chain/AlchemistNativePreset.h Source/chain/AlchemistMatchDSP.h Source/chain/AlchemistReferenceContour.h Source/chain/MicModelDSP.h
    Source/chain/PresetPackage.h Source/chain/PresetPackage.cpp
    Source/host/HostedVST3.h
    Source/ui/Theme.h Source/ui/ModulePicker.h Source/ui/SkullThemeButton.h Source/ui/ChainPanel.h Source/ui/ChainPanel.cpp
    Source/ui/SlotEditor.h Source/ui/SlotEditor.cpp Source/ui/HardwareFace.cpp Source/ui/HardwareLayout.h
    Source/ui/VocalFeelStyles.h Source/ui/PresetBrowser.h Source/ui/PresetBrowser.cpp Source/ui/BrowserViews.cpp
    Source/match/MatchCore.h)
target_compile_definitions(SkellPreset PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
# JUCE's in-process VST3 host is enabled only for Windows builds. Other
# targets retain an explicit unsupported result rather than a dummy processor.
if(WIN32)
    target_compile_definitions(SkellPreset PRIVATE JUCE_PLUGINHOST_VST3=1)
endif()
target_link_libraries(SkellPreset PRIVATE
    SkellPresetAssets SkellCore::DSP SkellCore::UI
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra juce::juce_cryptography)
target_include_directories(SkellPreset PRIVATE "${skellguidedtourheaders_SOURCE_DIR}/SkellCore/include")
target_include_directories(SkellPreset PRIVATE "${skellworldpresenceheaders_SOURCE_DIR}/SkellCore/include")
target_include_directories(SkellPreset PRIVATE "${skelladlibfxheaders_SOURCE_DIR}/SkellCore/include")
target_compile_features(SkellPreset PRIVATE cxx_std_17)

# UI strings carry UTF-8 glyphs (star, separator dot, arrow, en dash). Without
# /utf-8 MSVC decodes BOM-less sources as the system code page and those
# literals reach the editor as mojibake such as "â..." on the preset tiles.
if(MSVC)
    target_compile_options(SkellPreset PRIVATE /utf-8)
endif()

if(MSVC)
    target_compile_options(SkellPreset PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(SkellPreset PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# End-to-end native package regression tests (JUCE core only; no DAW required).
option(SKELL_PRESET_PACKAGE_TESTS "Build .skellpreset security and roundtrip tests" ON)
if(SKELL_PRESET_PACKAGE_TESTS)
    juce_add_console_app(SkellPresetPackageTests PRODUCT_NAME "Skell-Preset Package Tests")
    juce_generate_juce_header(SkellPresetPackageTests)
    target_sources(SkellPresetPackageTests PRIVATE
        Source/chain/PresetPackage.h Source/chain/PresetPackage.cpp
        tests/test_preset_package.cpp)
    target_compile_definitions(SkellPresetPackageTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SkellPresetPackageTests PRIVATE juce::juce_core)
    target_compile_features(SkellPresetPackageTests PRIVATE cxx_std_17)
    add_test(NAME NativePresetPackage COMMAND SkellPresetPackageTests)
    juce_add_console_app(SkellArtistBankTests PRODUCT_NAME "Skell-Preset Artist Bank Tests")
    juce_generate_juce_header(SkellArtistBankTests)
    target_sources(SkellArtistBankTests PRIVATE Source/chain/ArtistBank.cpp Source/chain/PresetPackage.cpp tests/test_artist_bank.cpp)
    target_compile_definitions(SkellArtistBankTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SkellArtistBankTests PRIVATE juce::juce_core juce::juce_cryptography)
    target_compile_features(SkellArtistBankTests PRIVATE cxx_std_17)
    add_test(NAME NativeArtistBank COMMAND SkellArtistBankTests)
    if(SKELL_ARTIST_BANK_FILE)
        target_link_libraries(SkellArtistBankTests PRIVATE SkellArtistBankAssets)
        target_compile_definitions(SkellArtistBankTests PRIVATE SKELL_EMBEDDED_ARTIST_BANK=1)
        add_test(NAME PrivateArtistBank COMMAND SkellArtistBankTests --embedded)
    endif()
endif()

# Real file-based clip recorder smoke test. It uses the same writer worker as
# the shipping plugin, but only writes to a unique temporary WAV.
juce_add_console_app(SkellPresetCaptureTests PRODUCT_NAME "Skell-Preset Capture Tests")
juce_generate_juce_header(SkellPresetCaptureTests)
target_sources(SkellPresetCaptureTests PRIVATE
    Source/chain/ClipRecorder.h tests/test_clip_recorder.cpp)
target_compile_definitions(SkellPresetCaptureTests PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SkellPresetCaptureTests PRIVATE
    juce::juce_audio_formats juce::juce_audio_utils)
target_include_directories(SkellPresetCaptureTests PRIVATE Source)
target_compile_features(SkellPresetCaptureTests PRIVATE cxx_std_17)
add_test(NAME SkellPresetCaptureTests COMMAND SkellPresetCaptureTests)

# Optional real-editor visual regression renderer. It has no third-party screenshot
# dependency; Linux execution needs an X server such as xvfb-run.
option(SKELL_PRESET_BUILD_UI_RENDERER "Build real JUCE editor PNG capture harness" OFF)
if(SKELL_PRESET_BUILD_UI_RENDERER)
    # This must be a plain executable. juce_add_console_app sets
    # JUCE_STANDALONE_APPLICATION=1, which conflicts with the plugin SharedCode
    # target's definitions. Reuse the plugin-generated JuceHeader and asset
    # include directories explicitly instead of creating a second JUCE app.
    add_executable(SkellPresetUIRenderer tests/ui_renderer.cpp)
    target_include_directories(SkellPresetUIRenderer PRIVATE
    "${skellguidedtourheaders_SOURCE_DIR}/SkellCore/include"
    "${skelladlibfxheaders_SOURCE_DIR}/SkellCore/include"
        "${CMAKE_CURRENT_BINARY_DIR}/SkellPreset_artefacts/JuceLibraryCode")
    # The plugin's include dependencies are PRIVATE, so the renderer also
    # links the two SkellCore interface targets to find Core.h and UI.h.
    target_link_libraries(SkellPresetUIRenderer PRIVATE SkellPreset
        SkellPresetAssets SkellCore::DSP SkellCore::UI SkellCore::UIRenderer
        juce::juce_audio_utils juce::juce_gui_extra)
    target_compile_definitions(SkellPresetUIRenderer PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(WIN32)
        target_compile_definitions(SkellPresetUIRenderer PRIVATE JUCE_PLUGINHOST_VST3=1)
    endif()
    target_compile_features(SkellPresetUIRenderer PRIVATE cxx_std_17)
    if(MSVC)
        # The real JUCE editor and rack processor are large; keep the test-only
        # screenshot harness off the 1 MiB default Windows executable stack.
        target_link_options(SkellPresetUIRenderer PRIVATE /STACK:8388608)
        target_compile_options(SkellPresetUIRenderer PRIVATE /utf-8)
    endif()
endif()

# Optional cross-platform diagnostic: run the exact compiled Skell-Preset factory
# processBlock against a local owner-provided dry WAV, never embed that audio.
# This exercises the plugin DSP directly; VST3 wrapper/FL Studio checks remain
# separate and must not be treated as passing by this tool alone.
option(SKELL_PRESET_ALCHEMIST_RENDERER "Build same-take Alchemist native WAV renderer" OFF)
if(SKELL_PRESET_ALCHEMIST_RENDERER)
    add_executable(SkellPresetAlchemistRender tests/render_alchemist_native.cpp)
    target_include_directories(SkellPresetAlchemistRender PRIVATE
        "${skellguidedtourheaders_SOURCE_DIR}/SkellCore/include"
        "${skelladlibfxheaders_SOURCE_DIR}/SkellCore/include"
        "${CMAKE_CURRENT_BINARY_DIR}/SkellPreset_artefacts/JuceLibraryCode")
    target_compile_definitions(SkellPresetAlchemistRender PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(WIN32)
        target_compile_definitions(SkellPresetAlchemistRender PRIVATE JUCE_PLUGINHOST_VST3=1)
    endif()
    target_link_libraries(SkellPresetAlchemistRender PRIVATE SkellPreset
        SkellPresetAssets SkellCore::DSP SkellCore::UI
        juce::juce_audio_formats juce::juce_audio_utils juce::juce_gui_extra)
    target_compile_features(SkellPresetAlchemistRender PRIVATE cxx_std_17)
    if(MSVC)
        # The full processor plus a prepared editor is larger than the 1 MiB
        # default Windows stack; this harness needs the same headroom as the
        # UI renderer or it faults before the first check.
        target_link_options(SkellPresetAlchemistRender PRIVATE /STACK:8388608)
        target_compile_options(SkellPresetAlchemistRender PRIVATE /utf-8)
    endif()
    add_test(NAME SkellPresetAlchemistNativeRenderSmoke
        COMMAND SkellPresetAlchemistRender --smoke)
    add_test(NAME SkellPresetBlankStartupStateRestore
        COMMAND SkellPresetAlchemistRender --smoke-blank)
    add_test(NAME SkellPresetPhaseStateRecall
        COMMAND SkellPresetAlchemistRender --phase-state-smoke)
    add_test(NAME SkellPresetStrictSlotOrderRestore
        COMMAND SkellPresetAlchemistRender --smoke-slot-order)
    add_test(NAME SkellPresetHostPercentageReadout
        COMMAND SkellPresetAlchemistRender --percent-smoke)
    # Real JUCE editor interaction/layout probe, no images or screenshot workflow.
    add_test(NAME SkellPresetWindowsUIInteractionSmoke
        COMMAND SkellPresetAlchemistRender --ui-smoke)
    add_test(NAME SkellPresetStereoSeparationProcessor
        COMMAND SkellPresetAlchemistRender --stereo-separation)
    add_test(NAME SkellPresetAlchemistMonoWavRenderSmoke
        COMMAND SkellPresetAlchemistRender --smoke-mono)
    add_test(NAME SkellPresetVocalPreviewSmoke
        COMMAND SkellPresetAlchemistRender --preview-smoke)
    add_test(NAME SkellPresetNativeSoundMatchTrialSmoke
        COMMAND SkellPresetAlchemistRender --native-trials-smoke)
    # Serial 5/10/20-instance workload of the real native processor with
    # finite/peak checks and host-PDC assertions (not DAW CPU certification).
    add_test(NAME SkellPresetRealRackStress
        COMMAND SkellPresetAlchemistRender --stress)
    set_tests_properties(SkellPresetRealRackStress PROPERTIES TIMEOUT 240)
endif()

# Windows-only in-process hosting integration fixture. It proves that an
# actual VST3 bundle can be scanned, instantiated and processed. This fixture
# is not included in the shipping Skell-Preset VST3 bundle.
option(SKELL_PRESET_HOST_TESTS "Build local nested-VST3 hosting regression" ON)
if(WIN32 AND SKELL_PRESET_HOST_TESTS)
    juce_add_plugin(SkellHostFixture
        COMPANY_NAME "Skell Audio"
        BUNDLE_ID "audio.skell.hostfixture"
        PLUGIN_MANUFACTURER_CODE SkAu
        PLUGIN_CODE SkHx
        FORMATS VST3
        PRODUCT_NAME "SkellHostFixture"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    juce_generate_juce_header(SkellHostFixture)
    target_sources(SkellHostFixture PRIVATE tests/host_fixture.cpp)
    target_compile_definitions(SkellHostFixture PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(SkellHostFixture PRIVATE juce::juce_audio_utils)

    juce_add_console_app(SkellPresetHostProbe PRODUCT_NAME "Skell VST3 Host Probe")
    juce_generate_juce_header(SkellPresetHostProbe)
    target_sources(SkellPresetHostProbe PRIVATE
        Source/host/HostedVST3.h tests/test_hosted_vst3.cpp)
    target_compile_definitions(SkellPresetHostProbe PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SkellPresetHostProbe PRIVATE
        juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra)
    target_include_directories(SkellPresetHostProbe PRIVATE Source)
endif()
