// Screenshot harness uses the real editor and processor, not a recreated skin.
// Windows: run from a console (no plugin host and no visible window).
// Linux: run under xvfb-run -a to provide the JUCE X11 message loop.
#include "../Source/PluginEditor.h"
#include "../Source/ui/Theme.h"
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <set>

namespace {
struct Scene {
    const char* slug;
    int theme;
    int mode;
    bool browser;
    float scale;
};

constexpr std::array<Scene, 16> scenes {{
    {"obsidian_70_advanced", 0, 1, false, .70f},
    {"obsidian_70_browser",  0, 1, true,  .70f},
    {"obsidian_70_simple",   0, 0, false, .70f},
    {"obsidian_70_bypass",   0, 3, false, .70f},
    {"cream_70_advanced",    1, 1, false, .70f},
    {"cream_70_browser",     1, 1, true,  .70f},
    {"cream_70_simple",      1, 0, false, .70f},
    {"cream_70_bypass",      1, 3, false, .70f},
    {"crimson_70_advanced",  2, 1, false, .70f},
    {"crimson_70_browser",   2, 1, true,  .70f},
    {"crimson_70_simple",    2, 0, false, .70f},
    {"crimson_70_bypass",    2, 3, false, .70f},
    {"obsidian_85_advanced", 0, 1, false, .85f},
    {"obsidian_100_advanced",0, 1, false, 1.00f},
    {"obsidian_125_advanced",0, 1, false, 1.25f},
    {"crimson_125_browser",  2, 1, true,  1.25f}
}};

// A sampled hash catches blank images and unintentionally identical views.
std::uint64_t imageFingerprint(const juce::Image& image, std::size_t& uniqueColours) {
    std::uint64_t hash = 14695981039346656037ull;
    std::set<juce::uint32> colours;
    for (int y = 2; y < image.getHeight(); y += 11)
        for (int x = 2; x < image.getWidth(); x += 11) {
            const auto c = image.getPixelAt(x, y).getARGB();
            colours.insert(c);
            hash ^= (std::uint64_t)c;
            hash *= 1099511628211ull;
        }
    uniqueColours = colours.size();
    return hash;
}

bool renderOne(const juce::File& directory, const Scene& scene,
               juce::StringArray& manifest, juce::String& error,
               std::uint64_t& fingerprint) {
    SkellPresetAudioProcessor processor;
    processor.prepareToPlay(48000., 512);
    SkellPresetAudioProcessorEditor editor(processor);
    editor.configureForScreenshot(scene.theme, scene.mode, scene.browser);
    const int w = juce::roundToInt((float)Theme::width * scene.scale);
    const int h = juce::roundToInt((float)Theme::height * scene.scale);
    editor.setSize(w,h);
    editor.resized();
    // All child controls are drawn via JUCE's standard snapshot path,
    // including the preset overlay and transformed 1168x688 canvas.
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true);
    if (! image.isValid() || image.getWidth() != w || image.getHeight() != h) {
        error = juce::String(scene.slug) + ": screenshot dimension mismatch";
        return false;
    }
    std::size_t distinct = 0;
    fingerprint = imageFingerprint(image, distinct);
    if (distinct < 12) {
        error = juce::String(scene.slug) + ": unexpectedly blank screenshot";
        return false;
    }
    const auto output = directory.getChildFile(juce::String(scene.slug) + ".png");
    if (auto stream = output.createOutputStream()) {
        juce::PNGImageFormat png;
        if (!png.writeImageToStream(image, *stream)) {
            error = juce::String(scene.slug) + ": failed writing PNG";
            return false;
        }
        stream->flush();
    } else {
        error = juce::String(scene.slug) + ": cannot create PNG file";
        return false;
    }
    if (output.getSize() < 1500) {
        error = juce::String(scene.slug) + ": PNG suspiciously small";
        return false;
    }
    manifest.add("{\"file\":\"" + output.getFileName() + "\",\"width\":"
                 + juce::String(w) + ",\"height\":" + juce::String(h)
                 + ",\"theme\":" + juce::String(scene.theme)
                 + ",\"mode\":" + juce::String(scene.mode)
                 + ",\"browser\":" + juce::String(scene.browser ? "true" : "false")
                 + ",\"sampledColours\":" + juce::String((int)distinct) + "}");
    juce::Logger::writeToLog("Rendered " + output.getFileName() + " (" + juce::String(w)
                             + "x" + juce::String(h) + ")");
    editor.setVisible(false);
    processor.releaseResources();
    return true;
}
} // namespace

int main(int argc, char** argv) {
    // Needs JUCE MessageManager even though we never create a native window.
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    if (argc != 2) {
        juce::Logger::writeToLog("Usage: SkellPresetUiRenderer <output-directory>");
        return 2;
    }
    const juce::File directory(juce::String::fromUTF8(argv[1]));
    if (!directory.createDirectory()) {
        juce::Logger::writeToLog("Cannot create output directory: " + directory.getFullPathName());
        return 3;
    }
    juce::StringArray manifest;
    std::array<std::uint64_t, scenes.size()> fingerprints {};
    for (std::size_t i = 0; i < scenes.size(); ++i) {
        juce::String error;
        if (!renderOne(directory, scenes[i], manifest, error, fingerprints[i])) {
            juce::Logger::writeToLog("ERROR: " + error);
            return 4;
        }
    }
    // The browser, bypass, simple view and alternate skins must be distinct.
    for (auto pair: {std::pair{0u, 1u}, std::pair{0u, 2u},
                     std::pair{0u, 3u}, std::pair{0u, 4u},
                     std::pair{4u, 8u}}) {
        if (fingerprints[pair.first] == fingerprints[pair.second]) {
            juce::Logger::writeToLog("ERROR: two distinct views rendered identically");
            return 5;
        }
    }
    const auto json = "{\n  \"renderer\": \"Skell-Preset actual JUCE AudioProcessorEditor\",\n"
                      "  \"count\": " + juce::String((int)scenes.size())
                    + ",\n  \"scenes\": [\n    " + manifest.joinIntoString(",\n    ")
                    + "\n  ]\n}\n";
    if (!directory.getChildFile("manifest.json").replaceWithText(json)) return 6;
    juce::Logger::writeToLog("Completed " + juce::String((int)scenes.size()) + " screenshots");
    return 0;
}
