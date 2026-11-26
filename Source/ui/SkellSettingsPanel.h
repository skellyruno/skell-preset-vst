#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <functional>
class SkellSettingsPanel final : public juce::Component {
public:
    std::function<void(int)> onSizeChange;
    std::function<void()> onClose;
    SkellSettingsPanel(){
        setWantsKeyboardFocus(true);
        resize.addItem("70%",1);resize.addItem("85%",2);resize.addItem("100%",3);resize.addItem("125%",4);
        resize.setName("Interface size");
        resize.onChange=[this]{if(onSizeChange)onSizeChange(resize.getSelectedId());close();};
        addAndMakeVisible(resize);
        applyTheme();
    }
    void applyTheme(){
        resize.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
        resize.setColour(juce::ComboBox::textColourId,Theme::white);
        resize.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        resize.setColour(juce::ComboBox::arrowColourId,Theme::green);repaint();
    }
    void open(int index){
        resize.setSelectedId(juce::jlimit(0,3,index)+1,juce::dontSendNotification);
        setVisible(true);toFront(true);if(isShowing())resize.grabKeyboardFocus();
    }
    void close(){setVisible(false);if(onClose)onClose();}
    void resized() override {resize.setBounds(12,42,getWidth()-24,34);}
    void paint(juce::Graphics& g) override{
        Theme::panelBox(g,getLocalBounds().toFloat(),8.f,true);
        Theme::fittedText(g,"RESIZE",{12,9,getWidth()-24,25},15.f,Theme::white,true);
    }
    bool keyPressed(const juce::KeyPress& key) override{
        if(key==juce::KeyPress::escapeKey){close();return true;}return false;
    }
    bool testLayoutForTest() const noexcept{return getWidth()>=200 && getHeight()>=86 && getLocalBounds().contains(resize.getBounds());}
    bool testKeyboardNavigationForTest() {
        const int old=resize.getSelectedId();
        resize.setSelectedId(3,juce::dontSendNotification);
        const bool ok=resize.getSelectedId()==3 && resize.getNumItems()==4;
        resize.setSelectedId(old,juce::dontSendNotification);return ok;
    }
private:
    juce::ComboBox resize;
};
