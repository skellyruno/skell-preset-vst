// Real Skell-Preset screenshot and interaction tests powered by shared SkellCore.
// Linux: run under Xvfb. Never instantiate a mock editor or call the audio thread.
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/chain/NativeSoundMatchTrials.h"
#include "../Source/chain/VocalFeelRecipes.h"
#include "../Source/host/InstalledVST3Index.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>

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
constexpr std::array<const char*, 2> themes {{"obsidian", "crimson"}};
// Keep the persistent numeric IDs stable (0/2; retired 1 remains reserved).
constexpr std::array<int,2> themeIds {{0,2}};
constexpr std::array<float, 4> scales {{.70f, .85f, 1.f, 1.25f}};
} // namespace

int main(int argc, char** argv) {
    bool quick4=true,extras=false,factorySlots=false;
    for(int i=1;i<argc;++i) {
        if(juce::String(argv[i])=="--full")quick4=false;
        if(juce::String(argv[i])=="--quick4")quick4=true; // compatibility with older launchers
        if(juce::String(argv[i])=="--extras")extras=true;
        if(juce::String(argv[i])=="--factory-slots"){factorySlots=true;quick4=false;}
    }
    const juce::String argument = argc >= 3 && juce::String(argv[1]) == "--output"
        ? juce::String::fromUTF8(argv[2]) : juce::String("ui-screenshots");
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argument);
    std::fprintf(stderr,"[Quick4] main entered; processor=%zu bytes editor=%zu bytes\\n",
                 sizeof(SkellPresetAudioProcessor),sizeof(SkellPresetAudioProcessorEditor));
    juce::ScopedJuceInitialiser_GUI gui;
    auto processorStorage=std::make_unique<SkellPresetAudioProcessor>();
    auto& processor=*processorStorage;
    std::fprintf(stderr,"[Quick4] processor allocated\\n");
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
        processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(25)); // Zero-PDC tracking chain.
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
    // End-to-end Alchemist regression: THIS tests the same compiled JUCE
    // processor, seven serial factory stages and actual APVTS controls used
    // by the shipped VST3. The separate contour-header smoke is insufficient.
    // Synthetic audio only: owner's reference WAV never enters source control.
    {
        auto renderAlchemist=[&](bool withContour) {
            processor.prepareToPlay(48000.,256);
            processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
            if(!withContour)
                processor.setSlotParam(6,8,.5f); // legacy neutral p9
            const bool correctFactory=processor.slotType(6)==
                    SkellPresetAudioProcessor::AlchemistMatchFinishFX
                && processor.getSlotParam(6,8)==(withContour?1.f:.5f)
                && processor.getLatencySamples()==0;
            if(!correctFactory) {
                std::fprintf(stderr,"Alchemist native contour is not wired into the real VST3 rack\\n");
                ++failures;
            }
            double sumSquares=0.;
            int used=0;
            for(int block=0;block<150;++block) {
                juce::AudioBuffer<float> audio(2,256);
                for(int n=0;n<256;++n) {
                    constexpr double pi=3.14159265358979323846;
                    const double t=(block*256+n)/48000.;
                    const float x=.026f*(float)std::sin(2.*pi*231.*t)
                                 +.006f*(float)std::sin(2.*pi*3900.*t);
                    audio.setSample(0,n,x);
                    audio.setSample(1,n,x*.91f);
                }
                juce::MidiBuffer midi;
                processor.processBlock(audio,midi);
                for(int n=0;n<256;++n) {
                    const float y=audio.getSample(0,n);
                    if(!std::isfinite(y) || std::abs(y)>1.001f) {
                        std::fprintf(stderr,"Alchemist full native rack produced NaN or clipping\\n");
                        ++failures;
                        return 0.;
                    }
                    if(block>=35) {sumSquares+=(double)y*y;++used;}
                }
            }
            return used?std::sqrt(sumSquares/used):0.;
        };
        const double contourOn=renderAlchemist(true);
        const double contourOff=renderAlchemist(false);
        if(contourOff<=1.e-5 || contourOn<=contourOff*1.06) {
            std::fprintf(stderr,"Alchemist factory contour has no real audible VST3 path:"
                                " old RMS %.6f, reference RMS %.6f\\n",
                                contourOff,contourOn);
            ++failures;
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
    processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(25));
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
        std::fprintf(stderr,"[Quick4] creating real editor\\n");
        auto editorStorage=std::make_unique<SkellPresetAudioProcessorEditor>(processor);
        auto& editor=*editorStorage;
        std::fprintf(stderr,"[Quick4] real editor created\\n");
        if(processor.preparedAudioRate()!=48000.) {
            std::fprintf(stderr,"Processor sample rate was not prepared\n");
            ++failures;
        }
        processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(25));
        if(processor.getLatencySamples()!=0) {
            std::fprintf(stderr,"Live Lead must report zero added PDC samples\n");
            ++failures;
        }
        processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(24));
        if(processor.getLatencySamples()<=0) {
            std::fprintf(stderr,"Vocal Polish limiter must report positive PDC\n");
            ++failures;
        }
        processor.loadFactoryPreset(0);
        editor.dismissGuidedTourForScreenshot(); // no first-launch overlay in normal UI baselines
        std::fprintf(stderr,"[Quick4] running real-editor interaction checks\\n");
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
                    scene.theme = themeIds[(size_t)theme];
                    scene.mode = view.mode;
                    scene.browser = view.browser;
                    scene.width = juce::roundToInt(1168.f * scale);
                    scene.height = juce::roundToInt(688.f * scale);
                    options.scenes.push_back(scene);
                }
        options.scenes.push_back({"obsidian-guided-tour-70",818,482,0,1,false});
        // Themed Settings is native JUCE and needs actual local visual
        // inspection at both small and large host scales, in both skins.
        options.scenes.push_back({"obsidian-settings-70",818,482,0,0,false});
        options.scenes.push_back({"obsidian-settings-125",1460,860,0,0,false});
        options.scenes.push_back({"crimson-settings-70",818,482,2,0,false});
        options.scenes.push_back({"obsidian-mic-panel-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-expanded-70",818,482,0,1,false});
        options.scenes.push_back({"crimson-deesser-70",818,482,2,1,false});
        options.scenes.push_back({"obsidian-live-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-external-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-wet-dot-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-wet-dot-125",1460,860,0,1,false});
        options.scenes.push_back({"obsidian-installed-vst3-browser-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-right-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-rack-right-browser-70",818,482,0,1,true});
        options.scenes.push_back({"obsidian-factory-digital-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-threeband-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-vox-velvet-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-module-picker-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-rider-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-eightband-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-opto-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-harsh-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-factory-noise-70",818,482,0,1,false});
        // Dedicated verified captures for every native effect visualization.
        // No mock UI: each scene switches the actual rack module/editor.
        const std::array<std::pair<const char*,int>,43> effectScenes {{
            {"empty",SkellPresetAudioProcessor::Empty},
            {"eq",SkellPresetAudioProcessor::EQ},
            {"studio-eq",SkellPresetAudioProcessor::StudioEQFX},
            {"studio-limiter",SkellPresetAudioProcessor::StudioLimiterFX},
            {"studio-compressor",SkellPresetAudioProcessor::StudioCompressorFX},
            {"studio-fast-dist",SkellPresetAudioProcessor::StudioFastDistFX},
            {"studio-delay",SkellPresetAudioProcessor::StudioDelayFX},
            {"adlib-chop",SkellPresetAudioProcessor::AdlibChop},
            {"adlib-motion",SkellPresetAudioProcessor::AdlibMotion},
            {"adlib-filter",SkellPresetAudioProcessor::AdlibFilter},
            {"adlib-saturate",SkellPresetAudioProcessor::AdlibSaturate},
            {"vocal-feel-opto",SkellPresetAudioProcessor::VocalFeelOptoFX},
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
            {"alchemist-match-finish",SkellPresetAudioProcessor::AlchemistMatchFinishFX},
            {"mic-modeling",SkellPresetAudioProcessor::MicModelFX},
            {"skelldizer",SkellPresetAudioProcessor::SkelldizerFX}
        }};
        for(const auto& effect:effectScenes) {
            options.scenes.push_back({juce::String("obsidian-effect-")+effect.first+"-70",818,482,0,1,false});
            options.scenes.push_back({juce::String("obsidian-effect-")+effect.first+"-100",1168,688,0,1,false});
        }
        options.scenes.push_back({"obsidian-external-100",1168,688,0,1,false});
        options.scenes.push_back({"obsidian-vocal-suite-voice-100",1168,688,0,1,false});
        // Real JUCE suite captures are part of the deep, local visual audit,
        // not Quick4's four stable fast-start views.
        options.scenes.push_back({"obsidian-vocal-suite-voice-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-vocal-suite-dynamics-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-vocal-suite-mixes-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-vocal-suite-output-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-vocal-suite-style-picker-125",1460,860,0,1,false});
        options.scenes.push_back({"obsidian-vocal-suite-spectrum-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-effect-chorus-digital-bones-70",818,482,0,1,false});
        options.scenes.push_back({"obsidian-effect-alchemist-calibration-70",
                                  818,482,0,1,false});
        options.scenes.push_back({"obsidian-effect-alchemist-reference-70",
                                  818,482,0,1,false});
        // Complete module inventory: external and vocal suite have independent scenes.
        std::array<bool,SkellPresetAudioProcessor::NumberOfModules> covered{};
        covered[SkellPresetAudioProcessor::ExternalVST3]=true;
        covered[SkellPresetAudioProcessor::VocalFeelSuiteFX]=true;
        for(const auto& effect:effectScenes){
            if(covered[(size_t)effect.second]){std::fprintf(stderr,"Duplicate effect capture type %d\n",effect.second);++failures;}
            covered[(size_t)effect.second]=true;
        }
        for(size_t i=0;i<covered.size();++i)if(!covered[i]){std::fprintf(stderr,"Missing effect capture type %zu\n",i);++failures;}
        options.scenes.push_back({"obsidian-stage-audience-100",1168,688,0,1,false});
        const char* directoryNames[]{"default","genres","genre-leaf","tiers","underground","mainstream","artist","artist-main","artist-adlib"};
        for(int page=0;page<9;++page)for(int view=0;view<2;++view)
            options.scenes.push_back({juce::String("audit-browser-")+juce::String(page)+"-"+directoryNames[page]+(view==0?"-thumbnails":"-list"),1168,688,0,1,true});
        options.scenes.push_back({"audit-browser-0-minimum",818,482,0,1,true});
        options.scenes.push_back({"audit-browser-0-maximum",1460,860,0,1,true});
        if(factorySlots){
            for(int index=0;index<SkellPresetAudioProcessor::emptyRackProgramIndex;++index){
                processor.loadFactoryPreset(index);
                const int recipe=skellpreset::recipeForFactoryIndex(index);
                for(int pos=0;pos<processor.activeSlotCount();++pos){
                    const int physical=processor.slotIdAt(pos);
                    if(processor.slotType(physical)==SkellPresetAudioProcessor::Empty)continue;
                    options.scenes.push_back({juce::String("audit-factory-")+juce::String(recipe)+"-slot-"+juce::String(physical),1168,688,0,1,false});
                }
            }
            processor.loadFactoryPreset(0);
        }
        if(quick4) {
            // Four actual-editor views; the complete current two-theme
            // matrix and its independent regressions remain available.
            options.scenes={
                {"obsidian-simple-100",1168,688,0,0,false},
                {"obsidian-advanced-100",1168,688,0,1,false},
                {"obsidian-module-picker-70",818,482,0,1,false},
                {"obsidian-mic-panel-70",818,482,0,1,false}
            };
        }
        options.configure = [&](const auto& scene) {
            if(scene.id.startsWith("audit-factory-")){
                const auto recipe=scene.id.fromFirstOccurrenceOf("audit-factory-",false,false).getIntValue();
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(recipe));
            }else if(scene.id=="obsidian-mic-panel-70") {
                processor.loadFactoryPreset(0);
                if(auto* p=processor.state.getParameter("micPreOn"))
                    p->setValueNotifyingHost(p->convertTo0to1(1.f));
                if(auto* p=processor.state.getParameter("micPreInput"))
                    p->setValueNotifyingHost(p->convertTo0to1(1.f));
                if(auto* p=processor.state.getParameter("micPreTarget"))
                    p->setValueNotifyingHost(p->convertTo0to1(3.f));
            }
            if(scene.id=="obsidian-expanded-70") {
                processor.loadFactoryPreset(0);
                while(processor.activeSlotCount()<16)processor.appendRackSlot();
                processor.setSlotType(15,SkellPresetAudioProcessor::DeEsser);
                processor.setSlotOn(15,true);
            } else if(scene.id=="crimson-deesser-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(24)); // Skell Vocal Polish
            } else if(scene.id=="obsidian-live-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(25)); // Skell Live Lead, 0 PDC
            } else if(scene.id=="obsidian-rack-right-70"
                      || scene.id=="obsidian-rack-right-browser-70") {
                processor.loadFactoryPreset(0);
            } else if(scene.id=="obsidian-factory-digital-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(27));
            } else if(scene.id=="obsidian-factory-threeband-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(28));
            } else if(scene.id=="obsidian-factory-vox-velvet-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(29));
            } else if(scene.id=="obsidian-factory-rider-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(34));
            } else if(scene.id=="obsidian-factory-eightband-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(37));
            } else if(scene.id=="obsidian-factory-opto-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(38));
            } else if(scene.id=="obsidian-factory-harsh-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(39));
            } else if(scene.id=="obsidian-factory-noise-70") {
                processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(40));
            } else if(scene.id=="obsidian-module-picker-70") {
                processor.loadFactoryPreset(0);
            } else if(scene.id=="obsidian-rack-wet-dot-70"
                      || scene.id=="obsidian-rack-wet-dot-125") {
                processor.loadFactoryPreset(0);
                processor.setSlotOn(0,false);
                if(auto* wet=processor.state.getParameter("s3_wet"))
                    wet->setValueNotifyingHost(wet->convertTo0to1(.36f));
            } else if(scene.id=="obsidian-external-70" || scene.id=="obsidian-external-100" ||
                      scene.id=="obsidian-installed-vst3-browser-70") {
                processor.loadFactoryPreset(0);
                processor.setSlotType(0,SkellPresetAudioProcessor::ExternalVST3);
                processor.setSlotOn(0,true);
            } else if(scene.id.startsWith("obsidian-vocal-suite-")) {
                processor.clearRackToEmpty();
                processor.setSlotType(0,SkellPresetAudioProcessor::VocalFeelSuiteFX);
                processor.setSlotOn(0,true);
                const auto& recipe=skellpreset::dsp::vocalFeelRecipes[1].values;
                for(int k=0;k<16;++k)
                    processor.setSlotParam(0,k,recipe[(size_t)k]);
            } else if(scene.id=="obsidian-effect-chorus-digital-bones-70") {
                processor.loadFactoryPreset(0);
                processor.setSlotType(0,SkellPresetAudioProcessor::VocalChorusFX);
                processor.setSlotOn(0,true);
                processor.setSlotParam(0,0,.48f);
                processor.setSlotParam(0,1,.87f);
            } else if(scene.id=="obsidian-effect-alchemist-calibration-70"
                      || scene.id=="obsidian-effect-alchemist-match-finish-70"
                      || scene.id=="obsidian-effect-alchemist-reference-70") {
                // Both page captures must use the real Alchemist factory
                // rack and saved match coefficients, not Clean Lead with
                // generic fixture knob values.
                processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
                // Preserve all seven stage types and calibrated controls.
                // Match Finish is already slot 7 (physical index 6).
                if(processor.slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX
                    || processor.loadedFactoryPresetIndex()!=skellpreset::alchemist::factoryIndex
                    || processor.getLatencySamples()!=0) {
                    std::fprintf(stderr,"Alchemist factory stage 7 missing or unexpected latency in screenshot fixture\\n");
                    ++failures;
                }
            } else if(scene.id.startsWith("obsidian-effect-")) {
                processor.loadFactoryPreset(0);
                for(const auto& effect:effectScenes)
                    if(scene.id==juce::String("obsidian-effect-")+effect.first+"-70" ||
                       scene.id==juce::String("obsidian-effect-")+effect.first+"-100") {
                        processor.setSlotType(0,effect.second);
                        processor.setSlotOn(0,effect.second!=SkellPresetAudioProcessor::Empty);
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
            if(scene.id.startsWith("obsidian-vocal-suite-")) {
                const int page=scene.id.contains("dynamics")?1:
                               scene.id.contains("mixes")?2:
                               scene.id.contains("output")?3:0;
                editor.setSuiteForScreenshot(page,
                    scene.id.contains("style-picker"));
                if(scene.id.contains("spectrum"))
                    editor.setSuiteSpectrumForScreenshot(true);
            }
            if(scene.id=="obsidian-module-picker-70")
                editor.openModulePickerForScreenshot();
            if(scene.id=="obsidian-guided-tour-70" && !editor.showGuidedTourForScreenshot())
                std::fprintf(stderr,"Guided tour did not show in real-editor render\\n");
            if(scene.id=="obsidian-expanded-70")
                editor.selectRackSlotForScreenshot(15,6);
            else if(scene.id=="crimson-deesser-70")
                editor.selectRackSlotForScreenshot(9,0);
            else if(scene.id=="obsidian-effect-alchemist-match-finish-70"
                    || scene.id=="obsidian-effect-alchemist-calibration-70"
                    || scene.id=="obsidian-effect-alchemist-reference-70")
                editor.selectRackSlotForScreenshot(6,0);
            else if(scene.id.startsWith("obsidian-effect-")
                    || scene.id.startsWith("obsidian-vocal-suite-")
                    || scene.id=="obsidian-external-70" || scene.id=="obsidian-external-100"
                    || scene.id=="obsidian-installed-vst3-browser-70")
                editor.selectRackSlotForScreenshot(0,0);
            else if(scene.id=="obsidian-factory-digital-70")
                editor.selectRackSlotForScreenshot(3,0);
            else if(scene.id=="obsidian-factory-threeband-70" ||
                    scene.id=="obsidian-factory-vox-velvet-70")
                editor.selectRackSlotForScreenshot(1,0);
            else if(scene.id=="obsidian-factory-rider-70")
                editor.selectRackSlotForScreenshot(8,0);
            else if(scene.id=="obsidian-factory-eightband-70" ||
                    scene.id=="obsidian-factory-harsh-70" ||
                    scene.id=="obsidian-factory-noise-70")
                editor.selectRackSlotForScreenshot(8,0);
            else if(scene.id=="obsidian-factory-opto-70")
                editor.selectRackSlotForScreenshot(1,0);
            if(scene.id=="obsidian-installed-vst3-browser-70")
                editor.openInstalledVST3BrowserForScreenshot();
            if(scene.id=="obsidian-effect-alchemist-calibration-70")
                editor.setMatchPageForScreenshot(1);
            if(scene.id=="obsidian-effect-alchemist-reference-70")
                editor.setMatchPageForScreenshot(2);
            editor.setModOrbForScreenshot(scene.id=="obsidian-effect-chorus-digital-bones-70");
            editor.setMicPanelForScreenshot(scene.id=="obsidian-mic-panel-70");
            editor.setSettingsPanelForScreenshot(scene.id=="obsidian-settings-70" ||
                scene.id=="obsidian-settings-125" || scene.id=="crimson-settings-70");
            if(scene.id=="obsidian-stage-audience-100")editor.showHomeForScreenshot();
            if(scene.id.startsWith("audit-browser-")){
                const int page=scene.id.fromFirstOccurrenceOf("audit-browser-",false,false).getIntValue();
                const bool small=scene.id.endsWith("minimum"),large=scene.id.endsWith("maximum");
                if(!editor.configureBrowserForScreenshot(page,small?620:large?1000:780,small?530:large?604:560,scene.id.endsWith("-list"))){
                    std::fprintf(stderr,"Browser scene configuration failed: %s\n",scene.id.toRawUTF8());++failures;
                }
            }
            if(scene.id.startsWith("audit-factory-")){
                const int physical=scene.id.fromLastOccurrenceOf("-slot-",false,false).getIntValue();
                int pos=0;for(int n=0;n<processor.activeSlotCount();++n)if(processor.slotIdAt(n)==physical)pos=n;
                editor.selectRackSlotForScreenshot(physical,juce::jmax(0,pos-6));
            }
        };
        options.inspect = [&] { return editor.getScreenshotLayoutIssues(); };
        options.mustDiffer = {
            {"obsidian-advanced-70", "obsidian-preset-browser-70"},
            {"obsidian-advanced-70", "obsidian-simple-70"},
            {"obsidian-advanced-70", "obsidian-bypass-70"},
            {"obsidian-advanced-70", "crimson-advanced-70"},
            {"obsidian-advanced-70", "obsidian-expanded-70"},
            {"crimson-advanced-70", "crimson-deesser-70"},
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
            {"obsidian-effect-voxrack-room-70", "obsidian-effect-voxrack-comp-70"},
            {"obsidian-advanced-70", "obsidian-module-picker-70"},
            {"obsidian-effect-vocal-chorus-70", "obsidian-effect-vocal-flanger-70"},
            {"obsidian-effect-ring-mod-70", "obsidian-effect-vocal-flanger-70"},
            {"obsidian-effect-vocal-rider-70", "obsidian-effect-vocal-vibrato-70"},
            {"obsidian-effect-ducking-pingpong-70", "obsidian-effect-delay-70"},
            {"obsidian-effect-eight-band-eq-70", "obsidian-effect-fet-opto-70"},
            {"obsidian-effect-harshness-tamer-70", "obsidian-effect-adaptive-noise-70"},
            {"obsidian-factory-eightband-70", "obsidian-factory-opto-70"},
            {"obsidian-effect-studio-pitch-formant-70", "obsidian-effect-vocal-vibrato-70"},
            {"obsidian-effect-alchemist-match-finish-70",
             "obsidian-effect-alchemist-calibration-70"},
            {"obsidian-effect-alchemist-calibration-70",
             "obsidian-effect-alchemist-reference-70"},
            {"obsidian-effect-mic-modeling-70","obsidian-effect-eight-band-eq-70"},
            {"obsidian-effect-skelldizer-70","obsidian-effect-mic-modeling-70"},
            {"obsidian-effect-vocal-chorus-70","obsidian-effect-chorus-digital-bones-70"},
            {"obsidian-advanced-70","obsidian-mic-panel-70"},
            {"obsidian-simple-70","obsidian-settings-70"},
            {"obsidian-settings-70","crimson-settings-70"}
        };
        if(quick4) {
            options.mustDiffer={
                {"obsidian-simple-100","obsidian-advanced-100"},
                {"obsidian-advanced-100","obsidian-module-picker-70"},
                {"obsidian-advanced-100","obsidian-mic-panel-70"}
            };
        }
        std::fprintf(stderr,"[Quick4] rendering %zu real-editor scenes\\n",options.scenes.size());
        const auto report = skellcore::ui::headless::render(editor, folder, options);
        std::fprintf(stderr,"[Quick4] render returned; captured=%d errors=%d\\n",report.captured,report.errors.size());
        for (const auto& error : report.errors) {
            std::fprintf(stderr, "UI screenshot: %s\n", error.toRawUTF8());
            ++failures;
        }
        const int expectedScenes=quick4?4:(int)options.scenes.size();
        if (report.captured != expectedScenes) {
            std::fprintf(stderr,"Expected %d PNGs, captured %d\n",
                         expectedScenes,report.captured);
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
            for(int y=267;y<302;++y)for(int x=400;x<575;++x){
                const auto colour=legendCheck.image.getPixelAt(x,y);
                if(colour.getGreen()>120
                    && colour.getGreen()>colour.getRed()*1.35f
                    && colour.getGreen()>colour.getBlue()*1.20f)
                    ++neonTextPixels;
            }
        if(neonTextPixels<25) {
            std::fprintf(stderr,"Radio hardware output readout is missing at 70%%\n");
            ++failures;
        }
        if(!quick4)processor.loadFactoryPreset(0); // Full-matrix legacy baseline only.
        // Quick4 captures exactly the 4 current-source real editor scenes.
        // Full audits show only the two supported dark themes.
        constexpr int thumbnailWidth = 818, thumbnailHeight = 482;
        juce::Image overview(juce::Image::RGB,2*thumbnailWidth,
                             (quick4?2:(int)themes.size())*thumbnailHeight,true);
        {
        juce::Graphics g(overview);
        if(quick4) {
            // Use the exact four previously saved PNGs. Re-rendering the
            // editor for contact-sheet thumbnails used to reset Alchemist
            // to Clean Lead and made the sheet disagree with each scene.
            for(int i=0;i<4;++i) {
                const auto& scene=options.scenes[(size_t)i];
                const auto saved=juce::ImageFileFormat::loadFrom(
                    folder.getChildFile(scene.id+".png"));
                if(!saved.isValid() || saved.getWidth()<100 || saved.getHeight()<100) {
                    std::fprintf(stderr,"Quick4 saved PNG missing or invalid\n");
                    ++failures;
                } else {
                    g.drawImageWithin(saved,(i%2)*thumbnailWidth,
                        (i/2)*thumbnailHeight,thumbnailWidth,thumbnailHeight,
                        juce::RectanglePlacement::stretchToFit);
                }
            }
        } else {
            for(int theme=0;theme<(int)themes.size();++theme)
                for(int column=0;column<2;++column) {
                    editor.configureForScreenshot(themeIds[(size_t)theme],
                                                  views[(size_t)column].mode,
                                                  views[(size_t)column].browser);
                    const auto capture=skellcore::ui::headless::capture(
                        editor,thumbnailWidth,thumbnailHeight);
                    if(!capture.image.isValid() || capture.distinctColours<12) {
                        std::fprintf(stderr,"Contact sheet scene failed\n");
                        ++failures;
                    } else {
                        g.drawImageAt(capture.image,column*thumbnailWidth,
                                     theme*thumbnailHeight);
                    }
                }
        }
        } // Flush the Windows graphics context before PNG encoding.
        if (!skellcore::ui::headless::writePng(overview, folder.getChildFile("contact-sheet.png")))
            ++failures;
        if(quick4 && extras) {
            // Two EXTRA real-editor scene PNGs. Quick4 remains four primary
            // screenshots and the contact sheet is unchanged.
            // Output-stage separation: capture the actual STEREO dial at both
            // audible extremes so widen/merge are verified in the real editor.
            struct SeparationShot {const char* file;int raw;};
            constexpr SeparationShot separationShots[]{
                {"obsidian-stereo-wide-100.png",-96},
                {"obsidian-stereo-mono-100.png",96}
            };
            for(const auto& shot:separationShots) {
                processor.loadFactoryPreset(0);
                // Drive the real host parameter exactly as automation would.
                if(auto* p=processor.state.getParameter("stereoSeparation"))
                    p->setValueNotifyingHost(p->convertTo0to1((float)shot.raw));
                editor.configureForScreenshot(0,0,false);
                const auto separation=skellcore::ui::headless::capture(editor,1168,688);
                if(!separation.image.isValid() || separation.distinctColours<12 ||
                   !skellcore::ui::headless::writePng(separation.image,
                       folder.getChildFile(shot.file))) {
                    std::fprintf(stderr,"Actual STEREO separation screenshot failed\n");
                    ++failures;
                }
            }
            processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
            editor.configureForScreenshot(0,1,false);
            editor.showHomeForScreenshot();
            const auto home=skellcore::ui::headless::capture(editor,1168,688);
            if(!home.image.isValid() || home.distinctColours<12 ||
               !skellcore::ui::headless::writePng(
                   home.image,folder.getChildFile("obsidian-home-100.png"))) {
                std::fprintf(stderr,"Actual animated home screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(0);
            processor.setSlotType(0,SkellPresetAudioProcessor::StudioLimiterFX);
            processor.setSlotOn(0,true);
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(0,0);
            const auto limiter=skellcore::ui::headless::capture(editor,1168,688);
            if(!limiter.image.isValid() || limiter.distinctColours<12 ||
               !skellcore::ui::headless::writePng(
                   limiter.image,folder.getChildFile("obsidian-studio-limiter-100.png"))) {
                std::fprintf(stderr,"Actual Studio limiter editor screenshot failed\n");
                ++failures;
            }
            // User-requested native listening trials. These are screenshots
            // of REAL plugin factory states, not renders of protected audio.
            processor.loadFactoryPreset(skellpreset::deuce::factoryIndex);
            if(processor.activeSlotCount()!=12 ||
               processor.slotType(6)!=SkellPresetAudioProcessor::Compressor ||
               processor.slotType(7)!=SkellPresetAudioProcessor::DeEsser) {
                std::fprintf(stderr,"Deuce D/Max native 12-slot factory not loaded\n");
                ++failures;
            }
            editor.configureForScreenshot(0,0,false);
            const auto deuceSimple=skellcore::ui::headless::capture(editor,1168,688);
            if(!deuceSimple.image.isValid() || deuceSimple.distinctColours<12 ||
               !skellcore::ui::headless::writePng(deuceSimple.image,
                   folder.getChildFile("obsidian-deuce-dmax-native-simple-100.png"))) {
                std::fprintf(stderr,"Deuce D/Max native Simple screenshot failed\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(6,3);
            const auto deuceRack=skellcore::ui::headless::capture(editor,1168,688);
            if(!deuceRack.image.isValid() || deuceRack.distinctColours<12 ||
               !skellcore::ui::headless::writePng(deuceRack.image,
                   folder.getChildFile("obsidian-deuce-dmax-native-rack-100.png"))) {
                std::fprintf(stderr,"Deuce D/Max native rack screenshot failed\n");
                ++failures;
            }
            // Latest *approved listening winner* v2.5 is a separate native
            // 13-slot preview, not a 1:1 recreation of offline v2.3 DSP.
            processor.loadFactoryPreset(skellpreset::deuce_v25::factoryIndex);
            if(processor.activeSlotCount()!=13 ||
               processor.slotType(12)!=SkellPresetAudioProcessor::EightBandToneFX)
                ++failures;
            editor.configureForScreenshot(0,0,false);
            const auto v25Simple=skellcore::ui::headless::capture(editor,1168,688);
            if(!v25Simple.image.isValid() || v25Simple.distinctColours<12 ||
               !skellcore::ui::headless::writePng(v25Simple.image,
                   folder.getChildFile("obsidian-deuce-v25-native-simple-100.png"))) {
                std::fprintf(stderr,"Deuce v2.5 native Simple screenshot failed\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(12,6);
            const auto v25Rack=skellcore::ui::headless::capture(editor,1168,688);
            if(!v25Rack.image.isValid() || v25Rack.distinctColours<12 ||
               !skellcore::ui::headless::writePng(v25Rack.image,
                   folder.getChildFile("obsidian-deuce-v25-native-7khz-100.png"))) {
                std::fprintf(stderr,"Deuce v2.5 presence EQ screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(skellpreset::alchemist_trial::factoryIndex);
            if(processor.getSlotParam(6,12)<=.75f ||
               processor.slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
                std::fprintf(stderr,"Alchemist refined opt-in factory not loaded\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(6,0);
            const auto refined=skellcore::ui::headless::capture(editor,1168,688);
            if(!refined.image.isValid() || refined.distinctColours<12 ||
               !skellcore::ui::headless::writePng(refined.image,
                   folder.getChildFile("obsidian-alchemist-refined-trial-100.png"))) {
                std::fprintf(stderr,"Alchemist refined factory screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(skellpreset::alchemist_lighter_dynamics_trial::factoryIndex);
            if(processor.slotType(2)!=skellpreset::alchemist_lighter_dynamics_trial::stages[2].module ||
               processor.slotType(3)!=skellpreset::alchemist_lighter_dynamics_trial::stages[3].module) {
                std::fprintf(stderr,"Alchemist lighter-dynamics factory did not load\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(2,0);
            const auto lighterDynamics=skellcore::ui::headless::capture(editor,1168,688);
            if(!lighterDynamics.image.isValid() || lighterDynamics.distinctColours<12 ||
               !skellcore::ui::headless::writePng(lighterDynamics.image,
                   folder.getChildFile("obsidian-alchemist-lighter-dynamics-100.png"))) {
                std::fprintf(stderr,"Alchemist lighter-dynamics factory screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(skellpreset::alchemist_fast_compressor_blend_trial::factoryIndex);
            if(processor.slotType(2)!=skellpreset::alchemist_fast_compressor_blend_trial::stages[2].module ||
               processor.slotType(3)!=skellpreset::alchemist_fast_compressor_blend_trial::stages[3].module) {
                std::fprintf(stderr,"Alchemist fast-compressor blend factory did not load\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(2,0);
            const auto fastBlend=skellcore::ui::headless::capture(editor,1168,688);
            if(!fastBlend.image.isValid() || fastBlend.distinctColours<12 ||
               !skellcore::ui::headless::writePng(fastBlend.image,
                   folder.getChildFile("obsidian-alchemist-fast-compressor-blend-100.png"))) {
                std::fprintf(stderr,"Alchemist fast-compressor blend screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(skellpreset::alchemist_smooth_compressor_blend_trial::factoryIndex);
            if(processor.slotType(2)!=skellpreset::alchemist_smooth_compressor_blend_trial::stages[2].module ||
               processor.slotType(3)!=skellpreset::alchemist_smooth_compressor_blend_trial::stages[3].module) {
                std::fprintf(stderr,"Alchemist smooth-compressor blend factory did not load\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(3,0);
            const auto smoothBlend=skellcore::ui::headless::capture(editor,1168,688);
            if(!smoothBlend.image.isValid() || smoothBlend.distinctColours<12 ||
               !skellcore::ui::headless::writePng(smoothBlend.image,
                   folder.getChildFile("obsidian-alchemist-smooth-compressor-blend-100.png"))) {
                std::fprintf(stderr,"Alchemist smooth-compressor blend screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(skellpreset::alchemist_serial_dynamics_trial::factoryIndex);
            if(processor.slotType(2)!=SkellPresetAudioProcessor::SerialDynamicsFX ||
               processor.slotType(5)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
               processor.slotType(6)!=SkellPresetAudioProcessor::Limiter) {
                std::fprintf(stderr,"Alchemist serial-dynamics trial factory did not load\n");
                ++failures;
            }
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(2,0);
            const auto serialDynamics=skellcore::ui::headless::capture(editor,1168,688);
            if(!serialDynamics.image.isValid() || serialDynamics.distinctColours<12 ||
               !skellcore::ui::headless::writePng(
                   serialDynamics.image,folder.getChildFile("obsidian-alchemist-serial-dynamics-100.png"))) {
                std::fprintf(stderr,"Alchemist serial-dynamics factory screenshot failed\n");
                ++failures;
            }
            processor.loadFactoryPreset(0);
            editor.configureForScreenshot(0,1,false);
            editor.selectRackSlotForScreenshot(0,0);
            // The rack menu was previously never photographed, so clipped
            // bottom actions could pass the full UI-scene audit unnoticed.
            if(!editor.openRackContextMenuForScreenshot(0)) {
                std::fprintf(stderr,"Real rack context menu out of bounds\n");
                ++failures;
            } else {
                const auto menu=skellcore::ui::headless::capture(editor,1168,688);
                if(!menu.image.isValid() || menu.distinctColours<12 ||
                   !skellcore::ui::headless::writePng(
                       menu.image,folder.getChildFile("obsidian-rack-context-menu-100.png"))) {
                    std::fprintf(stderr,"Real rack context menu screenshot failed\n");
                    ++failures;
                }
            }
        }
        editor.configureForScreenshot(0, 1, false);
        std::printf("Skell-Preset shared SkellCore renderer: %d scenes, %d failures\n",
                    report.captured, failures);
    }
    processor.releaseResources();
    return failures == 0 ? 0 : 1;
}
