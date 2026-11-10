// Real Skell-Preset editor screenshot harness.
// Requires a GUI message manager and, on Linux, a display server (Xvfb is fine).
// All drawing is performed by the production JUCE editor, not by a parallel mock UI.
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cstdio>
#include <memory>

namespace {
struct Scene {
    const char* name;
    int mode;
    bool browser;
};
constexpr std::array<Scene, 4> scenes {{
    {"advanced", 1, false},
    {"preset-browser", 1, true},
    {"simple", 0, false},
    {"bypass", 3, false}
}};
constexpr std::array<const char*, 3> themes {{"obsidian", "cream", "crimson"}};
constexpr std::array<float, 4> scales {{0.70f, 0.85f, 1.0f, 1.25f}};

bool writePng(const juce::Image& image, const juce::File& file) {
    if (!image.isValid()) return false;
    juce::FileOutputStream output(file);
    if (!output.openedOk()) return false;
    juce::PNGImageFormat png;
    return png.writeImageToStream(image, output) && output.flush();
}

bool hasVisibleContent(const juce::Image& image) {
    if (!image.isValid()) return false;
    const auto initial = image.getPixelAt(image.getWidth() / 2, image.getHeight() / 2);
    int different = 0;
    // Check a grid for paint output, rather than accepting an all-black image.
    for (int y = 10; y < image.getHeight(); y += juce::jmax(12, image.getHeight() / 25))
        for (int x = 10; x < image.getWidth(); x += juce::jmax(12, image.getWidth() / 25))
            if (image.getPixelAt(x, y) != initial) ++different;
    return different >= 12;
}

void printIssues(const juce::StringArray& issues, int& failures) {
    for (const auto& issue : issues) {
        std::fprintf(stderr, "UI layout: %s\n", issue.toRawUTF8());
        ++failures;
    }
}
} // namespace

int main(int argc, char** argv) {
    const juce::String outputArg = argc >= 3 && juce::String(argv[1]) == "--output"
        ? juce::String::fromUTF8(argv[2]) : juce::String("ui-screenshots");
    const auto outputDir = juce::File::getCurrentWorkingDirectory().getChildFile(outputArg);
    if (outputDir.createDirectory().failed()) {
        std::fprintf(stderr, "Could not create screenshot directory\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    SkellPresetAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    int failures = 0, generated = 0;
    {
        // A real editor and its child controls; no host plug-in window required.
        SkellPresetAudioProcessorEditor editor(processor);
        printIssues(editor.getScreenshotLayoutIssues(), failures);
        printIssues(editor.runScreenshotInteractionTests(), failures);
        for (int theme = 0; theme < (int)themes.size(); ++theme) {
            for (const auto& scene : scenes) {
                for (const float scale : scales) {
                    editor.configureForScreenshot(theme, scene.mode, scene.browser);
                    const int width = juce::roundToInt(1168.0f * scale);
                    const int height = juce::roundToInt(688.0f * scale);
                    editor.setSize(width, height);
                    // A snapshot traverses the real JUCE component tree and child paints.
                    auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds(), true);
                    const int percentage = juce::roundToInt(100.0f * scale);
                    const auto name = juce::String(themes[(size_t)theme]) + "-" + scene.name + "-"
                                    + juce::String(percentage) + ".png";
                    if (snapshot.getWidth() != width || snapshot.getHeight() != height
                        || !hasVisibleContent(snapshot) || !writePng(snapshot, outputDir.getChildFile(name))) {
                        std::fprintf(stderr, "FAILED: %s, expected %dx%d\n", name.toRawUTF8(), width, height);
                        ++failures;
                    } else {
                        ++generated;
                        std::printf("Captured %s (%dx%d)\n", name.toRawUTF8(), width, height);
                    }
                    printIssues(editor.getScreenshotLayoutIssues(), failures);
                }
            }
        }
        // Compact overview at the default 70% scale, one theme per row.
        constexpr int thumbW = 818, thumbH = 482;
        juce::Image overview(juce::Image::RGB, 2 * thumbW, 3 * thumbH, true);
        juce::Graphics g(overview);
        for (int theme = 0; theme < (int)themes.size(); ++theme) {
            for (int col = 0; col < 2; ++col) {
                const auto& scene = scenes[(size_t)col];
                editor.configureForScreenshot(theme, scene.mode, scene.browser);
                editor.setSize(thumbW, thumbH);
                const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true);
                g.drawImageAt(image, col * thumbW, theme * thumbH);
            }
        }
        if (!writePng(overview, outputDir.getChildFile("contact-sheet.png"))) ++failures;
        editor.configureForScreenshot(0, 1, false);
    }
    processor.releaseResources();
    std::printf("Skell-Preset UI renderer: %d PNG scenes + overview; %d failures\n", generated, failures);
    return failures == 0 && generated == 48 ? 0 : 1;
}
