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
        if(getButtonText()=="STAGE_HOME") {
            // Transparent accessible wordmark hit area; no Stage button face.
            if(over || hasKeyboardFocus(true)){
                g.setColour(Theme::muted.withAlpha(.48f));
                g.drawLine(2.f,bounds.getBottom()-1.f,bounds.getRight()-2.f,bounds.getBottom()-1.f,1.f);
            }
            return;
        }
        const bool enabled=isEnabled();
        const bool active=getToggleState();
        const auto accent=getButtonText().startsWithIgnoreCase("STOP")?Theme::red:Theme::green;
        const auto face=Theme::panelAlt;
        g.setGradientFill(juce::ColourGradient(
            face.brighter(down?.04f:over?.18f:.10f),bounds.getTopLeft(),
            face.darker(down?.04f:.18f),bounds.getBottomLeft(),false));
        g.fillRoundedRectangle(bounds,5.6f);
        g.setColour((over?accent:Theme::stroke).withAlpha(
            enabled?(over?.66f:.75f):.28f));
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
        if(buttonCaption=="MIC") {
            const float x=bounds.getCentreX(),y=bounds.getCentreY();
            g.setColour(enabled?(active||over?Theme::green:Theme::white):Theme::muted);
            g.drawRoundedRectangle(x-3.f,y-9.f,6.f,12.f,3.f,1.5f);
            juce::Path cradle;
            cradle.startNewSubPath(x-6.f,y-2.f);
            cradle.cubicTo(x-6.f,y+8.f,x+6.f,y+8.f,x+6.f,y-2.f);
            g.strokePath(cradle,juce::PathStrokeType(1.4f));
            g.drawLine(x,y+5.f,x,y+10.f,1.4f);
            g.drawLine(x-4.f,y+10.f,x+4.f,y+10.f,1.4f);
            if(hasKeyboardFocus(true)) {g.setColour(accent);g.drawRoundedRectangle(bounds.reduced(2.5f),4.f,1.1f);}
            return;
        }
        const bool historySymbol=buttonCaption.length()==1 &&
            (buttonCaption[0]==0x21B6 || buttonCaption[0]==0x21B7);
        if(historySymbol) {
            // Crisp, custom vector history arrows instead of platform
            // font glyphs that render too thin or off-centre at 70% scaling.
            const bool reverse=buttonCaption[0]==0x21B7;
            const float cx=bounds.getCentreX(),cy=bounds.getCentreY();
            juce::Graphics::ScopedSaveState saved(g);
            const float iconScale=juce::jmin(1.f,bounds.getWidth()/37.f);
            g.addTransform(juce::AffineTransform::translation(-cx,-cy)
                .scaled(reverse?-iconScale:iconScale,iconScale)
                .translated(cx,cy));
            juce::Path arc;
            arc.startNewSubPath(cx-8.f,cy-3.f);
            arc.cubicTo(cx-2.f,cy-9.f,cx+10.f,cy-6.f,cx+10.f,cy+2.f);
            arc.cubicTo(cx+10.f,cy+8.f,cx+4.f,cy+10.f,cx-3.f,cy+8.f);
            g.setColour(enabled?(over?accent:Theme::white):Theme::muted.withAlpha(.5f));
            g.strokePath(arc,juce::PathStrokeType(2.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            juce::Path arrow;
            arrow.startNewSubPath(cx-7.f,cy-10.f);
            arrow.lineTo(cx-9.f,cy-3.f);
            arrow.lineTo(cx-1.f,cy-1.f);
            g.strokePath(arrow,juce::PathStrokeType(2.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
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
        // Text action buttons need the same keyboard-focus visibility as
        // the hardware Power, Settings, performer and history buttons.
        if(hasKeyboardFocus(true)){
            g.setColour(accent.withAlpha(enabled?.84f:.38f));
            g.drawRoundedRectangle(bounds.reduced(2.5f),4.f,1.25f);
        }
    }
};
