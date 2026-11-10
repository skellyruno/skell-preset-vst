#pragma once
#include <JuceHeader.h>
#include <array>

namespace skellpreset::package {
constexpr int schemaVersion = 2;
constexpr int legacySlotCount = 10;
constexpr int slotCount = 16;
constexpr int parameterCount = 16;

struct Slot {
    int slotId = 0;
    int type = 0;
    bool enabled = false;
    float wet = 1.0f;
    std::array<float, parameterCount> params {};
};

struct Document {
    juce::String name = "Custom";
    juce::String author = "Skell Audio";
    juce::String bank = "My Presets";
    int mode = 1;
    int activeSlotCount = legacySlotCount;
    float inputDb = 0, mix = 1, outputDb = 0;
    std::array<float, 8> macros {};
    std::array<Slot, slotCount> slots {};
};

// Runs on the message thread only. ZIP reading has strict size and entry limits,
// rejects symlinks and unsupported members, and NEVER extracts archive paths.
juce::Result write(const juce::File& target, const Document& document);
juce::Result read(const juce::File& source, Document& result);
} // namespace skellpreset::package
