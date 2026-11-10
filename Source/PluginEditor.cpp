#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/ChainPanel.h"
#include "ui/SlotEditor.h"
#include "ui/PresetBrowser.h"
#include "ui/SkullThemeButton.h"
#include <SkellCore/UI.h>
#include <array>
#include <memory>

namespace {
void setParam(SkellPresetAudioProcessor& p,const juce::String& id,float physical) {
    if(auto* v=p.state.getParameter(id))v->setValueNotifyingHost(v->convertTo0to1(physical));
}
}

class SkellPresetAudioProcessorEditor::Canvas final:public juce::Component,private juce::Timer {
public:
    Canvas(SkellPresetAudioProcessorEditor& owner,SkellPresetAudioProcessor& p)
    : outer(owner),processor(p),chain(p),slot(p),browser(p),skin(std::make_unique<skellcore::ui::LookAndFeel>(skellcore::ui::Theme::neon())) {
        Theme::selectFor(processor.state);
        setLookAndFeel(skin.get());
        setSize(Theme::width,Theme::height);
        setWantsKeyboardFocus(true);
        addAndMakeVisible(chain);addAndMakeVisible(slot);
        themeButton.onClick=[this]{
            juce::PopupMenu menu;
            menu.addSectionHeader("SKELL THEMES");
            menu.addItem(1,"Obsidian  /  Neon Green",true,selectedTheme==Theme::Obsidian);
            menu.addItem(2,"Ivory  /  Cream",true,selectedTheme==Theme::Cream);
            menu.addItem(3,"Crimson  /  Red",true,selectedTheme==Theme::Crimson);
            juce::Component::SafePointer<Canvas> safe(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&themeButton),
                [safe](int id){if(safe && id>=1 && id<=3)safe->setTheme(id-1);});
        };
        addAndMakeVisible(themeButton);
        previousMode=juce::jlimit(0,1,(int)processor.state.state.getProperty("uiPreviousMode",1));

        chain.onSelection=[this](int s){slot.select(s);chain.setSelectedSlot(s);};

