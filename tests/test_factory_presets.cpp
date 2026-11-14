#include "../Source/chain/FactoryPresets.h"
#include <array>
#include <cmath>
#include <map>
#include <iostream>
#include <set>
#include <string>
#include <utility>

int main() {
    int failures = 0;
    std::set<std::pair<std::string, std::string>> identities;
    std::set<std::string> banks;
    std::map<std::string, std::size_t> bankCounts;
    for (const auto& preset : skellpreset::factoryPresets) {
        const std::string name = preset.name ? preset.name : "";
        const std::string bank = preset.bank ? preset.bank : "";
        const std::string description = preset.description ? preset.description : "";
        if (name.empty() || bank.empty() || description.empty()) {
            std::cerr << "Missing factory preset identity or description\n";
            ++failures;
        }
        if (!identities.emplace(bank, name).second) {
            std::cerr << "Duplicate factory preset: " << bank << "/" << name << "\n";
            ++failures;
        }
        banks.insert(bank);
        ++bankCounts[bank];
        if (preset.colour < 0 || preset.colour > 5) {
            std::cerr << "Invalid factory preset cover index: " << name << "\n";
            ++failures;
        }
        for (float value : {preset.tone, preset.punch, preset.space, preset.grit}) {
            if (!std::isfinite(value) || value < 0.f || value > 1.f) {
                std::cerr << "Invalid normalized factory parameter: " << name << "\n";
                ++failures;
            }
        }
    }
    // A nonempty, duplicate-free bank can still silently lose presets or an entire bank.
    // Pin the intended catalog shape so accidental removals fail CI.
    const std::map<std::string, std::size_t> expectedBankCounts {
        {"Main Vocals", 18}, {"Adlibs", 17}, {"Vocoding", 8}
    };
    if (bankCounts != expectedBankCounts) {
        std::cerr << "Factory bank catalog differs from expected counts\n";
        for (const auto& [bank, count] : bankCounts)
            std::cerr << "  " << bank << ": " << count << "\n";
        ++failures;
    }
    // Preserve stable factory identities for saved presets and host sessions.
    const std::set<std::pair<std::string, std::string>> expectedIdentities {
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
        {"Main Vocals", "Studio Six"}
    };
    if (identities != expectedIdentities) {
        for (const auto& identity : expectedIdentities)
            if (!identities.count(identity))
                std::cerr << "Missing factory preset: " << identity.first << "/" << identity.second << "\n";
        for (const auto& identity : identities)
            if (!expectedIdentities.count(identity))
                std::cerr << "Unexpected factory preset: " << identity.first << "/" << identity.second << "\n";
        ++failures;
    }
    // Host session state persists the factory program as a numeric index.
    // Reordering existing entries would silently recall a different sound.
    const std::array<std::pair<std::string, std::string>, 43> expectedProgramOrder {{
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
    }};
    for (std::size_t i = 0; i < expectedProgramOrder.size(); ++i) {
        const auto& actual = skellpreset::factoryPresets[i];
        if (std::string(actual.bank) != expectedProgramOrder[i].first ||
            std::string(actual.name) != expectedProgramOrder[i].second) {
            std::cerr << "Factory program index " << i << " changed: expected "
                      << expectedProgramOrder[i].first << "/" << expectedProgramOrder[i].second
                      << ", got " << actual.bank << "/" << actual.name << "\n";
            ++failures;
        }
    }
    if (identities.size() != 43) {
        std::cerr << "Expected 43 unique factory presets\n";
        ++failures;
    }
    std::cout << "Checked " << identities.size() << " unique presets in "
              << banks.size() << " banks; failures: " << failures << "\n";
    return failures ? 1 : 0;
}
