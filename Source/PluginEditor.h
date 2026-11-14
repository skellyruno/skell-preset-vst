#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
#include <SkellCore/GuidedTour.h>
class SkellPresetAudioProcessorEditor final:public juce::AudioProcessorEditor {
public:
    explicit SkellPresetAudioProcessorEditor(SkellPresetAudioProcessor&);
    ~SkellPresetAudioProcessorEditor() override;
    void resized() override;
    // Deterministic editor capture hooks used by the headless screenshot test.
    // They exercise the actual editor and controls without a separate mock UI.
    void configureForScreenshot(int themeVariant, int workspaceMode, bool showPresetBrowser);
    void selectRackSlotForScreenshot(int physicalSlot,int scrollOffset);
    void setMatchPageForScreenshot(int page);
    void setEqOrbForScreenshot(bool enabled);
    void dockRackForScreenshot(bool right);
    void openModulePickerForScreenshot();
    void openInstalledVST3BrowserForScreenshot();
    juce::StringArray getScreenshotLayoutIssues() const;
    juce::StringArray runScreenshotInteractionTests();
    bool showGuidedTourForScreenshot();
    void dismissGuidedTourForScreenshot();
private:
    class Canvas;
    std::unique_ptr<Canvas> canvas;
    SkellPresetAudioProcessor& processor;
    std::unique_ptr<skellcore::ui::GuidedTourController> guidedTour;
    void showGuidedTour(bool firstRun);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellPresetAudioProcessorEditor)
};
