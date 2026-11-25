#pragma once
// Read-only inventory for locally installed, licensed FL Studio effect modules.
// Inspired by FloopHole's discovery strategy, but this does not execute
// proprietary code, instantiate Fruity effects, or claim host compatibility.
// No file writes, global registry changes, or audio-thread work.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace skellpreset::host::flnative {
namespace fs = std::filesystem;

struct Effect {
    std::string name;
    std::string identifier; // lowercase native module stem; no path dependency
    fs::path binary;
};

inline std::string lowerAscii(std::string value) {
    for (auto& c : value) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte >= 'A' && byte <= 'Z') c = static_cast<char>(byte + ('a' - 'A'));
    }
    return value;
}

inline bool installedAt(const fs::path& root) {
    std::error_code ec;
    return fs::is_directory(root / "Shared", ec) &&
           fs::is_directory(root / "Plugins" / "Fruity" / "Effects", ec);
}

// File-only discovery. The effect loader itself requires a separate-process
// Fruity host, not a nested in-process LoadLibrary from the Skell audio plugin.
inline std::vector<Effect> discover(const fs::path& root, size_t maximum = 512) {
    std::vector<Effect> found;
    if (!installedAt(root) || maximum == 0) return found;
    const auto folder = root / "Plugins" / "Fruity" / "Effects";
    std::error_code ec;
    fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec);
    const fs::directory_iterator end{};
    std::set<std::string> identities;
    for (; !ec && it != end && found.size() < maximum; it.increment(ec)) {
        if (!it->is_directory(ec) || it->is_symlink(ec)) continue;
        const auto directory = it->path();
        const auto name = directory.filename().u8string();
        if (name.empty() || name[0] == '.') continue;
        fs::path module = directory / (name + "_x64.dll");
        if (!fs::is_regular_file(module, ec)) {
            module = directory / (name + ".dll");
            if (!fs::is_regular_file(module, ec)) {
                // Accept one uniquely identified x64 implementation only;
                // an ambiguous folder must never silently choose a binary.
                size_t count = 0;
                fs::directory_iterator filesIt(directory, fs::directory_options::skip_permission_denied, ec);
                for (; !ec && filesIt != end && count < 2; filesIt.increment(ec)) {
                    if (!filesIt->is_regular_file(ec)) continue;
                    const auto file = filesIt->path().filename().u8string();
                    const auto id = lowerAscii(file);
                    if (id.size() >= 8 && id.substr(id.size() - 8) == "_x64.dll" &&
                        id != "engine_x64.dll") { module = filesIt->path(); ++count; }
                }
                if (count != 1) continue;
            }
        }
        const auto identifier = lowerAscii(module.stem().u8string());
        if (identifier.empty() || !identities.insert(identifier).second) continue;
        found.push_back({name, identifier, module});
    }
    std::sort(found.begin(), found.end(), [](const Effect& a, const Effect& b) {
        return lowerAscii(a.name) < lowerAscii(b.name);
    });
    return found;
}
} // namespace skellpreset::host::flnative
