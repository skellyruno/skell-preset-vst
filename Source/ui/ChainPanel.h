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
    bool testWheelScrollDoesNotReorder();
    bool testWheelWetAndRack();
    bool testRemoveExtraSlot();
    bool canScrollForTest() const noexcept {
        return processor.activeSlotCount()>visibleRows;
    }
    int scrollPositionForTest() const noexcept {return scrollOffset;}
    std::function<void(int)> onSelection;
    std::function<void()> onClearSelection; // click empty rack to show idle visual
    // Double-clicking an effect opens the searchable replacement overlay.
    std::function<void(int)> onReplaceRequest;
    std::function<void()> onMicRequest;
    bool triggerMicForTest() {if(!onMicRequest)return false;onMicRequest();return true;}
    // Shared hit-test geometry for UI and regression tests.
    static constexpr int wetControlLeft=149,wetControlRight=181;
    static constexpr int powerControlLeft=182,powerControlRight=206;
    bool testRackControlsAndWet();
    bool wetReadoutVisibleForTest(int physicalSlot) const noexcept {
        return hoveredWetSlot==physicalSlot || wetDragSlot==physicalSlot;
    }
    bool triggerReplaceForTest(int visualRow) {return selectAndReplace(visualRow);}
    std::function<void(bool)> onDockRequest;
    void setDockedRight(bool value) {dockedRight=value;repaint();}
private:
    SkellPresetAudioProcessor& processor;
    int selected=0,dragFrom=-1,dragTo=-1,pendingFrom=-1;
    int wetDragSlot=-1;
    int hoveredWetSlot=-1; // physical ID under a wet dial; readout hidden elsewhere
    float wetDragStart=0.f;
    juce::Point<int> pressPoint;
    bool dragging=false;
    bool dockingDrag=false,dockedRight=false,dockCandidateRight=false;
    int hoveredRow=-1,scrollOffset=0,lastSlotCount=0;
    std::array<int,SkellPresetAudioProcessor::slotCount> lastVisual{};
    bool visualSnapshotValid=false;
    float railWheelCarry=0.f; // navigation only
    int rowAt(int y) const;
    void clampScroll();
    void applyWheelDelta(float deltaY);
    void addModule();
    void popupFor(int physicalSlot,juce::Point<int> screenPoint);
    void selectContextAction(int index);
    int contextPosition() const noexcept;
    bool contextOpen=false;
    int contextSlot=-1,contextTop=50,contextHover=-1;
    float slotWet(int physicalSlot) const noexcept;
    void setSlotWet(int physicalSlot,float wet);
    void applyWetWheel(int physicalSlot,float deltaY); // host-gestured fine trim
    void toggleSlotPower(int physicalSlot);
    bool selectAndReplace(int row);
};
