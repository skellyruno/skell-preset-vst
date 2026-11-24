#pragma once
#include <array>
#include <string_view>
namespace skellpreset {
struct FactoryPreset {
    const char* name;
    const char* bank;
    const char* description;
    int colour; // cover-art selection
    float tone,punch,space,grit;
};
// Original SKELL presets; each is generated using the same processors used in the live chain.
inline constexpr std::array<FactoryPreset,39> factoryPresets {{
    {"Clean Lead","Main Vocals","Balanced modern vocal",0,.50f,.48f,.19f,.10f},
    {"Dark Trap Lead","Main Vocals","Warm focus, tight dynamics",1,.26f,.73f,.23f,.31f},
    {"Airy Pop","Main Vocals","Open high-frequency sheen",2,.80f,.42f,.36f,.12f},
    {"Gritty Rap","Main Vocals","Forward and aggressive",3,.56f,.82f,.13f,.68f},
    {"Warm R&B","Main Vocals","Smooth controlled intimacy",4,.40f,.53f,.48f,.25f},
    {"Bright Radio","Main Vocals","Presence with restraint",5,.75f,.68f,.24f,.25f},
    {"Bedroom Intimate","Main Vocals","Close, natural and soft",0,.43f,.32f,.16f,.09f},
    {"Aggressive Hook","Main Vocals","Hard punch, wide presence",2,.68f,.91f,.30f,.50f},
    {"Robot Chorus","Vocoding","Electronic robotic colour",5,.65f,.76f,.24f,.70f},
    {"Harmony Pad","Vocoding","Ambient vocal foundation",4,.45f,.29f,.94f,.20f},
    {"Talkbox Glide","Vocoding","Bright talking colour",2,.91f,.58f,.56f,.46f},
    {"Neon Droid","Vocoding","Synthetic shimmer",1,.79f,.68f,.58f,.77f},
    {"Chrome Choir","Vocoding","Wide metallic glow",5,.66f,.47f,.81f,.46f},
    {"Digital Formant","Vocoding","Resonant vocal colour",3,.35f,.74f,.35f,.61f},
    {"Phase Broadcast","Vocoding","Filtered broadcast",0,.28f,.85f,.38f,.67f},
    {"Spectral Glow","Vocoding","Soft futuristic layers",4,.83f,.35f,.91f,.32f},
    {"Skell Vocal Polish","Main Vocals","Natural de-essing and smooth finish",2,.58f,.48f,.22f,.08f},
    {"Skell Live Lead","Main Vocals","Zero-lookahead vocal tracking chain",0,.57f,.54f,.12f,.05f},
    {"Digital Bones","Adlibs","Bit-crushed vocal and rhythmic VoxRack echoes",1,.33f,.58f,.75f,.67f},
    {"Three-Band Silk","Main Vocals","Controlled multiband focus and subtle width",2,.57f,.57f,.21f,.11f},
    {"VoxRack Velvet","Main Vocals","Soft-knee dynamics and a room-sized vocal finish",4,.51f,.47f,.39f,.09f},
    {"Rider Ready","Main Vocals","Steady phrase levelling before compression",0,.55f,.46f,.20f,.08f},
    {"Eight-Band Air","Main Vocals","Eight individually tuned vocal frequency bands",2,.68f,.48f,.19f,.12f},
    {"Opto Punch","Main Vocals","Fast FET peaks into slow optical leveling",0,.57f,.78f,.12f,.13f},
    {"Smooth Edge","Main Vocals","Dynamic resonance tamer in the presence region",4,.56f,.48f,.18f,.10f},
    {"Noise Guard","Main Vocals","Gentle adaptive background-noise floor control",1,.48f,.41f,.14f,.09f},
    {"Studio Six","Main Vocals","Six native Skell rack stages: EQ, comp, drive, delay, reverb and chorus",0,.50f,.43f,.32f,.10f},
    // Native, editable study of the Alchemist main-vocal reference. Protected
    // offline v0.3/v0.7 champions remain separate; this is not a null match.
    {"Alchemist (Native Preview)","Main Vocals",
     "Experimental vocal match: EQ, FET/Opto-role dynamics and gentle multiband colour",
     4,.50f,.42f,.02f,.12f},
    // Index 44 is D/Max-inspired NATIVE TRIAL, not the protected D/Max
    // lossless audio champion or an exact proprietary processor port.
    {"Deuce D/Max (Native Trial)","Main Vocals",
     "Editable Deuce REC to MAIN native chain based on documented references",
     5,.52f,.50f,.04f,.20f},
    // Index 45 is an opt-in quality candidate; old Alchemist at index 43
    // remains unchanged for instantaneous host-side A/B and rollback.
    {"Alchemist Refined (Trial)","Main Vocals",
     "Protected Alchemist preview with five opt-in causal polish refinements",
     4,.50f,.42f,.02f,.12f},
    // User APPROVED v2.5 OFFLINE sound, but live native v2.5 is only
    // a comparative candidate until its underlying v2.3 DSP is ported.
    {"Deuce v2.5 (Native Trial)","Main Vocals",
     "Preferred v2.5 reference: native REC/MAIN approximation with 7 kHz correction",
     5,.52f,.50f,.04f,.20f},
    // Index 47 is an opt-in Alchemist safety variant; 43 and 45 stay unchanged.
    {"Alchemist Peak Guard (Trial)","Main Vocals",
     "Alchemist refinement plus 4x interpolated-peak guard (~-0.97 dB, 3 ms PDC; not BS.1770 certified)",
     4,.50f,.42f,.02f,.12f},
    // Index 48 applies measured native-vs-MAIN tonal corrections while keeping
    // the previous 43/45/47 factory candidates available for A/B rollback.
    {"Alchemist Tone Match (Trial)","Main Vocals",
     "Measured 2.93k +1.0dB / 5.1k -0.6dB / 12k -1.1dB correction plus peak guard; 3 ms PDC",
     4,.50f,.42f,.02f,.12f},
    {"Alchemist Lighter Dynamics (Trial)","Main Vocals",
     "Factory 48 tone match with reduced fast/slow compressor wet blend for envelope A/B",
     4,.50f,.42f,.02f,.12f},
    {"Alchemist Fast Compressor Blend (Trial)","Main Vocals",
     "Factory 48 tone match with only fast compressor wet reduced to 20%; smooth stays 70%",
     4,.50f,.42f,.02f,.12f},
    {"Alchemist Smooth Compressor Blend (Trial)","Main Vocals",
     "Factory 48 tone match with only smooth compressor wet reduced to 50%; fast stays 35%",
     4,.50f,.42f,.02f,.12f},
    {"Alchemist Serial Dynamics (Trial)","Main Vocals",
     "Factory 48 tone match with native FET peak catcher and slower RMS leveling; 3 ms peak guard",
     4,.50f,.42f,.02f,.12f},
    {"Alchemist Low-End Contour (Trial)","Main Vocals",
     "Measured 20–120 Hz correction: 30 Hz high-pass and neutral 70 Hz bell; retains serial dynamics and peak guard",
     4,.50f,.42f,.02f,.12f},
    {"Alchemist Serial Dynamics Blend (Trial)","Main Vocals",
     "Factory 53 contour with -27 dB serial threshold and 45% FET/RMS wet blend; no makeup gain",
     4,.50f,.42f,.02f,.12f},
}};
// Dense public program indexes; stable recipe labels are only internal/test labels.
inline constexpr std::array<int,39> factoryRecipeIds{{0,1,2,3,4,5,6,7,16,17,18,19,20,21,22,23,24,25,27,28,29,34,37,38,39,40,42,43,44,45,46,47,48,49,50,51,52,53,54}};
inline constexpr int factoryIndexForRecipe(int recipe) noexcept {
    for(std::size_t i=0;i<factoryRecipeIds.size();++i)
        if(factoryRecipeIds[i]==recipe)return static_cast<int>(i);
    return -1;
}
inline constexpr int recipeForFactoryIndex(int index) noexcept {
    return index>=0 && index<static_cast<int>(factoryRecipeIds.size())
        ? factoryRecipeIds[static_cast<std::size_t>(index)] : -1;
}
static_assert(factoryPresets.size()==factoryRecipeIds.size());
static_assert(factoryPresets[27].name==std::string_view("Alchemist (Native Preview)"),
    "Do not change approved Alchemist factory index");
static_assert(factoryPresets[28].name==std::string_view("Deuce D/Max (Native Trial)"),
    "Deuce trial must be discoverable as a Main Vocals factory preset");
static_assert(factoryPresets[29].name==std::string_view("Alchemist Refined (Trial)"),
    "Alchemist refinement stays at index 29");
static_assert(factoryPresets[30].name==std::string_view("Deuce v2.5 (Native Trial)"),
    "Newest user-preferred Deuce study appended at 46");
static_assert(factoryPresets[31].name==std::string_view("Alchemist Peak Guard (Trial)"),
    "Alchemist peak-guard variant must remain append-only at 47");
static_assert(factoryPresets[32].name==std::string_view("Alchemist Tone Match (Trial)"),
    "Measured Alchemist tone-match trial must remain append-only at 48");
static_assert(factoryPresets[33].name==std::string_view("Alchemist Lighter Dynamics (Trial)"),
    "Lighter Alchemist dynamics trial must remain append-only at 49");
static_assert(factoryPresets[34].name==std::string_view("Alchemist Fast Compressor Blend (Trial)"),
    "Fast-compressor blend trial must append at 50");
static_assert(factoryPresets[35].name==std::string_view("Alchemist Smooth Compressor Blend (Trial)"),
    "Smooth-compressor blend trial must append at 51");
static_assert(factoryPresets[36].name==std::string_view("Alchemist Serial Dynamics (Trial)"),
    "Serial FET/RMS Alchemist trial must append at 52");
static_assert(factoryPresets[37].name==std::string_view("Alchemist Low-End Contour (Trial)"),
    "Measured low-end Alchemist trial must append at 53");
static_assert(factoryPresets[38].name==std::string_view("Alchemist Serial Dynamics Blend (Trial)"),
    "Serial dynamics blend trial must append at 54");
}
