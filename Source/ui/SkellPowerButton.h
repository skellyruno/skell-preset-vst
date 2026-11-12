#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

// One semantic power control: illuminated green means the rack is processing,
// muted/red means the entire chain is bypassed. Native JUCE Button handles
// mouse, keyboard, touch, accessibility and automation sync through Canvas.
class SkellPowerButton final : public juce::TextButton {
public:
    SkellPowerButton() : juce::TextButton("Power") {
        setClickingTogglesState(false);
        setTooltip("Power ON: processing active. Click to bypass.");
    }
    void setBypassed(bool bypass) {
        if(bypassed==bypass)return;
        bypassed=bypass;
        setTooltip(bypassed?"Power OFF: effect rack bypassed. Click to restore."
                           :"Power ON: processing active. Click to bypass.");
        repaint();
    }
    bool isBypassed() const noexcept {return bypassed;}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        auto rect=getLocalBounds().toFloat().reduced(.75f);
        const auto active=!bypassed;
        const auto accent=active?Theme::green:Theme::red;
        const auto base=Theme::panelAlt;
        g.setGradientFill(juce::ColourGradient(
            base.brighter(down?.13f:over?.10f:.06f),rect.getTopLeft(),
            base.darker(.22f),rect.getBottomRight(),false));
        g.fillRoundedRectangle(rect,7.f);
        g.setColour((over?accent:Theme::stroke).withAlpha(over?.79f:.70f));
        g.drawRoundedRectangle(rect.reduced(.5f),7.f,1.15f);
        const float cx=rect.getX()+26.f;
        const float cy=rect.getCentreY();
        const float radius=9.7f;
        juce::Path ring;
        constexpr float pi=juce::MathConstants<float>::pi;
        ring.addCentredArc(cx,cy,radius,radius,0.f,.22f*pi,1.78f*pi,true);
        g.setColour(accent.withAlpha(active?1.f:.72f));
        g.strokePath(ring,juce::PathStrokeType(
            2.65f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.drawLine(cx,cy-radius-3.f,cx,cy-1.25f,2.8f);
        if(active){
            g.setColour(accent.withAlpha(.1f));
            g.fillEllipse(cx-radius-2.f,cy-radius-2.f,
                          radius*2.f+4.f,radius*2.f+4.f);
        }
        g.setFont(juce::Font(juce::FontOptions(12.5f,juce::Font::bold)));
        g.setColour(active?Theme::white:Theme::muted);
        g.drawFittedText(active?"ON":"OFF",
                         juce::Rectangle<int>(49,0,getWidth()-53,getHeight()),
                         juce::Justification::centredLeft,1);
    }
private:
    bool bypassed=false;
};
