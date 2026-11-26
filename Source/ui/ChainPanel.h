#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <functional>
#include <array>
class ChainPanel final:public juce::Component,public juce::SettableTooltipClient {
public:
    static constexpr int visibleRows=8; // Two offscreen rows so the wheel can scroll a default ten-slot rack.
    explicit ChainPanel(SkellPresetAudioProcessor& p);
    ~ChainPanel() override;
    void visibilityChanged() override;
    void paint(juce::Graphics&) override;
    void refreshIfChanged(); // avoid painting unchanged effect rows
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void setSelectedSlot(int s) {
        selected=s;
        // External preset/slot selection should reveal its rack position,
        // without firing onSelection again or renumbering automation IDs.
        if(s>=0) {
            for(int row=0;row<processor.activeSlotCount();++row)
                if(processor.slotIdAt(row)==s){
                    if(row<scrollOffset)scrollOffset=row;
                    if(row>=scrollOffset+visibleRows)
                        scrollOffset=row-visibleRows+1;
                    break;
                }
        }
        visualSnapshotValid=false;repaint();
    }
    void setScreenshotScrollOffset(int offset) {
        scrollOffset=juce::jlimit(0,juce::jmax(0,processor.activeSlotCount()-visibleRows),offset);
        visualSnapshotValid=false;repaint();
    }
    bool testWheelScrollDoesNotReorder();
    bool testWheelWetAndRack();
    bool testWheelReordersPhysicalEffect();
    bool testRackKeyboardNavigation();
    bool testWheelRegions();
    bool testRackRailNavigation();
    bool testProgrammaticSelectionReveal();
    bool testRemoveExtraSlot();
    // Local Quick4 captures the ACTUAL custom right-click menu, including
    // the bottom Remove from Rack action. No mock popup is drawn.
    bool openContextMenuForScreenshot(int visualRow) {
        if(visualRow<scrollOffset || visualRow>=processor.activeSlotCount() ||
           visualRow>=scrollOffset+visibleRows)return false;
        popupFor(processor.slotIdAt(visualRow),{});
        return contextOpen && contextTop>=0 &&
               contextTop+218<=getHeight() && getWidth()>=209;
    }
    void dismissContextMenuForScreenshot() {contextOpen=false;contextHover=-1;repaint();}
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
    bool testSidechainDock();
    std::function<void(int)> onSidechainEdit;
    bool wetReadoutVisibleForTest(int physicalSlot) const noexcept {
        return hoveredWetSlot==physicalSlot || wetDragSlot==physicalSlot;
    }
    bool triggerReplaceForTest(int visualRow) {return selectAndReplace(visualRow);}
    std::function<void(bool)> onDockRequest;
    void setDockedRight(bool value) {dockedRight=value;repaint();}
private:
    SkellPresetAudioProcessor& processor;
    int selected=0,dragFrom=-1,dragTo=-1,pendingFrom=-1;
    int pendingPhysical=-1;
    int wetDragSlot=-1;
    int hoveredWetSlot=-1; // physical ID under a wet dial; readout hidden elsewhere
    float wetDragStart=0.f;
    juce::Point<int> pressPoint;
    bool dragging=false;
    bool dockingDrag=false,dockedRight=false,dockCandidateRight=false;
    int hoveredRow=-1,scrollOffset=0,lastSlotCount=0;
    std::array<int,SkellPresetAudioProcessor::slotCount> lastVisual{};
    bool visualSnapshotValid=false;
    float railWheelCarry=0.f; // wheel on gutter or Shift+effect = viewport
    float effectWheelCarry=0.f;
    int wheelTargetSlot=-1; // stable physical ID throughout one wheel gesture
    double lastWheelEventMs=0.;
    int lastWheelX=-1000,lastWheelY=-1000;
    bool railDragging=false,railHovered=false;
    float railGrabOffset=0.f;
    juce::String moveNotice; // updated only after a real reorder
    bool movePhysicalSlot(int physical,int destination);
    void selectPhysical(int physical);
    void applyEffectWheel(int physical,float deltaY);
    int wheelRegionAt(int x,int y,bool shift) const;
    void moveRailTo(int y);
    int rowAt(int y) const;
    void clampScroll();
    void applyWheelDelta(float deltaY);
    void addModule();
    void popupFor(int physicalSlot,juce::Point<int> screenPoint);
    void selectContextAction(int index);
    int contextPosition() const noexcept;
    static int contextActionAt(int x,int y,int top) noexcept;
    bool contextActionEnabled(int index) const noexcept;
    void finishWetGesture();
    void cancelPointerGestures();
    bool contextOpen=false;
    int contextSlot=-1,contextTop=50,contextHover=-1;
    float slotWet(int physicalSlot) const noexcept;
    void setSlotWet(int physicalSlot,float wet);
    void applyWetWheel(int physicalSlot,float deltaY); // host-gestured fine trim
    void toggleSlotPower(int physicalSlot);
    bool selectAndReplace(int row);
    bool sidechainLinkMode=false;
    void openSidechainTargets();
    juce::RangedAudioParameter* sidechainParameter(int physicalSlot) const;
    bool sidechainLinked(int physicalSlot) const;
    bool toggleSidechainLink(int physicalSlot);
};
