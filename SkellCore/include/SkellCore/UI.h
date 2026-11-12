#pragma once
// JUCE-dependent UI primitives. Include only from editor/UI code, never DSP.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdint>

namespace skellcore::ui {
struct Theme {
    juce::Colour background = juce::Colour::fromRGB(9, 14, 12);
    juce::Colour surface = juce::Colour::fromRGB(19, 28, 24);
    juce::Colour accent = juce::Colour::fromRGB(91, 242, 105);
    juce::Colour foreground = juce::Colour::fromRGB(230, 241, 228);
    juce::Colour secondary = juce::Colour::fromRGB(151, 173, 155);
    static Theme neon() { return {}; }
    static Theme ivory() {
        return { juce::Colour::fromRGB(235, 229, 215), juce::Colour::fromRGB(250, 246, 235),
                 juce::Colour::fromRGB(82, 111, 87), juce::Colour::fromRGB(39, 44, 39),
                 juce::Colour::fromRGB(97, 105, 95) };
    }
    static Theme obsidian() {
        return { juce::Colour::fromRGB(12, 12, 14), juce::Colour::fromRGB(29, 29, 33),
                 juce::Colour::fromRGB(201, 213, 217), juce::Colour::fromRGB(235, 237, 240),
                 juce::Colour::fromRGB(151, 156, 166) };
    }
};

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    explicit LookAndFeel(Theme theme = Theme::neon()) : colours_(theme) {
        setColour(juce::Label::textColourId, colours_.foreground);
        setColour(juce::Slider::thumbColourId, colours_.accent);
        setColour(juce::Slider::rotarySliderFillColourId, colours_.accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, colours_.surface);
        setColour(juce::TextButton::buttonColourId, colours_.surface);
        setColour(juce::TextButton::textColourOffId, colours_.foreground);
        setColour(juce::PopupMenu::backgroundColourId, colours_.surface);
        setColour(juce::PopupMenu::textColourId, colours_.foreground);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, colours_.accent.withAlpha(.20f));
        setColour(juce::PopupMenu::highlightedTextColourId, colours_.foreground);
        setColour(juce::ComboBox::outlineColourId, colours_.secondary.withAlpha(.4f));
    }
    Theme palette() const noexcept { return colours_; }
    // Rounded rectangles, not capsule buttons: readable on dark, cream and crimson skins.
    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                              const juce::Colour& backgroundColour,
                              bool hovered, bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(.75f);
        if(r.isEmpty())return;
        const auto base=backgroundColour.isTransparent()?colours_.surface:backgroundColour;
        const auto glow=colours_.accent.withAlpha(b.getToggleState()?.16f:(down?.12f:hovered?.065f:0.f));
        g.setColour(base);g.fillRoundedRectangle(r,7.f);
        if(glow.getFloatAlpha()>0.f){g.setColour(glow);g.fillRoundedRectangle(r,7.f);}
        g.setColour((hovered||b.getToggleState()?colours_.accent:colours_.secondary)
                        .withAlpha(hovered?.67f:.38f));
        g.drawRoundedRectangle(r.reduced(.35f),7.f,1.f);
        g.setColour(colours_.foreground.withAlpha(.07f));
        g.drawLine(r.getX()+9.f,r.getY()+2.f,r.getRight()-9.f,r.getY()+2.f,.9f);
    }
    // Balanced, labelled hardware-style rotary control. The pointer and arc use
    // the same angle and share an exact centre to avoid off-axis visual wobble.
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                          float position,float startAngle,float endAngle,juce::Slider&) override {
        if(width<=0||height<=0)return;
        position=std::isfinite(position)?juce::jlimit(0.f,1.f,position):0.f;
        const auto bounds=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height);
        const float cx=bounds.getCentreX(),cy=bounds.getCentreY();
        const float radius=juce::jmax(1.f,juce::jmin(bounds.getWidth(),bounds.getHeight())*.5f-4.5f);
        const float trackRadius=radius*.91f,bodyRadius=radius*.72f;
        const float angle=startAngle+(endAngle-startAngle)*position;
        juce::Path track,active;
        track.addCentredArc(cx,cy,trackRadius,trackRadius,0.f,startAngle,endAngle,true);
        active.addCentredArc(cx,cy,trackRadius,trackRadius,0.f,startAngle,angle,true);
        g.setColour(colours_.secondary.withAlpha(.30f));
        g.strokePath(track,juce::PathStrokeType(2.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(colours_.accent);
        g.strokePath(active,juce::PathStrokeType(3.25f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const auto knob=juce::Rectangle<float>(cx-bodyRadius,cy-bodyRadius,2.f*bodyRadius,2.f*bodyRadius);
        g.setColour(colours_.background.withAlpha(.65f));g.fillEllipse(knob.expanded(1.f));
        g.setGradientFill(juce::ColourGradient(colours_.surface.brighter(.50f),
                         cx-bodyRadius*.48f,cy-bodyRadius*.75f,
                         colours_.background.darker(.24f),cx+bodyRadius,cy+bodyRadius,true));
        g.fillEllipse(knob);
        g.setColour(colours_.secondary.withAlpha(.35f));g.drawEllipse(knob,1.f);
        g.setColour(colours_.foreground.withAlpha(.12f));
        g.drawEllipse(knob.reduced(3.f),.85f);
        const float pointerRadius=bodyRadius*.57f;
        const auto px=cx+std::sin(angle)*pointerRadius,py=cy-std::cos(angle)*pointerRadius;
        g.setColour(colours_.accent);
        g.drawLine(cx+std::sin(angle)*bodyRadius*.18f,cy-std::cos(angle)*bodyRadius*.18f,
                   px,py,juce::jmax(1.7f,bodyRadius*.15f));
        g.fillEllipse(px-1.5f,py-1.5f,3.f,3.f);
    }
private:
    Theme colours_;
};

// Render cache on the message/UI thread when size, colour or scale changes.
// paint() only draws a previously prepared image; never regenerate on audio thread.
class GlowCache {
public:
    // Cap bitmap memory at ~6 MiB. Large editors get no glow instead of OOM.
    static constexpr std::uint64_t maxPixels = 1572864;
    void rebuild(int width,int height,juce::Colour colour,float radius=8.f) {
        if(!std::isfinite(radius)) radius=8.f;
        radius=juce::jlimit(0.f,32.f,radius);
        const bool invalid=width<=0 || height<=0 || width>2048 || height>2048 ||
            static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)>maxPixels;
        if(invalid){image_={};cacheWidth_=cacheHeight_=0;return;}
        if(image_.isValid() && cacheWidth_==width && cacheHeight_==height &&
           cacheColour_==colour && cacheRadius_==radius) return;
        image_=juce::Image(juce::Image::ARGB,width,height,true);
        cacheWidth_=width;cacheHeight_=height;cacheColour_=colour;cacheRadius_=radius;
        juce::Graphics g(image_);
        const auto b=juce::Rectangle<float>(0,0,(float)width,(float)height).reduced(10);
        for(int i=7;i>=1;--i){
            g.setColour(colour.withAlpha(.018f*(8-i)));
            g.drawRoundedRectangle(b.expanded((float)i),radius+i,1.5f);
        }
    }
    void paint(juce::Graphics& g, int x, int y) const {
        if (image_.isValid()) g.drawImageAt(image_,x,y);
    }
    bool valid() const noexcept { return image_.isValid(); }
private:
    juce::Image image_;
    int cacheWidth_=0,cacheHeight_=0;
    float cacheRadius_=8.f;
    juce::Colour cacheColour_;
};
} // namespace skellcore::ui