        preset.setButtonText(p.presetName().toUpperCase()+"   v");
        preset.onClick=[this]{if(browser.isVisible())browser.setVisible(false);else browser.open();};
        style(preset);addAndMakeVisible(preset);
        match.setButtonText("MATCH");style(match);addAndMakeVisible(match);
        match.onClick=[this]{
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"MATCH | DEVELOPMENT STAGE",
                "The deterministic MATCH alignment + level test harness is implemented and passing. "
                "Dry/Wet WAV file capture, optimization, and full-chain reconstruction are not yet wired into this editor. "
                "No match percentage is claimed for real vocal material.","OK");
        };
        for(int i=0;i<3;i++){
            mode[(size_t)i].setButtonText(juce::StringArray{"SIMPLE","ADVANCED","BYPASS"}[i]);
            style(mode[(size_t)i]);addAndMakeVisible(mode[(size_t)i]);
            mode[(size_t)i].onClick=[this,i]{
                browser.setVisible(false);
                const int current=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
                if(i==2){
                    // Bypass is a toggle; returning keeps the previous workspace.
                    if(current==3)setParam(processor,"mode",(float)previousMode);
                    else {
                        previousMode=current==0?0:1;
                        processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
                        setParam(processor,"mode",3.f);
                    }
                } else {
                    previousMode=i;
                    processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
                    setParam(processor,"mode",(float)i);
                }
                refresh();
            };
        }
        for(int k=0;k<4;k++){
            auto& s=macro[(size_t)k];
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            s.setColour(juce::Slider::trackColourId,Theme::green);
            s.setColour(juce::Slider::thumbColourId,Theme::white);
            addAndMakeVisible(s);
            macroAttachment[(size_t)k]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"macro"+juce::String(k+1),s);
        }
        for(auto* s:{&input,&mix,&output}) {
            s->setSliderStyle(juce::Slider::LinearHorizontal);
            s->setTextBoxStyle(juce::Slider::TextBoxRight,false,66,20);
            s->setColour(juce::Slider::trackColourId,Theme::blue);
            s->setColour(juce::Slider::thumbColourId,Theme::white);
            s->setColour(juce::Slider::textBoxTextColourId,Theme::white);
            s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            addAndMakeVisible(s);
        }
        inputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"input",input);
        mixAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"mix",mix);
        outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"output",output);
        sizeSelect.addItemList({"70%","85%","100%","125%"},1);
        const float restored=(float)processor.state.state.getProperty("uiScale",.70f);
        const float available[]{.70f,.85f,1.f,1.25f};
        int nearest=0;
        for(int k=1;k<4;++k)
            if(std::abs(restored-available[k])<std::abs(restored-available[nearest]))nearest=k;
        sizeSelect.setSelectedId(nearest+1,juce::dontSendNotification);
        sizeSelect.onChange=[this]{
            const int index=sizeSelect.getSelectedId()-1;
            constexpr float scales[]{.70f,.85f,1.f,1.25f};
            if(index>=0&&index<4)outer.setSize(juce::roundToInt(Theme::width*scales[index]),juce::roundToInt(Theme::height*scales[index]));
        };
        addAndMakeVisible(sizeSelect);
        browser.onLoaded=[this]{slot.select(0);chain.setSelectedSlot(0);refresh();};
        browser.onClose=[this]{preset.repaint();};
        addAndMakeVisible(bypassShade);bypassShade.setVisible(false);
        addAndMakeVisible(browser);browser.setVisible(false);
        setTheme((int)processor.state.state.getProperty("uiTheme",0),false);
        addMouseListener(this,true); // Outside clicks on child panels dismiss the overlay.
        resized();refresh();startTimerHz(30);
    }
    ~Canvas()override {stopTimer();setLookAndFeel(nullptr);}
    struct BypassShade final:juce::Component {
        void paint(juce::Graphics& g)override {
            g.setColour(Theme::bg.withAlpha(.57f));g.fillRoundedRectangle(getLocalBounds().toFloat(),10.f);
            auto centre=getLocalBounds().getCentre();
            Theme::text(g,"BYPASSED",{centre.x-190,centre.y-38,380,50},34,Theme::muted,true,juce::Justification::centred);
            Theme::text(g,"CLICK BYPASS AGAIN TO RESTORE",{centre.x-220,centre.y+15,440,23},12,Theme::muted,true,juce::Justification::centred);
        }
    };
    void setTheme(int v,bool persist=true){
        selectedTheme=Theme::clampVariant(v);
        if(persist)processor.state.state.setProperty("uiTheme",selectedTheme,nullptr);
        Theme::select(selectedTheme);
        themeButton.setVariant(selectedTheme);
        setLookAndFeel(nullptr);
        skin=std::make_unique<skellcore::ui::LookAndFeel>(skellcore::ui::Theme{
            Theme::bg,Theme::panel,Theme::green,Theme::white,Theme::muted});
        setLookAndFeel(skin.get());
        for(auto* b:{&preset,&match,&mode[0],&mode[1],&mode[2]})style(*b);
        for(auto& s:macro){
            s.setColour(juce::Slider::trackColourId,Theme::green);
            s.setColour(juce::Slider::thumbColourId,Theme::white);
        }
        for(auto* s:{&input,&mix,&output}){
            s->setColour(juce::Slider::trackColourId,Theme::blue);
            s->setColour(juce::Slider::thumbColourId,Theme::white);
            s->setColour(juce::Slider::textBoxTextColourId,Theme::white);
            s->setColour(juce::Slider::textBoxBackgroundColourId,Theme::panelAlt);
            s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        }
        sizeSelect.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
        sizeSelect.setColour(juce::ComboBox::textColourId,Theme::white);
        sizeSelect.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        chain.repaint();slot.applyTheme();browser.applyTheme();
        refresh();
    }
    void style(juce::TextButton& b){
        b.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        b.setColour(juce::TextButton::textColourOffId,Theme::white);
        b.setColour(juce::TextButton::buttonOnColourId,Theme::green.withAlpha(.22f));
        b.setColour(juce::TextButton::textColourOnId,Theme::white);
    }
    void paint(juce::Graphics& g) override {
        Theme::select(selectedTheme);
        g.fillAll(Theme::bg);
        g.setColour(Theme::panel);g.fillRect(0,0,Theme::width,76);
        g.setColour(Theme::stroke);g.drawHorizontalLine(75,0.f,(float)Theme::width);
        Theme::text(g,"SKELL-PRESET",{75,18,247,36},27,Theme::white,true);
        Theme::text(g,"ONE PLUGIN. EVERY SOUND.",{76,54,239,17},10,Theme::muted,true);
        Theme::text(g,processor.state.state.getProperty("presetBank","Main Vocals").toString().toUpperCase(),{356,12,332,14},10,Theme::muted,true);
        g.setColour(Theme::stroke);g.drawHorizontalLine(634,0.f,(float)Theme::width);
        g.setColour(Theme::panel);g.fillRect(0,640,Theme::width,48);
        Theme::text(g,"INPUT",{22,652,68,23},12,Theme::muted);
        Theme::text(g,"MIX",{398,652,68,23},12,Theme::muted);
        Theme::text(g,"OUTPUT",{773,652,75,23},12,Theme::muted);
        for(int ix=0;ix<12;ix++){
            g.setColour(ix<12*juce::jlimit(0.f,1.f,processor.getInputPeak())?Theme::green:Theme::stroke);
            g.fillRect(310+ix*6,657,4,10);
            g.setColour(ix<12*juce::jlimit(0.f,1.f,processor.getOutputPeak())?Theme::green:Theme::stroke);
            g.fillRect(1023+ix*6,657,4,10);
        }
        const int current=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        const int viewMode=current==3?previousMode:(current==0?0:1);
        if(viewMode==0){
            Theme::panelBox(g,{14.f,90.f,1140.f,507.f});
            Theme::text(g,"SIMPLE  /  VOCAL FINISHING",{54,115,520,25},13,Theme::green,true);
            Theme::text(g,processor.presetName(),{74,166,988,73},49,Theme::white,true);
            Theme::text(g,"YOUR SOUND, WITHOUT THE CLUTTER",{79,247,900,24},14,Theme::muted,true);
            Theme::text(g,"Four macros control the same rack heard in Advanced.",{79,277,900,24},13,Theme::muted);
            g.setColour(Theme::stroke);g.drawHorizontalLine(320,74.f,1084.f);
        }
        const juce::StringArray labels{"TONE","PUNCH","SPACE","GRIT"};
        for(int k=0;k<4;k++){
            const int x=viewMode==0?96+k*254:260+k*222;
            Theme::text(g,labels[k],{x,viewMode==0?365:611,viewMode==0?200:80,18},11,
                        current==3?Theme::muted.withAlpha(.5f):Theme::muted,true);
        }
        Theme::text(g,"UI SIZE",{1057,637,80,10},8,Theme::muted);
    }
    void resized() override {
        themeButton.setBounds(15,12,53,56);
        chain.setBounds(13,90,216,535);
        slot.setBounds(247,90,906,519);
        preset.setBounds(356,27,332,39);
        match.setBounds(696,27,87,37);
        for(int k=0;k<3;k++)mode[(size_t)k].setBounds(796+k*119,27,115,37);
        const int raw=(int)processor.state.getRawParameterValue("mode")->load();
        const int display=raw==3?previousMode:(raw==0?0:1);
        for(int i=0;i<4;i++)macro[(size_t)i].setBounds(display==0?96+i*254:333+i*222,display==0?399:614,display==0?192:120,display==0?49:19);
        input.setBounds(80,648,228,34);
        mix.setBounds(433,648,241,34);
        output.setBounds(845,648,172,34);
        sizeSelect.setBounds(1094,648,69,26);
        browser.setBounds(445,77,650,538);
        bypassShade.setBounds(display==0?14:247,90,display==0?1140:906,display==0?507:519);
    }
    void refresh(){
        Theme::select(selectedTheme);
        const int m=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        if(m==0 || m==1) {
            previousMode=m;
            if((int)processor.state.state.getProperty("uiPreviousMode",-1)!=previousMode)
                processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
        }
        const bool bypass=m==3;
        const int display=bypass?previousMode:(m==0?0:1);
        chain.setVisible(display==1);slot.setVisible(display==1);
        chain.setAlpha(bypass?.30f:1.f);slot.setAlpha(bypass?.30f:1.f);
        chain.setEnabled(!bypass);slot.setEnabled(!bypass);
        for(auto& s:macro){s.setEnabled(!bypass);s.setAlpha(bypass?.35f:1.f);}
        for(auto* s:{&input,&mix,&output}){s->setEnabled(!bypass);s->setAlpha(bypass?.35f:1.f);}
        bypassShade.setVisible(bypass);
        if(bypass)bypassShade.toFront(false);
        if(browser.isVisible())browser.toFront(false);
        preset.setButtonText(processor.presetName().toUpperCase()+"  v");
        for(int i=0;i<3;i++){
            const int mapped=i==2?3:i;
            mode[(size_t)i].setColour(juce::TextButton::buttonColourId,
                mapped==m?(bypass?Theme::muted.withAlpha(.32f):Theme::green.withAlpha(.24f)):Theme::panelAlt);
        }
        resized();repaint();
    }
    void timerCallback() override{
        const int newTheme=Theme::clampVariant((int)processor.state.state.getProperty("uiTheme",0));
        if(newTheme!=selectedTheme){setTheme(newTheme,false);return;}
        const float currentScale=(float)outer.getWidth()/Theme::width;
        constexpr float sizes[]{.70f,.85f,1.f,1.25f};
        int nearest=0;
        for(int i=1;i<4;++i)
            if(std::abs(currentScale-sizes[i])<std::abs(currentScale-sizes[nearest]))nearest=i;
        if(sizeSelect.getSelectedId()!=nearest+1)sizeSelect.setSelectedId(nearest+1,juce::dontSendNotification);
        const auto n=processor.presetName().toUpperCase()+"  v";
        if(preset.getButtonText()!=n)preset.setButtonText(n);
        const int m=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        const int display=m==3?previousMode:(m==0?0:1);
        if(slot.isVisible()!=(display==1) || bypassShade.isVisible()!=(m==3))refresh();
        if(display==1 && m!=3){chain.refreshIfChanged();slot.refreshIfChanged();}
        const int inLevel=juce::jlimit(0,12,(int)(processor.getInputPeak()*12.f));
        const int outLevel=juce::jlimit(0,12,(int)(processor.getOutputPeak()*12.f));
        if(inLevel!=lastInputMeter){lastInputMeter=inLevel;repaint(juce::Rectangle<int>(307,651,88,26));}
        if(outLevel!=lastOutputMeter){lastOutputMeter=outLevel;repaint(juce::Rectangle<int>(1021,651,75,26));}
    }
    void mouseDown(const juce::MouseEvent& e)override {
        const auto point=e.getEventRelativeTo(this).getPosition();
        if(browser.isVisible() && !browser.getBounds().contains(point) && !preset.getBounds().contains(point)) {
            browser.setVisible(false);preset.repaint();
        }
    }
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::escapeKey&&browser.isVisible()){browser.setVisible(false);return true;}return false;
    }
