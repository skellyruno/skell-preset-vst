#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// One consistent, compact hardware treatment for header and utility buttons.
// Kept separate from the shared SkellCore look-and-feel so knobs, menus and
// hosted third-party plugin windows retain their own rendering.
class SkellActionButton final : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        auto bounds=getLocalBounds().toFloat().reduced(.65f);
        if(bounds.isEmpty())return;
        const bool enabled=isEnabled();
        const bool active=getToggleState();
        const auto accent=getButtonText().startsWithIgnoreCase("STOP")?Theme::red:Theme::green;
        const auto face=Theme::panelAlt;
        g.setGradientFill(juce::ColourGradient(
            face.brighter(down?.04f:over?.18f:.10f),bounds.getTopLeft(),
            face.darker(down?.04f:.18f),bounds.getBottomLeft(),false));
        g.fillRoundedRectangle(bounds,5.6f);
        g.setColour((over || active?accent:Theme::stroke).withAlpha(
            enabled?(over?.80f:.75f):.28f));
        g.drawRoundedRectangle(bounds.reduced(.5f),5.6f,1.05f);
        // Narrow inset glint and lower shadow give metal switch depth.
        g.setColour(Theme::white.withAlpha(over?.19f:.10f));
        g.drawLine(bounds.getX()+7.f,bounds.getY()+2.f,
                   bounds.getRight()-7.f,bounds.getY()+2.f,1.f);
        g.setColour(Theme::bg.withAlpha(.55f));
        g.drawLine(bounds.getX()+8.f,bounds.getBottom()-2.f,
                   bounds.getRight()-8.f,bounds.getBottom()-2.f,1.f);
        if(over || active) {
            g.setColour(accent.withAlpha(active?.64f:.30f));
            g.fillRoundedRectangle(bounds.getX()+7.f,bounds.getBottom()-3.1f,
                                   juce::jmax(0.f,bounds.getWidth()-14.f),1.35f,.6f);
        }
        g.setColour(enabled?(active?accent:Theme::white):Theme::muted.withAlpha(.4f));
        const float fontSize=getWidth()<=36?13.f:getWidth()<=92?11.6f:12.3f;
        g.setFont(Theme::font(fontSize,true));
        g.drawFittedText(getButtonText(),getLocalBounds().reduced(7,4),
                         juce::Justification::centred,1,.82f);
    }
};
