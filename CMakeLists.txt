cmake_minimum_required(VERSION 3.22)
project(SkellPreset VERSION 0.5.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SKELL_PRESET_PORTABLE_ONLY "Build portable DSP and MATCH tests without JUCE" OFF)
enable_testing()
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

juce_add_plugin(SkellPreset
    COMPANY_NAME "Skell Audio"
    BUNDLE_ID "audio.skell.preset"
    PLUGIN_MANUFACTURER_CODE SkAu
    PLUGIN_CODE SkPr
    FORMATS VST3 Standalone
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

target_sources(SkellPreset PRIVATE
    Source/PluginProcessor.h Source/PluginProcessor.cpp
    Source/PluginEditor.h Source/PluginEditor.cpp
    Source/chain/BasicDSP.h Source/chain/FactoryPresets.h
    Source/chain/PresetPackage.h Source/chain/PresetPackage.cpp
    Source/host/HostedVST3.h
    Source/ui/Theme.h Source/ui/SkullThemeButton.h Source/ui/ChainPanel.h Source/ui/ChainPanel.cpp
    Source/ui/SlotEditor.h Source/ui/SlotEditor.cpp
    Source/ui/PresetBrowser.h Source/ui/PresetBrowser.cpp
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
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra)
target_compile_features(SkellPreset PRIVATE cxx_std_17)

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
endif()

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
endif()
