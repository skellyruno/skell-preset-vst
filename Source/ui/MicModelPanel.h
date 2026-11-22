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
    // Native, theme-painted mic source choices; no OS/DAW ComboBox popup.
    class SourceMenu final : public juce::Component {
    public:
        SourceMenu(){setWantsKeyboardFocus(true);}
        std::function<void(int)> onChoose;
        void setSelected(int index) {
            selected=juce::jlimit(0,3,index);
            repaint();
        }
        void chooseSelectedForTest() {choose(selected);}
        void paint(juce::Graphics& g) override {
            Theme::panelBox(g,getLocalBounds().toFloat(),6.f);
            const juce::StringArray sources{"NEUTRAL","CONDENSER","DYNAMIC","RIBBON"};
            for(int i=0;i<4;++i) {
                const int y=3+28*i;
                const auto rect=juce::Rectangle<float>(4.f,(float)y,
                                      (float)getWidth()-8.f,26.f);
                g.setColour(i==selected?Theme::green.withAlpha(.21f):
                            (i%2?Theme::panelAlt:Theme::plot));
                g.fillRoundedRectangle(rect,4.f);
                if(i==selected) {
                    g.setColour(Theme::green.withAlpha(.78f));
                    g.drawRoundedRectangle(rect.reduced(.6f),4.f,1.f);
                }
                Theme::text(g,sources[i],{13,y+3,getWidth()-26,20},
                            12,i==selected?Theme::green:Theme::white,true);
            }
        }
        void mouseDown(const juce::MouseEvent& event) override {
            const int row=(event.y-3)/28;
            if(row>=0&&row<4&&event.y>=3)choose(row);
        }
        bool keyPressed(const juce::KeyPress& key) override {
            if(key==juce::KeyPress::upKey){setSelected(selected-1);return true;}
            if(key==juce::KeyPress::downKey){setSelected(selected+1);return true;}
            if(key==juce::KeyPress::returnKey){choose(selected);return true;}
            if(key==juce::KeyPress::escapeKey){setVisible(false);return true;}
            return false;
        }
    private:
        void choose(int index){
            setSelected(index);setVisible(false);
            if(onChoose)onChoose(selected);
        }
        int selected=0;
    };
