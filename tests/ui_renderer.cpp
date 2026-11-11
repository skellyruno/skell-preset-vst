// Real Skell-Preset screenshot and interaction tests powered by shared SkellCore.
// Linux: run under Xvfb. Never instantiate a mock editor or call the audio thread.
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
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
    // Exercise the real AudioProcessor, not only the portable DSP header.
    // Variable and oversized DAW blocks must be safe after the rack's
    // block-based refactor for external plugin hosting.
    for(double sampleRate:{44100.,48000.,96000.}) {
        processor.prepareToPlay(sampleRate,256);
        processor.loadFactoryPreset(25); // Zero-PDC tracking chain.
        for(int frameCount:{63,256,8193}) {
            juce::AudioBuffer<float> probe(2,frameCount);
            for(int n=0;n<frameCount;++n) {
                const float wave=.12f*std::sin((float)(n*0.06));
                probe.setSample(0,n,wave);
                probe.setSample(1,n,wave*.8f);
            }
            juce::MidiBuffer midi;
            processor.processBlock(probe,midi);
            for(int ch=0;ch<2;++ch)
                for(int n=0;n<frameCount;++n) {
                    const float value=probe.getSample(ch,n);
                    if(!std::isfinite(value) || std::abs(value)>3.f) {
                        std::fprintf(stderr,"DSP invalid output at %.0f Hz block %d\n",
                                     sampleRate,frameCount);
                        ++failures;
                        break;
                    }
                }
        }
    }
    processor.prepareToPlay(48000.,512);
    processor.loadFactoryPreset(25);
    if(auto* mode=processor.state.getParameter("mode"))
        mode->setValueNotifyingHost(mode->convertTo0to1(3.f));
    for(int step=0;step<12;++step) {
        juce::AudioBuffer<float> probe(2,512);
        for(int ch=0;ch<2;++ch)
            for(int n=0;n<512;++n)probe.setSample(ch,n,.125f);
        juce::MidiBuffer midi;
        processor.processBlock(probe,midi);
        if(step==11 && std::abs(probe.getSample(0,511)-.125f)>1.e-4f){
            std::fprintf(stderr,"Bypass did not settle to sample-aligned input\n");
            ++failures;
        }
    }
    processor.prepareToPlay(48000.,512);
    processor.loadFactoryPreset(0);
    {
        SkellPresetAudioProcessorEditor editor(processor);
        if(processor.preparedAudioRate()!=48000.) {
            std::fprintf(stderr,"Processor sample rate was not prepared\n");
            ++failures;
        }
        processor.loadFactoryPreset(25);
        if(processor.getLatencySamples()!=0) {
            std::fprintf(stderr,"Live Lead must report zero added PDC samples\n");
            ++failures;
        }
        processor.loadFactoryPreset(24);
        if(processor.getLatencySamples()<=0) {
            std::fprintf(stderr,"Vocal Polish limiter must report positive PDC\n");
            ++failures;
        }
        processor.loadFactoryPreset(0);
        editor.dismissGuidedTourForScreenshot(); // no first-launch overlay in normal UI baselines
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
        options.scenes.push_back({"obsidian-guided-tour-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-expanded-70",818,482,0,1,false});
        options.scenes.push_back({"cream-deesser-70",818,482,1,1,false});
        options.scenes.push_back({"obsidian-live-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-external-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-right-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-right-browser-70",818,482,0,1,true});
        // Dedicated verified captures for every native effect visualization.
        // No mock UI: each scene switches the actual rack module/editor.
        const std::array<std::pair<const char*,int>,10> effectScenes {{
            {"compressor",SkellPresetAudioProcessor::Compressor},
            {"limiter",SkellPresetAudioProcessor::Limiter},
            {"gate",SkellPresetAudioProcessor::Gate},
            {"saturator",SkellPresetAudioProcessor::Saturator},
            {"reverb",SkellPresetAudioProcessor::Reverb},
            {"delay",SkellPresetAudioProcessor::Delay},
            {"exciter",SkellPresetAudioProcessor::Exciter},
            {"adlib-throw",SkellPresetAudioProcessor::AdlibThrow},
            {"adlib-doubler",SkellPresetAudioProcessor::AdlibDoubler},
            {"deesser",SkellPresetAudioProcessor::DeEsser}
        }};
        for(const auto& effect:effectScenes)
            options.scenes.push_back({juce::String("obsidian-effect-")+effect.first+"-70",
                                      818,482,0,1,false});
        options.configure = [&](const auto& scene) {
            if(scene.id=="obsidian-expanded-70") {
                processor.loadFactoryPreset(0);
                while(processor.activeSlotCount()<16)processor.appendRackSlot();
                processor.setSlotType(15,SkellPresetAudioProcessor::DeEsser);
                processor.setSlotOn(15,true);
            } else if(scene.id=="cream-deesser-70") {
                processor.loadFactoryPreset(24); // Skell Vocal Polish
            } else if(scene.id=="obsidian-live-70") {
                processor.loadFactoryPreset(25); // Skell Live Lead, 0 PDC
            } else if(scene.id=="obsidian-rack-right-70"
                      || scene.id=="obsidian-rack-right-browser-70") {
                processor.loadFactoryPreset(0);
            } else if(scene.id=="obsidian-external-70") {
                processor.loadFactoryPreset(0);
                processor.setSlotType(0,SkellPresetAudioProcessor::ExternalVST3);
                processor.setSlotOn(0,true);
            } else if(scene.id.startsWith("obsidian-effect-")) {
                processor.loadFactoryPreset(0);
                for(const auto& effect:effectScenes)
                    if(scene.id==juce::String("obsidian-effect-")+effect.first+"-70") {
                        processor.setSlotType(0,effect.second);
                        processor.setSlotOn(0,true);
                        processor.setSlotParam(0,0,.68f);
                        processor.setSlotParam(0,1,.42f);
                        break;
                    }
            }
            editor.dismissGuidedTourForScreenshot();
            editor.configureForScreenshot(scene.theme, scene.mode, scene.browser);
            if(scene.id=="obsidian-rack-right-70"
               || scene.id=="obsidian-rack-right-browser-70")
                editor.dockRackForScreenshot(true);
            if(scene.id=="obsidian-guided-tour-70" && !editor.showGuidedTourForScreenshot())
                std::fprintf(stderr,"Guided tour did not show in real-editor render\\n");
            if(scene.id=="obsidian-expanded-70")
                editor.selectRackSlotForScreenshot(15,6);
            else if(scene.id=="cream-deesser-70")
                editor.selectRackSlotForScreenshot(9,0);
            else if(scene.id.startsWith("obsidian-effect-")
                    || scene.id=="obsidian-external-70")
                editor.selectRackSlotForScreenshot(0,0);
        };
        options.inspect = [&] { return editor.getScreenshotLayoutIssues(); };
        options.mustDiffer = {
            {"obsidian-advanced-70", "obsidian-preset-browser-70"},
            {"obsidian-advanced-70", "obsidian-simple-70"},
            {"obsidian-advanced-70", "obsidian-bypass-70"},
            {"obsidian-advanced-70", "cream-advanced-70"},
            {"cream-advanced-70", "crimson-advanced-70"},
            {"obsidian-advanced-70", "obsidian-expanded-70"},
            {"cream-advanced-70", "cream-deesser-70"},
            {"obsidian-advanced-70", "obsidian-live-70"},
            {"obsidian-effect-saturator-70", "obsidian-effect-reverb-70"},
            {"obsidian-effect-delay-70", "obsidian-effect-adlib-throw-70"},
            {"obsidian-effect-limiter-70", "obsidian-effect-exciter-70"},
            {"obsidian-effect-adlib-doubler-70", "obsidian-effect-gate-70"},
            {"obsidian-advanced-70", "obsidian-external-70"},
            {"obsidian-advanced-70", "obsidian-rack-right-70"},
            {"obsidian-rack-right-70", "obsidian-rack-right-browser-70"},
            {"obsidian-advanced-70", "obsidian-guided-tour-70"}
        };
        const auto report = skellcore::ui::headless::render(editor, folder, options);
        for (const auto& error : report.errors) {
            std::fprintf(stderr, "UI screenshot: %s\n", error.toRawUTF8());
            ++failures;
        }
        if (report.captured != 65) {
            std::fprintf(stderr, "Expected 65 PNGs, captured %d\n", report.captured);
            ++failures;
        }

        // Pixel-level branding regression: a previous AttributedString bug
        // drew PRESET at JUCE's 14pt default despite a 27pt design size.
        // Exclude the skull badge and the content area from the measurement.
        editor.dismissGuidedTourForScreenshot();
        editor.configureForScreenshot(0,1,false);
        const auto wordmark=skellcore::ui::headless::capture(editor,818,482);
        int firstX=818,lastX=-1,firstY=482,lastY=-1;
        if(wordmark.image.isValid())
            for(int y=7;y<45;++y)for(int x=53;x<242;++x) {
                const auto colour=wordmark.image.getPixelAt(x,y);
                if(colour.getGreen()>130
                   && colour.getGreen()>colour.getRed()*1.4f
                   && colour.getGreen()>colour.getBlue()*1.2f) {
                    firstX=juce::jmin(firstX,x);lastX=juce::jmax(lastX,x);
                    firstY=juce::jmin(firstY,y);lastY=juce::jmax(lastY,y);
                }
            }
        if(lastX-firstX<56 || lastY-firstY<11) {
            std::fprintf(stderr,"PRESET wordmark is missing or too small at 70%%\n");
            ++failures;
        }
        processor.loadFactoryPreset(0); // Restore ten-slot baseline for contact sheet.
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
