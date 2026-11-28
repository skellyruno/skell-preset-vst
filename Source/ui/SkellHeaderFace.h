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
inline juce::Rectangle<float> rackDisc(juce::Graphics& g,juce::Rectangle<float> r,bool enabled,bool over,bool down) {
    const auto disc=r.withSizeKeepingCentre(25.f,25.f);
    g.setColour(Theme::bg.withAlpha(.62f));g.fillEllipse(disc.translated(0.f,.8f));
    g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(over?.28f:.21f),disc.getTopLeft(),Theme::panelAlt.darker(down?.42f:.32f),disc.getBottomRight(),false));g.fillEllipse(disc);
    g.setColour(Theme::white.withAlpha(enabled?.23f:.08f));g.drawEllipse(disc.reduced(.5f),.85f);
    g.setColour(Theme::bg.withAlpha(.72f));g.drawEllipse(disc.reduced(2.f),.9f);
    const auto centre=disc.getCentre();
    juce::Path bevel;bevel.addCentredArc(centre.x,centre.y,10.8f,10.8f,0.f,-1.25f,1.25f,true);
    g.setColour(Theme::white.withAlpha(enabled?.16f:.05f));g.strokePath(bevel,juce::PathStrokeType(.75f));
    return disc;
}
}
