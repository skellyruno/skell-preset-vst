#pragma once
#include <JuceHeader.h>
#include "Theme.h"
// Transparent native hit target over the painted moon; keyboard-accessible.
class SkellMoonButton final : public juce::Button {
public:
    SkellMoonButton():juce::Button("Stage sound controls"){setWantsKeyboardFocus(true);}
    bool expanded=false;
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        auto r=getLocalBounds().toFloat().reduced(1.f);
        if(hover || hasKeyboardFocus(true)) {
            g.setColour(Theme::green.withAlpha(down?.9f:.55f));g.drawEllipse(r,1.6f);
        }
        const float x=r.getCentreX(),y=r.getBottom()-9.f,d=expanded?-1.f:1.f;
        juce::Path p;p.startNewSubPath(x-4.f,y-2.f*d);p.lineTo(x,y+2.f*d);p.lineTo(x+4.f,y-2.f*d);
        g.setColour(Theme::bg.withAlpha(.86f));g.fillEllipse(x-8.f,y-8.f,16.f,16.f);
        g.setColour(Theme::white.withAlpha(.9f));g.strokePath(p,juce::PathStrokeType(1.5f));
    }
};
