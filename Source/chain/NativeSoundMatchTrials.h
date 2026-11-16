#pragma once
// Native, editable Deuce D/Max-inspired experiment for the Main Vocals bank.
// The protected D/Max CHAMPION IS AUDIO, not native plugin parameters.
// This native rack uses only source-evidenced settings and bounded estimates
// for unavailable proprietary FL/Waves processors. Do not claim null match.
// Append the factory slot; preserve all preceding factory names/indices.
#include "AlchemistNativePreset.h"
#include <array>
#include <cstddef>

namespace skellpreset::deuce {
using skellpreset::alchemist::NativeStage;
using skellpreset::alchemist::neutral;
constexpr auto makeStages() noexcept {
    auto recEq=neutral(1);
    recEq.p[3]=.19f;                 // recording HPF ~70Hz, provisional
    auto recComp=neutral(2,.62f);
    recComp.p[0]=.46f; recComp.p[1]=.32f; recComp.p[2]=.10f;
    recComp.p[3]=.22f; recComp.p[4]=.58f;
    auto recDizer=neutral(33,.24f); // Soundgoodizer Mode A *role*; raw 43 != 43%
    recDizer.p[0]=.25f;recDizer.p[1]=0.f;recDizer.p[2]=.5f;
    recDizer.p[3]=1.f;
    auto recLevel=neutral(19,.25f); // gentle leveller, avoids duplicated PDC
    recLevel.p[0]=.43f;recLevel.p[1]=.20f;
    auto mainEq=neutral(26);
    // MAIN R-EQ6: recorded dips around -4.4 and -4.3 dB, but frequencies
    // were not reliably recovered. These wide native positions are trial.
    mainEq.p={{.56f,.50f,.60f,.34f,.65f,.45f,.73f,.45f,
               .68f,.50f,.76f,.52f,.58f,.54f,.57f,.56f}};
    mainEq.q={{.35f,.35f,.35f,.35f,.30f,.31f,.32f,.30f}};
    auto sslRole=neutral(34,.45f); // native Studio EQ; not SSLChannel emulation
    auto compressor=neutral(2,.80f);
    // Main Fruity Compressor screenshot: -37.7 dB, 8:1, 12.9ms,
    // 200ms, +13.7dB makeup. Native makeup is capped at +12dB,
    // so remain bounded and DO NOT invent a complete 13.7 dB clone.
    compressor.p[0]=(-37.7f+48.f)/42.f;
    compressor.p[1]=(8.f-1.f)/11.f;
    compressor.p[2]=(12.9f-2.f)/98.f;
    compressor.p[3]=(200.f-20.f)/480.f;
    compressor.p[4]=1.f;
    auto deesser=neutral(11,.88f);
    deesser.p[0]=(5860.f-3500.f)/8000.f; // FST verified frequency
    deesser.p[1]=(-29.9f+53.f)/39.f; // estimated threshold
    deesser.p[2]=.35f;deesser.p[3]=.38f;
    auto mainDizer=neutral(33,.26f);
    mainDizer.p[0]=.44f;mainDizer.p[1]=1.f; // C-type airy role only
    mainDizer.p[2]=.50f;mainDizer.p[3]=1.f;
    auto air=neutral(8,.32f); // Fresh Air *role*, not hosted proprietary plugin
    air.p[0]=.35f;air.p[1]=.58f;
    auto multiband=neutral(17,.38f); // C6 broad level-control role
    auto pultecRole=neutral(34,.46f); // Pultec tonal role, no proprietary IR
    return std::array<NativeStage,12>{{
        recEq,recComp,recDizer,recLevel,mainEq,sslRole,compressor,
        deesser,mainDizer,air,multiband,pultecRole
    }};
}
inline constexpr auto stages=makeStages();
inline constexpr int factoryIndex=44;
inline constexpr const char* name="Deuce D/Max (Native Trial)";
inline constexpr const char* bank="Main Vocals";
static_assert(stages.size()<=16,"Deuce chain must fit Skell physical rack");
static_assert(stages[6].module==2 && stages[7].module==11,
    "Preserve evidence-mapped Deuce compressor and De-Esser order");
}

namespace skellpreset::deuce_v25 {
// The owner's preferred *offline* Deuce v2.5 was v2.3 Sustained Air followed
// by a causal RBJ bell at 7000 Hz, Q .75, +.75 dB and -0.146715 dB trim.
// v2.3's entire REC/MAIN algorithm is NOT available as this live native rack.
// This append-only experiment retains the earlier D/Max-inspired stages
// bit-for-bit and adds the measured presence correction as an editable
// EightBandTone module (ID 26), with no new APVTS IDs or audio-thread work.
// The offline gain trim is deliberately omitted: its loudness reference was
// v2.3 audio and cannot correctly calibrate this different native base.
constexpr auto makeStages() noexcept {
    std::array<alchemist::NativeStage,13> result{};
    for(size_t i=0;i<deuce::stages.size();++i)
        result[i]=deuce::stages[i];
    auto presence=alchemist::neutral(26);
    // EightBandTone band 7 maps [3600,7500] Hz logarithmically;
    // (0.525 - 0.5) * 30 dB = +0.75 dB, Q = 0.4 * 20^0.209835.
    presence.p[12]=.9060003f;  // ~7000 Hz
    presence.p[13]=.525f;      // +0.75 dB
    presence.q[6]=.2098347f;   // Q = 0.75
    result[12]=presence;
    return result;
}
inline constexpr auto stages=makeStages();
inline constexpr int factoryIndex=46;
inline constexpr const char* name="Deuce v2.5 (Native Trial)";
inline constexpr const char* bank="Main Vocals";
static_assert(stages.size()<=16,"Deuce v2.5 must fit existing physical rack");
static_assert(stages[12].module==26 && stages[12].wet==1.f,
              "Presence correction must be a fully wet native eight-band EQ");
}

namespace skellpreset::alchemist_trial {
constexpr auto makeStages() noexcept {
    auto stages=alchemist::makeStages();
    // The protected factory index 43 remains bit-for-bit unchanged.
    // This source-relative refinement is only a listening candidate:
    // five quality routines enabled via existing MatchFinish p13..p16.
    stages[6].p[12]=1.f;  // quality-enable (legacy p13=.5 bypass)
    stages[6].p[13]=.46f; // stereo-linked sibilance control
    stages[6].p[14]=.42f; // phrase density
    stages[6].p[15]=.38f; // transient retention
    return stages;
}
inline constexpr auto stages=makeStages();
inline constexpr int factoryIndex=45;
inline constexpr const char* name="Alchemist Refined (Trial)";
inline constexpr const char* bank="Main Vocals";
static_assert(stages[6].module==31,"Alchemist refinement must use legacy MatchFinish ID");
static_assert(alchemist::stages[6].p[12]==.5f,
    "Approved Alchemist preview's optional quality path remains OFF");
}
