// Reconciled from audit/preset-slot-order-strict-20261113. Keep the
// original 43 host program indexes and 12 subsequently shipped identities.
// New factories can only be appended; never renumber old FL Studio programs.
// Portable C++17, no JUCE dependency.
#include "../Source/chain/FactoryPresets.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

namespace {
using Identity=std::pair<std::string_view,std::string_view>;
constexpr std::array<Identity,55> expected{{
    {"Main Vocals", "Clean Lead"},
    {"Main Vocals", "Dark Trap Lead"},
    {"Main Vocals", "Airy Pop"},
    {"Main Vocals", "Gritty Rap"},
    {"Main Vocals", "Warm R&B"},
    {"Main Vocals", "Bright Radio"},
    {"Main Vocals", "Bedroom Intimate"},
    {"Main Vocals", "Aggressive Hook"},
    {"Adlibs", "Demon Adlibs"},
    {"Adlibs", "Ghost Echo"},
    {"Adlibs", "Radio Adlib"},
    {"Adlibs", "Wide Stack"},
    {"Adlibs", "Stutter Throw"},
    {"Adlibs", "Alien Pulse"},
    {"Adlibs", "Whisper Trail"},
    {"Adlibs", "Distorted Shout"},
    {"Vocoding", "Robot Chorus"},
    {"Vocoding", "Harmony Pad"},
    {"Vocoding", "Talkbox Glide"},
    {"Vocoding", "Neon Droid"},
    {"Vocoding", "Chrome Choir"},
    {"Vocoding", "Digital Formant"},
    {"Vocoding", "Phase Broadcast"},
    {"Vocoding", "Spectral Glow"},
    {"Main Vocals", "Skell Vocal Polish"},
    {"Main Vocals", "Skell Live Lead"},
    {"Adlibs", "Broadcast Throw"},
    {"Adlibs", "Digital Bones"},
    {"Main Vocals", "Three-Band Silk"},
    {"Main Vocals", "VoxRack Velvet"},
    {"Adlibs", "VoxRack Chamber"},
    {"Adlibs", "Wide Choir Adlib"},
    {"Adlibs", "Phantom Sweep"},
    {"Adlibs", "Alien Ring"},
    {"Main Vocals", "Rider Ready"},
    {"Adlibs", "Ducked Echo"},
    {"Adlibs", "Skell Flutter"},
    {"Main Vocals", "Eight-Band Air"},
    {"Main Vocals", "Opto Punch"},
    {"Main Vocals", "Smooth Edge"},
    {"Main Vocals", "Noise Guard"},
    {"Adlibs", "Formant Sketch"},
    {"Main Vocals", "Studio Six"},
    {"Main Vocals", "Alchemist (Native Preview)"},
    {"Main Vocals", "Deuce D/Max (Native Trial)"},
    {"Main Vocals", "Alchemist Refined (Trial)"},
    {"Main Vocals", "Deuce v2.5 (Native Trial)"},
    {"Main Vocals", "Alchemist Peak Guard (Trial)"},
    {"Main Vocals", "Alchemist Tone Match (Trial)"},
    {"Main Vocals", "Alchemist Lighter Dynamics (Trial)"},
    {"Main Vocals", "Alchemist Fast Compressor Blend (Trial)"},
    {"Main Vocals", "Alchemist Smooth Compressor Blend (Trial)"},
    {"Main Vocals", "Alchemist Serial Dynamics (Trial)"},
    {"Main Vocals", "Alchemist Low-End Contour (Trial)"},
    {"Main Vocals", "Alchemist Serial Dynamics Blend (Trial)"}
}};
constexpr bool historicIdentityOrderUnchanged() {
    if(skellpreset::factoryPresets.size()<expected.size())return false;
    for(std::size_t i=0;i<expected.size();++i) {
        const auto& p=skellpreset::factoryPresets[i];
        if(!p.name || !p.bank || std::string_view(p.bank)!=expected[i].first ||
           std::string_view(p.name)!=expected[i].second)return false;
    }
    return true;
}
static_assert(historicIdentityOrderUnchanged(),
    "Factory host program order changed: saved FL Studio projects may recall different presets");
}
int main() {
    std::array<std::size_t,3> legacyBankCounts{};
    for(std::size_t i=0;i<skellpreset::factoryPresets.size();++i) {
        const auto& p=skellpreset::factoryPresets[i];
        if(!p.name || !*p.name || !p.bank || !*p.bank ||
           !p.description || !*p.description ||
           p.colour<0 || p.colour>5 ||
           !std::isfinite(p.tone) || !std::isfinite(p.punch) ||
           !std::isfinite(p.space) || !std::isfinite(p.grit) ||
           p.tone<0.f || p.tone>1.f || p.punch<0.f || p.punch>1.f ||
           p.space<0.f || p.space>1.f || p.grit<0.f || p.grit>1.f) {
            std::fprintf(stderr,"Invalid preset at host index %zu\n",i);
            return 1;
        }
        if(i<43) {
            const std::string_view bank(p.bank);
            if(bank=="Main Vocals")++legacyBankCounts[0];
            else if(bank=="Adlibs")++legacyBankCounts[1];
            else if(bank=="Vocoding")++legacyBankCounts[2];
            else {
                std::fprintf(stderr,"Unknown legacy bank at index %zu\n",i);
                return 2;
            }
        }
        for(std::size_t j=0;j<i;++j) {
            if(std::string_view(p.name)==skellpreset::factoryPresets[j].name) {
                std::fprintf(stderr,"Duplicate factory identity %zu and %zu\n",j,i);
                return 3;
            }
        }
    }
    if(legacyBankCounts!=std::array<std::size_t,3>{{18,17,8}}) {
        std::fprintf(stderr,"Original factory bank counts changed\n");
        return 4;
    }
    std::printf("PASS: %zu factories, all 55 historic host program indexes, bank split 18/17/8, unique valid metadata\n",
                skellpreset::factoryPresets.size());
    return 0;
}
