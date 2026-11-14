// Real Skell-Preset screenshot and interaction tests powered by shared SkellCore.
// Linux: run under Xvfb. Never instantiate a mock editor or call the audio thread.
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/host/InstalledVST3Index.h"
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
    // Verify filesystem-only VST3 browsing independently of installed plugin
    // binaries. This runs in the actual JUCE UI harness on Linux as well.
    {
        const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("skell-ui-vst3-index-"+juce::Uuid().toString());
        const auto first=root.getChildFile("Vendor").getChildFile("Space Echo.vst3");
        const auto second=root.getChildFile("Gate.vst3");
        const auto inside=first.getChildFile("Ignore Inside.vst3");
        const bool created=root.createDirectory().wasOk()
            && first.createDirectory().wasOk()
            && second.createDirectory().wasOk()
            && inside.createDirectory().wasOk();
        if(!created) {
            std::fprintf(stderr,"Could not create UI VST3-browser fixture\n");
            ++failures;
        } else {
            skellpreset::host::InstalledVST3Index index;
            juce::Array<juce::File> paths;
            paths.add(root);paths.add(root);
            index.start(paths);
            for(int pass=0;pass<100 && !index.finished();++pass)index.step(2);
            if(!index.finished() || index.truncated() || index.entries().size()!=2
               || index.filter("SPACE").size()!=1 || index.filter("gate").size()!=1
               || !index.filter("Ignore Inside").empty()) {
                std::fprintf(stderr,"Installed VST3 browser did not filter/stop at opaque bundles\n");
                ++failures;
            }
        }
        root.deleteRecursively();
    }
    // Exact playback/tempo position supplied to hosted effects at each chunk.
    // This tests the JUCE transport bridge without loading arbitrary DLLs.
    {
        skellpreset::host::HostedTransport transport;
        juce::AudioPlayHead::PositionInfo position;
        position.setBpm(120.);
        position.setTimeInSamples(std::int64_t{96000});
        position.setTimeInSeconds(2.);
        position.setPpqPosition(4.);
        position.setIsPlaying(true);
        transport.update(position,512,48000.);
        const auto out=transport.getPosition();
        if(!out || !out->getIsPlaying()
           || !out->getTimeInSamples() || *out->getTimeInSamples()!=96512
           || !out->getTimeInSeconds() || std::abs(*out->getTimeInSeconds()-2.010666666666667)>1.e-9
           || !out->getPpqPosition() || std::abs(*out->getPpqPosition()-4.021333333333333)>1.e-9) {
            std::fprintf(stderr,"Hosted transport chunk position regression\n");
            ++failures;
        }
        transport.update({},0,48000.);
        if(transport.getPosition()) {
            std::fprintf(stderr,"Hosted transport must not invent a missing DAW position\n");
            ++failures;
        }
    }
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
    // Genuine processBlock regression for the new FL-style wet/bypass controls.
    // Use a constant, low-level real buffer so a discontinuity is not masked
    // by a waveform transient. The original per-slot APVTS parameters remain
    // the sole source of automation; the DSP must interpolate switches.
    {
        processor.prepareToPlay(48000.,256);
        processor.loadFactoryPreset(0);
        for(int n=0;n<processor.activeSlotCount();++n) {
            const int slotId=processor.slotIdAt(n);
            processor.setSlotType(slotId,SkellPresetAudioProcessor::Empty);
            processor.setSlotOn(slotId,false);
        }
        processor.setSlotType(0,SkellPresetAudioProcessor::Saturator);
        processor.setSlotParam(0,0,.90f);
        processor.setSlotParam(0,1,.48f);
        processor.setSlotOn(0,true);
        auto* wet=processor.state.getParameter("s1_wet");
        if(!wet) {
            std::fprintf(stderr,"Rack wet host automation parameter missing\n");
            ++failures;
        } else {
            const std::array<float,5> wetStages{1.f,0.f,1.f,1.f,1.f};
            const std::array<bool,5> powerStages{true,true,true,false,true};
            std::array<float,5> steady{};
            float previous=0.f;
            bool havePrevious=false;
            float greatestStep=0.f;
            for(int stage=0;stage<5;++stage) {
                wet->setValueNotifyingHost(wet->convertTo0to1(wetStages[(size_t)stage]));
                processor.setSlotOn(0,powerStages[(size_t)stage]);
                // 12 blocks @ 256 = 64 ms, comfortably beyond 20 ms smoothing.
                for(int block=0;block<12;++block) {
                    juce::AudioBuffer<float> probe(2,256);
                    for(int ch=0;ch<2;++ch)probe.clear(ch,0,256);
                    for(int ch=0;ch<2;++ch)
                        for(int n=0;n<256;++n)probe.setSample(ch,n,.04f);
                    juce::MidiBuffer midi;
                    processor.processBlock(probe,midi);
                    for(int n=0;n<256;++n) {
                        const float value=probe.getSample(0,n);
                        if(!std::isfinite(value) || std::abs(value)>1.f ||
                           std::fpclassify(value)==FP_SUBNORMAL) {
                            std::fprintf(stderr,"Rack wet/bypass NaN, clip or denormal\n");
                            ++failures;break;
                        }
                        if(havePrevious)
                            greatestStep=std::max(greatestStep,std::abs(value-previous));
                        previous=value;havePrevious=true;
                    }
                    steady[(size_t)stage]=probe.getSample(0,255);
                }
            }
            if(greatestStep>.08f || std::abs(steady[0]-steady[1])<.002f ||
               std::abs(steady[1]-.04f)>.005f ||
               std::abs(steady[3]-.04f)>.005f ||
               std::abs(steady[4]-steady[0])>.005f) {
                std::fprintf(stderr,"Rack wet/bypass ramp or audible effect regression"
                                    " / max step %f / on %f / dry %f / muted %f / restore %f\n",
                    greatestStep,steady[0],steady[1],steady[3],steady[4]);
                ++failures;
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
        options.scenes.push_back({"obsidian-rack-wet-dot-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-wet-dot-125",1460,860,0,1,false});
        options.scenes.push_back({"obsidian-installed-vst3-browser-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-right-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-right-browser-70",818,482,0,1,true});
        options.scenes.push_back({"obsidian-factory-broadcast-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-digital-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-threeband-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-vox-velvet-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-vox-chamber-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-wide-choir-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-phantom-sweep-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-alien-ring-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-module-picker-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-rider-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-ducked-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-flutter-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-eightband-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-opto-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-harsh-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-noise-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-formant-70",818,482,0,1,false});
        // Dedicated verified captures for every native effect visualization.
        // No mock UI: each scene switches the actual rack module/editor.
        const std::array<std::pair<const char*,int>,29> effectScenes {{
            {"compressor",SkellPresetAudioProcessor::Compressor},
            {"limiter",SkellPresetAudioProcessor::Limiter},
            {"gate",SkellPresetAudioProcessor::Gate},
            {"saturator",SkellPresetAudioProcessor::Saturator},
            {"reverb",SkellPresetAudioProcessor::Reverb},
            {"delay",SkellPresetAudioProcessor::Delay},
            {"exciter",SkellPresetAudioProcessor::Exciter},
            {"adlib-throw",SkellPresetAudioProcessor::AdlibThrow},
            {"adlib-doubler",SkellPresetAudioProcessor::AdlibDoubler},
            {"deesser",SkellPresetAudioProcessor::DeEsser},
            {"radio",SkellPresetAudioProcessor::Radio},
            {"retro-crush",SkellPresetAudioProcessor::RetroCrush},
            {"tremolo",SkellPresetAudioProcessor::Tremolo},
            {"stereo-width",SkellPresetAudioProcessor::StereoWidth},
            {"threeband",SkellPresetAudioProcessor::ThreeBandDynamics},
            {"voxrack-room",SkellPresetAudioProcessor::VoxRackRoom},
            {"voxrack-comp",SkellPresetAudioProcessor::VoxRackSmoothComp},
            {"vocal-chorus",SkellPresetAudioProcessor::VocalChorusFX},
            {"vocal-flanger",SkellPresetAudioProcessor::VocalFlangerFX},
            {"ring-mod",SkellPresetAudioProcessor::RingModFX},
            {"vocal-rider",SkellPresetAudioProcessor::VocalRiderFX},
            {"ducking-pingpong",SkellPresetAudioProcessor::DuckingPingPongFX},
            {"vocal-vibrato",SkellPresetAudioProcessor::VocalVibratoFX},
            {"eight-band-eq",SkellPresetAudioProcessor::EightBandToneFX},
            {"fet-opto",SkellPresetAudioProcessor::SerialDynamicsFX},
            {"harshness-tamer",SkellPresetAudioProcessor::DynamicHarshnessFX},
            {"adaptive-noise",SkellPresetAudioProcessor::AdaptiveNoiseFX},
            {"studio-pitch-formant",SkellPresetAudioProcessor::StudioPitchFormantFX},
            {"alchemist-match-finish",SkellPresetAudioProcessor::AlchemistMatchFinishFX}
        }};
        for(const auto& effect:effectScenes)
            options.scenes.push_back({juce::String("obsidian-effect-")+effect.first+"-70",
                                      818,482,0,1,false});
        options.scenes.push_back({"obsidian-effect-alchemist-calibration-70",
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
            } else if(scene.id=="obsidian-factory-broadcast-70") {
                processor.loadFactoryPreset(26);
            } else if(scene.id=="obsidian-factory-digital-70") {
                processor.loadFactoryPreset(27);
            } else if(scene.id=="obsidian-factory-threeband-70") {
                processor.loadFactoryPreset(28);
            } else if(scene.id=="obsidian-factory-vox-velvet-70") {
                processor.loadFactoryPreset(29);
            } else if(scene.id=="obsidian-factory-vox-chamber-70") {
                processor.loadFactoryPreset(30);
            } else if(scene.id=="obsidian-factory-wide-choir-70") {
                processor.loadFactoryPreset(31);
            } else if(scene.id=="obsidian-factory-phantom-sweep-70") {
                processor.loadFactoryPreset(32);
            } else if(scene.id=="obsidian-factory-alien-ring-70") {
                processor.loadFactoryPreset(33);
            } else if(scene.id=="obsidian-factory-rider-70") {
                processor.loadFactoryPreset(34);
            } else if(scene.id=="obsidian-factory-ducked-70") {
                processor.loadFactoryPreset(35);
            } else if(scene.id=="obsidian-factory-flutter-70") {
                processor.loadFactoryPreset(36);
            } else if(scene.id=="obsidian-factory-eightband-70") {
                processor.loadFactoryPreset(37);
            } else if(scene.id=="obsidian-factory-opto-70") {
                processor.loadFactoryPreset(38);
            } else if(scene.id=="obsidian-factory-harsh-70") {
                processor.loadFactoryPreset(39);
            } else if(scene.id=="obsidian-factory-noise-70") {
                processor.loadFactoryPreset(40);
            } else if(scene.id=="obsidian-factory-formant-70") {
                processor.loadFactoryPreset(41);
            } else if(scene.id=="obsidian-module-picker-70") {
                processor.loadFactoryPreset(0);
            } else if(scene.id=="obsidian-rack-wet-dot-70"
                      || scene.id=="obsidian-rack-wet-dot-125") {
                processor.loadFactoryPreset(0);
                processor.setSlotOn(0,false);
                if(auto* wet=processor.state.getParameter("s3_wet"))
                    wet->setValueNotifyingHost(wet->convertTo0to1(.36f));
            } else if(scene.id=="obsidian-external-70" ||
                      scene.id=="obsidian-installed-vst3-browser-70") {
                processor.loadFactoryPreset(0);
                processor.setSlotType(0,SkellPresetAudioProcessor::ExternalVST3);
                processor.setSlotOn(0,true);
            } else if(scene.id=="obsidian-effect-alchemist-calibration-70"
                      || scene.id=="obsidian-effect-alchemist-match-finish-70") {
                // Both page captures must use the real Alchemist factory
                // rack and saved match coefficients, not Clean Lead with
                // generic fixture knob values.
                processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
                // Preserve all seven stage types and calibrated controls.
                // Match Finish is already slot 7 (physical index 6).
                if(processor.slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX
                    || processor.loadedFactoryPresetIndex()!=skellpreset::alchemist::factoryIndex) {
                    std::fprintf(stderr,"Alchemist factory stage 7 missing in screenshot fixture\\n");
                    ++failures;
                }
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
            if(scene.id=="obsidian-module-picker-70")
                editor.openModulePickerForScreenshot();
            if(scene.id=="obsidian-guided-tour-70" && !editor.showGuidedTourForScreenshot())
                std::fprintf(stderr,"Guided tour did not show in real-editor render\\n");
            if(scene.id=="obsidian-expanded-70")
                editor.selectRackSlotForScreenshot(15,6);
            else if(scene.id=="cream-deesser-70")
                editor.selectRackSlotForScreenshot(9,0);
            else if(scene.id=="obsidian-effect-alchemist-match-finish-70"
                    || scene.id=="obsidian-effect-alchemist-calibration-70")
                editor.selectRackSlotForScreenshot(6,0);
            else if(scene.id.startsWith("obsidian-effect-")
                    || scene.id=="obsidian-external-70"
                    || scene.id=="obsidian-installed-vst3-browser-70")
                editor.selectRackSlotForScreenshot(0,0);
            else if(scene.id=="obsidian-factory-broadcast-70"
                    || scene.id=="obsidian-factory-digital-70")
                editor.selectRackSlotForScreenshot(3,0);
            else if(scene.id=="obsidian-factory-threeband-70" ||
                    scene.id=="obsidian-factory-vox-velvet-70")
                editor.selectRackSlotForScreenshot(1,0);
            else if(scene.id=="obsidian-factory-vox-chamber-70")
                editor.selectRackSlotForScreenshot(5,0);
            else if(scene.id=="obsidian-factory-wide-choir-70" ||
                    scene.id=="obsidian-factory-alien-ring-70")
                editor.selectRackSlotForScreenshot(3,0);
            else if(scene.id=="obsidian-factory-phantom-sweep-70")
                editor.selectRackSlotForScreenshot(7,0);
            else if(scene.id=="obsidian-factory-rider-70" ||
                    scene.id=="obsidian-factory-flutter-70")
                editor.selectRackSlotForScreenshot(8,0);
            else if(scene.id=="obsidian-factory-ducked-70")
                editor.selectRackSlotForScreenshot(4,0);
            else if(scene.id=="obsidian-factory-eightband-70" ||
                    scene.id=="obsidian-factory-harsh-70" ||
                    scene.id=="obsidian-factory-noise-70")
                editor.selectRackSlotForScreenshot(8,0);
            else if(scene.id=="obsidian-factory-opto-70")
                editor.selectRackSlotForScreenshot(1,0);
            else if(scene.id=="obsidian-factory-formant-70")
                editor.selectRackSlotForScreenshot(8,0);
            if(scene.id=="obsidian-installed-vst3-browser-70")
                editor.openInstalledVST3BrowserForScreenshot();
            if(scene.id=="obsidian-effect-alchemist-calibration-70")
                editor.setMatchPageForScreenshot(1);
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
            {"obsidian-advanced-70", "obsidian-rack-wet-dot-70"},
            {"obsidian-external-70", "obsidian-installed-vst3-browser-70"},
            {"obsidian-advanced-70", "obsidian-rack-right-70"},
            {"obsidian-rack-right-70", "obsidian-rack-right-browser-70"},
            {"obsidian-advanced-70", "obsidian-guided-tour-70"},
            {"obsidian-effect-radio-70", "obsidian-effect-retro-crush-70"},
            {"obsidian-effect-tremolo-70", "obsidian-effect-stereo-width-70"},
            {"obsidian-effect-threeband-70", "obsidian-effect-radio-70"},
            {"obsidian-factory-broadcast-70", "obsidian-factory-threeband-70"},
            {"obsidian-effect-voxrack-room-70", "obsidian-effect-voxrack-comp-70"},
            {"obsidian-factory-vox-velvet-70", "obsidian-factory-vox-chamber-70"},
            {"obsidian-advanced-70", "obsidian-module-picker-70"},
            {"obsidian-effect-vocal-chorus-70", "obsidian-effect-vocal-flanger-70"},
            {"obsidian-effect-ring-mod-70", "obsidian-effect-vocal-flanger-70"},
            {"obsidian-factory-wide-choir-70", "obsidian-factory-alien-ring-70"},
            {"obsidian-effect-vocal-rider-70", "obsidian-effect-vocal-vibrato-70"},
            {"obsidian-effect-ducking-pingpong-70", "obsidian-effect-delay-70"},
            {"obsidian-factory-rider-70", "obsidian-factory-ducked-70"},
            {"obsidian-effect-eight-band-eq-70", "obsidian-effect-fet-opto-70"},
            {"obsidian-effect-harshness-tamer-70", "obsidian-effect-adaptive-noise-70"},
            {"obsidian-factory-eightband-70", "obsidian-factory-opto-70"},
            {"obsidian-effect-studio-pitch-formant-70", "obsidian-effect-vocal-vibrato-70"},
            {"obsidian-effect-alchemist-match-finish-70",
             "obsidian-effect-alchemist-calibration-70"}
        };
        const auto report = skellcore::ui::headless::render(editor, folder, options);
        for (const auto& error : report.errors) {
            std::fprintf(stderr, "UI screenshot: %s\n", error.toRawUTF8());
            ++failures;
        }
        if (report.captured != 105) {
            std::fprintf(stderr, "Expected 105 PNGs, captured %d\n", report.captured);
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
        processor.loadFactoryPreset(0);
        processor.setSlotType(0,SkellPresetAudioProcessor::Radio);
        editor.configureForScreenshot(0,1,false);
        editor.selectRackSlotForScreenshot(0,0);
        const auto legendCheck=skellcore::ui::headless::capture(editor,818,482);
        int neonTextPixels=0;
        if(legendCheck.image.isValid())
            for(int y=128;y<159;++y)for(int x=198;x<640;++x){
                const auto colour=legendCheck.image.getPixelAt(x,y);
                if(colour.getGreen()>120
                    && colour.getGreen()>colour.getRed()*1.35f
                    && colour.getGreen()>colour.getBlue()*1.20f)
                    ++neonTextPixels;
            }
        if(neonTextPixels<25) {
            std::fprintf(stderr,"Radio graph title is hidden by the plot clip at 70%%\n");
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
