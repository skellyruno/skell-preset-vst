# Keep C++ assert() enabled in test Release builds without causing MSVC
# warning D9025 by passing /UNDEBUG after the global /DNDEBUG.
# Flags in this subdirectory do not affect the shipping plugin.
foreach(_config RELEASE RELWITHDEBINFO MINSIZEREL)
    string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_${_config} "${CMAKE_CXX_FLAGS_${_config}}")
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_${_config} "${CMAKE_CXX_FLAGS_${_config}}")
endforeach()

add_executable(SkellPresetMatchTest test_match.cpp)
target_compile_features(SkellPresetMatchTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetMatchTest COMMAND SkellPresetMatchTest)
add_executable(SkellPresetDSPTest test_dsp.cpp)
target_compile_features(SkellPresetDSPTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetDSPTest COMMAND SkellPresetDSPTest)
add_executable(SkellPresetRestrictedChainFitTest test_chain_fitter.cpp)
target_compile_features(SkellPresetRestrictedChainFitTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetRestrictedChainFitTest COMMAND SkellPresetRestrictedChainFitTest)
add_executable(SkellPresetAutomationTest test_automation.cpp)
target_compile_features(SkellPresetAutomationTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetAutomationTest COMMAND SkellPresetAutomationTest)
add_executable(SkellPresetLatencyAdlibTest test_latency_adlib.cpp)
target_compile_features(SkellPresetLatencyAdlibTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetLatencyAdlibTest COMMAND SkellPresetLatencyAdlibTest)

add_executable(SkellPresetGateEqTest test_gate_eq.cpp)
target_compile_features(SkellPresetGateEqTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetGateEqTest COMMAND SkellPresetGateEqTest)
add_executable(SkellPresetRealtimeStressTest test_realtime_stress.cpp)
target_compile_features(SkellPresetRealtimeStressTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetRealtimeStressTest COMMAND SkellPresetRealtimeStressTest)
# Regression for faster-than-crossfade delay automation and gate hold semantics.
add_executable(SkellPresetDelayGateRegressionTest test_delay_gate_automation.cpp)
target_compile_features(SkellPresetDelayGateRegressionTest PRIVATE cxx_std_17)

add_test(NAME SkellPresetDelayGateRegressionTest COMMAND SkellPresetDelayGateRegressionTest)

add_executable(SkellPresetDoublerModulationTest test_doubler_modulation.cpp)
target_compile_features(SkellPresetDoublerModulationTest PRIVATE cxx_std_17)

add_test(NAME SkellPresetDoublerModulationTest COMMAND SkellPresetDoublerModulationTest)

add_executable(SkellPresetDeEsserTest test_deesser.cpp)
target_compile_features(SkellPresetDeEsserTest PRIVATE cxx_std_17)

add_test(NAME SkellPresetDeEsserTest COMMAND SkellPresetDeEsserTest)

# Owner-authorized VoxRack feedback filtering port; no extra plugin latency.
add_executable(SkellPresetVoxRackDelayTest test_voxrack_delay.cpp)
target_compile_features(SkellPresetVoxRackDelayTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetVoxRackDelayTest COMMAND SkellPresetVoxRackDelayTest)

# Independent portable tests for imported vocal-effect behaviour.
add_executable(SkellPresetVocalEffectsTest test_vocal_effects.cpp)
target_compile_features(SkellPresetVocalEffectsTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetVocalEffectsTest COMMAND SkellPresetVocalEffectsTest)

add_executable(SkellPresetVoxRackFXTest test_voxrack_fx.cpp)
target_compile_features(SkellPresetVoxRackFXTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetVoxRackFXTest COMMAND SkellPresetVoxRackFXTest)

# Original modulation DSP (VML concepts reimplemented, no GPL code copied).
add_executable(SkellPresetModulationFXTest test_modulation_fx.cpp)
target_compile_features(SkellPresetModulationFXTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetModulationFXTest COMMAND SkellPresetModulationFXTest)

# User-authorized adaptation regression: Voice Changer vibrato and FLAMEEWRLD vocal rider/ducking delay.
add_executable(SkellPresetImportedVocalFXTest test_imported_vocal_fx.cpp)
target_compile_features(SkellPresetImportedVocalFXTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetImportedVocalFXTest COMMAND SkellPresetImportedVocalFXTest)
