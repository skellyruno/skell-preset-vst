#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>
#include <functional>

// Hardware-style bipolar stereo separation dial for the Simple output stage.
//
// It is a real juce::Slider bound to the persistent "stereoSeparation" APVTS
// parameter, so host automation, gestures and keyboard stay intact. Only the
// presentation is bespoke: the same recessed bezel, engraved ticks and green
// arc as the existing macro and INPUT/MIX/OUTPUT knobs.
//
// The scale is bipolar around a centre detent at 0 (verified neutral). Turning
// left widens the stereo image, turning right merges toward mono. The readout
// names the direction instead of showing a bare number.
class SkellStereoSeparationKnob final : public juce::Slider {
public:
    SkellStereoSeparationKnob() {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        // Symmetric sweep so the pointer rests straight up at neutral.
        setRotaryParameters(juce::MathConstants<float>::pi*1.2f,
                            juce::MathConstants<float>::pi*2.8f,true);
        setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setDoubleClickReturnValue(true,0.);
        setVelocityBasedMode(false);
        setScrollWheelEnabled(true);
        setColour(juce::Slider::trackColourId,Theme::green);
        setColour(juce::Slider::thumbColourId,Theme::white);
        setName("Stereo separation");
        setTooltip("Stereo separation  |  left widens, right merges toward mono  |  double-click resets to 0%");
        textFromValueFunction=[](double value) {
            return SkellStereoSeparationKnob::readoutFor(value);
        };
        valueFromTextFunction=[](const juce::String& text) {
            const double value=text.getDoubleValue();
            const double raw=text.contains("%")?value*96./100.:value;
            return juce::jlimit(-96.,96.,text.containsIgnoreCase("WIDE")?-std::abs(raw):
                text.containsIgnoreCase("MONO")?std::abs(raw):raw);
        };
        updateText();
    }
    // A SliderAttachment installs the APVTS formatter, replacing ours. Call
    // this after binding so automation still shows the percentage plus the
    // separation direction instead of a bare raw number. Presentation only:
    // the underlying value, range and host normalization are untouched.
    void applyReadoutFormat() {
        textFromValueFunction=[](double value) {
            return SkellStereoSeparationKnob::readoutFor(value);
        };
        valueFromTextFunction=[](const juce::String& text) {
            const double value=text.getDoubleValue();
            const double raw=text.contains("%")?value*96./100.:value;
            return juce::jlimit(-96.,96.,text.containsIgnoreCase("WIDE")?-std::abs(raw):
                text.containsIgnoreCase("MONO")?std::abs(raw):raw);
        };
        updateText();
    }
    // Percentage of available travel, and the direction the matrix moves.
    static int percentFor(double value) noexcept {
        return juce::jlimit(0,100,
            juce::roundToInt(std::abs(juce::jlimit(-96.,96.,value))*100./96.));
    }
    static bool widening(double value) noexcept { return value < -.5; }
    static bool merging(double value) noexcept { return value > .5; }
    static juce::String readoutFor(double value) {
        if (percentFor(value)==0) return "0%  NEUTRAL";
        const auto amount=juce::String(percentFor(value));
        return widening(value) ? amount+"%  WIDE" : amount+"%  MONO";
    }
    void paint(juce::Graphics& g) override {
        if(themeSource)Theme::selectFor(*themeSource);
        const bool compact=getHeight()<48;
        auto area=getLocalBounds().toFloat();
        const auto readout=compact?area.removeFromBottom(12.f):juce::Rectangle<float>{};
        if (area.getWidth()<15.f || area.getHeight()<15.f) return;
        constexpr float pi=juce::MathConstants<float>::pi;
        const float centreX=area.getCentreX(),centreY=area.getCentreY();
        const float dial=std::min(area.getWidth(),area.getHeight())*.5f-(compact?3.f:7.f);
        const float rim=dial*.77f;
        const float body=dial*.67f;
        const double range=getMaximum()-getMinimum();
        const float value=range>0.
            ? (float)juce::jlimit(0.,1.,(getValue()-getMinimum())/range) : .5f;
        const float start=1.2f*pi,end=2.8f*pi;
        const float angle=start+(end-start)*value;
        // The centre tick is the audible detent: it reads brighter at neutral.
        const bool neutral=percentFor(getValue())==0;
        juce::Path track,fill;
        track.addCentredArc(centreX,centreY,dial*.92f,dial*.92f,0.f,start,end,true);
        // Bipolar fill: the lit arc always grows *out of* the centre detent in
        // the direction of travel, so neutral paints no arc at all rather than
        // a misleading half ring.
        const float centreAngle=start+(end-start)*.5f;
        if(!neutral) {
            if(angle>centreAngle)
                fill.addCentredArc(centreX,centreY,dial*.92f,dial*.92f,0.f,
                                   centreAngle,angle,true);
            else
                fill.addCentredArc(centreX,centreY,dial*.92f,dial*.92f,0.f,
                                   angle,centreAngle,true);
        }
        for (int i=0;i<=10;++i) {
            const float theta=start+(end-start)*(float)i/10.f;
            const float sn=std::sin(theta),cs=-std::cos(theta);
            const bool centreTick=i==5;
            const float inner=dial*(centreTick?.97f:(i==0||i==10?.99f:1.015f));
            const float outer=dial*(centreTick?1.20f:(i==0||i==10?1.145f:1.10f));
            g.setColour(centreTick?Theme::white.withAlpha(neutral?.85f:.42f)
                                  :Theme::muted.withAlpha(hovered?.48f:.29f));
            g.drawLine(centreX+sn*inner,centreY+cs*inner,
                       centreX+sn*outer,centreY+cs*outer,centreTick?1.5f:.9f);
        }
        g.setColour(Theme::muted.withAlpha(.35f));
        g.strokePath(track,juce::PathStrokeType(2.4f,
                     juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(Theme::green.withAlpha(hovered||adjusting?1.f:.86f));
        g.strokePath(fill,juce::PathStrokeType(3.f,
                     juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const auto bezel=juce::Rectangle<float>(centreX-rim,centreY-rim,2.f*rim,2.f*rim);
        g.setColour(Theme::bg.withAlpha(.68f));
        g.fillEllipse(bezel.translated(.7f,1.3f));
        g.setColour(Theme::stroke.darker(.18f));
        g.fillEllipse(bezel);
        g.setColour(Theme::white.withAlpha(.18f));
        g.drawEllipse(bezel.reduced(1.f),.9f);
        const auto cap=juce::Rectangle<float>(centreX-body,centreY-body,2.f*body,2.f*body);
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(.22f),cap.getTopLeft(),
            Theme::panelAlt.darker(.34f),cap.getBottomRight(),false));
        g.fillEllipse(cap);
        g.setColour(Theme::stroke.withAlpha(.75f));
        g.drawEllipse(cap.reduced(.6f),.9f);
        // Bone-white pointer, the same machined marker as the macro knobs.
        const float theta=angle;
        const float dx=std::sin(theta),dy=-std::cos(theta);
        g.setColour(Theme::white.withAlpha(.93f));
        g.drawLine(centreX+dx*body*.30f,centreY+dy*body*.30f,
                   centreX+dx*body*.80f,centreY+dy*body*.80f,2.1f);
        g.setColour(Theme::white.withAlpha(.16f));
        g.drawLine(centreX+dx*body*.30f,centreY+dy*body*.30f,
                   centreX+dx*body*.62f,centreY+dy*body*.62f,compact?2.4f:4.2f);
        if(compact) {
            const auto caption=neutral?juce::String("NEUTRAL"):
                juce::String(percentFor(getValue()))+(widening(getValue())?"% W":"% M");
            g.setColour(neutral?Theme::muted:Theme::green);
            g.setFont(Theme::font(8.8f,true));
            g.drawFittedText(caption,readout.toNearestInt(),juce::Justification::centred,1,.75f);
        }
        if(hasKeyboardFocus(true)) {
            g.setColour(Theme::green.withAlpha(.8f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f),4.f,1.f);
        }
    }
    void mouseEnter(const juce::MouseEvent& e) override {
        hovered=true;juce::Slider::mouseEnter(e);repaint();
    }
    void mouseExit(const juce::MouseEvent& e) override {
        hovered=false;juce::Slider::mouseExit(e);repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        adjusting=true;juce::Slider::mouseDown(e);repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override {
        juce::Slider::mouseUp(e);adjusting=false;repaint();
    }
    // Presentation only: which palette to paint with. Never affects values.
    void setThemeSource(const juce::AudioProcessorValueTreeState* state) {
        themeSource=state;
    }
private:
    const juce::AudioProcessorValueTreeState* themeSource=nullptr;
    bool hovered=false,adjusting=false;
};
