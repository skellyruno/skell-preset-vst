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
inline constexpr std::array<FactoryPreset,44> factoryPresets {{
    {"Clean Lead","Main Vocals","Balanced modern vocal",0,.50f,.48f,.19f,.10f},
    {"Dark Trap Lead","Main Vocals","Warm focus, tight dynamics",1,.26f,.73f,.23f,.31f},
    {"Airy Pop","Main Vocals","Open high-frequency sheen",2,.80f,.42f,.36f,.12f},
    {"Gritty Rap","Main Vocals","Forward and aggressive",3,.56f,.82f,.13f,.68f},
    {"Warm R&B","Main Vocals","Smooth controlled intimacy",4,.40f,.53f,.48f,.25f},
    {"Bright Radio","Main Vocals","Presence with restraint",5,.75f,.68f,.24f,.25f},
    {"Bedroom Intimate","Main Vocals","Close, natural and soft",0,.43f,.32f,.16f,.09f},
    {"Aggressive Hook","Main Vocals","Hard punch, wide presence",2,.68f,.91f,.30f,.50f},
    {"Demon Adlibs","Adlibs","Dark fuzz and low space",1,.21f,.55f,.61f,.82f},
    {"Ghost Echo","Adlibs","Airy trailing repeats",4,.63f,.26f,.89f,.23f},
    {"Radio Adlib","Adlibs","Narrow grit in the middle",3,.49f,.74f,.20f,.69f},
    {"Wide Stack","Adlibs","Layer-ready wide room",0,.69f,.34f,.76f,.15f},
    {"Stutter Throw","Adlibs","Tempo echoes",5,.47f,.50f,.94f,.35f},
    {"Alien Pulse","Adlibs","Unusual bright resonance",2,.88f,.38f,.58f,.60f},
    {"Whisper Trail","Adlibs","Soft intimate repeats",4,.72f,.15f,.72f,.08f},
    {"Distorted Shout","Adlibs","Shout-ready punch",3,.34f,.94f,.21f,.94f},
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
    {"Broadcast Throw","Adlibs","Radio bite with VoxRack-filtered phrase throws",3,.43f,.53f,.84f,.23f},
    {"Digital Bones","Adlibs","Bit-crushed vocal and rhythmic VoxRack echoes",1,.33f,.58f,.75f,.67f},
    {"Three-Band Silk","Main Vocals","Controlled multiband focus and subtle width",2,.57f,.57f,.21f,.11f},
    {"VoxRack Velvet","Main Vocals","Soft-knee dynamics and a room-sized vocal finish",4,.51f,.47f,.39f,.09f},
    {"VoxRack Chamber","Adlibs","VoxRack room reflections with phrase-end echoes",5,.40f,.50f,.78f,.34f},
    {"Wide Choir Adlib","Adlibs","Lush moving vocal layers and stereo chorus",0,.60f,.42f,.67f,.19f},
    {"Phantom Sweep","Adlibs","Phased comb motion for energetic adlib sends",5,.55f,.64f,.44f,.32f},
    {"Alien Ring","Adlibs","Metallic ring voice with dark phrase throws",1,.30f,.61f,.56f,.77f},
    {"Rider Ready","Main Vocals","Steady phrase levelling before compression",0,.55f,.46f,.20f,.08f},
    {"Ducked Echo","Adlibs","Tempo-locked stereo repeats that duck under the vocal",2,.50f,.52f,.78f,.19f},
    {"Skell Flutter","Adlibs","Expressive stereo vibrato with soft throws",4,.48f,.39f,.60f,.26f},
    {"Eight-Band Air","Main Vocals","Eight individually tuned vocal frequency bands",2,.68f,.48f,.19f,.12f},
    {"Opto Punch","Main Vocals","Fast FET peaks into slow optical leveling",0,.57f,.78f,.12f,.13f},
    {"Smooth Edge","Main Vocals","Dynamic resonance tamer in the presence region",4,.56f,.48f,.18f,.10f},
    {"Noise Guard","Main Vocals","Gentle adaptive background-noise floor control",1,.48f,.41f,.14f,.09f},
    {"Formant Sketch","Adlibs","Creative spectral pitch/formant colour; studio PDC",5,.51f,.42f,.67f,.27f},
    {"Studio Six","Main Vocals","Six native Skell rack stages: EQ, comp, drive, delay, reverb and chorus",0,.50f,.43f,.32f,.10f},
    // Native, editable study of the Alchemist main-vocal reference. Protected
    // offline v0.3/v0.7 champions remain separate; this is not a null match.
    {"Alchemist (Native Preview)","Main Vocals",
     "Experimental vocal match: EQ, FET/Opto-role dynamics and gentle multiband colour",
     4,.50f,.42f,.02f,.12f},
}};
}
