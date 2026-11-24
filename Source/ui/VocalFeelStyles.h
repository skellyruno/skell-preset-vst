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
    void close() {
        setVisible(false);
        setMouseCursor(juce::MouseCursor::NormalCursor);
        closeHovered=false;
    }
    // Share these precise rectangles between paint, pointer handling and
    // regression tests. The 2px inter-row gutters are deliberately inert.
    static int recipeHitForTest(int x,int y,int width) noexcept {
        if(x<9 || x>=width-9 || y<42 || y>=42+11*26)return -1;
        const int offset=y-42;
        if(offset%26>=24)return -1;
        return offset/26;
    }
    static bool closeHitForTest(int x,int y,int width) noexcept {
        return x>=width-44 && x<width-8 && y>=6 && y<35;
    }
    static bool testHitTargetsForTest() noexcept {
        for(int width:{228,270,400}){
            for(int i=0;i<11;++i){
                const int y=42+i*26;
                if(recipeHitForTest(20,y,width)!=i ||
                   recipeHitForTest(width-10,y+23,width)!=i ||
                   recipeHitForTest(20,y+24,width)!=-1 ||
                   recipeHitForTest(20,y+25,width)!=-1)
                    return false;
            }
            if(recipeHitForTest(20,40,width)!=-1 ||
               recipeHitForTest(20,328,width)!=-1 ||
               recipeHitForTest(5,48,width)!=-1 ||
               closeHitForTest(width-26,20,width)!=true ||
               closeHitForTest(25,20,width)!=false)
                return false;
        }
        return true;
    }
    int activeRecipe() const noexcept {return active;}
    void selectForTest(int index) {choose(index);}
    bool testLayoutForTest() const {
        if(getWidth()<228 || getHeight()<337)return false;
        const auto area=getLocalBounds();
        for(int i=0;i<11;++i) {
            const auto rect=juce::Rectangle<int>(9,42+i*26,getWidth()-18,24);
            if(!area.contains(rect))return false;
        }
        return testHitTargetsForTest();
    }
    void paint(juce::Graphics& g)override {
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        g.setColour(Theme::green);
        g.fillRoundedRectangle(juce::Rectangle<float>(12.f,12.f,3.f,18.f),1.5f);
        Theme::text(g,"VOCAL STYLES",{23,9,getWidth()-53,23},14.f,Theme::white,true);
        const float W=(float)getWidth();
        const auto closeRect=juce::Rectangle<float>(W-44.f,7.f,35.f,27.f);
        g.setColour(closeHovered?Theme::panelAlt.brighter(.20f):Theme::panelAlt);
        g.fillRoundedRectangle(closeRect,5.f);
        g.setColour(closeHovered?Theme::green:Theme::stroke);
        g.drawRoundedRectangle(closeRect.reduced(.5f),4.5f,1.f);
        g.setColour(closeHovered?Theme::white:Theme::muted);
        g.drawLine(W-33.f,15.f,W-20.f,27.f,1.3f);
        g.drawLine(W-20.f,15.f,W-33.f,27.f,1.3f);
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
            if(i==hover && hasKeyboardFocus(false)){
                g.setColour(Theme::white.withAlpha(.48f));
                g.drawRoundedRectangle(rect.reduced(1.25f),3.5f,1.f);
            } else if(i==hover){
                g.setColour(Theme::stroke.withAlpha(.65f));
                g.drawRoundedRectangle(rect.reduced(.5f),4.f,.85f);
            }
            Theme::fittedText(g,skellpreset::dsp::vocalFeelRecipes[(size_t)i].name,
                {19,y+3,getWidth()-36,19},11.7f,
                selected?Theme::green:Theme::white,selected);
        }
    }
    void mouseMove(const juce::MouseEvent& e)override {
        const bool nextClose=closeHitForTest(e.x,e.y,getWidth());
        const int next=recipeHitForTest(e.x,e.y,getWidth());
        if(next!=hoveredRow || nextClose!=closeHovered){
            hoveredRow=next;
            closeHovered=nextClose;
            if(next>=0)hover=next;
            repaint();
        }
        setMouseCursor(juce::MouseCursor(next>=0 || nextClose?
            juce::MouseCursor::PointingHandCursor:
            juce::MouseCursor::NormalCursor));
    }
    void mouseExit(const juce::MouseEvent&)override {
        if(closeHovered || hoveredRow!=-1){
            closeHovered=false;hoveredRow=-1;repaint();
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    void mouseDown(const juce::MouseEvent& e)override {
        if(closeHitForTest(e.x,e.y,getWidth())){close();return;}
        const int index=recipeHitForTest(e.x,e.y,getWidth());
        if(index>=0)choose(index);
    }
    bool keyPressed(const juce::KeyPress& key)override {
        if(key==juce::KeyPress::escapeKey){close();return true;}
        const int code=key.getKeyCode();
        if(code==juce::KeyPress::upKey || code==juce::KeyPress::downKey){
            hover=juce::jlimit(0,10,hover+
                (code==juce::KeyPress::upKey?-1:1));
            repaint();return true;
        }
        if(code==juce::KeyPress::homeKey){hover=0;repaint();return true;}
        if(code==juce::KeyPress::endKey){hover=10;repaint();return true;}
        if(code==juce::KeyPress::returnKey || code==juce::KeyPress::spaceKey){
            choose(hover);return true;
        }
        return false;
    }
private:
    void choose(int index) {
        if(index<0||index>=11)return;
        active=index;close();
        if(onChoose)onChoose(index);
    }
    int active=-1,hover=0,hoveredRow=-1;
    bool closeHovered=false;
};
