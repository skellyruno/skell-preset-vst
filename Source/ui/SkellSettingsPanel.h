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
    std::function<void()> onWorldLinkToggle;
    std::function<void()> onClose;

    SkellSettingsPanel() {
        setWantsKeyboardFocus(true);
        setTooltip("Arrow keys choose a control; Enter applies; Escape closes");
    }
    void select(int index) { selected=juce::jlimit(0,3,index);repaint(); }
    void open(int index) {
        select(index);
        focusedTarget=selected;
        setVisible(true);toFront(true);grabKeyboardFocus();
    }
    void close() {
        setVisible(false);
        if(auto* parent=getParentComponent())parent->grabKeyboardFocus();
        if(onClose)onClose();
    }

    // All keyboard targets are genuine existing actions: four scale cards
    // plus optional SkellWorld presence. Arrows only move the focus outline;
    // they do not resize the DAW window or mutate settings until activation.
    static int nextKeyboardTarget(int target,int code) noexcept {
        target=juce::jlimit(0,4,target);
        if(code==juce::KeyPress::leftKey)
            return target<4 && target%2==1?target-1:target;
        if(code==juce::KeyPress::rightKey)
            return target<4 && target%2==0?target+1:target;
        if(code==juce::KeyPress::upKey)
            return target==4?2:target>=2?target-2:target;
        if(code==juce::KeyPress::downKey)
            return target<=1?target+2:target<=3?4:target;
        return target;
    }
    bool testKeyboardNavigationForTest() const noexcept {
        return nextKeyboardTarget(0,juce::KeyPress::rightKey)==1 &&
               nextKeyboardTarget(1,juce::KeyPress::downKey)==3 &&
               nextKeyboardTarget(3,juce::KeyPress::leftKey)==2 &&
               nextKeyboardTarget(2,juce::KeyPress::downKey)==4 &&
               nextKeyboardTarget(4,juce::KeyPress::upKey)==2 &&
               nextKeyboardTarget(0,juce::KeyPress::leftKey)==0 &&
               nextKeyboardTarget(4,juce::KeyPress::rightKey)==4;
    }
    bool keyPressed(const juce::KeyPress& key) override {
        const int code=key.getKeyCode();
        if(code==juce::KeyPress::escapeKey){close();return true;}
        const int next=nextKeyboardTarget(focusedTarget,code);
        if(next!=focusedTarget){focusedTarget=next;repaint();return true;}
        if(code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey ||
           code==juce::KeyPress::upKey || code==juce::KeyPress::downKey)
            return true;
        if(code==juce::KeyPress::returnKey || code==' ') {
            if(focusedTarget==4){
                if(onWorldLinkToggle)onWorldLinkToggle();
            } else {
                select(focusedTarget);
                if(onSizeChange)onSizeChange(focusedTarget+1);
                close();
            }
            return true;
        }
        return false;
    }

    void setWorldLinkState(bool requested,bool localOnline,bool hubOnline) {
        const juce::String next=!requested
            ?"JOIN SKELLWORLD  |  PRESENCE ONLY"
            :hubOnline?"LEAVE SKELLWORLD  |  HUB CONNECTED"
            :localOnline?"LEAVE SKELLWORLD  |  LOCAL ONLY"
            :"LEAVE SKELLWORLD  |  CONNECTING";
        // The world-link status may be polled at 30 Hz. Do not repaint a
        // static popover thirty times a second when nothing has changed.
        if(next!=worldLinkLabel){worldLinkLabel=next;repaint();}
    }
    bool testLayoutForTest() const noexcept {
        const auto bounds=getLocalBounds();
        if(getWidth()<388 || getHeight()<326)return false;
        const auto close=juce::Rectangle<int>(getWidth()-47,10,37,40);
        const auto world=juce::Rectangle<int>(25,239,getWidth()-50,39);
        const auto legend=juce::Rectangle<int>(25,285,getWidth()-50,18);
        const auto keyboardHint=juce::Rectangle<int>(25,306,getWidth()-50,18);
        if(!bounds.contains(close) || !bounds.contains(world) ||
           !bounds.contains(legend) || !bounds.contains(keyboardHint) ||
           world.intersects(legend) || legend.intersects(keyboardHint))
            return false;
        for(int i=0;i<4;++i) {
            const auto a=juce::Rectangle<int>(25+(i%2)*177,119+(i/2)*47,166,39);
            if(!bounds.contains(a)||a.intersects(world))return false;
            for(int j=i+1;j<4;++j)
                if(a.intersects(juce::Rectangle<int>(
                    25+(j%2)*177,119+(j/2)*47,166,39)))
                    return false;
        }
        return true;
    }
    bool hasWorldLinkControlForTest() const noexcept {
        return worldLinkLabel.isNotEmpty() && (bool)onWorldLinkToggle;
    }

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
        static constexpr const char* labels[]{"70%","85%","100%","125%"};
        for(int i=0;i<4;++i){
            const int x=25+(i%2)*177, y=119+(i/2)*47;
            const auto r=juce::Rectangle<float>((float)x,(float)y,166.f,39.f);
            const bool active=i==selected;
            const bool focused=hasKeyboardFocus(false) && focusedTarget==i;
            g.setColour(active?Theme::green.withAlpha(.21f):Theme::panelAlt);
            g.fillRoundedRectangle(r,7.f);
            g.setColour(active?Theme::green:Theme::stroke);
            g.drawRoundedRectangle(r.reduced(.5f),7.f,1.15f);
            if(focused){
                g.setColour(Theme::white.withAlpha(.88f));
                g.drawRoundedRectangle(r.reduced(2.8f),5.f,1.2f);
            }
            Theme::text(g,labels[(size_t)i],{x+13,y+8,122,23},14,
                        active?Theme::green:Theme::white,true);
            if(active){g.setColour(Theme::green);g.fillEllipse((float)x+142.f,(float)y+16.f,7.f,7.f);}
        }
        Theme::text(g,"SKELLWORLD  /  OPTIONAL LOCAL PRESENCE",
                    {25,214,getWidth()-50,18},10,Theme::green,true);
        const auto linkButton=juce::Rectangle<float>(25.f,239.f,
                                                       (float)getWidth()-50.f,39.f);
        g.setColour(Theme::panelAlt);
        g.fillRoundedRectangle(linkButton,7.f);
        g.setColour(worldLinkLabel.startsWith("LEAVE")
                    ?Theme::green:Theme::stroke);
        g.drawRoundedRectangle(linkButton.reduced(.5f),7.f,1.15f);
        if(hasKeyboardFocus(false) && focusedTarget==4){
            g.setColour(Theme::white.withAlpha(.88f));
            g.drawRoundedRectangle(linkButton.reduced(2.8f),5.f,1.2f);
        }
        Theme::text(g,worldLinkLabel,
                    {34,247,getWidth()-68,22},11.2f,Theme::white,true,
                    juce::Justification::centred);
        Theme::text(g,"PRESENCE ONLY  ·  NO AUDIO, PRESETS OR CHAIN DATA",
                    {25,285,getWidth()-50,18},10.2f,Theme::muted);
        Theme::text(g,"ARROWS: MOVE   ·   ENTER: SELECT   ·   ESC: CLOSE",
                    {25,306,getWidth()-50,17},10.f,Theme::muted);
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
        if(juce::Rectangle<int>(25,239,getWidth()-50,39).contains(e.getPosition())) {
            if(onWorldLinkToggle)onWorldLinkToggle();
            return;
        }
    }
private:
    int selected=0,focusedTarget=0;
    juce::String worldLinkLabel{"JOIN SKELLWORLD  |  PRESENCE ONLY"};
};
