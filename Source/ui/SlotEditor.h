#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "ModulePicker.h"
#include <array>
#include <memory>
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
    void openModulePickerForScreenshot() {moduleTrigger.onClick();}
    bool testModulePickerInteraction();
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    SkellPresetAudioProcessor& processor;
    int slot=0;
    int eqBank=0; // two editable EQ bands per knob row; graph shows all eight
    int activeEqBand=0;
    void showEqBank(int firstBand);
    // ComboBox remains as a noninteractive compatibility/synchronization
    // adapter; the user-facing selector is the searchable native overlay.
    juce::ComboBox moduleBox;
    juce::TextButton moduleTrigger{"SELECT EFFECT"};
    SkellModulePicker modulePicker;
    juce::TextButton loadExternal{"LOAD VST3"},editExternal{"OPEN PLUGIN UI"};
    std::unique_ptr<juce::FileChooser> externalChooser;
    juce::ToggleButton activeButton;
    juce::Slider wetKnob;
    juce::Slider eqQSlider;
    std::array<juce::Slider,4> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqQAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> knobAttachment;
    bool draggingGraph=false;
    bool graphSnapshotValid=false;
    int lastType=-1;
    std::array<float,6> lastControls{};
    std::array<float,24> lastEqControls{};
    void updateGraphNode(juce::Point<float>);
};
