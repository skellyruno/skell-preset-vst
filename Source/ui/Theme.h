#pragma once
#include <JuceHeader.h>
namespace Theme {
constexpr int width=1168,height=688;
const juce::Colour bg=juce::Colour(0xff0a1220);
const juce::Colour panel=juce::Colour(0xff101b2e);
const juce::Colour panelAlt=juce::Colour(0xff17263a);
const juce::Colour stroke=juce::Colour(0xff1d2e48);
const juce::Colour green=juce::Colour(0xff39ff3a);
const juce::Colour blue=juce::Colour(0xff5aa0d8);
const juce::Colour pink=juce::Colour(0xffff2fb3);
const juce::Colour white=juce::Colour(0xffe6ecf5);
const juce::Colour muted=juce::Colour(0xff8aa0bd);
const juce::Colour amber=juce::Colour(0xffffb02e);
const juce::Colour red=juce::Colour(0xffff4a4a);
inline void panelBox(juce::Graphics&g,juce::Rectangle<float> box,float radius=10.f,bool active=false) {
    g.setColour(panel);g.fillRoundedRectangle(box,radius);
    g.setColour(active?green.withAlpha(.72f):stroke);g.drawRoundedRectangle(box,radius,1.f);
}
inline juce::Font font(float size,bool bold=false) {
    return juce::Font(juce::FontOptions(size).withStyle(bold?"Bold":"Regular"));
}
inline void text(juce::Graphics&g,const juce::String& value,juce::Rectangle<int> area,
                 float size=14,juce::Colour colour=white,bool bold=false,
                 juce::Justification justify=juce::Justification::centredLeft) {
    g.setColour(colour);g.setFont(font(size,bold));g.drawText(value,area,justify,true);
}
}
