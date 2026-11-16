#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "ModulePicker.h"
#include "InstalledVST3Browser.h"
#include "SkellEQNodeMenu.h"
#include <array>
#include <memory>
#include <functional>
#include "Theme.h"

// The actual DSP parameter value is always painted beside the dial by
// SlotEditor. The old normalized 0-100 hover badge was contradictory (e.g.
// "68" over a -19.4 dBFS threshold) and is intentionally removed.
// Retain precise host automation and mouse-wheel behavior.
class SkellHoverPercentKnob final:public juce::Slider {
public:
    void mouseEnter(const juce::MouseEvent& e) override {
        hovered=true;juce::Slider::mouseEnter(e);repaint();
    }
    void mouseExit(const juce::MouseEvent& e) override {
        hovered=false;juce::Slider::mouseExit(e);repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        editing=true;juce::Slider::mouseDown(e);repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        juce::Slider::mouseDrag(e);repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override {
        juce::Slider::mouseUp(e);editing=false;repaint();
    }
    bool showsValueForTest() const noexcept {return hovered||editing;}
protected:
    void paintOverChildren(juce::Graphics& g) override {
        juce::Slider::paintOverChildren(g);
        // Physical-unit readout is permanently visible beside the dial;
        // no competing normalized percentage bubble on hover/drag.
    }
private:
    bool hovered=false,editing=false;
};
class SlotEditor final:public juce::Component {
public:
    explicit SlotEditor(SkellPresetAudioProcessor& p);
    std::function<void()> onBackgroundClick; // blank effect-deck click -> home
    void paint(juce::Graphics&) override;
    void resized() override;
    void select(int newSlot);
    void applyTheme();
    void refreshIfChanged(); // only redraw when DSP controls or type change
    int selectedSlot()const noexcept{return slot;}
    bool moduleSelectorMatchesAudio() const noexcept {
        return moduleBox.getSelectedId()==processor.slotType(slot)+1;
    }
    void testGraphGesture(juce::Point<float> point) {updateGraphNode(point);}
    // Rack double-click opens this search overlay; no duplicate selector.
    void openModulePicker();
    void openModulePickerForScreenshot() {openModulePicker();}
    // UI-only: prevent overlapping modal effect selectors when opening the MIC panel.
    void dismissTransientOverlays() {
        modulePicker.setVisible(false);
        installedBrowser.setVisible(false);
        eqNodeMenu.setVisible(false);
    }
    bool modulePickerVisibleForTest() const noexcept {return modulePicker.isVisible();}
    bool hasEmbeddedEditorForTest() const noexcept {return embeddedEditor!=nullptr;}
    void openInstalledVST3BrowserForScreenshot() {loadExternal.onClick();}
    bool testModulePickerInteraction();
    bool testExternalBrowseCancel();
    bool testEightBandGraphAndQ();
    bool testStudioEqSeventhBand();
    bool testStudioEqAddAndFilterTypes();
    bool testStudioEqGraphDoubleClick();
    bool testKnobReadouts();
    bool testClassicDynamicsUnits();
    bool testMatchFinishPages();
    bool testStudioLimiterPages();
    bool testMicModelCards();
    bool testModOrbToggle();
    void setModOrbForScreenshot(bool enabled);
    void setMatchPageForScreenshot(int page) {showMatchPage(page);}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    SkellPresetAudioProcessor& processor;
    int slot=0;
    int eqBank=0; // two editable EQ bands per knob row; graph shows all eight
    int activeEqBand=0;
    int matchPage=0; // 0: sound shape / 1: calibration / 2: native reference match
    int limiterPage=0; // local UI pages only; existing p1..p9 host IDs
    void showEqBank(int firstBand);
    void showMatchPage(int page);
    void showLimiterPage(int page);
    void applyKnobReadouts();
    // ComboBox remains as a noninteractive compatibility/synchronization
    // adapter; the user-facing selector is the searchable native overlay.
    juce::ComboBox moduleBox;
    SkellModulePicker modulePicker;
    SkellInstalledVST3Browser installedBrowser;
    juce::TextButton loadExternal{"LOAD VST3"},editExternal{"OPEN PLUGIN UI"};
    juce::TextButton upgradeEq{"UPGRADE TO PARAMETRIC EQ"};
    juce::TextButton addEqNode{"+ ADD NODE"};
    SkellEQNodeMenu eqNodeMenu;
    int contextEqBand=0;
    void setStudioEqBandType(int band,int type);
    void setStudioEqBandEnabled(int band,bool enabled);
    int studioEqBandType(int band)const;
    bool studioEqBandEnabled(int band)const;
    void syncStudioEqNodeButtons();
    bool addStudioEqNodeAt(juce::Point<float> point);
    juce::TextButton modOrbButton{"ORB"};
    bool modOrbView=false; // UI-only; never serialized or used in DSP
    std::unique_ptr<juce::FileChooser> externalChooser;
    bool externalBrowsePending=false; // UI-only, not audio-thread state
    // Owned UI view, never created in the audio callback. Each installed
    // plugin may expose a JUCE wrapper editor; unsupported editors use
    // the original separate-window path without affecting DSP processing.
    std::unique_ptr<juce::AudioProcessorEditor> embeddedEditor;
    juce::AudioPluginInstance* embeddedHost=nullptr;
    void releaseEmbeddedEditor();
    void syncEmbeddedEditor();
    void layoutEmbeddedEditor();
    juce::Slider eqQSlider;
    std::array<juce::Slider,2> eqCutSliders;
    std::array<juce::TextButton,8> eqBandButtons;
    std::array<juce::TextButton,3> matchPageButtons;
    std::array<juce::TextButton,2> limiterPageButtons;
    juce::Slider studioGateFloor;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateFloorAttachment;
    std::array<SkellHoverPercentKnob,4> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqQAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,2> eqCutAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> knobAttachment;
    bool draggingGraph=false;
    int gestureEqBand=-1; // same parameter IDs receive begin/end gestures
    bool graphSnapshotValid=false;
    int lastType=-1;
    int lastPeakLevel=-1;
    int lastHostedLatency=-1;
    bool lastHostedLoaded=false;
    std::array<float,6> lastControls{};
    std::array<float,42> lastEqControls{}; // includes eight new filter types/enables
    bool updateMicCard(juce::Point<float> point);
    bool updateSkelldizerCard(juce::Point<float> point);
    void updateGraphNode(juce::Point<float>);
    void chooseVST3File();
    void loadVST3FromFile(const juce::File& file);
};
