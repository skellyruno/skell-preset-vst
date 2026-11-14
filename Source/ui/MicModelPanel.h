#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"
#include <array>
#include <cmath>
#include <memory>

// Dedicated front-end: not a rack insert, not a separate audio graph.
// All five parameters are new, append-only APVTS IDs, so FL Studio can
// automate, save and recall without touching legacy insert parameters.
class SkellMicModelPanel final : public juce::Component {
public:
    explicit SkellMicModelPanel(SkellPresetAudioProcessor& p) : processor(p) {
        setWantsKeyboardFocus(true);
        close.setButtonText("X");
        close.onClick=[this]{setVisible(false);if(onClose)onClose();};
        addAndMakeVisible(close);
        enabled.setButtonText("OFF");
        enabled.onClick=[this] {
            const bool next=!on();
            setPhysical("micPreOn",next?1.f:0.f);
            refresh();
        };
        addAndMakeVisible(enabled);
        input.addItemList({"Neutral","Condenser","Dynamic","Ribbon"},1);
        input.onChange=[this] {
            if(input.getSelectedId()>0)
                setPhysical("micPreInput",(float)(input.getSelectedId()-1));
            refresh();
        };
        addAndMakeVisible(input);
        constexpr const char* targets[]{"Flat","Warm","Focus","Air"};
        constexpr const char* subtitles[]{"Uncolored","Soft low-mids",
                                         "Mid presence","High-frequency air"};
        for(int i=0;i<4;++i) {
            auto& b=simulations[(size_t)i];
            b.setButtonText(juce::String(targets[i])+"   /   "+subtitles[i]);
            b.setTooltip("Simulate generic "+juce::String(targets[i])+
                         " tonal character (not a measured microphone model)");
            b.onClick=[this,i]{setPhysical("micPreTarget",(float)i);refresh();};
            addAndMakeVisible(b);
        }
        amount.setSliderStyle(juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle(juce::Slider::TextBoxRight,false,58,20);
        trim.setSliderStyle(juce::Slider::LinearHorizontal);
        trim.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,20);
        addAndMakeVisible(amount);
        addAndMakeVisible(trim);
        amountLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,"micPreAmount",amount);
        trimLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,"micPreTrim",trim);
        amount.textFromValueFunction=[](double v) {
            return juce::String(juce::roundToInt(v*100.))+"%";
        };
        amount.valueFromTextFunction=[](const juce::String& t) {
            return juce::jlimit(0.,1.,t.getDoubleValue()/100.);
        };
        trim.textFromValueFunction=[](double v) {
            return juce::String(-6.+12.*v,1)+"dB";
        };
        trim.valueFromTextFunction=[](const juce::String& t) {
            return juce::jlimit(0.,1.,(t.getDoubleValue()+6.)/12.);
        };
        amount.updateText();trim.updateText();
        refresh();
        setVisible(false);
    }
    std::function<void()> onClose;
    bool on() const {
        const auto* p=processor.state.getRawParameterValue("micPreOn");
        return p!=nullptr && p->load()>.5f;
    }
    void refresh() {
        Theme::selectFor(processor.state);
        const auto* in=processor.state.getRawParameterValue("micPreInput");
        const auto* out=processor.state.getRawParameterValue("micPreTarget");
        const int source=in?juce::jlimit(0,3,(int)in->load()):0;
        const int model=out?juce::jlimit(0,3,(int)out->load()):0;
        if(input.getSelectedId()!=source+1)
            input.setSelectedId(source+1,juce::dontSendNotification);
        enabled.setButtonText(on()?"ON":"OFF");
        enabled.setColour(juce::TextButton::buttonColourId,
                          on()?Theme::green.withAlpha(.24f):Theme::panelAlt);
        enabled.setColour(juce::TextButton::textColourOffId,
                          on()?Theme::green:Theme::muted);
        close.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        close.setColour(juce::TextButton::textColourOffId,Theme::white);
        input.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
        input.setColour(juce::ComboBox::textColourId,Theme::white);
        input.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        for(int i=0;i<4;++i) {
            auto& b=simulations[(size_t)i];
            b.setColour(juce::TextButton::buttonColourId,
                        i==model?Theme::green.withAlpha(.19f):Theme::panelAlt);
            b.setColour(juce::TextButton::textColourOffId,
                        i==model?Theme::green:Theme::white);
        }
        for(auto* slider:{&amount,&trim}) {
            slider->setColour(juce::Slider::trackColourId,Theme::green);
            slider->setColour(juce::Slider::thumbColourId,Theme::white);
            slider->setColour(juce::Slider::textBoxTextColourId,Theme::white);
            slider->setColour(juce::Slider::textBoxBackgroundColourId,Theme::panelAlt);
            slider->setColour(juce::Slider::textBoxOutlineColourId,Theme::stroke);
        }
        repaint();
    }
    void resized() override {
        const int w=getWidth();
        close.setBounds(w-42,13,27,27);
        enabled.setBounds(w-111,16,58,26);
        input.setBounds(24,109,w-48,38);
        for(int i=0;i<4;++i)
            simulations[(size_t)i].setBounds(24+(i%2)*(w/2-17),
                                              199+(i/2)*63,w/2-31,54);
        amount.setBounds(110,354,w-145,29);
        trim.setBounds(110,393,w-145,29);
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        Theme::panelBox(g,getLocalBounds().toFloat(),10.f);
        g.setColour(Theme::green.withAlpha(.62f));
        g.fillRoundedRectangle({21.f,19.f,3.f,20.f},1.f);
        Theme::text(g,"MIC MODELING",{33,14,getWidth()-176,31},19,Theme::white,true);
        Theme::text(g,"INPUT MIC",{24,81,220,20},11,Theme::muted,true);
        Theme::text(g,"SIMULATE",{24,172,250,19},11,Theme::muted,true);
        Theme::text(g,"AMOUNT",{24,353,90,25},12,Theme::white,true);
        Theme::text(g,"TRIM",{24,392,90,25},12,Theme::white,true);
        g.setColour(Theme::stroke.withAlpha(.8f));
        g.drawLine(24.f,338.f,(float)getWidth()-24.f,338.f,1.f);
        Theme::text(g,"GENERIC TONAL PROFILES · NOT MEASURED BRAND MICROPHONES",
            {24,437,getWidth()-48,18},10,Theme::muted,false,
             juce::Justification::centred);
        // Capsule-microphone silhouettes on four cards, vector only.
        for(int i=0;i<4;++i) {
            const float x=39.f+(float)(i%2)*(getWidth()/2.f-17.f);
            const float y=218.f+(float)(i/2)*63.f;
            const auto* p=processor.state.getRawParameterValue("micPreTarget");
            const bool selected=p && (int)p->load()==i;
            g.setColour(selected?Theme::green.withAlpha(.78f):Theme::muted);
            g.drawRoundedRectangle(x,y-9.f,11.f,20.f,5.f,1.1f);
            g.drawLine(x+5.5f,y+11.f,x+5.5f,y+17.f,1.f);
            g.drawLine(x+1.5f,y+17.f,x+9.5f,y+17.f,1.f);
        }
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::escapeKey){close.onClick();return true;}
        return false;
    }
    bool testInputAndBypass() {
        const auto currentOn=on();
        const auto* raw=processor.state.getRawParameterValue("micPreTarget");
        const auto prior=raw?raw->load():0.f;
        setPhysical("micPreTarget",3.f);
        setPhysical("micPreOn",1.f);
        refresh();
        const bool changed=on() && simulations[3].getButtonText().containsIgnoreCase("Air") &&
                           raw && (int)raw->load()==3;
        setPhysical("micPreTarget",prior);
        setPhysical("micPreOn",currentOn?1.f:0.f);
        refresh();
        return changed;
    }
private:
    void setPhysical(const juce::String& id,float v) {
        if(auto* parameter=processor.state.getParameter(id)) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
            parameter->endChangeGesture();
        }
    }
    SkellPresetAudioProcessor& processor;
    juce::TextButton close,enabled;
    juce::ComboBox input;
    std::array<juce::TextButton,4> simulations;
    juce::Slider amount,trim;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountLink,trimLink;
};
