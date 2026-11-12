#pragma once
#include <JuceHeader.h>

// Paint-time palette selection is thread-local, so independent plugin editors
// do not race over a shared global colour scheme. Each component selects the
// theme persisted by its own processor before drawing.
namespace Theme {
constexpr int width=1168, height=688;
enum Variant { Obsidian=0, Cream=1, Crimson=2 };
inline thread_local int current = Obsidian;
inline thread_local juce::Colour bg       {0xff050607};
inline thread_local juce::Colour panel    {0xff0c1112};
inline thread_local juce::Colour panelAlt {0xff151c1d};
inline thread_local juce::Colour stroke   {0xff293c36};
inline thread_local juce::Colour green    {0xff39ff3a};
inline thread_local juce::Colour blue     {0xff77c7ae};
inline thread_local juce::Colour pink     {0xffff638d};
inline thread_local juce::Colour white    {0xffeff7ef};
inline thread_local juce::Colour muted    {0xff91a89a};
inline thread_local juce::Colour amber    {0xffffbc62};
inline thread_local juce::Colour red      {0xffff4a4a};
inline thread_local juce::Colour plot     {0xff070b0c};

inline int clampVariant(int v) noexcept { return v >= Obsidian && v <= Crimson ? v : Obsidian; }
inline void select(int v) {
    v=clampVariant(v);
    if (current==v) return;
    current=v;
    if (v==Cream) {
        bg=juce::Colour(0xffeee8dd); panel=juce::Colour(0xfffffaf0);
        panelAlt=juce::Colour(0xfff0e8db); stroke=juce::Colour(0xffcbc0ae);
        green=juce::Colour(0xff286c54); blue=juce::Colour(0xff557c75);
        pink=juce::Colour(0xffa34c66); white=juce::Colour(0xff292d29);
        muted=juce::Colour(0xff756f66); amber=juce::Colour(0xffa66a24);
        red=juce::Colour(0xffb83b43); plot=juce::Colour(0xfff7f1e8);
    } else if(v==Crimson) {
        bg=juce::Colour(0xff0e0a0c); panel=juce::Colour(0xff1b1317);
        panelAlt=juce::Colour(0xff291b22); stroke=juce::Colour(0xff55313d);
        green=juce::Colour(0xffff3a55); blue=juce::Colour(0xffe68e9e);
        pink=juce::Colour(0xffff6c97); white=juce::Colour(0xffffecf0);
        muted=juce::Colour(0xffbc9ba5); amber=juce::Colour(0xffefb46b);
        red=juce::Colour(0xffff4458); plot=juce::Colour(0xff130d10);
    } else {
        bg=juce::Colour(0xff050607); panel=juce::Colour(0xff0c1112);
        panelAlt=juce::Colour(0xff151c1d); stroke=juce::Colour(0xff293c36);
        green=juce::Colour(0xff39ff3a); blue=juce::Colour(0xff77c7ae);
        pink=juce::Colour(0xffff638d); white=juce::Colour(0xffeff7ef);
        muted=juce::Colour(0xff91a89a); amber=juce::Colour(0xffffbc62);
        red=juce::Colour(0xffff4a4a); plot=juce::Colour(0xff070b0c);
    }
}
inline void selectFor(const juce::AudioProcessorValueTreeState& state) {
    select(clampVariant((int)state.state.getProperty("uiTheme",0)));
}
inline void panelBox(juce::Graphics& g,juce::Rectangle<float> box,float radius=10.f,bool active=false) {
    g.setColour(panel);g.fillRoundedRectangle(box,radius);
    g.setColour(active?green.withAlpha(.72f):stroke);g.drawRoundedRectangle(box,radius,1.f);
}
inline juce::Font font(float size,bool bold=false) {
    return juce::Font(juce::FontOptions(size).withStyle(bold?"Bold":"Regular"));
}
inline void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> area,
                 float size=14,juce::Colour colour=white,bool bold=false,
                 juce::Justification justify=juce::Justification::centredLeft) {
    g.setColour(colour);g.setFont(font(size,bold));g.drawText(value,area,justify,true);
}
}
