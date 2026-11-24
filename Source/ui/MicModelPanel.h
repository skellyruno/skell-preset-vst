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
    // Native profile cards with schematic microphones, not licensed
    // hardware model photographs or claims of measured microphone IRs.
    class ModelCard final : public juce::TextButton {
    public:
        void setProfile(int profile,bool selected) {
            const int next=juce::jlimit(0,3,profile);
            if(index!=next||active!=selected){index=next;active=selected;repaint();}
        }
        void paintButton(juce::Graphics& g,bool hover,bool down) override {
            const auto r=getLocalBounds().toFloat().reduced(.7f);
            g.setColour(active?Theme::green.withAlpha(.13f):
                Theme::panelAlt.brighter(hover?.12f:0.f));
            g.fillRoundedRectangle(r,5.f);
            g.setColour((active?Theme::green:Theme::stroke).withAlpha(active?.90f:.85f));
            g.drawRoundedRectangle(r.reduced(.5f),5.f,1.f);
            const float x=r.getX()+17.f,y=r.getCentreY();
            const float w=index==1?13.f:index==3?9.f:11.f;
            g.setGradientFill(juce::ColourGradient(
                Theme::white.withAlpha(.88f),x-w*.5f,y-11.f,
                Theme::muted.withAlpha(.42f),x+w*.5f,y+10.f,false));
            if(index==2)g.fillEllipse(x-w*.5f,y-12.f,w,12.f);
            else g.fillRoundedRectangle(x-w*.5f,y-12.f,w,15.f,index==3?1.4f:3.f);
            g.fillRoundedRectangle(x-2.5f,y+1.f,5.f,12.f,1.f);
            g.setColour(Theme::bg.withAlpha(.68f));
            for(int k=0;k<4;++k)g.drawLine(x-w*.35f,y-8.f+k*2.4f,
                x+w*.35f,y-8.f+k*2.4f,.7f);
            constexpr const char* subtitles[]{"Uncolored","Soft low-mids","Mid presence","High air"};
            const int textW=juce::jmax(1,getWidth()-52);
            Theme::fittedText(g,getButtonText(),{33,4,textW,18},11.3f,
                active?Theme::green:Theme::white,true);
            Theme::fittedText(g,subtitles[index],{33,22,textW,13},9.4f,Theme::muted);
            if(active){
                g.setColour(Theme::green);
                g.fillEllipse(r.getRight()-9.f,r.getY()+6.f,4.f,4.f);
            }
            if(hasKeyboardFocus(true)){
                g.setColour(Theme::white.withAlpha(.7f));
                g.drawRoundedRectangle(r.reduced(2.4f),4.f,1.f);
            }
            if(down){
                g.setColour(Theme::bg.withAlpha(.16f));g.fillRoundedRectangle(r,5.f);
            }
        }
    private:
        int index=0;
        bool active=false;
    };
    // Native, theme-painted mic source choices; no OS/DAW ComboBox popup.
    class SourceMenu final : public juce::Component {
    public:
        SourceMenu(){setWantsKeyboardFocus(true);}
        std::function<void(int)> onChoose;
        std::function<void()> onDismiss;
        void setSelected(int index) {
            selected=juce::jlimit(0,3,index);
            focused=selected;
            hovered=-1;
            repaint();
        }
        void chooseSelectedForTest() {choose(selected);}
        void visibilityChanged() override {
            hovered=-1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        static int nextFocus(int index,int delta) noexcept {
            return juce::jlimit(0,3,index+delta);
        }
        void moveFocus(int delta) {
            focused=nextFocus(focused,delta);
            repaint();
        }
        bool testFocusForTest() const noexcept {
            return nextFocus(0,-1)==0 && nextFocus(3,1)==3 &&
                nextFocus(1,1)==2 && nextFocus(2,-1)==1;
        }
        static int rowHitForTest(int x,int y,int width) noexcept {
            // Painted rows are 26 high, with 2px inert gutters.
            if(x<4 || x>=width-4 || y<3 || y>=3+4*28)return -1;
            const int offset=y-3;
            return offset%28<26?offset/28:-1;
        }
        static bool testHitTargetsForTest() noexcept {
            for(int width:{160,190,270}) {
                for(int row=0;row<4;++row) {
                    const int y=3+28*row;
                    if(rowHitForTest(10,y,width)!=row ||
                       rowHitForTest(width-5,y+25,width)!=row ||
                       rowHitForTest(10,y+26,width)!=-1 ||
                       rowHitForTest(10,y+27,width)!=-1)
                        return false;
                }
                if(rowHitForTest(1,10,width)!=-1 ||
                   rowHitForTest(10,0,width)!=-1 ||
                   rowHitForTest(10,116,width)!=-1)
                    return false;
            }
            return true;
        }
        void paint(juce::Graphics& g) override {
            Theme::panelBox(g,getLocalBounds().toFloat(),6.f);
            const juce::StringArray sources{"NEUTRAL","CONDENSER","DYNAMIC","RIBBON"};
            for(int i=0;i<4;++i) {
                const int y=3+28*i;
                const auto rect=juce::Rectangle<float>(4.f,(float)y,
                                      (float)getWidth()-8.f,26.f);
                g.setColour(i==selected?Theme::green.withAlpha(.21f):
                            i==hovered?Theme::panelAlt.brighter(.18f):
                            (i%2?Theme::panelAlt:Theme::plot));
                g.fillRoundedRectangle(rect,4.f);
                if(i==selected || i==hovered) {
                    g.setColour(i==selected?Theme::green.withAlpha(.78f):
                               Theme::muted.withAlpha(.64f));
                    g.drawRoundedRectangle(rect.reduced(.6f),4.f,1.f);
                }
                if(hasKeyboardFocus(false) && i==focused){
                    g.setColour(Theme::white.withAlpha(.52f));
                    g.drawRoundedRectangle(rect.reduced(2.f),3.f,1.f);
                }
                Theme::text(g,sources[i],{13,y+3,getWidth()-26,20},
                            12,i==selected?Theme::green:Theme::white,true);
            }
        }
        void mouseMove(const juce::MouseEvent& event) override {
            const int row=rowHitForTest(event.x,event.y,getWidth());
            if(row!=hovered || (row>=0 && row!=focused)){
                hovered=row;
                if(row>=0)focused=row;
                repaint();
            }
            setMouseCursor(juce::MouseCursor(row>=0?
                juce::MouseCursor::PointingHandCursor:
                juce::MouseCursor::NormalCursor));
        }
        void mouseExit(const juce::MouseEvent&) override {
            if(hovered>=0){hovered=-1;repaint();}
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        void mouseDown(const juce::MouseEvent& event) override {
            const int row=rowHitForTest(event.x,event.y,getWidth());
            if(row>=0 && event.mods.isLeftButtonDown())choose(row);
        }
        bool keyPressed(const juce::KeyPress& key) override {
            if(key==juce::KeyPress::upKey){moveFocus(-1);return true;}
            if(key==juce::KeyPress::downKey){moveFocus(1);return true;}
            if(key==juce::KeyPress::homeKey){moveFocus(-3);return true;}
            if(key==juce::KeyPress::endKey){moveFocus(3);return true;}
            if(key==juce::KeyPress::returnKey || key==juce::KeyPress::spaceKey){
                choose(focused);return true;
            }
            if(key==juce::KeyPress::escapeKey){
                setVisible(false);if(onDismiss)onDismiss();return true;
            }
            return false;
        }
    private:
        void choose(int index){
            setSelected(index);setVisible(false);
            if(onChoose)onChoose(selected);
        }
        int selected=0,focused=0,hovered=-1;
    };
public:
    explicit SkellMicModelPanel(SkellPresetAudioProcessor& p) : processor(p) {
        setWantsKeyboardFocus(true);
        close.setButtonText("X");
        close.onClick=[this]{sourceMenu.setVisible(false);setVisible(false);if(onClose)onClose();};
        addAndMakeVisible(close);
        modelTab.setButtonText("MIC MODEL");
        gateTab.setButtonText("NOISE GATE");
        modelTab.setTooltip("Original microphone modeling and tonal simulation");
        gateTab.setTooltip("Independent low-latency input noise gate");
        modelTab.onClick=[this]{showPage(false);};
        gateTab.onClick=[this]{showPage(true);};
        addAndMakeVisible(modelTab);addAndMakeVisible(gateTab);
        gateEnabled.setButtonText("OFF");
        gateEnabled.setTooltip("Enable the input noise gate independently of Mic Model");
        gateEnabled.onClick=[this] {
            const auto* raw=processor.state.getRawParameterValue("micGateOn");
            setPhysical("micGateOn",raw && raw->load()>.5f?0.f:1.f);
            refresh();
        };
        addAndMakeVisible(gateEnabled);
        gateAdvanced.setButtonText("MORE");
        gateAdvanced.setTooltip("Show attack, hold and hysteresis without changing their stored values");
        gateAdvanced.onClick=[this]{
            advancedGate=!advancedGate;
            updateGateVisibility();
            if(getWidth()>=312 && getHeight()>=308)resized();
            repaint();
        };
        addAndMakeVisible(gateAdvanced);
        juce::Slider* gateKnobs[]{&gateThreshold,&gateAttack,&gateHold,
                                  &gateRelease,&gateHysteresis,&gateDepth};
        constexpr double gateDefaults[]{-45.,4.,55.,160.,3.,1.};
        constexpr const char* gateHints[]{
            "Below this dBFS level the mic gate starts to close",
            "How quickly the gate opens for a voice",
            "Minimum open time to preserve quiet syllables",
            "How smoothly the gate closes after a phrase",
            "Extra dB below threshold needed before closing",
            "0% leaves audio dry; 100% applies full closed-state reduction"
        };
        for(int i=0;i<6;++i) {
            auto* slider=gateKnobs[i];
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,78,20);
            slider->setDoubleClickReturnValue(true,gateDefaults[i]);
            slider->setTooltip(gateHints[i]);
            addAndMakeVisible(*slider);
        }
        gateThresholdLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,"micGateThreshold",gateThreshold);
        gateReleaseLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,"micGateRelease",gateRelease);
        gateHoldLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,"micGateHold",gateHold);
        gateAttackLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,"micGateAttackMs",gateAttack);
        gateHysteresisLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,"micGateHysteresisDb",gateHysteresis);
        gateDepthLink=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,"micGateDepth",gateDepth);
        gateThreshold.textFromValueFunction=[](double v){return juce::String(v,1)+" dBFS";};
        gateRelease.textFromValueFunction=[](double v){return juce::String(v,0)+" ms";};
        gateHold.textFromValueFunction=[](double v){return juce::String(v,0)+" ms";};
        gateAttack.textFromValueFunction=[](double v){return juce::String(v,1)+" ms";};
        gateHysteresis.textFromValueFunction=[](double v){return juce::String(v,1)+" dB";};
        gateDepth.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100.))+"%";};
        gateDepth.valueFromTextFunction=[](const juce::String& s){return juce::jlimit(0.,1.,s.getDoubleValue()/100.);};
        for(auto* slider:{&gateThreshold,&gateAttack,&gateHold,&gateRelease,
                          &gateHysteresis,&gateDepth})slider->updateText();
        enabled.setButtonText("OFF");
        enabled.onClick=[this] {
            const bool next=!on();
            setPhysical("micPreOn",next?1.f:0.f);
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
            refresh();input.grabKeyboardFocus();
        };
        sourceMenu.onDismiss=[this]{input.grabKeyboardFocus();};
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
        showPage(false);
        setVisible(false);
    }
    std::function<void()> onClose;
    void showPage(bool gate) {
        gatePage=gate;
        if(gate)sourceMenu.setVisible(false);
        for(auto* part:{&input,&enabled})part->setVisible(!gate);
        for(auto& simulation:simulations)simulation.setVisible(!gate);
        amount.setVisible(!gate);trim.setVisible(!gate);
        gateEnabled.setVisible(gate);
        gateAdvanced.setVisible(gate);
        updateGateVisibility();
        // Constructor calls showPage() before JUCE assigns panel bounds.
        // Never set negative slider widths during zero-size startup.
        if(getWidth()>=312 && getHeight()>=308)resized();
        refresh();
    }
    void updateGateVisibility() {
        gateThreshold.setVisible(gatePage);
        gateRelease.setVisible(gatePage);
        gateDepth.setVisible(gatePage && advancedGate);
        gateAttack.setVisible(gatePage && advancedGate);
        gateHold.setVisible(gatePage && advancedGate);
        gateHysteresis.setVisible(gatePage && advancedGate);
        gateAdvanced.setButtonText(advancedGate?"LESS":"MORE");
    }
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
                const int w=juce::jlimit(312,juce::jmin(440,parent->getWidth()-12),
                    resizeStart.getWidth()+e.getDistanceFromDragStartX());
                const int h=juce::jlimit(308,juce::jmin(400,parent->getHeight()-12),
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
    // The rack MIC indicator reflects EITHER independent front-end processor.
    // Keep on() specific to mic modeling so its ON button never toggles the gate.
    bool anyActive() const {
        const auto* gate=processor.state.getRawParameterValue("micGateOn");
        return on() || (gate && gate->load()>.5f);
    }
    void refreshIfChanged() {
        const auto* in=processor.state.getRawParameterValue("micPreInput");
        const auto* out=processor.state.getRawParameterValue("micPreTarget");
        const int inputIndex=in?juce::jlimit(0,3,(int)in->load()):0;
        const int targetIndex=out?juce::jlimit(0,3,(int)out->load()):0;
        const bool enabledNow=on();
        const auto* gate=processor.state.getRawParameterValue("micGateOn");
        const bool gateNow=gate && gate->load()>.5f;
        if(inputIndex!=lastInput || targetIndex!=lastTarget ||
           enabledNow!=lastEnabled || gateNow!=lastGateEnabled)
            refresh();
    }
    void refresh() {
        Theme::selectFor(processor.state);
        const auto* in=processor.state.getRawParameterValue("micPreInput");
        const auto* out=processor.state.getRawParameterValue("micPreTarget");
        const int source=in?juce::jlimit(0,3,(int)in->load()):0;
        const int model=out?juce::jlimit(0,3,(int)out->load()):0;
        if(sourceMenu.isVisible() && source!=lastInput)sourceMenu.setSelected(source);
        lastInput=source;lastTarget=model;lastEnabled=on();
        const auto* gateOn=processor.state.getRawParameterValue("micGateOn");
        lastGateEnabled=gateOn && gateOn->load()>.5f;
        gateEnabled.setButtonText(lastGateEnabled?"ON":"OFF");
        gateEnabled.setColour(juce::TextButton::buttonColourId,
                              lastGateEnabled?Theme::green.withAlpha(.24f):Theme::panelAlt);
        gateEnabled.setColour(juce::TextButton::textColourOffId,
                              lastGateEnabled?Theme::green:Theme::white);
        gateAdvanced.setColour(juce::TextButton::buttonColourId,
            advancedGate?Theme::green.withAlpha(.16f):Theme::panelAlt);
        gateAdvanced.setColour(juce::TextButton::textColourOffId,
            advancedGate?Theme::green:Theme::white);
        for(auto* tab:{&modelTab,&gateTab}){
            const bool selected=(tab==&gateTab)==gatePage;
            tab->setColour(juce::TextButton::buttonColourId,
                           selected?Theme::green.withAlpha(.23f):Theme::panelAlt);
            tab->setColour(juce::TextButton::textColourOffId,
                           selected?Theme::green:Theme::white);
        }
        const juce::StringArray sources{"NEUTRAL","CONDENSER","DYNAMIC","RIBBON"};
        input.setButtonText(sources[source]+"  ▼");
        input.setTooltip("Select the actual input microphone type. The tonal profile is generic.");
        enabled.setTooltip(on()?"Bypass microphone modeling":"Enable microphone modeling");
        enabled.setButtonText(on()?"ON":"OFF");
        enabled.setColour(juce::TextButton::buttonColourId,
                          on()?Theme::green.withAlpha(.24f):Theme::panelAlt);
        enabled.setColour(juce::TextButton::textColourOffId,
                          on()?Theme::green:Theme::muted);
        close.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        close.setColour(juce::TextButton::textColourOffId,Theme::white);
        input.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        input.setColour(juce::TextButton::textColourOffId,Theme::green);
        sourceMenu.repaint();
        for(int i=0;i<4;++i) {
            auto& b=simulations[(size_t)i];
            b.setProfile(i,i==model);
            b.setColour(juce::TextButton::buttonColourId,
                        i==model?Theme::green.withAlpha(.19f):Theme::panelAlt);
            b.setColour(juce::TextButton::textColourOffId,
                        i==model?Theme::green:Theme::white);
        }
        for(auto* slider:{&amount,&trim,&gateThreshold,&gateAttack,&gateHold,
                          &gateRelease,&gateHysteresis,&gateDepth}) {
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
        // Compact 324x320 default; source/model remains movable and resizable.
        // All advanced controls retain their original APVTS attachments.
        close.setBounds(w-32,7,23,23);
        enabled.setBounds(w-91,7,51,23);
        modelTab.setBounds(12,38,(w-28)/2,26);
        gateTab.setBounds(16+(w-28)/2,38,(w-28)/2,26);
        input.setBounds(12,90,w-24,27);
        sourceMenu.setBounds(12,120,w-24,115);
        const int cardWidth=(w-36)/2;
        for(int i=0;i<4;++i)
            simulations[(size_t)i].setBounds(12+(i%2)*(cardWidth+12),
                                             145+(i/2)*44,cardWidth,40);
        amount.setBounds(77,235,w-89,25);
        trim.setBounds(77,263,w-89,25);
        gateEnabled.setBounds(w-79,88,62,27);
        gateAdvanced.setBounds(12,88,82,27);
        if(advancedGate) {
            juce::Slider* controls[]{&gateThreshold,&gateAttack,&gateHold,
                                    &gateRelease,&gateHysteresis,&gateDepth};
            for(int i=0;i<6;++i)
                controls[i]->setBounds(102,129+27*i,w-114,24);
        } else {
            gateThreshold.setBounds(102,149,w-114,27);
            gateRelease.setBounds(102,198,w-114,27);
            for(auto* control:{&gateAttack,&gateHold,&gateHysteresis,&gateDepth})
                control->setBounds(0,0,0,0);
        }
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        Theme::panelBox(g,getLocalBounds().toFloat(),10.f);
        g.setColour(Theme::green.withAlpha(.62f));
        g.fillRoundedRectangle({13.f,13.f,3.f,17.f},1.f);
        Theme::text(g,"MIC MODELING",{22,7,getWidth()-135,23},
                    13.4f,Theme::white,true);
        // The title bar itself is draggable; avoid redundant caption text
        // colliding with the source label in this compact layout.
        if(gatePage){
            if(advancedGate) {
                constexpr const char* labels[]{"THRESHOLD","ATTACK","HOLD",
                                               "RELEASE","HYSTERESIS","DEPTH"};
                for(int i=0;i<6;++i)
                    Theme::fittedText(g,labels[i],{14,129+27*i,84,23},
                        10.f,Theme::white,true);
            } else {
                Theme::text(g,"THRESHOLD",{14,150,87,22},10.5f,
                            Theme::white,true);
                Theme::text(g,"RELEASE",{14,199,87,22},10.5f,
                            Theme::white,true);
            }
            Theme::fittedText(g,"ADVANCED SETTINGS PRESERVED",
                {13,getHeight()-17,getWidth()-26,13},8.5f,Theme::muted);
        } else {
        Theme::text(g,"INPUT MIC",{13,75,100,14},10.3f,Theme::muted,true);
        const juce::StringArray descriptions{
            "FLAT","WARM","FOCUS","AIR"};
        const auto* rawTarget=processor.state.getRawParameterValue("micPreTarget");
        const int target=rawTarget?juce::jlimit(0,3,(int)rawTarget->load()):0;
        Theme::text(g,"SIMULATE",{13,125,83,16},10.3f,
                    Theme::muted,true);
        Theme::fittedText(g,descriptions[target],
                          {getWidth()-117,125,100,16},10.f,Theme::green,true,
                          juce::Justification::centredRight);
        g.setColour(Theme::stroke.withAlpha(.75f));
        g.drawHorizontalLine(232,13.f,(float)getWidth()-13.f);
        Theme::text(g,"AMOUNT",{13,238,62,18},10.6f,Theme::white,true);
        Theme::text(g,"TRIM",{13,266,60,18},10.6f,Theme::white,true);
        Theme::fittedText(g,"TONAL PROFILE / ZERO ADDED LATENCY",
            {13,getHeight()-16,getWidth()-36,12},8.1f,Theme::muted,false);
        }
        // Resize handle, intentionally inside the panel bounds.
        g.setColour(Theme::green.withAlpha(.7f));
        const float right=(float)getWidth()-5.f,bottom=(float)getHeight()-5.f;
        for(int k=0;k<3;++k)
            g.drawLine(right-9.f+4.f*k,bottom,right,bottom-9.f+4.f*k,1.f);
        // Selected model buttons provide the state. No extra decorative
        // microphones are painted behind the compact interactive controls.
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::escapeKey){
            if(sourceMenu.isVisible()){sourceMenu.setVisible(false);input.grabKeyboardFocus();}
            else close.onClick();
            return true;
        }
        return false;
    }
    bool testCompactGeometryForTest() {
        const auto oldBounds=getBounds();
        const bool oldGate=gatePage,oldAdvanced=advancedGate;
        bool okay=true;
        constexpr int sizes[3][2]{{312,308},{324,320},{440,400}};
        for(const auto& dims:sizes){
            setSize(dims[0],dims[1]);
            showPage(false);
            const auto area=getLocalBounds();
            for(const auto* component:{static_cast<const juce::Component*>(&input),
                static_cast<const juce::Component*>(&amount),
                static_cast<const juce::Component*>(&trim),
                static_cast<const juce::Component*>(&modelTab),
                static_cast<const juce::Component*>(&gateTab),
                static_cast<const juce::Component*>(&close)})
                okay=okay && area.contains(component->getBounds());
            for(size_t i=0;i<simulations.size();++i){
                const auto rect=simulations[i].getBounds();
                okay=okay && area.contains(rect) && rect.getHeight()==40 &&
                    !rect.intersects(trim.getBounds()) && !rect.intersects(amount.getBounds());
                for(size_t j=i+1;j<simulations.size();++j)
                    okay=okay && !rect.intersects(simulations[j].getBounds());
            }
            okay=okay && !amount.getBounds().intersects(trim.getBounds()) &&
                trim.getBottom()<getHeight()-16;
            showPage(true);
            advancedGate=false;updateGateVisibility();resized();
            okay=okay && gateThreshold.isVisible() &&
                gateRelease.isVisible() && !gateDepth.isVisible() &&
                !gateAttack.isVisible() && !gateHold.isVisible() &&
                !gateHysteresis.isVisible() &&
                !gateThreshold.getBounds().intersects(gateRelease.getBounds());
            advancedGate=true;updateGateVisibility();resized();
            const juce::Slider* advanced[]{
                &gateThreshold,&gateAttack,&gateHold,
                &gateRelease,&gateHysteresis,&gateDepth};
            for(int i=0;i<6;++i){
                const auto rect=advanced[i]->getBounds();
                okay=okay && advanced[i]->isVisible() &&
                    area.contains(rect) && rect.getBottom()<getHeight()-16;
                for(int j=i+1;j<6;++j)
                    okay=okay && !rect.intersects(advanced[j]->getBounds());
            }
        }
        advancedGate=oldAdvanced;
        setBounds(oldBounds);
        showPage(oldGate);
        return okay;
    }
    bool testNoiseGateAndBypass() {
        const auto* gateParameter=processor.state.getRawParameterValue("micGateOn");
        const auto* threshold=processor.state.getRawParameterValue("micGateThreshold");
        if(!gateParameter || !threshold)return false;
        const auto* model=processor.state.getRawParameterValue("micPreOn");
        if(!model)return false;
        const float priorModel=model->load();
        const float priorEnabled=gateParameter->load(),priorThreshold=threshold->load();
        showPage(true);
        setPhysical("micPreOn",0.f);
        setPhysical("micGateOn",0.f);
        gateEnabled.onClick();
        const bool switched=gatePage && lastGateEnabled && gateEnabled.isVisible()
            && !on() && anyActive();
        gateThreshold.setValue(-54.f,juce::sendNotificationSync);
        const bool mapped=std::abs(threshold->load()+54.f)<.05f;
        const bool simple=gatePage && gateAdvanced.isVisible() &&
            gateAdvanced.getButtonText()=="MORE"
            && gateThreshold.isVisible() && gateRelease.isVisible()
            && !gateDepth.isVisible() && !gateAttack.isVisible()
            && !gateHold.isVisible() && !gateHysteresis.isVisible();
        gateAdvanced.onClick();
        const bool expanded=advancedGate && gateAttack.isVisible()
            && gateHold.isVisible() && gateHysteresis.isVisible()
            && gateThreshold.isVisible() && gateRelease.isVisible()
            && gateDepth.isVisible();
        const juce::Slider* sliders[]{&gateThreshold,&gateAttack,&gateHold,
                                      &gateRelease,&gateHysteresis,&gateDepth};
        bool noOverlaps=getHeight()>=308 && getWidth()>=312;
        for(int i=0;i<6;++i) {
            const auto bounds=sliders[i]->getBounds();
            noOverlaps=noOverlaps && getLocalBounds().contains(bounds) &&
                bounds.getBottom()<=getHeight()-12 && bounds.getX()>=100 &&
                bounds.getWidth()>=175 &&
                !bounds.intersects(gateEnabled.getBounds()) &&
                !bounds.intersects(gateAdvanced.getBounds());
            for(int j=i+1;j<6;++j)
                noOverlaps=noOverlaps && !bounds.intersects(sliders[j]->getBounds());
        }
        gateAdvanced.onClick();
        setPhysical("micGateOn",priorEnabled);
        setPhysical("micGateThreshold",priorThreshold);
        setPhysical("micPreOn",priorModel);
        showPage(false);
        return switched && mapped && simple && expanded && noOverlaps &&
               !gatePage && modelTab.isVisible();
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
        return changed&&opened&&selected&&sourceMenu.testHitTargetsForTest()
            &&sourceMenu.testFocusForTest();
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
    juce::TextButton close,enabled,modelTab,gateTab,gateEnabled,gateAdvanced;
    juce::TextButton input;
    SourceMenu sourceMenu;
    std::array<ModelCard,4> simulations;
    juce::Slider amount,trim,gateThreshold,gateAttack,gateHold,gateRelease,gateHysteresis,gateDepth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountLink,trimLink;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateThresholdLink,gateReleaseLink,gateHoldLink,gateAttackLink,gateHysteresisLink,gateDepthLink;
    int lastInput=-1,lastTarget=-1;
    bool lastEnabled=false,lastGateEnabled=false,gatePage=false,advancedGate=false;
    bool movedByUser=false,dragHeader=false,resizing=false;
    juce::Point<int> dragOrigin{};
    juce::Rectangle<int> resizeStart{};
};
