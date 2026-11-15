#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <functional>
#include <array>

// Mouse context overlay uses the same Skell skin as the preset browser.
// No native DAW/JUCE PopupMenu is invoked for Studio EQ band shaping.
class SkellEQNodeMenu final : public juce::Component {
public:
    std::function<void(int)> onFilterType;
    std::function<void()> onRemove;
    void open(int kind){
        selected=juce::jlimit(0,4,kind);
        setVisible(true);toFront(true);repaint();
    }
    void close(){setVisible(false);}
    void chooseTypeForTest(int kind){choose(kind);}
    void removeForTest(){if(onRemove)onRemove();close();}
    void paint(juce::Graphics& g)override{
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        Theme::text(g,"EQ NODE / FILTER TYPE",{13,10,getWidth()-26,24},
                    13,Theme::green,true);
        constexpr std::array<const char*,5> names{{
            "PEAK / BELL","HIGH PASS","LOW PASS","LOW SHELF","HIGH SHELF"}};
        for(int i=0;i<5;++i){
            const float y=43.f+i*30.f;
            const auto rect=juce::Rectangle<float>(10.f,y,getWidth()-20.f,27.f);
            g.setColour(i==selected?Theme::green.withAlpha(.22f):Theme::panelAlt);
            g.fillRoundedRectangle(rect,5.f);
            g.setColour(i==selected?Theme::green:Theme::stroke);
            g.drawRoundedRectangle(rect.reduced(.5f),5.f,1.f);
            Theme::text(g,names[(size_t)i],
                        {20,(int)y+5,getWidth()-40,17},11.5f,
                        i==selected?Theme::green:Theme::white,true);
        }
        g.setColour(Theme::stroke);
        g.drawLine(11.f,199.f,getWidth()-11.f,199.f,.8f);
        Theme::text(g,"REMOVE THIS NODE",{20,206,getWidth()-40,22},
                    11.5f,Theme::red,true);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        const int y=e.y;
        if(y>=43 && y<193){choose(juce::jlimit(0,4,(y-43)/30));return;}
        if(y>=200 && y<=232){if(onRemove)onRemove();close();return;}
        if(y<9)close();
    }
private:
    void choose(int kind){
        selected=juce::jlimit(0,4,kind);
        if(onFilterType)onFilterType(selected);
        close();
    }
    int selected=0;
};
