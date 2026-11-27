#pragma once
#include <JuceHeader.h>
#include "Theme.h"
// Transparent native hit target over the painted moon; keyboard-accessible.
class SkellMoonButton final : public juce::Button {
public:
    SkellMoonButton():juce::Button("Stage sound controls"){setWantsKeyboardFocus(true);}
    bool expanded=false;
    void setVisualHover(bool show){if(visualHover!=show){visualHover=show;repaint();}}
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        if(!hover&&!down&&!visualHover)return;
        const auto r=getLocalBounds().toFloat().reduced(1.f);
        const float x=r.getCentreX(),y=r.getBottom()-9.f,d=expanded?-1.f:1.f;
        juce::Path p;p.startNewSubPath(x-4.f,y-2.f*d);p.lineTo(x,y+2.f*d);p.lineTo(x+4.f,y-2.f*d);
        g.setColour(Theme::bg.withAlpha(.72f));
        g.strokePath(p,juce::PathStrokeType(3.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(hover||down?Theme::green:Theme::white.withAlpha(.65f));
        g.strokePath(p,juce::PathStrokeType(1.65f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
private:
    bool visualHover=false;
};
