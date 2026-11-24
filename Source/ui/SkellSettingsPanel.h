#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <functional>
#include <array>

// Compact settings. Native JUCE ComboBox uses the parent SkellCore skin.
class SkellSettingsPanel final : public juce::Component {
public:
    std::function<void(int)> onSizeChange;
    std::function<void()> onRackToggle,onEmptyRack,onClose;
    SkellSettingsPanel(){
        setWantsKeyboardFocus(true);
        scale.addItemList({"Compact (70%)","Medium (85%)","Full (100%)","Large (125%)"},1);
        scale.onChange=[this]{
            if(onSizeChange && scale.getSelectedId()>0)onSizeChange(scale.getSelectedId());
        };
        rack.onClick=[this]{if(onRackToggle)onRackToggle();};
        empty.onClick=[this]{if(onEmptyRack)onEmptyRack();};
        closeButton.onClick=[this]{close();};
        for(auto* b:{&rack,&empty,&closeButton})addAndMakeVisible(*b);
        addAndMakeVisible(scale);
        empty.setButtonText("LOAD EMPTY RACK");
        empty.setTooltip("Reset to unity input/output, 100% mix, no effects. Undo restores the previous native preset.");
        rack.setTooltip("Move the effect rack to the other side");
        closeButton.setTooltip("Close settings");
        setRackRight(false);
    }
    void setRackRight(bool right){rack.setButtonText(right?"RACK: RIGHT":"RACK: LEFT");}
    void open(int index){
        scale.setSelectedId(juce::jlimit(0,3,index)+1,juce::dontSendNotification);
        scale.setColour(juce::ComboBox::backgroundColourId,Theme::plot);
        scale.setColour(juce::ComboBox::textColourId,Theme::white);
        scale.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        scale.setColour(juce::ComboBox::arrowColourId,Theme::green);
        for(auto* b:{&rack,&empty,&closeButton}){
            b->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
            b->setColour(juce::TextButton::textColourOffId,Theme::white);
        }
        empty.setColour(juce::TextButton::textColourOffId,Theme::green);
        setVisible(true);toFront(true);
        if(isShowing())scale.grabKeyboardFocus();
        repaint();
    }
    void close(){
        setVisible(false);
        if(onClose)onClose();
    }
    void resized() override{
        closeButton.setBounds(getWidth()-39,8,28,27);
        scale.setBounds(14,69,getWidth()-28,33);
        rack.setBounds(14,114,getWidth()-28,29);
        empty.setBounds(14,151,getWidth()-28,32);
    }
    void paint(juce::Graphics& g)override{
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        Theme::fittedText(g,"SETTINGS",{15,9,getWidth()-65,28},17,Theme::white,true);
        Theme::fittedText(g,"INTERFACE SIZE",{15,47,getWidth()-30,18},11,Theme::muted,true);
        g.setColour(Theme::stroke);
        g.drawHorizontalLine(40,14.f,(float)getWidth()-14.f);
    }
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::escapeKey){close();return true;}
        return false;
    }
    bool testKeyboardNavigationForTest() const noexcept{
        return scale.getNumItems()==4 && scale.isEnabled() &&
            rack.isEnabled() && empty.isEnabled();
    }
    bool testLayoutForTest() const noexcept{
        const auto area=getLocalBounds();
        const std::array<const juce::Component*,4> controls{{&scale,&rack,&empty,&closeButton}};
        for(size_t i=0;i<controls.size();++i){
            if(!area.contains(controls[i]->getBounds()))return false;
            for(size_t j=i+1;j<controls.size();++j)
                if(controls[i]->getBounds().intersects(controls[j]->getBounds()))return false;
        }
        return getWidth()>=300 && getHeight()>=192;
    }
private:
    juce::ComboBox scale;
    juce::TextButton rack,empty,closeButton{"X"};
};
