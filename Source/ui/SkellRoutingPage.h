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
        source.addItem("VOCAL - AFTER MIC",1);source.addItem("VOCAL - BEFORE MIC",2);
        source.addItem("EFFECT INPUT",3);source.addItem("DAW SIDECHAIN",4);
        source.setSelectedId(1,juce::dontSendNotification);
        source.setTooltip("Choose the signal that ducks the effect. Link or update to apply.");
        addAndMakeVisible(source);source.onChange=[this]{refresh();};
        connect.setButtonText("CONNECT");disconnect.setButtonText("DISCONNECT");
        edit.setButtonText("DUCKING SETTINGS");back.setButtonText("CLOSE");
        for(auto* b:{&connect,&disconnect,&edit,&back})addAndMakeVisible(*b);
        connect.onClick=[this]{
            const bool same=valid(target) && isConnected(target) && source.getSelectedId()==committedSource(target);
            apply(!same);
        };disconnect.onClick=[this]{apply(false);};
        disconnect.setVisible(false);
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
        setVisible(true);
        if(!valid(physical)){
            for(int i=0;i<processor.activeSlotCount();++i){
                const int id=processor.slotIdAt(i);
                if(valid(id)){physical=id;break;}
            }
        }
        selectTarget(physical);refresh();
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
                (valid(id)?(isConnected(id)?"  [LINKED]":""):"  [N/A]");
            if(b.getButtonText()!=label)b.setButtonText(label);
            b.setEnabled(valid(id));b.setToggleState(id==target,juce::dontSendNotification);
            b.setBounds(targetBounds(position));
        }
        const bool available=valid(target);
        const bool linked=available && isConnected(target);
        const bool pending=available && (!linked || source.getSelectedId()!=committedSource(target));
        connect.setEnabled(available);disconnect.setVisible(false);edit.setEnabled(available);
        const auto label=linked?(pending?"UPDATE SOURCE":"UNLINK EFFECT"):"LINK EFFECT";
        if(connect.getButtonText()!=label)connect.setButtonText(label);
        source.setEnabled(available);repaint();
    }
    void resized() override{
        back.setBounds(getWidth()-122,18,102,32);source.setBounds(30,139,244,42);
        connect.setBounds(30,235,244,44);disconnect.setBounds({});edit.setBounds(30,288,244,34);
        refresh();
    }
    void paint(juce::Graphics& g) override{
        Theme::selectFor(processor.state);
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        Theme::fittedText(g,"SIDECHAIN",{24,18,530,36},26.f,Theme::white,true);
        Theme::fittedText(g,"Use one signal to lower an effect while it plays.",{24,57,getWidth()-48,23},13.f,Theme::muted);
        g.setColour(Theme::stroke);g.drawHorizontalLine(88,24.f,(float)getWidth()-24.f);
        Theme::panelBox(g,{18.f,103.f,268.f,408.f},8.f);
        Theme::text(g,"SIGNAL",{30,111,244,25},14.f,Theme::white,true);
        Theme::text(g,"CHOOSE EFFECT",{318,106,getWidth()-342,24},14.f,Theme::white,true);
        if(valid(target)){
            const bool linked=isConnected(target);
            const bool same=linked && source.getSelectedId()==committedSource(target);
            Theme::fittedText(g,processor.moduleDisplayName(processor.slotType(target)),
                {30,187,244,23},15.f,Theme::white,true);
            Theme::text(g,same?"LINKED":linked?"SOURCE CHANGED":"NOT LINKED",{30,211,244,19},11.5f,same?Theme::green:Theme::muted,true);
            Theme::text(g,"DUCKING",{30,347,244,20},11.f,Theme::muted,true);
            const float reduction=linked?processor.getSlotDuckReductionDb(target):0.f;
            Theme::text(g,juce::String(reduction,1)+" dB",{30,369,244,30},22.f,Theme::white,true);
            const juce::Rectangle<float> meter(30.f,405.f,244.f,8.f);
            g.setColour(Theme::stroke);g.fillRoundedRectangle(meter,4.f);
            const float fill=meter.getWidth()*juce::jlimit(0.f,1.f,std::abs(reduction)/24.f);
            if(fill>0.f){g.setColour(Theme::green);g.fillRoundedRectangle(meter.withWidth(fill),4.f);}
            Theme::fittedText(g,"Linked effects reduce their wet signal.",{30,430,244,24},12.f,Theme::muted);
        }else{
            Theme::fittedText(g,"Choose an effect on the right.",{30,190,244,35},13.f,Theme::muted);
        }
        int linkedCount=0,eligible=0,occupied=0;
        for(int i=0;i<processor.activeSlotCount();++i){
            const int id=processor.slotIdAt(i);
            if(processor.slotType(id)==P::Empty)continue;
            ++occupied;
            if(valid(id)){++eligible;if(isConnected(id))++linkedCount;}
        }
        const int rows=juce::jlimit(1,8,(occupied+1)/2);
        const int summaryY=140+rows*43+10;
        if(summaryY+56<getHeight()-22){
            Theme::fittedText(g,juce::String(linkedCount)+" of "+juce::String(eligible)+" effects linked",
                {318,summaryY,getWidth()-344,25},14.f,Theme::white,true);
            Theme::fittedText(g,"Select any effect to see or change its link.",{318,summaryY+27,getWidth()-344,21},12.f,Theme::muted);
        }
        Theme::fittedText(g,"DAW sidechain needs a host input. External plugins are not supported here.",
            {24,getHeight()-37,getWidth()-48,22},11.f,Theme::muted);
    }
    bool testConnectionForTest(int id){
        if(!valid(id))return false;
        const auto prefix=slotPrefix(id);
        const auto suffix=prefix+P::sidechainEnableSuffix(processor.slotType(id));
        const float oldOn=read(suffix),oldExternal=read(prefix+"duckExternal"),oldSource=read(prefix+"duckSource");
        selectTarget(id);source.setSelectedId(4,juce::dontSendNotification);
        const bool unchanged=read(suffix)==oldOn && read(prefix+"duckExternal")==oldExternal;
        apply(true,false);const bool linked=isConnected(id) && read(prefix+"duckExternal")>.5f;
        const bool unlinkLabel=connect.getButtonText()=="UNLINK EFFECT";
        source.setSelectedId(2,juce::dontSendNotification);refresh();
        const bool previewSafe=isConnected(id) && read(prefix+"duckExternal")>.5f &&
            connect.getButtonText()=="UPDATE SOURCE";
        apply(true,false);
        const bool updated=isConnected(id) && read(prefix+"duckExternal")<.5f && read(prefix+"duckSource")==1.f;
        apply(false,false);const bool removed=!isConnected(id);
        set(suffix,oldOn);set(prefix+"duckExternal",oldExternal);set(prefix+"duckSource",oldSource);
        refresh();return unchanged&&linked&&unlinkLabel&&previewSafe&&updated&&removed;
    }
private:
    P& processor;int target=-1;
    juce::ComboBox source;
    SkellActionButton connect,disconnect,edit,back;
    std::array<SkellActionButton,P::slotCount> targets;
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
        int count=0,displayIndex=0;
        for(int i=0;i<processor.activeSlotCount();++i){
            if(processor.slotType(processor.slotIdAt(i))==P::Empty)continue;
            if(i<position)++displayIndex;
            ++count;
        }
        const int rows=juce::jlimit(1,8,(count+1)/2);
        const int col=displayIndex/rows,row=displayIndex%rows;
        return {318+col*284,140+row*43,266,37};
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
