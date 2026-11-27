#pragma once
#include <JuceHeader.h>

// Paint-time palette selection is thread-local, so independent plugin editors
// do not race over a shared global colour scheme. Each component selects the
// theme persisted by its own processor before drawing.
namespace Theme {
constexpr int width=1168, height=688;
// Keep saved numeric IDs stable. Cream=1 remains a legacy read value,
 // but is no longer an offered theme. Existing projects render in Obsidian.
enum Variant { Obsidian=0, Cream=1, Crimson=2 };
inline thread_local int current = Obsidian;
inline thread_local juce::Colour bg       {0xff000000};
inline thread_local juce::Colour panel    {0xff0a0b0b};
inline thread_local juce::Colour panelAlt {0xff17191a};
inline thread_local juce::Colour stroke   {0xff353c39};
inline thread_local juce::Colour green    {0xff39ff55};
inline thread_local juce::Colour blue     {0xff9ac9c6};
inline thread_local juce::Colour pink     {0xffe08eaa};
inline thread_local juce::Colour white    {0xffeff5f2};
inline thread_local juce::Colour muted    {0xffaeb8b4};
inline thread_local juce::Colour amber    {0xffe9b980};
inline thread_local juce::Colour red      {0xfff67878};
inline thread_local juce::Colour plot     {0xff050606};

inline int clampVariant(int v) noexcept {
    return v==Crimson ? Crimson : Obsidian; // legacy Cream=1 -> Obsidian
}
inline int nextVariant(int currentVariant) noexcept {
    return clampVariant(currentVariant)==Obsidian ? Crimson : Obsidian;
}
inline void select(int v) {
    v=clampVariant(v);
    if (current==v) return;
    current=v;
    if(v==Crimson) {
        bg=juce::Colour(0xff100e12); panel=juce::Colour(0xff19151a);
        panelAlt=juce::Colour(0xff292027); stroke=juce::Colour(0xff55424b);
        green=juce::Colour(0xfff56b87); blue=juce::Colour(0xffe0a8bd);
        pink=juce::Colour(0xfff59ab1); white=juce::Colour(0xfff7eff3);
        muted=juce::Colour(0xffc4aab6); amber=juce::Colour(0xffe8b983);
        red=juce::Colour(0xffff647b); plot=juce::Colour(0xff110d11);
    } else {
        bg=juce::Colour(0xff000000); panel=juce::Colour(0xff0a0b0b);
        panelAlt=juce::Colour(0xff17191a); stroke=juce::Colour(0xff353c39);
        green=juce::Colour(0xff39ff55); blue=juce::Colour(0xff9ac9c6);
        pink=juce::Colour(0xffe08eaa); white=juce::Colour(0xffeff5f2);
        muted=juce::Colour(0xffaeb8b4); amber=juce::Colour(0xffe9b980);
        red=juce::Colour(0xfff67878); plot=juce::Colour(0xff050606);
    }
}
inline void selectFor(const juce::AudioProcessorValueTreeState& state) {
    select(clampVariant((int)state.state.getProperty("uiTheme",0)));
}
inline void panelBox(juce::Graphics& g,juce::Rectangle<float> box,float radius=10.f,bool active=false) {
    // Restrained anodized-metal depth shared across the rack, native editors
    // and floating menus. No drop-shadow layers, image allocations or
    // animation timers. This executes only inside UI paint(), never DSP.
    // Satin-smoked metal: restrained borders and two specular lines
    // instead of repeated neon outlines. Message-thread paint only.
    g.setGradientFill(juce::ColourGradient(
        panelAlt.withMultipliedBrightness(1.045f),box.getTopLeft(),
        panel.darker(.12f),box.getBottomRight(),false));
    g.fillRoundedRectangle(box,radius);
    g.setColour(bg.withAlpha(.78f));
    g.drawRoundedRectangle(box.reduced(.25f),radius,1.f);
    g.setColour(active?green.withAlpha(.67f):stroke.withAlpha(.57f));
    g.drawRoundedRectangle(box.reduced(.9f),radius,1.f);
    if(box.getWidth()>35.f && box.getHeight()>22.f) {
        const float inset=std::min(17.f,radius+6.f);
        const float x0=box.getX()+inset,x1=box.getRight()-inset;
        g.setColour(white.withAlpha(.085f));
        g.drawLine(x0,box.getY()+1.6f,x1,box.getY()+1.6f,.65f);
        g.setColour(bg.withAlpha(.49f));
        g.drawLine(x0,box.getBottom()-1.4f,x1,box.getBottom()-1.4f,.8f);
        if(active){
            g.setColour(green.withAlpha(.31f));
            g.drawLine(x0,box.getY()+2.8f,x1,box.getY()+2.8f,.65f);
        }
    }
}
// A little more definition on the two main workspace frames, not on
// every small knob or control. Keep the neutral hardware finish.
inline void workspaceFrame(juce::Graphics& g,juce::Rectangle<float> box,float radius=9.f) {
    g.setColour(stroke.brighter(.18f).withAlpha(.88f));
    g.drawRoundedRectangle(box.reduced(1.0f),radius,1.55f);
    g.setColour(white.withAlpha(.065f));
    g.drawLine(box.getX()+radius+3.f,box.getY()+2.5f,box.getRight()-radius-3.f,box.getY()+2.5f,.65f);
}
inline juce::Font font(float size,bool bold=false) {
    return juce::Font(juce::FontOptions(size).withStyle(bold?"Bold":"Regular"));
}
inline void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> area,
                 float size=14,juce::Colour colour=white,bool bold=false,
                 juce::Justification justify=juce::Justification::centredLeft) {
    g.setColour(colour);g.setFont(font(size,bold));g.drawText(value,area,justify,true);
}
inline void fittedText(juce::Graphics& g,const juce::String& value,
                       juce::Rectangle<int> area,float size,
                       juce::Colour colour=white,bool bold=false,
                       juce::Justification justify=juce::Justification::centredLeft) {
    // Effect names, popup headings and long knobs must never clip at 70% UI.
    // Leave font size intact whenever text fits; scale only as a last resort.
    g.setColour(colour);g.setFont(font(size,bold));
    g.drawFittedText(value,area,justify,1,.74f);
}
}
