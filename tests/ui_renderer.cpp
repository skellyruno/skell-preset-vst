// Real Skell-Preset screenshot and interaction tests powered by shared SkellCore.
// Linux: run under Xvfb. Never instantiate a mock editor or call the audio thread.
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cstdio>

namespace {
struct View {
    const char* name;
    int mode;
    bool browser;
};
constexpr std::array<View, 4> views {{
    {"advanced", 1, false},
    {"preset-browser", 1, true},
    {"simple", 0, false},
    {"bypass", 3, false}
}};
constexpr std::array<const char*, 3> themes {{"obsidian", "cream", "crimson"}};
constexpr std::array<float, 4> scales {{.70f, .85f, 1.f, 1.25f}};
} // namespace

int main(int argc, char** argv) {
    const juce::String argument = argc >= 3 && juce::String(argv[1]) == "--output"
        ? juce::String::fromUTF8(argv[2]) : juce::String("ui-screenshots");
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argument);
    juce::ScopedJuceInitialiser_GUI gui;
    SkellPresetAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    int failures = 0;
    {
        SkellPresetAudioProcessorEditor editor(processor);
        for (const auto& issue : editor.runScreenshotInteractionTests()) {
            std::fprintf(stderr, "UI interaction: %s\n", issue.toRawUTF8());
            ++failures;
        }
        skellcore::ui::headless::Options options;
        options.project = "Skell-Preset actual JUCE AudioProcessorEditor";
        for (int theme = 0; theme < static_cast<int>(themes.size()); ++theme)
            for (const auto& view : views)
                for (auto scale : scales) {
                    skellcore::ui::headless::Scene scene;
                    scene.id = juce::String(themes[(size_t)theme]) + "-" + view.name + "-"
                        + juce::String(juce::roundToInt(scale * 100.f));
                    scene.theme = theme;
                    scene.mode = view.mode;
                    scene.browser = view.browser;
                    scene.width = juce::roundToInt(1168.f * scale);
                    scene.height = juce::roundToInt(688.f * scale);
                    options.scenes.push_back(scene);
                }
        options.configure = [&](const auto& scene) {
            editor.configureForScreenshot(scene.theme, scene.mode, scene.browser);
        };
        options.inspect = [&] { return editor.getScreenshotLayoutIssues(); };
        options.mustDiffer = {
            {"obsidian-advanced-70", "obsidian-preset-browser-70"},
            {"obsidian-advanced-70", "obsidian-simple-70"},
            {"obsidian-advanced-70", "obsidian-bypass-70"},
            {"obsidian-advanced-70", "cream-advanced-70"},
            {"cream-advanced-70", "crimson-advanced-70"}
        };
        const auto report = skellcore::ui::headless::render(editor, folder, options);
        for (const auto& error : report.errors) {
            std::fprintf(stderr, "UI screenshot: %s\n", error.toRawUTF8());
            ++failures;
        }
        if (report.captured != 48) {
            std::fprintf(stderr, "Expected 48 PNGs, captured %d\n", report.captured);
            ++failures;
        }

        // One compact overview: Advanced and Browser at default 70% for each skin.
        constexpr int thumbnailWidth = 818, thumbnailHeight = 482;
        juce::Image overview(juce::Image::RGB, 2 * thumbnailWidth, 3 * thumbnailHeight, true);
        juce::Graphics g(overview);
        for (int theme = 0; theme < static_cast<int>(themes.size()); ++theme)
            for (int column = 0; column < 2; ++column) {
                editor.configureForScreenshot(theme, views[(size_t)column].mode,
                                              views[(size_t)column].browser);
                const auto capture = skellcore::ui::headless::capture(editor, thumbnailWidth, thumbnailHeight);
                if (!capture.image.isValid() || capture.distinctColours < 12) {
                    std::fprintf(stderr, "Contact sheet scene failed\n");
                    ++failures;
                } else {
                    g.drawImageAt(capture.image, column * thumbnailWidth, theme * thumbnailHeight);
                }
            }
        if (!skellcore::ui::headless::writePng(overview, folder.getChildFile("contact-sheet.png")))
            ++failures;
        editor.configureForScreenshot(0, 1, false);
        std::printf("Skell-Preset shared SkellCore renderer: %d scenes, %d failures\n",
                    report.captured, failures);
    }
    processor.releaseResources();
    return failures == 0 ? 0 : 1;
}
