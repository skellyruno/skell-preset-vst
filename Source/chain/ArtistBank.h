#pragma once
#include <JuceHeader.h>
#include <map>

namespace skellpreset::artistbank {
// Build-time private bank, no network, Python, or filesystem work in audio code.
struct Metadata { juce::String artist, title, type, pair; };
using MetadataMap = std::map<juce::String, Metadata>;
struct SyncResult {
    MetadataMap metadata;
    int installed=0, updated=0, current=0, conflicts=0;
    juce::String error;
};
// Message-independent worker only. All input is validated before writes.
SyncResult sync(const juce::File& libraryRoot, const juce::String& bankJson);
SyncResult syncEmbedded(const juce::File& libraryRoot);
} // namespace skellpreset::artistbank
