#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "ModulePicker.h"
#include "InstalledVST3Browser.h"
#include <array>
#include <memory>
#include "Theme.h"

// Stable 0-100 effect control indicator: numeric readouts are hidden while
// idle and appear only during hover, drag or mouse-wheel interaction.
// Host parameter precision, Hz/dB/Q conversion and automation IDs are
// unchanged. Labels remain visible in the parent effect panel.
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
        if(!hovered && !editing)return;
        const float w=(float)getWidth();
        const auto rect=juce::Rectangle<float>(w*.5f-23.f,(float)getHeight()-19.f,46.f,18.f);
        g.setColour(Theme::panelAlt);
        g.fillRoundedRectangle(rect,4.f);
        g.setColour(Theme::green.withAlpha(.7f));
        g.drawRoundedRectangle(rect,4.f,.85f);
        g.setColour(Theme::white);
        g.setFont(Theme::font(13.f,true));
        g.drawFittedText(juce::String(juce::roundToInt(
            juce::jlimit(0.,1.,getValue())*100.)),rect.toNearestInt(),
            juce::Justification::centred,1);
    }
private:
    bool hovered=false,editing=false;
};
class SlotEditor final:public juce::Component {
public:
    explicit SlotEditor(SkellPresetAudioProcessor& p);
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
    bool modulePickerVisibleForTest() const noexcept {return modulePicker.isVisible();}
    void openInstalledVST3BrowserForScreenshot() {loadExternal.onClick();}
    bool testModulePickerInteraction();
    bool testEightBandGraphAndQ();
    bool testKnobReadouts();
    bool testMatchFinishPages();
    bool testMicModelCards();
    bool testEqOrbToggle();
    void setEqOrbForScreenshot(bool enabled);
    void setMatchPageForScreenshot(int page) {showMatchPage(page);}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    SkellPresetAudioProcessor& processor;
    int slot=0;
    int eqBank=0; // two editable EQ bands per knob row; graph shows all eight
    int activeEqBand=0;
    int matchPage=0; // 0: sound shape / 1: calibrated fit inputs
    void showEqBank(int firstBand);
    void showMatchPage(int page);
    void applyKnobReadouts();
    // ComboBox remains as a noninteractive compatibility/synchronization
    // adapter; the user-facing selector is the searchable native overlay.
    juce::ComboBox moduleBox;
    SkellModulePicker modulePicker;
    SkellInstalledVST3Browser installedBrowser;
    juce::TextButton loadExternal{"LOAD VST3"},editExternal{"OPEN PLUGIN UI"};
    juce::TextButton upgradeEq{"UPGRADE TO PARAMETRIC EQ"};
    juce::TextButton eqOrbButton{"ORB"};
    bool eqOrbView=false; // UI-only; never serialized or used in DSP
    std::unique_ptr<juce::FileChooser> externalChooser;
    juce::Slider eqQSlider;
    std::array<juce::Slider,2> eqCutSliders;
    std::array<juce::TextButton,8> eqBandButtons;
    std::array<juce::TextButton,2> matchPageButtons;
    std::array<SkellHoverPercentKnob,4> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqQAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,2> eqCutAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> knobAttachment;
    bool draggingGraph=false;
    bool graphSnapshotValid=false;
    int lastType=-1;
    int lastPeakLevel=-1;
    int lastHostedLatency=-1;
    bool lastHostedLoaded=false;
    std::array<float,6> lastControls{};
    std::array<float,26> lastEqControls{};
    bool updateMicCard(juce::Point<float> point);
    void updateGraphNode(juce::Point<float>);
    void chooseVST3File();
    void loadVST3FromFile(const juce::File& file);
};
