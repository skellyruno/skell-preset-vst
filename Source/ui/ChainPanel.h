#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <functional>
class ChainPanel final:public juce::Component {
public:
    explicit ChainPanel(SkellPresetAudioProcessor& p);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void setSelectedSlot(int s) {selected=s;repaint();}
    std::function<void(int)> onSelection;
private:
    SkellPresetAudioProcessor& processor;
    int selected=0,dragFrom=-1,dragTo=-1;
    static int rowAt(int y);
    void popupFor(int physicalSlot);
};
