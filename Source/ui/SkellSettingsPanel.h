#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <array>
#include <functional>

// Local, theme-painted modal surface, shared visually with PresetBrowser.
// Never invokes JUCE's unthemed native PopupMenu or opens a new window.
class SkellSettingsPanel final : public juce::Component {
public:
    std::function<void(int)> onSizeChange;
    std::function<void()> onClose;
    void select(int index) { selected=juce::jlimit(0,3,index);repaint(); }
    void open(int index) {
        select(index);
        setVisible(true);toFront(true);
    }
    void close() {setVisible(false);if(onClose)onClose();}
    void paint(juce::Graphics& g) override {
        Theme::panelBox(g,getLocalBounds().toFloat(),12.f);
        g.setColour(Theme::green);
        g.fillRoundedRectangle(juce::Rectangle<float>(18.f,17.f,4.f,24.f),2.f);
        Theme::text(g,"SKELL SETTINGS",{33,12,getWidth()-98,34},19,Theme::white,true);
        Theme::text(g,"X",{getWidth()-41,12,22,30},17,Theme::muted,true,
                    juce::Justification::centred);
        g.setColour(Theme::stroke);
        g.drawLine(20.f,53.f,(float)getWidth()-20.f,53.f,.9f);
        Theme::text(g,"INTERFACE SCALE",{25,65,getWidth()-50,23},12,Theme::green,true);
        Theme::text(g,"Choose a size. The layout keeps its aspect ratio.",
                    {25,88,getWidth()-50,20},10.8f,Theme::muted);
        const std::array<juce::String,4> labels{{"70%","85%","100%","125%"}};
        for(int i=0;i<4;++i){
            const int x=25+(i%2)*177, y=119+(i/2)*47;
            const auto r=juce::Rectangle<float>((float)x,(float)y,166.f,39.f);
            const bool active=i==selected;
            g.setColour(active?Theme::green.withAlpha(.21f):Theme::panelAlt);
            g.fillRoundedRectangle(r,7.f);
            g.setColour(active?Theme::green:Theme::stroke);
            g.drawRoundedRectangle(r.reduced(.5f),7.f,1.15f);
            Theme::text(g,labels[(size_t)i],{x+13,y+8,122,23},14,
                        active?Theme::green:Theme::white,true);
            if(active)Theme::text(g,"●",{x+138,y+8,16,23},12,Theme::green);
        }
        Theme::text(g,"MORE SETTINGS WILL APPEAR HERE",
                    {25,getHeight()-33,getWidth()-50,18},9.2f,Theme::muted);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.x>=getWidth()-47 && e.y<50) {close();return;}
        for(int i=0;i<4;++i) {
            const int x=25+(i%2)*177, y=119+(i/2)*47;
            if(juce::Rectangle<int>(x,y,166,39).contains(e.getPosition())){
                select(i);
                if(onSizeChange)onSizeChange(i+1);
                close();
                return;
            }
        }
    }
private:
    int selected=0;
};
