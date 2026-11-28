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
// Serialize native saves/imports with managed bank updates across host processes.
// Construct only on a worker; no audio/message-thread lock waits.
class LibraryMutationLock {
public:
    explicit LibraryMutationLock(const juce::File& libraryRoot);
    ~LibraryMutationLock();
    bool acquired() const noexcept {return locked;}
    LibraryMutationLock(const LibraryMutationLock&)=delete;
    LibraryMutationLock& operator=(const LibraryMutationLock&)=delete;
private:
    juce::InterProcessLock lock;
    bool locked=false;
};
// Message-independent worker only. All input is validated before writes.
SyncResult sync(const juce::File& libraryRoot, const juce::String& bankJson);
SyncResult syncEmbedded(const juce::File& libraryRoot);
} // namespace skellpreset::artistbank
