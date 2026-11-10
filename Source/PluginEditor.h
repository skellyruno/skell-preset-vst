#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
class SkellPresetAudioProcessorEditor final:public juce::AudioProcessorEditor {
public:
    explicit SkellPresetAudioProcessorEditor(SkellPresetAudioProcessor&);
    ~SkellPresetAudioProcessorEditor() override;
    void resized() override;
    // Deterministic editor capture hooks used by the headless screenshot test.
    // They exercise the actual editor and controls without a separate mock UI.
    void configureForScreenshot(int themeVariant, int workspaceMode, bool showPresetBrowser);
    void selectRackSlotForScreenshot(int physicalSlot,int scrollOffset);
    juce::StringArray getScreenshotLayoutIssues() const;
    juce::StringArray runScreenshotInteractionTests();
private:
    class Canvas;
    std::unique_ptr<Canvas> canvas;
    SkellPresetAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellPresetAudioProcessorEditor)
};