private:
    SkellPresetAudioProcessorEditor& outer;
    SkellPresetAudioProcessor& processor;
    ChainPanel chain;SlotEditor slot;PresetBrowser browser;
    SkullThemeButton themeButton;
    BypassShade bypassShade;
    std::unique_ptr<skellcore::ui::LookAndFeel> skin;
    int selectedTheme=Theme::Obsidian,previousMode=1;
    int lastInputMeter=-1,lastOutputMeter=-1;
    juce::TextButton preset,match;
    std::array<juce::TextButton,3> mode;
    std::array<juce::Slider,4> macro;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> macroAttachment;
    juce::Slider input,mix,output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment,mixAttachment,outputAttachment;
    juce::ComboBox sizeSelect;
};

SkellPresetAudioProcessorEditor::SkellPresetAudioProcessorEditor(SkellPresetAudioProcessor& p)
:AudioProcessorEditor(&p),processor(p){
    canvas=std::make_unique<Canvas>(*this,processor);
    addAndMakeVisible(*canvas);
    setResizable(true,true);
    setResizeLimits(818,482,1460,860);
    getConstrainer()->setFixedAspectRatio((double)Theme::width/Theme::height);
    const float restored=juce::jlimit(.70f,1.25f,(float)processor.state.state.getProperty("uiScale",.70f));
    setSize(juce::roundToInt(Theme::width*restored),juce::roundToInt(Theme::height*restored));
}
SkellPresetAudioProcessorEditor::~SkellPresetAudioProcessorEditor(){canvas.reset();}
void SkellPresetAudioProcessorEditor::resized(){
    const float scale=(float)getWidth()/Theme::width;
    const float previous=(float)processor.state.state.getProperty("uiScale",.70f);
    if(std::abs(previous-scale)>.001f)
        processor.state.state.setProperty("uiScale",scale,nullptr);
    canvas->setTransform(juce::AffineTransform::scale(scale));
    canvas->setBounds(0,0,Theme::width,Theme::height);
}

juce::AudioProcessorEditor* SkellPresetAudioProcessor::createEditor(){return new SkellPresetAudioProcessorEditor(*this);}
