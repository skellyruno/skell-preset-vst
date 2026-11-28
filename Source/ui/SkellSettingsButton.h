#pragma once
#include <JuceHeader.h>
#include <SkellCore/SkellHardwareUI.h>
#include "Theme.h"
#include "SkellHeaderFace.h"

// The exact shared Skell Silence hardware gear renderer, with palette sampled
// at paint-time so switching between Preset themes remains instantaneous.
class SkellSettingsButton final : public juce::TextButton {
public:
    SkellSettingsButton() : juce::TextButton("") {
        setTooltip("Skell Preset settings and window size");
        setWantsKeyboardFocus(true);
    }
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        if(getComponentID()=="header-control"){
            const auto r=skellpreset::header::face(g,getLocalBounds().toFloat(),isEnabled(),over,down,false,hasKeyboardFocus(true));
            const auto c=r.getCentre();juce::Path gear;
            for(int i=0;i<32;++i){const float theta=(float)i*juce::MathConstants<float>::twoPi/32.f;const float radius=(i%4==0||i%4==3)?7.f:9.f;const float x=c.x+radius*std::cos(theta),y=c.y+radius*std::sin(theta);if(i==0)gear.startNewSubPath(x,y);else gear.lineTo(x,y);}
            gear.closeSubPath();gear.addEllipse(c.x-3.f,c.y-3.f,6.f,6.f);gear.setUsingNonZeroWinding(false);
            g.setColour(Theme::white.withAlpha(over?1.f:.86f));g.fillPath(gear);return;
        }
        skellcore::ui::drawHardwareIconButton(
            g,getLocalBounds().toFloat(),skellcore::ui::HardwareGlyph::Settings,
            false,over,down,Theme::muted,Theme::panelAlt,Theme::bg,
            hasKeyboardFocus(true));
    }
};
