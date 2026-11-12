#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

// Icon-only master bypass switch. Existing mode automation/keyboard semantics
// are preserved by TextButton, and no text crowds the far-right toolbar.
class SkellPowerButton final : public juce::TextButton {
public:
    SkellPowerButton() : juce::TextButton("") {
        setClickingTogglesState(false);
        setTooltip("Rack powered on; click to bypass the full chain");
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
        auto r=getLocalBounds().toFloat().reduced(.7f);
        const auto active=!bypassed;
        const auto accent=active?Theme::green:Theme::muted;
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(down?.03f:over?.16f:.08f),r.getTopLeft(),
            Theme::bg,r.getBottomLeft(),false));
        g.fillRoundedRectangle(r,7.f);
        g.setColour((active?Theme::green:Theme::stroke).withAlpha(
            over?.77f:active?.43f:.73f));
        g.drawRoundedRectangle(r.reduced(.6f),7.f,1.1f);
        const float cx=r.getCentreX(),cy=r.getCentreY()+.5f;
        const float radius=juce::jmin(r.getHeight(),r.getWidth())*.225f;
        if(active) {
            g.setColour(accent.withAlpha(over?.18f:.12f));
            g.fillEllipse(cx-radius-7.f,cy-radius-7.f,
                          2.f*radius+14.f,2.f*radius+14.f);
        }
        g.setColour(Theme::bg);
        g.fillEllipse(cx-radius-2.4f,cy-radius-2.4f,
                      2.f*radius+4.8f,2.f*radius+4.8f);
        juce::Path ring;
        constexpr float pi=juce::MathConstants<float>::pi;
        ring.addCentredArc(cx,cy,radius,radius,0.f,.24f*pi,1.76f*pi,true);
        g.setColour(accent.withAlpha(active?1.f:.63f));
        g.strokePath(ring,juce::PathStrokeType(
            over?2.8f:2.3f,juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
        g.drawLine(cx,cy-radius-2.9f,cx,cy-.4f,2.6f);
        g.setColour(Theme::white.withAlpha(.12f));
        g.drawLine(r.getX()+6.f,r.getY()+2.f,r.getRight()-6.f,r.getY()+2.f,1.f);
    }
private:
    bool bypassed=false;
};
