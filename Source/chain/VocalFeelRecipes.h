#pragma once
// Vocal Feel style recipes mapped into 16 Skell per-slot automation params.
// User-provided agent's 11 program names and tonal settings are retained.
// The factory BANK is untouched; styles live inside the opt-in new effect.
#include "VocalFeelSuiteDSP.h"
#include <array>

namespace skellpreset::dsp {
struct VocalFeelRecipe {
    const char* name;
    std::array<float,16> values;
};
constexpr float vfGainNorm(float db) noexcept {return (db+12.f)/24.f;}
constexpr std::array<float,16> vfRecipeValues(
    int voice,float low,float mid,float high,float deess,float comp,int colour,
    float magic,float space,float attack,float attitude,float presence) noexcept {
    return {{
        .5f*(float)voice, .5f,
        vfGainNorm(low),vfGainNorm(mid),vfGainNorm(high),
        deess*.01f,3500.f/9000.f,comp*.01f,.5f*(float)colour,
        .7f,magic*.1f,space*.1f,attack*.1f,attitude*.1f,presence*.1f,.7f
    }};
}
inline constexpr std::array<VocalFeelRecipe,11> vocalFeelRecipes{{
    {"Init",               vfRecipeValues(0, 0.f,  0.f,  0.f,30.f,35.f,1,0.f,0.f,0.f,0.f,0.f)},
    {"Pop Lead",           vfRecipeValues(0,-1.5f, 0.f, 2.5f,40.f,45.f,1,5.f,3.5f,2.f,0.f,4.5f)},
    {"Rap Power",          vfRecipeValues(0, 0.f, 1.5f, 0.f,35.f,60.f,2,2.f,1.5f,4.f,6.f,4.f)},
    {"Bhojpuri Bright",    vfRecipeValues(0,-2.f, 2.f, 4.f,50.f,50.f,1,4.f,4.f,0.f,3.f,6.f)},
    {"Devotional Warm",    vfRecipeValues(0, 2.f, 0.f, 1.f,30.f,35.f,1,4.f,6.f,0.f,0.f,2.f)},
    {"Podcast / Voice-Over",vfRecipeValues(0,1.f,-1.f,0.f,45.f,55.f,0,0.f,0.f,1.5f,0.f,3.f)},
    {"Female Air",         vfRecipeValues(2,-2.f, 0.f, 4.5f,45.f,40.f,1,5.f,4.f,0.f,0.f,4.f)},
    {"Slapback Retro",     vfRecipeValues(0, 0.f, 2.f,-1.f,30.f,50.f,2,2.f,7.f,0.f,4.f,0.f)},
    {"Intimate Close",     vfRecipeValues(0, 2.5f,0.f, 1.5f,25.f,30.f,1,3.f,1.f,0.f,0.f,0.f)},
    {"Big Chorus Vocal",   vfRecipeValues(0, 0.f, 0.f, 3.f,40.f,55.f,1,7.f,6.f,2.f,3.f,5.f)},
    {"Deep Male",          vfRecipeValues(1, 3.f,-1.5f, 0.f,30.f,45.f,1,3.f,3.f,0.f,0.f,3.f)}
}};
static_assert(vocalFeelRecipes.size()==11,"Preserve all source style identities");
} // namespace skellpreset::dsp
