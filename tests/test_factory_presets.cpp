#include "../Source/chain/FactoryPresets.h"
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <utility>

int main() {
    int failures = 0;
    std::set<std::pair<std::string, std::string>> identities;
    std::set<std::string> banks;
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
    if (banks.empty()) { std::cerr << "No factory preset banks\n"; ++failures; }
    std::cout << "Checked " << identities.size() << " unique presets in "
              << banks.size() << " banks; failures: " << failures << "\n";
    return failures ? 1 : 0;
}
