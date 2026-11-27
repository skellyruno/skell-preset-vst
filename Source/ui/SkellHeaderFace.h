#pragma once
#include <JuceHeader.h>
#include "Theme.h"
namespace skellpreset::header {
inline juce::Rectangle<float> face(juce::Graphics& g,juce::Rectangle<float> bounds,bool enabled,bool over,bool down,bool active,bool focus) {
    auto r=bounds.reduced(.7f);
    const auto base=Theme::panelAlt;
    g.setGradientFill(juce::ColourGradient(base.brighter(down?.02f:over?.18f:.09f),r.getTopLeft(),base.darker(down?.22f:.16f),r.getBottomLeft(),false));g.fillRoundedRectangle(r,5.5f);
    g.setColour((active||focus?Theme::green:Theme::stroke).withAlpha(enabled?(active?.75f:over?.95f:.70f):.35f));g.drawRoundedRectangle(r.reduced(.5f),5.5f,1.f);
    g.setColour(Theme::white.withAlpha(enabled?.10f:.04f));g.drawLine(r.getX()+5.f,r.getY()+2.f,r.getRight()-5.f,r.getY()+2.f,.8f);
    return r;
}
}
