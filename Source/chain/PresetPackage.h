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
    // A blank native package must agree with APVTS' 50% parameter defaults.
    // Otherwise importing a programmatically-created empty rack leaves new
    // effects at their minimum values instead of their neutral positions.
    std::array<float, parameterCount> params = [] {
        std::array<float, parameterCount> values {};
        values.fill(.5f);
        return values;
    }();
    // Optional v2 extension, ignored by older readers. Preserves all eight
    // parametric-EQ bandwidths when saving and reloading native rack presets.
    std::array<float,8> eqQ {{.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f}};
    // Optional Studio EQ shape/active state. Readers of older archives
    // retain original 7-band defaults; eighth band starts switched off.
    std::array<int,8> studioEqKind {{3,0,0,0,0,0,4,0}};
    std::array<bool,8> studioEqOn {{true,true,true,true,true,true,true,false}};
    // Optional v2 fields; absent in old presets, so both filters are flat.
    float eqLowCut=0.f,eqHighCut=0.f;
};

struct Document {
    juce::String name = "Custom";
    juce::String author = "Skell Audio";
    juce::String bank = "My Presets";
    int mode = 1;
    int activeSlotCount = legacySlotCount;
    float inputDb = 0, mix = 1, outputDb = 0;
    // Optional schema-v2 fields. Older ZIPs remain neutral/off on import.
    bool micPreOn = false;
    int micPreInput = 0, micPreTarget = 0;
    float micPreAmount = 1.f, micPreTrim = .5f;
    // Optional v2 extension; absence means the original input gate stays OFF.
    // Keep old .skellpreset packages readable and allow new gate state recall.
    bool micGateOn = false;
    float micGateThreshold = -45.f, micGateRelease = 160.f, micGateHold = 55.f;
    std::array<float, 8> macros {{.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f}};
    std::array<Slot, slotCount> slots = [] {
        std::array<Slot, slotCount> result {};
        for (int i = 0; i < slotCount; ++i)
            result[static_cast<size_t>(i)].slotId = i;
        return result;
    }();
};

// Runs on the message thread only. ZIP reading has strict size and entry limits,
// rejects symlinks and unsupported members, and NEVER extracts archive paths.
juce::Result write(const juce::File& target, const Document& document);
juce::Result read(const juce::File& source, Document& result);
} // namespace skellpreset::package
