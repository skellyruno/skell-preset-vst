#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>
class PresetBrowser final:public juce::Component {
public:
    explicit PresetBrowser(SkellPresetAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void open();
    void applyTheme();
    std::function<void()> onClose;
    std::function<void()> onLoaded;
private:
    SkellPresetAudioProcessor& processor;
    juce::TextEditor search;
    juce::TextButton importButton{"IMPORT"},exportButton{"SAVE PRESET"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool chooserActive=false;
    void setChooserActive(bool active);
    void chooseImport();
    void chooseExport();
    int bank=0,scroll=0,hoveredCard=-1;
    std::vector<int> filtered() const;
};
