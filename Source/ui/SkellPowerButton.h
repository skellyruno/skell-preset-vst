#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <algorithm>

// Skell Glitch-inspired recessed circular hardware power switch. Preserve
// Skell Preset's independent host-automatable bypass and its immediate state
// feedback; only the paint treatment changes.
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
        const auto b=getLocalBounds().toFloat().reduced(2.f);
        const auto centre=b.getCentre();
        const float radius=std::min(b.getWidth(),b.getHeight())*.47f;
        const auto ring=[&](float r){
            return juce::Rectangle<float>(centre.x-r,centre.y-r,2.f*r,2.f*r);
        };
        g.setColour(juce::Colours::black.withAlpha(.42f));
        g.fillEllipse(ring(radius+1.3f).translated(.6f,2.f));
        g.setColour(Theme::stroke.darker(.4f));
        g.fillEllipse(ring(radius));
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(down?.14f:over?.52f:.31f),
            centre.x-radius*.7f,centre.y-radius*.8f,
            Theme::bg,centre.x+radius*.8f,centre.y+radius,false));
        g.fillEllipse(ring(radius-1.5f));
        g.setColour(Theme::white.withAlpha(.15f));
        g.drawEllipse(ring(radius-3.6f),.9f);
        const bool active=!bypassed;
        if(active) {
            g.setColour(Theme::green.withAlpha(over?.15f:.09f));
            g.fillEllipse(ring(radius-4.4f));
        }
        juce::Path arc;
        arc.addCentredArc(centre.x,centre.y+.8f,8.6f,8.6f,0.f,.7f,
            juce::MathConstants<float>::twoPi-.7f,true);
        g.setColour(juce::Colours::black.withAlpha(.6f));
        g.strokePath(arc,juce::PathStrokeType(4.8f));
        g.drawLine(centre.x+1.f,centre.y-10.9f,centre.x+1.f,centre.y+.8f,4.f);
        const auto accent=active?Theme::green:Theme::muted.darker(.21f);
        g.setColour(accent);
        g.strokePath(arc,juce::PathStrokeType(2.7f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.drawLine(centre.x,centre.y-10.5f,centre.x,centre.y+.5f,2.5f);
        if(over||hasKeyboardFocus(true)) {
            g.setColour(Theme::green.withAlpha(.62f));
            g.drawEllipse(ring(radius-1.f),1.1f);
        }
    }
private:
    bool bypassed=false;
};
