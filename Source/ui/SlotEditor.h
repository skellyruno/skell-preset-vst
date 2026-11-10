#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <array>
#include <memory>
class SlotEditor final:public juce::Component {
public:
    explicit SlotEditor(SkellPresetAudioProcessor& p);
    void paint(juce::Graphics&) override;
    void resized() override;
    void select(int newSlot);
    void applyTheme();
    int selectedSlot()const noexcept{return slot;}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    SkellPresetAudioProcessor& processor;
    int slot=0;
    juce::ComboBox moduleBox;
    juce::ToggleButton activeButton;
    juce::Slider wetKnob;
    std::array<juce::Slider,4> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> knobAttachment;
    bool draggingGraph=false;
    void updateGraphNode(const juce::MouseEvent&);
};
