#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "SkellHeaderFace.h"

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
        const auto caption=getButtonText();
        if(getComponentID()=="header-control"){
            const auto r=skellpreset::header::face(g,getLocalBounds().toFloat(),enabled,over,down,getToggleState(),hasKeyboardFocus(true));
            const float cx=r.getCentreX(),cy=r.getCentreY();
            g.setColour(enabled?(getToggleState()?Theme::green:Theme::white):Theme::muted.withAlpha(.45f));
            if(caption.length()==1&&(caption[0]==0x21B6||caption[0]==0x21B7)){
                const float d=caption[0]==0x21B7?-1.f:1.f;
                juce::Path path;path.startNewSubPath(cx-d,cy-6);path.lineTo(cx-5*d,cy-2);path.lineTo(cx-d,cy+2);
                path.startNewSubPath(cx-5*d,cy-2);path.lineTo(cx+2*d,cy-2);path.cubicTo(cx+8*d,cy-2,cx+8*d,cy+6,cx+1*d,cy+6);path.lineTo(cx-d,cy+6);
                g.strokePath(path,juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            }else{
                if(caption=="MATCH"){const float x=r.getX()+15.f;juce::Path wave;wave.startNewSubPath(x-6,cy);wave.lineTo(x-3,cy);wave.lineTo(x-1,cy-5);wave.lineTo(x+2,cy+5);wave.lineTo(x+4,cy);wave.lineTo(x+7,cy);g.setColour(Theme::green.withAlpha(enabled?1.f:.4f));g.strokePath(wave,juce::PathStrokeType(1.5f));g.setColour(enabled?Theme::white:Theme::muted);}
                g.setFont(Theme::font(12.4f,true));g.drawFittedText(caption,getLocalBounds().reduced(7,3).withTrimmedLeft(caption=="MATCH"?18:0),juce::Justification::centred,1,.80f);
            }
            return;
        }
        if(caption.length()==1 && (caption[0]==0x21B6 || caption[0]==0x21B7)) {
            // Owner reference: recessed circular, neutral utility buttons.
            // Draw before the generic rectangular button face.
            const auto disc=bounds.withSizeKeepingCentre(
                juce::jmin(bounds.getWidth(),bounds.getHeight())-1.f,
                juce::jmin(bounds.getWidth(),bounds.getHeight())-1.f);
            const auto base=Theme::bg.interpolatedWith(Theme::panelAlt,enabled?.52f:.30f);
            g.setColour(Theme::bg.withAlpha(.65f));g.fillEllipse(disc.translated(0.f,.55f));
            g.setGradientFill(juce::ColourGradient(base.brighter(enabled?(down?.01f:over?.23f:.12f):.035f),
                disc.getTopLeft(),base.darker(down?.35f:.12f),disc.getBottomRight(),false));
            g.fillEllipse(disc);
            g.setColour(Theme::white.withAlpha(enabled?(over?.36f:.22f):.10f));
            g.drawEllipse(disc.reduced(.45f),.8f);
            g.setColour(Theme::white.withAlpha(enabled?.045f:.018f));
            g.drawEllipse(disc.reduced(1.65f),.65f);
            const float cx=disc.getCentreX(),cy=disc.getCentreY()+(down?.4f:0.f);
            const float scale=disc.getWidth()/31.f;
            const bool redo=caption[0]==0x21B7;
            juce::Graphics::ScopedSaveState saved(g);
            g.addTransform(juce::AffineTransform::translation(-cx,-cy)
                .scaled(redo?-scale:scale,scale).translated(cx,cy));
            juce::Path arrow;
            arrow.startNewSubPath(cx-1.5f,cy-6.5f);
            arrow.lineTo(cx-5.5f,cy-2.5f);
            arrow.lineTo(cx-1.5f,cy+1.1f);
            arrow.startNewSubPath(cx-5.5f,cy-2.5f);
            arrow.lineTo(cx+1.f,cy-2.5f);
            arrow.cubicTo(cx+8.f,cy-2.5f,cx+8.f,cy+6.f,cx+1.5f,cy+6.f);
            arrow.lineTo(cx-1.5f,cy+6.f);
            g.setColour(Theme::white.withAlpha(enabled?(over?.96f:.78f):.29f));
            g.strokePath(arrow,juce::PathStrokeType(1.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            if(hasKeyboardFocus(true)){
                g.setColour(Theme::white.withAlpha(.58f));g.drawEllipse(disc.reduced(2.8f),.85f);
            }
            return;
        }
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
            // Engraved inset round control, sharing the stereo dial's bezel.
            const float radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*.35f;
            const juce::Rectangle<float> disc(x-radius,y-radius,2.f*radius,2.f*radius);
            g.setColour(Theme::bg.withAlpha(.75f));g.fillEllipse(disc.translated(.5f,1.f));
            g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(.22f),disc.getTopLeft(),
                Theme::panelAlt.darker(.34f),disc.getBottomRight(),false));g.fillEllipse(disc);
            g.setColour(Theme::stroke);g.drawEllipse(disc,.9f);
            g.setColour(enabled?(active||over?Theme::green:Theme::white):Theme::muted);
            g.drawRoundedRectangle(x-2.6f,y-8.f,5.2f,10.5f,2.6f,1.3f);
            juce::Path cradle;
            cradle.startNewSubPath(x-6.f,y-2.f);
            cradle.cubicTo(x-6.f,y+8.f,x+6.f,y+8.f,x+6.f,y-2.f);
            g.strokePath(cradle,juce::PathStrokeType(1.4f));
            g.drawLine(x,y+5.f,x,y+10.f,1.4f);
            g.drawLine(x-4.f,y+10.f,x+4.f,y+10.f,1.4f);
            if(hasKeyboardFocus(true)) {g.setColour(accent);g.drawRoundedRectangle(bounds.reduced(2.5f),4.f,1.1f);}
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
