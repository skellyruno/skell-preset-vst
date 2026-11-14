#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <functional>
#include <array>
class ChainPanel final:public juce::Component,public juce::SettableTooltipClient {
public:
    static constexpr int visibleRows=8; // Two offscreen rows so the wheel can scroll a default ten-slot rack.
    explicit ChainPanel(SkellPresetAudioProcessor& p);
    void paint(juce::Graphics&) override;
    void refreshIfChanged(); // avoid painting unchanged effect rows
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void setSelectedSlot(int s) {selected=s;repaint();}
    void setScreenshotScrollOffset(int offset) {
        scrollOffset=juce::jlimit(0,juce::jmax(0,processor.activeSlotCount()-visibleRows),offset);
        visualSnapshotValid=false;repaint();
    }
    std::function<void(int)> onSelection;
    // Double-clicking an effect opens the searchable replacement overlay.
    std::function<void(int)> onReplaceRequest;
    // Shared hit-test geometry for UI and regression tests.
    static constexpr int wetControlLeft=149,wetControlRight=181;
    static constexpr int powerControlLeft=182,powerControlRight=206;
    bool testRackControlsAndWet();
    bool triggerReplaceForTest(int visualRow) {return selectAndReplace(visualRow);}
    std::function<void(bool)> onDockRequest;
    void setDockedRight(bool value) {dockedRight=value;repaint();}
private:
    SkellPresetAudioProcessor& processor;
    int selected=0,dragFrom=-1,dragTo=-1,pendingFrom=-1;
    int wetDragSlot=-1;
    float wetDragStart=0.f;
    juce::Point<int> pressPoint;
    bool dragging=false;
    bool dockingDrag=false,dockedRight=false,dockCandidateRight=false;
    int hoveredRow=-1,scrollOffset=0,lastSlotCount=0;
    std::array<int,SkellPresetAudioProcessor::slotCount> lastVisual{};
    bool visualSnapshotValid=false;
    int rowAt(int y) const;
    void clampScroll();
    void addModule();
    void popupFor(int physicalSlot);
    float slotWet(int physicalSlot) const noexcept;
    void setSlotWet(int physicalSlot,float wet);
    void toggleSlotPower(int physicalSlot);
    bool selectAndReplace(int row);
};
