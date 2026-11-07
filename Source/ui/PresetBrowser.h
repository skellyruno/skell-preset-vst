#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <array>
#include <functional>
#include <vector>
class PresetBrowser final:public juce::Component {
public:
    explicit PresetBrowser(SkellPresetAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void open();
    std::function<void()> onClose;
    std::function<void()> onLoaded;
private:
    SkellPresetAudioProcessor& processor;
    juce::TextEditor search;
    std::array<juce::Image,6> covers;
    int bank=0,scroll=0;
    std::vector<int> filtered() const;
};
