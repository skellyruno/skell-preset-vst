#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>
#include <functional>

// A real JUCE Slider (host automation, mouse and keyboard stay untouched),
// with a crisp hardware dial ONLY in the Simple macro view. The Advanced view
// delegates to the existing SkellCore horizontal slider.
class SkellMacroKnob final : public juce::Slider {
public:
    std::function<void()> onReadoutVisibilityChange;
    bool showsValueForTest() const noexcept {return hovered||adjusting;}
    void mouseEnter(const juce::MouseEvent& e) override {
        hovered=true;juce::Slider::mouseEnter(e);updateReadout();
    }
    void mouseExit(const juce::MouseEvent& e) override {
        hovered=false;juce::Slider::mouseExit(e);updateReadout();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        adjusting=true;juce::Slider::mouseDown(e);updateReadout();
    }
    void mouseUp(const juce::MouseEvent& e) override {
        juce::Slider::mouseUp(e);adjusting=false;updateReadout();
    }
    void paint(juce::Graphics& g) override {
        if(getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag) {
            juce::Slider::paint(g);
            return;
        }
        const auto area=getLocalBounds().toFloat();
        if(area.getWidth()<15.f||area.getHeight()<15.f)return;
        constexpr float pi=juce::MathConstants<float>::pi;
        const float centreX=area.getCentreX(),centreY=area.getCentreY();
        const float dial=std::min(area.getWidth(),area.getHeight())*.5f-7.f;
        const float rim=dial*.77f;
        const float body=dial*.67f;
        const float range=(float)(getMaximum()-getMinimum());
        const float value=range>0.f
            ?juce::jlimit(0.f,1.f,(float)((getValue()-getMinimum())/range))
            :0.f;
        const float start=1.25f*pi,end=2.75f*pi;
        const float angle=start+(end-start)*value;
        juce::Path track,fill;
        track.addCentredArc(centreX,centreY,dial*.92f,dial*.92f,0.f,start,end,true);
        fill.addCentredArc(centreX,centreY,dial*.92f,dial*.92f,0.f,start,angle,true);
        g.setColour(Theme::muted.withAlpha(.40f));
        g.strokePath(track,juce::PathStrokeType(2.6f,
                     juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(Theme::green);
        g.strokePath(fill,juce::PathStrokeType(3.4f,
                     juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // Recessed bezel and concentric, brushed black-metal hardware face.
        const auto bezel=juce::Rectangle<float>(
            centreX-rim,centreY-rim,2.f*rim,2.f*rim);
        g.setColour(Theme::bg.brighter(.30f));
        g.fillEllipse(bezel);
        g.setColour(Theme::muted.withAlpha(.44f));
        g.drawEllipse(bezel,.95f);
        const auto face=juce::Rectangle<float>(
            centreX-body,centreY-body,2.f*body,2.f*body);
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(.8f),face.getTopLeft(),
            Theme::bg.brighter(.15f),face.getBottomRight(),true));
        g.fillEllipse(face);
        g.setColour(Theme::white.withAlpha(.16f));
        g.drawEllipse(face.reduced(2.f),.85f);
        g.setColour(Theme::bg.withAlpha(.70f));
        g.drawEllipse(face.reduced(4.5f),1.f);
        // Precisely centred pointer: both stem and end dot use the same
        // angular position as the arc, eliminating lopsided movement.
        const float sn=std::sin(angle),cs=-std::cos(angle);
        const float x0=centreX+sn*body*.19f,y0=centreY+cs*body*.19f;
        const float x1=centreX+sn*body*.74f,y1=centreY+cs*body*.74f;
        g.setColour(Theme::bg.withAlpha(.65f));
        g.drawLine(x0+1.f,y0+1.f,x1+1.f,y1+1.f,3.9f);
        g.setColour(Theme::white);
        g.drawLine(x0,y0,x1,y1,2.35f);
        g.setColour(Theme::green.withAlpha(.92f));
        g.fillEllipse(x1-2.3f,y1-2.3f,4.6f,4.6f);
        g.setColour(Theme::white.withAlpha(.24f));
        g.fillEllipse(centreX-1.3f,centreY-1.3f,2.6f,2.6f);
    }
private:
    void updateReadout(){if(onReadoutVisibilityChange)onReadoutVisibilityChange();}
    bool hovered=false,adjusting=false;
};
