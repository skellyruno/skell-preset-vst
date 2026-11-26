#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "SkellActionButton.h"
#include "../PluginProcessor.h"
#include <array>
#include <functional>
#include <cmath>
class SkellRoutingPage final:public juce::Component {
    using P=SkellPresetAudioProcessor;
public:
    explicit SkellRoutingPage(P& p):processor(p){
        source.addItem("POST MIC",1);source.addItem("PRE MIC",2);
        source.addItem("SLOT INPUT",3);source.addItem("EXTERNAL HOST KEY",4);
        source.setSelectedId(1,juce::dontSendNotification);
        source.setTooltip("Choose a key; routing changes only when you press Connect");
        addAndMakeVisible(source);source.onChange=[this]{repaint();};
        connect.setButtonText("CONNECT");disconnect.setButtonText("DISCONNECT");
        edit.setButtonText("EDIT TARGET");back.setButtonText("BACK TO STAGE");
        for(auto* b:{&connect,&disconnect,&edit,&back})addAndMakeVisible(*b);
        connect.onClick=[this]{apply(true);};disconnect.onClick=[this]{apply(false);};
        edit.onClick=[this]{if(valid(target) && onEdit)onEdit(target);};
        back.onClick=[this]{if(onClose)onClose();};
        for(int i=0;i<P::slotCount;++i){
            auto& b=targets[(size_t)i];addAndMakeVisible(b);
            b.onClick=[this,i]{selectTarget(i);};
        }
        applyTheme();setVisible(false);
    }
    std::function<void()> onClose,onBeforeChange,onAfterChange;
    std::function<void(int)> onEdit;
    void applyTheme(){
        source.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
        source.setColour(juce::ComboBox::textColourId,Theme::white);
        source.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        source.setColour(juce::ComboBox::arrowColourId,Theme::green);
        for(auto& b:targets){
            b.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
            b.setColour(juce::TextButton::buttonOnColourId,Theme::green.withAlpha(.22f));
            b.setColour(juce::TextButton::textColourOffId,Theme::white);
            b.setColour(juce::TextButton::textColourOnId,Theme::green);
        }
        repaint();
    }
    void open(int physical){
        target=-1;source.setSelectedId(1,juce::dontSendNotification);
        setVisible(true);selectTarget(physical);refresh();
    }
    void selectTarget(int physical){
        target=valid(physical)?physical:-1;
        if(target>=0){
            const auto prefix=slotPrefix(target);
            const bool external=read(prefix+"duckExternal")>.5f;
            source.setSelectedId(external?4:juce::jlimit(1,3,1+(int)read(prefix+"duckSource")),juce::dontSendNotification);
        }
        refresh();
    }
    bool isConnected(int id) const{
        return valid(id) && read(slotPrefix(id)+P::sidechainEnableSuffix(processor.slotType(id)))>.5f;
    }
    bool valid(int id) const{
        if(id<0 || id>=P::slotCount || !P::supportsEffectSidechain(processor.slotType(id)))return false;
        for(int i=0;i<processor.activeSlotCount();++i)if(processor.slotIdAt(i)==id)return true;
        return false;
    }
    void refresh(){
        if(target>=0 && !valid(target))target=-1;
        for(int id=0;id<P::slotCount;++id){
            auto& b=targets[(size_t)id];int position=-1;
            for(int n=0;n<processor.activeSlotCount();++n)if(processor.slotIdAt(n)==id){position=n;break;}
            const int type=processor.slotType(id);
            const bool show=position>=0 && type!=P::Empty;
            b.setVisible(show);
            if(!show)continue;
            const auto name=processor.moduleDisplayName(type);
            const juce::String label=juce::String(position+1).paddedLeft('0',2)+"  "+name+
                (valid(id)?(isConnected(id)?"  / LINKED":""):"  / HOSTED");
            if(b.getButtonText()!=label)b.setButtonText(label);
            b.setEnabled(valid(id));b.setToggleState(id==target,juce::dontSendNotification);
            b.setBounds(targetBounds(position));
        }
        const bool available=valid(target);
        connect.setEnabled(available);disconnect.setEnabled(available && isConnected(target));edit.setEnabled(available);
        const auto label=available && isConnected(target)?"UPDATE LINK":"CONNECT";
        if(connect.getButtonText()!=label)connect.setButtonText(label);
        source.setEnabled(available);repaint();
    }
    void resized() override{
        back.setBounds(getWidth()-168,16,148,32);source.setBounds(22,135,222,36);
        connect.setBounds(22,208,222,36);disconnect.setBounds(22,251,222,32);edit.setBounds(22,302,222,32);
        refresh();
    }
    void paint(juce::Graphics& g) override{
        Theme::selectFor(processor.state);
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        Theme::fittedText(g,"SIDECHAIN ROUTING",{22,15,getWidth()-218,33},25.f,Theme::white,true);
        Theme::fittedText(g,"CHOOSE SOURCE  /  SELECT EFFECT  /  CONNECT",{22,51,getWidth()-44,21},12.f,Theme::muted,true);
        Theme::panelBox(g,{12.f,89.f,242.f,420.f},8.f,true);
        Theme::text(g,"KEY SOURCE",{22,102,222,24},13.f,Theme::white,true);
        Theme::text(g,"EFFECT TARGETS",{300,91,getWidth()-320,24},13.f,Theme::white,true);
        g.setColour(Theme::stroke);g.drawHorizontalLine(77,18.f,(float)getWidth()-18.f);
        if(valid(target)){
            int position=0;for(int i=0;i<processor.activeSlotCount();++i)if(processor.slotIdAt(i)==target)position=i;
            const auto card=targetBounds(position);
            const float x=(float)card.getX(),y=(float)card.getCentreY();
            juce::Path cable;cable.startNewSubPath(254.f,153.f);
            const bool secondColumn=card.getX()>300;
            const float rail=secondColumn?574.f:276.f;
            cable.lineTo(276.f,153.f);
            if(secondColumn){cable.lineTo(276.f,120.f);cable.lineTo(rail,120.f);}
            cable.lineTo(rail,y);cable.lineTo(x,y);
            const bool linked=isConnected(target) && source.getSelectedId()==committedSource(target);
            const auto ink=linked?Theme::green:Theme::muted;
            g.setColour(ink.withAlpha(.16f));g.strokePath(cable,juce::PathStrokeType(6.f));
            g.setColour(ink);g.strokePath(cable,juce::PathStrokeType(1.8f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            g.fillEllipse(x-4.f,y-3.f,6.f,6.f);
            Theme::fittedText(g,linked?"CONNECTED":isConnected(target)?"READY TO UPDATE":"READY TO CONNECT",{22,181,222,20},11.f,ink,true);
            const float reduction=processor.getSlotDuckReductionDb(target);
            Theme::text(g,"MEASURED REDUCTION",{22,359,222,18},10.5f,Theme::muted,true);
            Theme::text(g,juce::String(reduction,1)+" dB",{22,382,222,30},20.f,Theme::green,true);
            if(processor.slotType(target)==P::Limiter || processor.slotType(target)==P::StudioLimiterFX)
                Theme::fittedText(g,"Ducking this effect reduces limiting.",{22,424,222,62},11.5f,Theme::amber);
            else Theme::fittedText(g,"Key controls wet contribution, not the audio input.",{22,424,222,62},11.5f,Theme::muted);
        }else{
            Theme::fittedText(g,"Select a rack effect first.",{22,180,222,40},12.f,Theme::muted);
            Theme::fittedText(g,"Add an effect in the rack if no target is available.",{22,424,222,62},11.5f,Theme::muted);
        }
        Theme::fittedText(g,"Native effect and return ducking. Hosted VST3 key buses are not yet routed.",
            {18,getHeight()-37,getWidth()-36,24},11.f,Theme::muted);
    }
    bool testConnectionForTest(int id){
        if(!valid(id))return false;
        const auto prefix=slotPrefix(id);
        const auto suffix=prefix+P::sidechainEnableSuffix(processor.slotType(id));
        const float oldOn=read(suffix),oldExternal=read(prefix+"duckExternal"),oldSource=read(prefix+"duckSource");
        selectTarget(id);source.setSelectedId(4,juce::dontSendNotification);
        const bool unchanged=read(suffix)==oldOn && read(prefix+"duckExternal")==oldExternal;
        apply(true,false);const bool linked=isConnected(id) && read(prefix+"duckExternal")>.5f;
        apply(false,false);const bool removed=!isConnected(id);
        set(suffix,oldOn);set(prefix+"duckExternal",oldExternal);set(prefix+"duckSource",oldSource);
        refresh();return unchanged&&linked&&removed;
    }
private:
    P& processor;int target=-1;
    juce::ComboBox source;
    SkellActionButton connect,disconnect,edit,back;
    std::array<juce::TextButton,P::slotCount> targets;
    static juce::String slotPrefix(int id){return "s"+juce::String(id+1)+"_";}
    float read(const juce::String& id) const{if(auto* p=processor.state.getRawParameterValue(id))return p->load();return 0.f;}
    void set(const juce::String& id,float v){
        if(auto* p=processor.state.getParameter(id)){
            const auto n=p->convertTo0to1(v);
            if(std::abs(p->getValue()-n)<1.e-6f)return;
            p->beginChangeGesture();p->setValueNotifyingHost(n);p->endChangeGesture();
        }
    }
    juce::Rectangle<int> targetBounds(int position) const{
        const int rows=juce::jlimit(1,8,(processor.activeSlotCount()+1)/2);
        const int col=position/rows,row=position%rows;return {300+col*284,132+row*45,266,36};
    }
    int committedSource(int id) const {
        const auto prefix=slotPrefix(id);
        return read(prefix+"duckExternal")>.5f?4:juce::jlimit(1,3,1+(int)read(prefix+"duckSource"));
    }
    void apply(bool on,bool journal=true){
        if(!valid(target))return;
        const bool changed=on!=isConnected(target) || (on && source.getSelectedId()!=committedSource(target));
        if(!changed)return;
        if(journal && onBeforeChange)onBeforeChange();
        const auto prefix=slotPrefix(target);
        if(on){
            const int chosen=source.getSelectedId();
            set(prefix+"duckSource",(float)juce::jlimit(0,2,chosen-1));
            set(prefix+"duckExternal",chosen==4?1.f:0.f);
        }
        set(prefix+P::sidechainEnableSuffix(processor.slotType(target)),on?1.f:0.f);
        if(journal && onAfterChange)onAfterChange();
        refresh();
    }
};
