#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../chain/VocalFeelRecipes.h"
#include <functional>

// Native, keyboard-capable studio popover. Not an OS PopupMenu or mockup.
// It preserves the user's preferred dark studio palette across all themes.
class SkellVocalFeelStyles final:public juce::Component {
public:
    SkellVocalFeelStyles() {
        setVisible(false);
        setWantsKeyboardFocus(true);
    }
    std::function<void(int)> onChoose;
    void open(int selected=-1) {
        active=juce::jlimit(-1,10,selected);
        hover=active<0?0:active;
        setVisible(true);toFront(true);
        grabKeyboardFocus();repaint();
    }
    void close() {setVisible(false);}
    int activeRecipe() const noexcept {return active;}
    void selectForTest(int index) {choose(index);}
    bool testLayoutForTest() const {
        if(getWidth()<228 || getHeight()<337)return false;
        const auto area=getLocalBounds();
        for(int i=0;i<11;++i) {
            const auto rect=juce::Rectangle<int>(9,42+i*26,getWidth()-18,24);
            if(!area.contains(rect))return false;
        }
        return true;
    }
    void paint(juce::Graphics& g)override {
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        g.setColour(Theme::green);
        g.fillRoundedRectangle(juce::Rectangle<float>(12.f,12.f,3.f,18.f),1.5f);
        Theme::text(g,"VOCAL STYLES",{23,9,getWidth()-53,23},14.f,Theme::white,true);
        Theme::text(g,"ESC",{getWidth()-43,12,32,20},9.3f,Theme::muted,true,
                    juce::Justification::centredRight);
        g.setColour(Theme::stroke);
        g.drawLine(11.f,37.f,(float)getWidth()-11.f,37.f,.8f);
        for(int i=0;i<11;++i) {
            const int y=42+i*26;
            const auto rect=juce::Rectangle<float>(9.f,(float)y,
                                                    (float)getWidth()-18.f,24.f);
            const bool selected=i==active;
            g.setColour(selected?Theme::green.withAlpha(.19f):
                        i==hover?Theme::panelAlt.brighter(.10f):Theme::panelAlt);
            g.fillRoundedRectangle(rect,4.5f);
            if(selected){
                g.setColour(Theme::green);
                g.fillRoundedRectangle(juce::Rectangle<float>(10.f,(float)y+4.f,2.6f,16.f),1.2f);
            }
            Theme::fittedText(g,skellpreset::dsp::vocalFeelRecipes[(size_t)i].name,
                {19,y+3,getWidth()-36,19},11.7f,
                selected?Theme::green:Theme::white,selected);
        }
    }
    void mouseMove(const juce::MouseEvent& e)override {
        const int index=e.y>=42 ?(e.y-42)/26:-1;
        if(index>=0&&index<11&&index!=hover){hover=index;repaint();}
    }
    void mouseDown(const juce::MouseEvent& e)override {
        if(e.y<39){close();return;}
        const int index=(e.y-42)/26;
        if(index>=0&&index<11)choose(index);
    }
    bool keyPressed(const juce::KeyPress& key)override {
        if(key==juce::KeyPress::escapeKey){close();return true;}
        if(key==juce::KeyPress::upKey) {
            hover=juce::jlimit(0,10,hover-1);repaint();return true;
        }
        if(key==juce::KeyPress::downKey) {
            hover=juce::jlimit(0,10,hover+1);repaint();return true;
        }
        if(key==juce::KeyPress::returnKey){choose(hover);return true;}
        return false;
    }
private:
    void choose(int index) {
        if(index<0||index>=11)return;
        active=index;close();
        if(onChoose)onChoose(index);
    }
    int active=-1,hover=0;
};
