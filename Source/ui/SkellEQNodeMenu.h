#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <functional>
#include <array>

// Shared geometry for real EQ shape and remove actions.
class SkellEQNodeMenu final : public juce::Component {
public:
    SkellEQNodeMenu(){setWantsKeyboardFocus(true);}
    std::function<void(int)> onFilterType;
    std::function<void()> onRemove;
    void open(int kind){
        selected=juce::jlimit(0,4,kind);focused=selected;hovered=-1;
        setVisible(true);toFront(true);
        if(isShowing())grabKeyboardFocus();
        repaint();
    }
    void close(){
        hovered=-1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        setVisible(false);
    }
    void chooseTypeForTest(int kind){choose(kind);}
    void removeForTest(){if(onRemove)onRemove();close();}
    static juce::Rectangle<int> actionBounds(int width,int index) noexcept {
        return index==5?juce::Rectangle<int>(10,204,width-20,26):
            juce::Rectangle<int>(10,43+index*30,width-20,27);
    }
    static int hitAction(int x,int y,int width) noexcept {
        for(int i=0;i<6;++i)
            if(actionBounds(width,i).contains(x,y))return i;
        return -1;
    }
    bool testLayoutForTest() const noexcept {
        if(getWidth()<195 || getHeight()<233)return false;
        const auto bounds=getLocalBounds();
        for(int w:{195,205,260}){
            for(int i=0;i<6;++i){
                const auto row=actionBounds(w,i);
                if(hitAction(row.getCentreX(),row.getCentreY(),w)!=i ||
                   hitAction(row.getRight(),row.getCentreY(),w)!=-1 ||
                   hitAction(row.getCentreX(),row.getBottom(),w)!=-1)return false;
            }
        }
        for(int i=0;i<6;++i){
            const auto row=actionBounds(getWidth(),i);
            if(!bounds.contains(row))return false;
            for(int j=i+1;j<6;++j)
                if(row.intersects(actionBounds(getWidth(),j)))return false;
        }
        return true;
    }
    void paint(juce::Graphics& g)override{
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        Theme::fittedText(g,"EQ NODE / FILTER TYPE",{13,10,getWidth()-26,24},
                    13,Theme::green,true);
        constexpr std::array<const char*,6> names{{
            "PEAK / BELL","HIGH PASS","LOW PASS","LOW SHELF","HIGH SHELF","REMOVE THIS NODE"}};
        for(int i=0;i<6;++i){
            const auto rect=actionBounds(getWidth(),i).toFloat();
            const bool active=i==selected,hover=i==hovered;
            const auto ink=i==5?Theme::red:Theme::green;
            g.setColour(active?ink.withAlpha(.19f):
                hover?Theme::panelAlt.brighter(.18f):Theme::panelAlt);
            g.fillRoundedRectangle(rect,5.f);
            g.setColour(active||hover?ink:Theme::stroke);
            g.drawRoundedRectangle(rect.reduced(.5f),5.f,1.f);
            if(hasKeyboardFocus(false)&&i==focused){
                g.setColour(Theme::white.withAlpha(.65f));
                g.drawRoundedRectangle(rect.reduced(2.f),4.f,.9f);
            }
            Theme::fittedText(g,names[(size_t)i],
                actionBounds(getWidth(),i).reduced(10,3),11.5f,
                active||i==5?ink:Theme::white,true);
        }
        g.setColour(Theme::stroke);
        g.drawLine(11.f,199.f,getWidth()-11.f,199.f,.8f);
    }
    void mouseMove(const juce::MouseEvent& e)override{
        const int next=hitAction(e.x,e.y,getWidth());
        if(next!=hovered){hovered=next;if(next>=0)focused=next;repaint();}
        setMouseCursor(juce::MouseCursor(next>=0?
            juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor));
    }
    void mouseExit(const juce::MouseEvent&)override{
        hovered=-1;setMouseCursor(juce::MouseCursor::NormalCursor);repaint();
    }
    void mouseDown(const juce::MouseEvent& e)override{
        if(!e.mods.isLeftButtonDown())return;
        activate(hitAction(e.x,e.y,getWidth()));
    }
    bool keyPressed(const juce::KeyPress& key)override{
        const int code=key.getKeyCode();
        if(code==juce::KeyPress::escapeKey){close();return true;}
        if(code==juce::KeyPress::upKey||code==juce::KeyPress::downKey){
            focused=juce::jlimit(0,5,focused+(code==juce::KeyPress::upKey?-1:1));
            repaint();return true;
        }
        if(code==juce::KeyPress::homeKey){focused=0;repaint();return true;}
        if(code==juce::KeyPress::endKey){focused=5;repaint();return true;}
        if(code==juce::KeyPress::returnKey||code==juce::KeyPress::spaceKey){
            activate(focused);return true;
        }
        return false;
    }
private:
    void activate(int index){
        if(index==5)removeForTest();
        else if(index>=0&&index<5)choose(index);
    }
    void choose(int kind){
        selected=juce::jlimit(0,4,kind);
        if(onFilterType)onFilterType(selected);
        close();
    }
    int selected=0,focused=0,hovered=-1;
};
