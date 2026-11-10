#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <functional>
#include <array>
class ChainPanel final:public juce::Component {
public:
    explicit ChainPanel(SkellPresetAudioProcessor& p);
    void paint(juce::Graphics&) override;
    void refreshIfChanged(); // avoid painting all ten rows while the chain is idle
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void setSelectedSlot(int s) {selected=s;repaint();}
    std::function<void(int)> onSelection;
private:
    SkellPresetAudioProcessor& processor;
    int selected=0,dragFrom=-1,dragTo=-1,pendingFrom=-1;
    juce::Point<int> pressPoint;
    bool dragging=false;
    int hoveredRow=-1;
    std::array<int,SkellPresetAudioProcessor::slotCount> lastVisual{};
    bool visualSnapshotValid=false;
    static int rowAt(int y);
    void addModule();
    void popupFor(int physicalSlot);
};
