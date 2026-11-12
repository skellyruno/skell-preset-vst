#pragma once
#include <JuceHeader.h>
#include <SkellCore/SkellHardwareUI.h>
#include "Theme.h"

// Retains Skell Preset's separate host-automatable bypass control and its
// immediate on/off feedback, but uses the shared Skell Silence hardware
// power glyph. No audio mode or automation IDs are changed.
class SkellPowerButton final : public juce::TextButton {
public:
    SkellPowerButton() : juce::TextButton("") {
        setClickingTogglesState(false);
        setTooltip("Rack powered on; click to bypass the full chain");
        setWantsKeyboardFocus(true);
    }
    void setBypassed(bool bypass) {
        if(bypassed==bypass)return;
        bypassed=bypass;
        setTooltip(bypassed?"Rack bypassed; click to restore processing":
                            "Rack powered on; click to bypass the full chain");
        repaint();
    }
    bool isBypassed() const noexcept {return bypassed;}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        skellcore::ui::drawHardwareIconButton(
            g,getLocalBounds().toFloat(),skellcore::ui::HardwareGlyph::Power,
            !bypassed,over,down,Theme::green,Theme::panelAlt,Theme::bg,
            hasKeyboardFocus(true));
    }
private:
    bool bypassed=false;
};
