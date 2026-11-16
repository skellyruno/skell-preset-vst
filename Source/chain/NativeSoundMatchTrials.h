#pragma once
// Native, editable Deuce D/Max-inspired experiment for the Main Vocals bank.
// The protected D/Max CHAMPION IS AUDIO, not native plugin parameters.
// This native rack uses only source-evidenced settings and bounded estimates
// for unavailable proprietary FL/Waves processors. Do not claim null match.
// Append the factory slot; preserve all preceding factory names/indices.
#include "AlchemistNativePreset.h"
#include <array>

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
// Native listening TRIAL informed by the latest user-approved OFFLINE v2.5.
// v2.5 = v2.3 Sustained Air + 7 kHz 0.75dB Q.75 repair. The v2.3
// original processing has NOT been ported, so do not call this an exact match.
//
// Same-take initial native render showed >10dB low output, excess sub-250Hz,
// and weak air. Add a SMALL broad corrective EQ, preserve the exact +0.75dB
// 7k bell, and apply bounded final lookahead limiting. This native trial
// does not change the separately protected D/Max index 44 or Alchemist 43.
constexpr auto makeStages() noexcept {
    std::array<alchemist::NativeStage,14> result{};
    for(std::size_t i=0;i<deuce::stages.size();++i)
        result[i]=deuce::stages[i];
    auto equalizer=alchemist::neutral(26); // 8 Band Tone; full native editor
    // Conservative held-out comparison against provided same-take Deuce
    // main-vocal audio: ±3.5dB broad balance to avoid v1's bass-heavy/airless
    // native sound. Band 7 remains the APPROVED v2.5 +0.75 dB / Q0.75.
    equalizer.p[1]=.5f-2.5f/30.f; // band1 56Hz   -2.5dB
    equalizer.p[3]=.5f-3.f/30.f;   // band2 129Hz  -3dB
    equalizer.p[5]=.5f-2.f/30.f;   // band3 265Hz  -2dB
    equalizer.p[9]=.5f+1.f/30.f;   // band5 1.18kHz +1dB
    equalizer.p[11]=.5f+1.f/30.f;  // band6 2.47kHz +1dB
    equalizer.p[12]=.9060003f;    // band7 ~7000Hz
    equalizer.p[13]=.525f;       // exact approved +0.75dB presence repair
    equalizer.p[15]=.5f+3.5f/30.f; // band8 11.5kHz +3.5dB air
    constexpr float broadQ=.196f; // Q~0.72
    for(auto& value:equalizer.q)value=broadQ;
    equalizer.q[6]=.2098347f;     // Q~0.75 at the 7k presence band
    result[12]=equalizer;
    // A final real native Skell limiter restores the observed ~10.7dB
    // level gap safely. This adds the plugin's existing ~3ms lookahead PDC
    // (tested by host); never pretend to be zero-latency or Fruity Limiter.
    auto finalLimiter=alchemist::neutral(3);
    finalLimiter.p[0]=.90f;           // ceiling ~-0.9 dBFS
    finalLimiter.p[1]=10.5f/18.f;     // input drive +10.5dB
    finalLimiter.p[2]=.40f;           // smooth release
    result[13]=finalLimiter;
    return result;
}
inline constexpr auto stages=makeStages();
inline constexpr int factoryIndex=46;
inline constexpr const char* name="Deuce v2.5 (Native Trial)";
inline constexpr const char* bank="Main Vocals";
static_assert(stages.size()==14 && stages[12].module==26
              && stages[13].module==3,
    "Current Deuce v2.5 trial has editable EQ and output limiter");
}

namespace skellpreset::alchemist_trial {
constexpr auto makeStages() noexcept {
    auto stages=alchemist::makeStages();
    // The protected factory index 43 remains bit-for-bit unchanged.
    // This source-relative refinement is only a listening candidate:
    // five quality routines enabled via existing MatchFinish p13..p16.
    stages[6].p[12]=1.f;  // quality-enable (legacy p13=.5 bypass)
    stages[6].p[13]=.25f; // gentle stereo-linked HF sibilance control
    stages[6].p[14]=.45f; // bounded phrase density
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
