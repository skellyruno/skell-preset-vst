#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "ModulePicker.h"
#include "VocalFeelStyles.h"
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
// Single corner light controls the exact sN_on automation used by the rack.
// Drawn as a real button with a generous hit target, not a decorative LED.
class SkellSlotPowerDot final:public juce::Button {
public:
    SkellSlotPowerDot():juce::Button("Toggle this rack slot") {
        setTooltip("Click to bypass or enable this effect in both the editor and rack");
    }
    void setStatus(bool active,bool available) {
        if(on!=active || isEnabled()!=available) {
            on=active;setEnabled(available);repaint();
        }
    }
    void paintButton(juce::Graphics& g,bool hover,bool pressed) override {
        const float cx=getWidth()*.5f,cy=getHeight()*.5f;
        if(on && isEnabled()) {
            g.setColour(Theme::green.withAlpha(hover?.32f:.15f));
            g.fillEllipse(cx-13.f,cy-13.f,26.f,26.f);
        }
        g.setColour(Theme::bg);g.fillEllipse(cx-9.f,cy-9.f,18.f,18.f);
        g.setColour(on && isEnabled()?Theme::green:Theme::stroke);
        g.drawEllipse(cx-9.f,cy-9.f,18.f,18.f,1.7f);
        g.setColour(on && isEnabled()?Theme::green.brighter(pressed?0.f:.18f):
                    Theme::muted.withAlpha(.55f));
        g.fillEllipse(cx-5.f,cy-5.f,10.f,10.f);
    }
private:
    bool on=false;
};
// Compact JUCE popup with real APVTS attachments and no audio-thread GUI.
class SkellWetDuckPanel final:public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        Theme::panelBox(g,getLocalBounds().toFloat().reduced(1.f),8.f,true);
        Theme::text(g,"VOCAL SIDECHAIN / DUCK FX RETURNS",
                    {16,11,getWidth()-57,24},13.f,Theme::green,true);
        Theme::text(g,"DUCKING",{17,84,86,20},11,Theme::white,true);
        Theme::text(g,"THRESHOLD",{17,125,86,20},11,Theme::white,true);
        Theme::text(g,"ATTACK",{17,166,86,20},11,Theme::white,true);
        Theme::text(g,"RELEASE",{17,207,86,20},11,Theme::white,true);
        Theme::fittedText(g,"DRY VOCAL TRIGGERS / 0 PDC / NO SIDECHAIN BUS",
                          {17,getHeight()-17,getWidth()-33,12},9,Theme::muted);
    }
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
        vocalFeelStyles.close();
        modulePicker.setVisible(false);
        installedBrowser.setVisible(false);
        eqNodeMenu.setVisible(false);
        duckPanel.setVisible(false);
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
    bool testStudioDelayPages();
    bool testVocalFeelSuiteUI();
    bool testSuiteSpectrumUIForTest();
    bool testAdlibControls();
    bool testWetSidechainPanel();
    bool testMicModelCards();
    bool testPowerDotSync();
    bool testIntegratedEffectLayout() const;
    bool testPopupLayoutsForTest() const;
    bool testAllNativeEffectLayoutsForTest();
    bool testModOrbToggle();
    void setModOrbForScreenshot(bool enabled);
    void setMatchPageForScreenshot(int page) {showMatchPage(page);}
    void setSuiteForScreenshot(int page,bool stylesOpen) {
        showSuitePage(page);
        if(stylesOpen && vocalFeelStyleButton.onClick)
            vocalFeelStyleButton.onClick();
    }
    void setSuiteSpectrumForScreenshot(bool enabled) {
        if(processor.slotType(slot)==SkellPresetAudioProcessor::VocalFeelSuiteFX
           && suiteSpectrumView!=enabled && suiteSpectrumButton.onClick)
            suiteSpectrumButton.onClick();
    }
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
    int delayPage=0; // opt-in Studio Delay: 16 controls across four UI pages
    int suitePage=0; // Vocal Feel Suite: source 16 controls in four pages
    int gestureSuiteParameter=-1; // return p11..p15 graph gestures
    bool suiteSpectrumView=false;
    int suiteSpectrumTick=0;
    skellpreset::visual::LiveSpectrumAnalyzer suiteSpectrumAnalyzer;
    void showEqBank(int firstBand);
    void showMatchPage(int page);
    void showLimiterPage(int page);
    void showDelayPage(int page);
    void showSuitePage(int page);
    void selectSuiteRecipe(int index);
    void applyKnobReadouts();
    // ComboBox remains as a noninteractive compatibility/synchronization
    // adapter; the user-facing selector is the searchable native overlay.
    juce::ComboBox moduleBox;
    SkellSlotPowerDot slotPower;
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
    std::array<juce::TextButton,4> delayPageButtons;
    std::array<juce::TextButton,4> suitePageButtons;
    juce::TextButton vocalFeelStyleButton{"VOICE STYLES  ▼"};
    SkellVocalFeelStyles vocalFeelStyles;
    juce::TextButton suiteSpectrumButton{"SPECTRUM"};
    juce::Slider studioGateFloor;
    juce::Slider studioCompKnee;
    juce::TextButton duckOpen{"SIDECHAIN"},duckClose{"X"};
    SkellWetDuckPanel duckPanel;
    juce::ToggleButton duckOn{"DUCK WHEN VOCAL IS PRESENT"};
    juce::Slider duckDepth,duckThreshold,duckAttack,duckRelease;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> duckOnAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> duckAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateFloorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> studioCompKneeAttachment;
    std::array<SkellHoverPercentKnob,4> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqQAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,2> eqCutAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> knobAttachment;
    bool draggingGraph=false;
    int gestureEqBand=-1; // same parameter IDs receive begin/end gestures
    bool graphSnapshotValid=false;
    int lastType=-1;
    int lastPeakLevel=-1,lastGainReductionStep=-1,lastDeessStep=-1;
    std::array<int,5> lastSuiteLevels{{-1,-1,-1,-1,-1}};
    bool lastPowerOn=false;
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
