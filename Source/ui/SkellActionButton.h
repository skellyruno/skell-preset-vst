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
        // Draw the settings wheel as vector hardware instead of depending on
        // platform emoji fonts. At the compact 70% plugin size, the symbol
        // must remain centred and readable in its own header button.
        if(getButtonText()==juce::String::fromUTF8("\xE2\x9A\x99")) {
            const float cx=bounds.getCentreX(),cy=bounds.getCentreY();
            const auto steel=over?Theme::green:Theme::white.withAlpha(.85f);
            g.setColour(steel);
            constexpr float pi=juce::MathConstants<float>::pi;
            for(int spoke=0;spoke<8;++spoke) {
                const float angle=(float)spoke*pi*.25f;
                const float dx=std::cos(angle),dy=std::sin(angle);
                g.drawLine(cx+dx*8.0f,cy+dy*8.0f,
                           cx+dx*11.3f,cy+dy*11.3f,2.5f);
            }
            g.drawEllipse(cx-7.9f,cy-7.9f,15.8f,15.8f,2.0f);
            g.setColour(Theme::bg);
            g.fillEllipse(cx-3.0f,cy-3.0f,6.0f,6.0f);
            g.setColour(steel.withAlpha(.80f));
            g.drawEllipse(cx-3.0f,cy-3.0f,6.0f,6.0f,1.0f);
            return;
        }
        const float fontSize=getWidth()<=36?13.f:getWidth()<=92?11.6f:12.3f;
        g.setFont(Theme::font(fontSize,true));
        g.drawFittedText(getButtonText(),getLocalBounds().reduced(7,4),
                         juce::Justification::centred,1,.82f);
    }
};
