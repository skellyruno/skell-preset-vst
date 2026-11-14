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

add_executable(SkellPresetAdvancedVocalDSPTest test_advanced_vocal_dsp.cpp)
target_compile_features(SkellPresetAdvancedVocalDSPTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetAdvancedVocalDSPTest COMMAND SkellPresetAdvancedVocalDSPTest)

add_executable(SkellPresetSpectralPitchFormantTest test_spectral_pitch_formant.cpp)
target_compile_features(SkellPresetSpectralPitchFormantTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetSpectralPitchFormantTest COMMAND SkellPresetSpectralPitchFormantTest)

# 322 DAW architecture: canonical native effect registry and real output scope.
add_executable(SkellPreset322RegistryScopeTest test_322_registry_scope.cpp)
target_compile_features(SkellPreset322RegistryScopeTest PRIVATE cxx_std_17)
add_test(NAME SkellPreset322RegistryScopeTest COMMAND SkellPreset322RegistryScopeTest)

# Alchemist factory study is an editable native rack preset, not an offline
# WAV replacement. Exercise its exact stage values and audio path portably.
add_executable(SkellPresetAlchemistNativeTest test_alchemist_native.cpp)
target_compile_features(SkellPresetAlchemistNativeTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetAlchemistNativeTest COMMAND SkellPresetAlchemistNativeTest)

# Native causal Alchemist finish is portable, stereo-linked and has no
# audio-thread allocations, Python dependency, or external VST3.
add_executable(SkellPresetAlchemistMatchDSPTest test_alchemist_match_dsp.cpp)
target_compile_features(SkellPresetAlchemistMatchDSPTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetAlchemistMatchDSPTest COMMAND SkellPresetAlchemistMatchDSPTest)

# Auditable same-take Alchemist tone-fit constraints. Avoid regressing to the
# incorrect upper-band cuts from the first native preview.
add_executable(SkellPresetAlchemistReferenceToneTest test_alchemist_reference_tone.cpp)
target_compile_features(SkellPresetAlchemistReferenceToneTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetAlchemistReferenceToneTest COMMAND SkellPresetAlchemistReferenceToneTest)

# Native Mic Modeling profile (append-only ID 32): frequency coloration,
# stereo-linked zero-latency processing and safe host automation.
add_executable(SkellPresetMicModelTest test_mic_model_dsp.cpp)
target_compile_features(SkellPresetMicModelTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetMicModelTest COMMAND SkellPresetMicModelTest)

# The fitted contour is opted into only by Alchemist factory p9=1;
# all historic p9=0.5 finish slots must retain their original signal path.
add_executable(SkellPresetAlchemistContourTest test_alchemist_reference_contour.cpp)
target_compile_features(SkellPresetAlchemistContourTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetAlchemistContourTest COMMAND SkellPresetAlchemistContourTest)
add_executable(SkellPresetSkelldizerDSPTest test_skelldizer_dsp.cpp)
target_compile_features(SkellPresetSkelldizerDSPTest PRIVATE cxx_std_17)
add_test(NAME SkellPresetSkelldizerDSPTest COMMAND SkellPresetSkelldizerDSPTest)

# Portable zero-latency policy and 20-instance Skelldizer CPU stress.
add_executable(SkellPresetRealtimePolicyTests test_rack_realtime_policy.cpp)
target_compile_features(SkellPresetRealtimePolicyTests PRIVATE cxx_std_17)
add_test(NAME SkellPresetRealtimePolicyTests COMMAND SkellPresetRealtimePolicyTests)
add_executable(SkellPresetSkelldizerStress test_skelldizer_multinstance_stress.cpp)
target_compile_features(SkellPresetSkelldizerStress PRIVATE cxx_std_17)
add_test(NAME SkellPresetSkelldizerStress COMMAND SkellPresetSkelldizerStress)
