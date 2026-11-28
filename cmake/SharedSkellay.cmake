# Shared DSP, with explicit revision and an opt-in local source checkout.
# Do not configure Skellay's plugin/UI or duplicate native engine code.
include(FetchContent)
set(SKELLAY_ENGINE_REVISION "8a0904b99dba671195f139907e09ac86a09ad5f4")
set(SKELLAY_SOURCE_DIR "" CACHE PATH "Optional local Skellay source checkout")
if(SKELLAY_SOURCE_DIR)
    set(_skellay_root "${SKELLAY_SOURCE_DIR}")
else()
    FetchContent_Declare(SkellaySharedDSP
        GIT_REPOSITORY https://github.com/skellyruno/skellay.git
        GIT_TAG "${SKELLAY_ENGINE_REVISION}"
        GIT_SHALLOW FALSE
        SOURCE_SUBDIR _native_sources_only)
    FetchContent_MakeAvailable(SkellaySharedDSP)
    set(_skellay_root "${skellayshareddsp_SOURCE_DIR}")
endif()
add_library(skellay_shared_native STATIC
    "${_skellay_root}/native/Delay.cpp"
    "${_skellay_root}/native/FruityDelayBank.cpp"
    "${_skellay_root}/native/SkellayEngine.cpp")
target_include_directories(skellay_shared_native PUBLIC "${_skellay_root}/native")
target_compile_features(skellay_shared_native PUBLIC cxx_std_17)
set_target_properties(skellay_shared_native PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(skellay_shared_native PRIVATE /W4 /fp:strict /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(skellay_shared_native PRIVATE -Wall -Wextra -Werror -ffp-contract=off)
endif()
message(STATUS "Skellay shared native source revision: ${SKELLAY_ENGINE_REVISION}")
