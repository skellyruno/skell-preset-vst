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
        const auto accent=colours_.accent;
        const bool active=b.getToggleState();
        const float emphasis=active?.13f:down?.085f:hovered?.055f:0.f;
        // Restrained depth instead of hard, rounded capsule buttons.
        g.setGradientFill(juce::ColourGradient(
            base.brighter(down?.045f:.11f),r.getTopLeft(),
            base.darker(.17f),r.getBottomRight(),false));
        g.fillRoundedRectangle(r,6.f);
        if(emphasis>.0001f){
            g.setColour(accent.withAlpha(emphasis));
            g.fillRoundedRectangle(r,6.f);
        }
        g.setColour((active||hovered?accent:colours_.secondary)
                        .withAlpha(active?.62f:hovered?.57f:.31f));
        g.drawRoundedRectangle(r.reduced(.45f),6.f,1.1f);
        g.setColour(colours_.foreground.withAlpha(.09f));
        g.drawLine(r.getX()+9.f,r.getY()+2.f,r.getRight()-9.f,r.getY()+2.f,.85f);
        g.setColour(colours_.background.withAlpha(.35f));
        g.drawLine(r.getX()+9.f,r.getBottom()-2.f,r.getRight()-9.f,
                   r.getBottom()-2.f,.7f);
    }
    // Balanced, labelled hardware-style rotary control. The pointer and arc use
    // the same angle and share an exact centre to avoid off-axis visual wobble.
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                          float position,float startAngle,float endAngle,juce::Slider& slider) override {
        if(width<=0||height<=0)return;
        position=std::isfinite(position)?juce::jlimit(0.f,1.f,position):0.f;
        const auto bounds=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height);
        const float cx=bounds.getCentreX(),cy=bounds.getCentreY();
        const float radius=juce::jmax(1.f,juce::jmin(bounds.getWidth(),bounds.getHeight())*.5f-4.5f);
        const float trackRadius=radius*.91f,bodyRadius=radius*.76f;
        const bool enabled=slider.isEnabled(),engaged=enabled && (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true));
        const auto accent=slider.findColour(juce::Slider::rotarySliderFillColourId);
        const float detail=enabled?1.f:.40f;
        const float angle=startAngle+(endAngle-startAngle)*position;
        juce::Path track,active;
        track.addCentredArc(cx,cy,trackRadius,trackRadius,0.f,startAngle,endAngle,true);
        active.addCentredArc(cx,cy,trackRadius,trackRadius,0.f,startAngle,angle,true);
        g.setColour(colours_.secondary.withAlpha(.26f*detail));
        g.strokePath(track,juce::PathStrokeType(2.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        if(radius>19.f){
            for(int tick=0;tick<=10;++tick){const float a=startAngle+(endAngle-startAngle)*tick/10.f;
                const float sn=std::sin(a),cs=-std::cos(a);const float inner=radius*.99f,outer=radius*(tick%5==0?1.045f:1.02f);
                g.setColour(colours_.foreground.withAlpha((tick%5==0?.34f:.17f)*detail));g.drawLine(cx+sn*inner,cy+cs*inner,cx+sn*outer,cy+cs*outer,.85f);}
        }
        if(position>.0001f){
            g.setColour(accent.withAlpha((engaged?.22f:.10f)*detail));g.strokePath(active,juce::PathStrokeType(5.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            g.setColour(accent.withAlpha((engaged?1.f:.84f)*detail));g.strokePath(active,juce::PathStrokeType(2.55f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        const auto knob=juce::Rectangle<float>(cx-bodyRadius,cy-bodyRadius,2.f*bodyRadius,2.f*bodyRadius);
        g.setColour(colours_.background.withAlpha(.90f));g.fillEllipse(knob.expanded(2.f).translated(0.f,1.f));
        auto metal=juce::ColourGradient(colours_.surface.interpolatedWith(colours_.foreground,.28f),knob.getTopLeft(),colours_.background,knob.getBottomRight(),false);
        metal.addColour(.43,colours_.surface.brighter(.06f));metal.addColour(.67,colours_.background.brighter(.08f));
        g.setGradientFill(metal);g.fillEllipse(knob);
        if(bodyRadius>17.f){
            for(int n=0;n<40;++n){const float a=juce::MathConstants<float>::twoPi*n/40.f;
                const float sn=std::sin(a),cs=-std::cos(a);g.setColour(colours_.foreground.withAlpha((n%2?.10f:.18f)*detail));
                g.drawLine(cx+sn*bodyRadius*.88f,cy+cs*bodyRadius*.88f,cx+sn*bodyRadius*.97f,cy+cs*bodyRadius*.97f,.7f);}
        }
        const auto cap=knob.reduced(juce::jmax(1.3f,bodyRadius*.13f));
        g.setGradientFill(juce::ColourGradient(colours_.surface.interpolatedWith(colours_.foreground,.19f),cap.getCentreX()-bodyRadius*.32f,cap.getY(),colours_.background.brighter(.08f),cap.getCentreX()+bodyRadius*.50f,cap.getBottom(),true));g.fillEllipse(cap);
        g.setColour(colours_.background.withAlpha(.65f));g.drawEllipse(cap,.85f);
        g.setColour(colours_.secondary.withAlpha(.35f));g.drawEllipse(knob,1.f);
        g.setColour(colours_.foreground.withAlpha(.23f));
        g.drawEllipse(knob.reduced(2.2f),.9f);
        juce::Path bevel;
        bevel.addCentredArc(cx,cy,bodyRadius*.89f,bodyRadius*.89f,0.f,4.65f,6.05f,true);
        g.setColour(colours_.foreground.withAlpha(.29f));
        g.strokePath(bevel,juce::PathStrokeType(1.1f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float sn=std::sin(angle),cs=-std::cos(angle);
        const float pointerRadius=bodyRadius*.73f;
        const auto px=cx+sn*pointerRadius,py=cy+cs*pointerRadius;
        const auto x0=cx+sn*bodyRadius*.34f,y0=cy+cs*bodyRadius*.34f;
        g.setColour(colours_.background.withAlpha(.9f));g.drawLine(x0+.55f,y0+.7f,px+.55f,py+.7f,juce::jmax(1.6f,bodyRadius*.085f));
        g.setColour(colours_.foreground.withAlpha(.90f*detail));
        g.drawLine(x0,y0,px,py,juce::jmax(1.15f,bodyRadius*.052f));
        if(engaged){g.setColour(accent.withAlpha(.48f));g.drawEllipse(knob.expanded(1.2f),.8f);}
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
