#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
class SkellPresetAudioProcessorEditor final:public juce::AudioProcessorEditor {
public:
    explicit SkellPresetAudioProcessorEditor(SkellPresetAudioProcessor&);
    ~SkellPresetAudioProcessorEditor() override;
    void resized() override;
#if defined(SKELL_PRESET_UI_RENDERER)
    // Screenshot-only test seam; never compiled into the VST3 plugin target.
    void configureForScreenshot(int theme, int mode, bool presetBrowserOpen);
#endif
private:
    class Canvas;
    std::unique_ptr<Canvas> canvas;
    SkellPresetAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellPresetAudioProcessorEditor)
};
