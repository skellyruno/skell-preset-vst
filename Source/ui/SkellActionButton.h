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
        const auto buttonCaption=getButtonText();
        const bool historySymbol=buttonCaption.length()==1 &&
            (buttonCaption[0]==0x21B6 || buttonCaption[0]==0x21B7);
        if(historySymbol) {
            // Crisp, custom vector history arrows instead of platform
            // font glyphs that render too thin or off-centre at 70% scaling.
            const bool reverse=buttonCaption[0]==0x21B7;
            const float cx=bounds.getCentreX(),cy=bounds.getCentreY();
            juce::Graphics::ScopedSaveState saved(g);
            if(reverse)
                g.addTransform(juce::AffineTransform::scale(-1.f,1.f)
                    .translated(2.f*cx,0.f));
            juce::Path arc;
            arc.startNewSubPath(cx+10.2f,cy+7.f);
            arc.cubicTo(cx+14.8f,cy-7.f,cx+2.f,cy-10.6f,cx-6.8f,cy-5.f);
            arc.cubicTo(cx-10.f,cy-3.f,cx-10.8f,cy-1.f,cx-11.2f,cy+1.f);
            g.setColour(enabled?(over?accent:Theme::white):Theme::muted.withAlpha(.34f));
            g.strokePath(arc,juce::PathStrokeType(2.35f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            juce::Path arrow;
            arrow.startNewSubPath(cx-14.f,cy-5.5f);
            arrow.lineTo(cx-4.6f,cy-2.1f);
            arrow.lineTo(cx-13.0f,cy+4.9f);
            arrow.closeSubPath();
            g.fillPath(arrow);
            if(hasKeyboardFocus(true)){
                g.setColour(accent.withAlpha(.85f));
                g.drawRoundedRectangle(bounds.reduced(2.5f),4.f,1.25f);
            }
            return;
        }
        const float fontSize=getWidth()<=36?13.f:
            getWidth()<=92?11.6f:12.3f;
        g.setFont(Theme::font(fontSize,true));
        g.drawFittedText(getButtonText(),getLocalBounds().reduced(7,4),
                         juce::Justification::centred,1,.82f);
    }
};