public:
    explicit SkellMicModelPanel(SkellPresetAudioProcessor& p) : processor(p) {
        setWantsKeyboardFocus(true);
        close.setButtonText("X");
        close.onClick=[this]{sourceMenu.setVisible(false);setVisible(false);if(onClose)onClose();};
        addAndMakeVisible(close);
        modPage.setButtonText("MIC MOD");
        gatePage.setButtonText("NOISE GATE");
        modPage.onClick=[this]{showGatePage=false;refresh();};
        gatePage.onClick=[this]{
            sourceMenu.setVisible(false);showGatePage=true;refresh();
        };
        addAndMakeVisible(modPage);addAndMakeVisible(gatePage);
        enabled.setButtonText("OFF");
        enabled.onClick=[this] {
            const bool next=showGatePage?!gateOn():!on();
            setPhysical(showGatePage?"micGateOn":"micPreOn",next?1.f:0.f);
            refresh();
        };
        addAndMakeVisible(enabled);
        input.setButtonText("NEUTRAL  ▼");
        input.onClick=[this] {
            const bool opening=!sourceMenu.isVisible();
            if(opening){
                const auto* p=processor.state.getRawParameterValue("micPreInput");
                sourceMenu.setSelected(p?juce::jlimit(0,3,(int)p->load()):0);
            }
            sourceMenu.setVisible(opening);
            if(opening) {
                sourceMenu.toFront(true);
                sourceMenu.grabKeyboardFocus();
            }
        };
        addAndMakeVisible(input);
        sourceMenu.onChoose=[this](int index) {
            setPhysical("micPreInput",(float)index);
            refresh();
        };
        addAndMakeVisible(sourceMenu);
        sourceMenu.setVisible(false);
        constexpr const char* targets[]{"Flat","Warm","Focus","Air"};
        constexpr const char* subtitles[]{"Uncolored","Soft low-mids",
                                         "Mid presence","High-frequency air"};
        for(int i=0;i<4;++i) {
            auto& b=simulations[(size_t)i];
            b.setButtonText(juce::String(targets[i]).toUpperCase());
            b.setTooltip(juce::String(targets[i])+" · "+juce::String(subtitles[i])+
                         " · Generic tonal character, not an IR of a real microphone");
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
        const char* gateIds[]{"micGateThreshold","micGateAttackMs","micGateHoldMs",
                              "micGateReleaseMs","micGateHysteresisDb"};
        juce::Slider* sliders[]{&gateThreshold,&gateAttack,&gateHold,
                                &gateRelease,&gateHysteresis};
        for(int k=0;k<5;++k) {
            sliders[k]->setSliderStyle(juce::Slider::LinearHorizontal);
            sliders[k]->setTextBoxStyle(juce::Slider::TextBoxRight,false,64,20);
            addAndMakeVisible(*sliders[k]);
            gateLinks[(size_t)k]=std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,gateIds[k],*sliders[k]);
        }
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
    bool hasBeenMoved() const noexcept {return movedByUser;}
    void visibilityChanged() override {
        if(!isVisible())sourceMenu.setVisible(false);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.x>=getWidth()-23 && e.y>=getHeight()-23) {
            resizing=true;
            resizeStart=getBounds();
            toFront(true);
            return;
        }
        if(sourceMenu.isVisible()&&!sourceMenu.getBounds().contains(e.getPosition()))
            sourceMenu.setVisible(false);
        if(e.y>=0 && e.y<45 && e.x<getWidth()-112) {
            dragHeader=true;
            dragOrigin=getPosition();
            toFront(true);
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(resizing) {
            if(auto* parent=getParentComponent()){
                const int w=juce::jlimit(310,juce::jmin(490,parent->getWidth()-12),
                    resizeStart.getWidth()+e.getDistanceFromDragStartX());
                const int h=juce::jlimit(295,juce::jmin(405,parent->getHeight()-12),
                    resizeStart.getHeight()+e.getDistanceFromDragStartY());
                const int x=juce::jlimit(0,parent->getWidth()-w,resizeStart.getX());
                const int y=juce::jlimit(0,parent->getHeight()-h,resizeStart.getY());
                movedByUser=true;
                setBounds(x,y,w,h);
            }
            return;
        }
        if(!dragHeader)return;
        if(auto* parent=getParentComponent()) {
            const int x=juce::jlimit(0,juce::jmax(0,parent->getWidth()-getWidth()),
                                    dragOrigin.x+e.getDistanceFromDragStartX());
            const int y=juce::jlimit(0,juce::jmax(0,parent->getHeight()-getHeight()),
                                    dragOrigin.y+e.getDistanceFromDragStartY());
            if(x!=getX() || y!=getY()) {
                movedByUser=true;
                setTopLeftPosition(x,y);
            }
        }
    }
    void mouseUp(const juce::MouseEvent&) override {
        dragHeader=false;
        resizing=false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    void mouseMove(const juce::MouseEvent& e) override {
        setMouseCursor(e.x>=getWidth()-23 && e.y>=getHeight()-23?
            juce::MouseCursor::BottomRightCornerResizeCursor:
            (e.y<45 && e.x<getWidth()-112?
                juce::MouseCursor::DraggingHandCursor:
                juce::MouseCursor::NormalCursor));
    }
    bool on() const {
        const auto* p=processor.state.getRawParameterValue("micPreOn");
        return p!=nullptr && p->load()>.5f;
    }
    bool gateOn() const {
        const auto* p=processor.state.getRawParameterValue("micGateOn");
        return p!=nullptr && p->load()>.5f;
    }
    void refreshIfChanged() {
        const auto* in=processor.state.getRawParameterValue("micPreInput");
        const auto* out=processor.state.getRawParameterValue("micPreTarget");
        const int inputIndex=in?juce::jlimit(0,3,(int)in->load()):0;
        const int targetIndex=out?juce::jlimit(0,3,(int)out->load()):0;
        const bool enabledNow=on();
        const bool gateEnabledNow=gateOn();
        if(inputIndex!=lastInput || targetIndex!=lastTarget ||
           enabledNow!=lastEnabled || gateEnabledNow!=lastGateEnabled)
            refresh();
    }
    void refresh() {
        Theme::selectFor(processor.state);
        const auto* in=processor.state.getRawParameterValue("micPreInput");
        const auto* out=processor.state.getRawParameterValue("micPreTarget");
        const int source=in?juce::jlimit(0,3,(int)in->load()):0;
        const int model=out?juce::jlimit(0,3,(int)out->load()):0;
        lastInput=source;lastTarget=model;lastEnabled=on();
        lastGateEnabled=gateOn();
        const bool active=showGatePage?gateOn():on();
        modPage.setColour(juce::TextButton::buttonColourId,
                          showGatePage?Theme::panelAlt:Theme::green.withAlpha(.23f));
        gatePage.setColour(juce::TextButton::buttonColourId,
                           showGatePage?Theme::green.withAlpha(.23f):Theme::panelAlt);
        modPage.setColour(juce::TextButton::textColourOffId,
                          showGatePage?Theme::muted:Theme::green);
        gatePage.setColour(juce::TextButton::textColourOffId,
                           showGatePage?Theme::green:Theme::muted);
        input.setVisible(!showGatePage);
        amount.setVisible(!showGatePage);
        trim.setVisible(!showGatePage);
        for(auto& card:simulations)card.setVisible(!showGatePage);
        for(auto* slider:{&gateThreshold,&gateAttack,&gateHold,
                           &gateRelease,&gateHysteresis})
            slider->setVisible(showGatePage);
        if(showGatePage)sourceMenu.setVisible(false);
        const juce::StringArray sources{"NEUTRAL","CONDENSER","DYNAMIC","RIBBON"};
        input.setButtonText(sources[source]+"  ▼");
        enabled.setButtonText(active?"ON":"OFF");
        enabled.setColour(juce::TextButton::buttonColourId,
                          active?Theme::green.withAlpha(.24f):Theme::panelAlt);
        enabled.setColour(juce::TextButton::textColourOffId,
                          active?Theme::green:Theme::muted);
        close.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        close.setColour(juce::TextButton::textColourOffId,Theme::white);
        input.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        input.setColour(juce::TextButton::textColourOffId,Theme::green);
        sourceMenu.repaint();
        for(int i=0;i<4;++i) {
            auto& b=simulations[(size_t)i];
            b.setColour(juce::TextButton::buttonColourId,
                        i==model?Theme::green.withAlpha(.19f):Theme::panelAlt);
            b.setColour(juce::TextButton::textColourOffId,
                        i==model?Theme::green:Theme::white);
        }
        for(auto* slider:{&amount,&trim,&gateThreshold,&gateAttack,
                          &gateHold,&gateRelease,&gateHysteresis}) {
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
        close.setBounds(w-35,9,25,25);
        enabled.setBounds(w-102,9,57,25);
        modPage.setBounds(16,36,juce::jmax(85,(w-46)/2),22);
        gatePage.setBounds(27+juce::jmax(85,(w-46)/2),36,
                           juce::jmax(90,(w-46)/2),22);
        juce::Slider* gateSliders[]{&gateThreshold,&gateAttack,&gateHold,
                                    &gateRelease,&gateHysteresis};
        for(int i=0;i<5;++i)
            gateSliders[i]->setBounds(112,87+35*i,w-128,26);
        input.setBounds(16,68,w-32,29);
        sourceMenu.setBounds(16,100,w-32,118);
        const int cardWidth=(w-46)/2;
        for(int i=0;i<4;++i)
            simulations[(size_t)i].setBounds(16+(i%2)*(cardWidth+14),
                                              128+(i/2)*45,cardWidth,38);
        const int lower=juce::jmax(215,getHeight()-83);
        amount.setBounds(85,lower,w-101,25);
        trim.setBounds(85,lower+40,w-101,25);
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        Theme::panelBox(g,getLocalBounds().toFloat(),10.f);
        g.setColour(Theme::green.withAlpha(.62f));
        g.fillRoundedRectangle({13.f,13.f,3.f,17.f},1.f);
        Theme::text(g,showGatePage?"MIC NOISE GATE":"MIC MODEL",
                    {25,9,getWidth()-143,27},16,Theme::white,true);
        if(showGatePage) {
            constexpr const char* names[]{"THRESHOLD","ATTACK","HOLD","RELEASE","HYSTERESIS"};
            for(int i=0;i<5;++i)
                Theme::text(g,names[i],{16,87+35*i,95,26},11.f,Theme::white,true);
            Theme::fittedText(g,"STEREO-LINKED / PRE-RACK / ZERO LOOKAHEAD",
                {15,getHeight()-24,getWidth()-44,16},9.f,Theme::muted,false,
                juce::Justification::centredLeft);
            g.setColour(Theme::green.withAlpha(.7f));
            const float x=(float)getWidth()-5.f,y=(float)getHeight()-5.f;
            for(int i=0;i<3;++i)g.drawLine(x-9.f+4.f*i,y,x,y-9.f+4.f*i,1.f);
            return;
        }
        Theme::text(g,"INPUT MICROPHONE",{16,51,188,18},11.5f,Theme::white,true);
        const juce::StringArray descriptions{
            "FLAT","WARM","FOCUS","AIR"};
        const auto* rawTarget=processor.state.getRawParameterValue("micPreTarget");
        const int target=rawTarget?juce::jlimit(0,3,(int)rawTarget->load()):0;
        // The old target description at y=212 covered AMOUNT at compact
        // heights. Keep active-model feedback beside SIMULATE instead.
        Theme::text(g,"MODEL",{16,109,80,18},11.5f,Theme::white,true);
        Theme::fittedText(g,descriptions[target]+" MODEL",
                          {getWidth()-153,109,132,18},11.4f,Theme::green,true,
                          juce::Justification::centredRight);
        const int lower=juce::jmax(215,getHeight()-83);
        Theme::text(g,"AMOUNT",{16,lower,70,23},11.4f,Theme::white,true);
        Theme::text(g,"TRIM",{16,lower+40,62,23},11.4f,Theme::white,true);
        g.setColour(Theme::stroke.withAlpha(.8f));
        g.drawLine(16.f,(float)lower-10.f,(float)getWidth()-16.f,(float)lower-10.f,1.f);
        Theme::fittedText(g,"TONAL MODEL / NOT A MEASURED MIC IR",
            {16,getHeight()-16,getWidth()-55,12},8.2f,Theme::muted,false,
             juce::Justification::centredLeft);
        // Resize handle, intentionally inside the panel bounds.
        g.setColour(Theme::green.withAlpha(.7f));
        const float right=(float)getWidth()-5.f,bottom=(float)getHeight()-5.f;
        for(int k=0;k<3;++k)
            g.drawLine(right-9.f+4.f*k,bottom,right,bottom-9.f+4.f*k,1.f);
        // Compact vector capsules remain subordinate to the real controls.
        for(int i=0;i<4;++i) {
            const int cardWidth=(getWidth()-46)/2;
            const float x=26.f+(float)(i%2)*(cardWidth+14);
            const float y=145.f+(float)(i/2)*45.f;
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
    bool testNoiseGateControls() {
        const auto* raw=processor.state.getRawParameterValue("micGateOn");
        const bool before=gateOn();
        gatePage.onClick();
        const bool tab=showGatePage && gateThreshold.isVisible()
                       && !input.isVisible();
        enabled.onClick();
        const bool toggled=raw && (raw->load()>.5f)!=before;
        setPhysical("micGateOn",before?1.f:0.f);
        modPage.onClick();
        refresh();
        return tab && toggled && !showGatePage && input.isVisible();
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
        const auto* src=processor.state.getRawParameterValue("micPreInput");
        const float oldSource=src?src->load():0.f;
        input.onClick();
        const bool opened=sourceMenu.isVisible();
        sourceMenu.onChoose(2);
        const bool selected=src && (int)src->load()==2 &&
                            input.getButtonText().containsIgnoreCase("Dynamic");
        sourceMenu.setVisible(false);
        setPhysical("micPreInput",oldSource);
        refresh();
        return changed&&opened&&selected;
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
    juce::TextButton close,enabled,modPage,gatePage;
    juce::TextButton input;
    SourceMenu sourceMenu;
    std::array<juce::TextButton,4> simulations;
    juce::Slider amount,trim;
    juce::Slider gateThreshold,gateAttack,gateHold,gateRelease,gateHysteresis;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountLink,trimLink;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,5> gateLinks;
    int lastInput=-1,lastTarget=-1;
    bool lastEnabled=false,lastGateEnabled=false,showGatePage=false;
    bool movedByUser=false,dragHeader=false,resizing=false;
    juce::Point<int> dragOrigin{};
    juce::Rectangle<int> resizeStart{};
};
