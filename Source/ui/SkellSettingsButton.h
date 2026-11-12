#pragma once
#include <JuceHeader.h>
#include <SkellCore/SkellHardwareUI.h>
#include "Theme.h"

// The exact shared Skell Silence hardware gear renderer, with palette sampled
// at paint-time so switching between Preset themes remains instantaneous.
class SkellSettingsButton final : public juce::TextButton {
public:
    SkellSettingsButton() : juce::TextButton("") {
        setTooltip("Skell Preset settings and window size");
        setWantsKeyboardFocus(true);
    }
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        skellcore::ui::drawHardwareIconButton(
            g,getLocalBounds().toFloat(),skellcore::ui::HardwareGlyph::Settings,
            true,over,down,Theme::green,Theme::panelAlt,Theme::bg,
            hasKeyboardFocus(true));
    }
};
