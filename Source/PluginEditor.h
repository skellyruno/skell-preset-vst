#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
class SkellPresetAudioProcessorEditor final:public juce::AudioProcessorEditor {
public:
    explicit SkellPresetAudioProcessorEditor(SkellPresetAudioProcessor&);
    ~SkellPresetAudioProcessorEditor() override;
    void resized() override;
private:
    class Canvas;
    std::unique_ptr<Canvas> canvas;
    SkellPresetAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellPresetAudioProcessorEditor)
};
