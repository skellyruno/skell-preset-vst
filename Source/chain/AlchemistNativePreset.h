#pragma once
// Alchemist Native Preview — documented approximation of the owner's
// 2026-11 Alchemist vocal reference. Not a Waves/Image-Line emulation or
// a port of the non-causal offline v0.7 optimizer.
//
// All stage IDs are EXISTING Skell Preset module IDs. Never renumber these
// IDs or alter existing factory program indices; append the preview instead.
#include <array>

namespace skellpreset::alchemist {
struct NativeStage {
    int module=0;
    float wet=1.f;
    std::array<float,16> p{};
    std::array<float,8> q{};
};

// C++17 constexpr construction without dynamic storage, allocations or JUCE.
constexpr NativeStage neutral(int module,float wet=1.f) noexcept {
    NativeStage s{};
    s.module=module;
    s.wet=wet;
    for(auto& v:s.p)v=.5f;
    for(auto& v:s.q)v=.5f;
    return s;
}

constexpr auto makeStages() noexcept {
    // Screenshot evidence shows Parametric EQ 2 high-pass and sharp notches.
    // Two native EQ slots split the low cut from the eight peaking bands;
    // this does NOT imply there were exactly two independent FL EQ instances.
    auto highpass=neutral(1); // legacy single-bell EQ, gain=0dB
    highpass.p[0]=.50f; highpass.p[1]=.50f;
    highpass.p[2]=.50f; highpass.p[3]=.18f; // 65Hz cutoff

    auto tone=neutral(26); // 8-Band EQ
    // Band frequency, gain and independent Q: conservative listening-start
    // estimates, NOT the original FL plugin state or a fitted transfer curve.
    tone.p={{.733907f,.500000f, .781054f,.446667f,
             .761258f,.466667f, .717780f,.446667f,
             .713961f,.520000f, .725537f,.526667f,
             .474552f,.433333f, .554709f,.470000f}};
    tone.q={{.418183f,.694135f,.672591f,.704406f,
             .418183f,.441213f,.800437f,.637421f}};

    // Fast CLA-76-role stage. Threshold/ratio come from the bounded v0.5
    // envelope-fit hypothesis (-34.76 dBFS, ~7:1); partial wet protects
    // against importing an aggressive same-take fit as generic compression.
    auto fast=neutral(2,.35f); // native compressor, not CLA-76 code
    fast.p[0]=.315238f; // -34.76dB
    fast.p[1]=.545364f; // 7:1
    fast.p[2]=.030612f; // 5ms attack (provisional)
    fast.p[3]=.145833f; // 90ms release (provisional)
    fast.p[4]=.50f;     // zero makeup

    // Slow CLA-2A-role stage; v0.5 measured -19.46dBFS, ~1.196:1.
    // Real unit topology/response is intentionally not claimed.
    auto slow=neutral(19,.70f); // native smooth compressor
    slow.p[0]=.670566f;
    slow.p[1]=.017818f;
    slow.p[2]=.333333f; // soft knee
    slow.p[3]=.292683f; // 160ms release

    // Soundgoodizer Mode A is visible in the reference screenshot. Native
    // multiband + subtle harmonic coloration is a *substitute*, not Maximus.
    auto multiband=neutral(17,.26f); // ThreeBandDynamics
    multiband.p[0]=.27f;
    multiband.p[1]=.38f;
    multiband.p[2]=.30f;
    multiband.p[3]=.50f;

    auto colour=neutral(5,.17f); // native Saturator
    colour.p[0]=.08f;
    colour.p[1]=.45f;

    // v0.7 learned harmonic and two-timescale envelope finish from its
    // auditable report. This works live without future samples, replacing the
    // offline symmetric 15ms smoother with causal one-pole gain smoothing.
    // The exact .979 held-out score does NOT transfer to this native chain.
    auto finish=neutral(31,1.f);
    finish.p[0]=.2001f;    // cubic coefficient -0.017994
    finish.p[1]=1.f;       // +/-0.6dB max dynamic contour
    finish.p[2]=.323236f; // slow coefficient -0.123735
    finish.p[3]=0.f;      // fast-minus-slow coefficient -0.35
    finish.p[4]=.670724f; // -19.878dBFS training slow-envelope median
    finish.p[5]=.335037f; // 0.3012775 training signal 99th percentile
    finish.p[6]=.53875f;  // +0.0775dB normalized harmonic makeup
    finish.p[7]=.270833f; // causal ~15ms smoothing
    return std::array<NativeStage,7>{{
        highpass,tone,fast,slow,multiband,colour,finish
    }};
}

inline constexpr auto stages=makeStages();
inline constexpr int originalFactoryCount=43;
inline constexpr int factoryIndex=43;
inline constexpr const char* name="Alchemist (Native Preview)";
inline constexpr const char* bank="Main Vocals";
static_assert(stages.size()==7, "Alchemist chain must have seven native effects");
static_assert(stages[0].module==1 && stages[1].module==26
    && stages[2].module==2 && stages[3].module==19
    && stages[4].module==17 && stages[5].module==5
    && stages[6].module==31,
    "Alchemist stage IDs are ABI stable");
} // namespace skellpreset::alchemist
