#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/ChainPanel.h"
#include "ui/SlotEditor.h"
#include "ui/PresetBrowser.h"
#include <SkellCore/UI.h>
#include <BinaryData.h>
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
    : outer(owner),processor(p),chain(p),slot(p),browser(p),skin(skellcore::ui::Theme::neon()) {
        setLookAndFeel(&skin);
        setSize(Theme::width,Theme::height);
        setWantsKeyboardFocus(true);
        bg=juce::ImageCache::getFromMemory(BinaryData::bg_panel_png,BinaryData::bg_panel_pngSize);
        logo=juce::ImageCache::getFromMemory(BinaryData::logo_skull_png,BinaryData::logo_skull_pngSize);
        addAndMakeVisible(chain);addAndMakeVisible(slot);
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
            mode[(size_t)i].onClick=[this,i]{browser.setVisible(false);setParam(processor,"mode",(float)(i==2?3:i));refresh();};
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
        sizeSelect.setSelectedId(1,juce::dontSendNotification);
        sizeSelect.onChange=[this]{
            const int index=sizeSelect.getSelectedId()-1;
            constexpr float scales[]{.70f,.85f,1.f,1.25f};
            if(index>=0&&index<4)outer.setSize(juce::roundToInt(Theme::width*scales[index]),juce::roundToInt(Theme::height*scales[index]));
        };
        addAndMakeVisible(sizeSelect);
        browser.onLoaded=[this]{slot.select(0);chain.setSelectedSlot(0);refresh();};
        browser.onClose=[this]{preset.repaint();};
        addAndMakeVisible(browser);browser.setVisible(false);
        addMouseListener(this,true); // Outside clicks on child panels dismiss the overlay.
        resized();refresh();startTimerHz(30);
    }
    ~Canvas()override {stopTimer();setLookAndFeel(nullptr);}
    void style(juce::TextButton& b){
        b.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        b.setColour(juce::TextButton::textColourOffId,Theme::white);
        b.setColour(juce::TextButton::buttonOnColourId,Theme::green.withAlpha(.22f));
        b.setColour(juce::TextButton::textColourOnId,Theme::white);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(Theme::bg);
        if(bg.isValid())g.drawImage(bg,getLocalBounds().toFloat());
        g.setColour(juce::Colour(0xf10a1220));g.fillRect(0,0,Theme::width,76);
        g.setColour(Theme::stroke);g.drawHorizontalLine(75,0.f,1168.f);
        if(logo.isValid())g.drawImage(logo,juce::Rectangle<float>(16.f,15.f,52.f,52.f),juce::RectanglePlacement::centred);
        Theme::text(g,"SKELL-PRESET",{73,19,235,35},26,Theme::white,true);
        Theme::text(g,"ONE PLUGIN. EVERY SOUND.",{75,54,230,16},10,Theme::muted,true);
        Theme::text(g,processor.state.state.getProperty("presetBank","Main Vocals").toString().toUpperCase(),{356,12,332,14},10,Theme::muted,true);
        g.setColour(Theme::stroke);g.drawHorizontalLine(633,0.f,1168.f);
        g.setColour(Theme::panel);g.fillRect(0,640,Theme::width,48);
        Theme::text(g,"INPUT",{22,652,68,23},12,Theme::muted);
        Theme::text(g,"MIX",{398,652,68,23},12,Theme::muted);
        Theme::text(g,"OUTPUT",{773,652,75,23},12,Theme::muted);
        for(int ix=0;ix<12;ix++){
            g.setColour(ix<12*juce::jlimit(0.f,1.f,processor.getInputPeak())?Theme::green:Theme::stroke);
            g.fillRect(310+ix*6,657,4,10);
            g.setColour(ix<12*juce::jlimit(0.f,1.f,processor.getOutputPeak())?Theme::green:Theme::stroke);
            g.fillRect(1070+ix*6,657,4,10);
        }
        const int current=(int)processor.state.getRawParameterValue("mode")->load();
        if(current==0){
            Theme::panelBox(g,{14,90,1140.f,507.f});
            Theme::text(g,"SIMPLE / VOCAL FINISHING",{55,110,510,24},13,Theme::green,true);
            Theme::text(g,processor.presetName(),{75,165,990,73},49,Theme::white,true);
            Theme::text(g,"FOUR MACROS. ONE SOUND.",{79,249,500,24},14,Theme::muted,true);
            Theme::text(g,"Tone, punch, space and grit use the same processing chain as Advanced mode.",{79,278,900,24},13,Theme::muted);
        }
        if(current==3){
            Theme::panelBox(g,{14,90,1140.f,507.f});
            Theme::text(g,"SIGNAL BYPASSED",{63,143,980,70},42,Theme::muted,true);
            Theme::text(g,"Click-free bypass preserves the latency-aligned dry signal.",
                        {68,222,1010,50},16,Theme::muted);
        }
        const juce::StringArray m{"TONE","PUNCH","SPACE","GRIT"};
        for(int k=0;k<4;k++){
            const int x=current==0?96+k*254:260+k*222;
            Theme::text(g,m[k],{x,current==0?363:611,current==0?200:80,18},11,Theme::muted,true);
        }
        Theme::text(g,"UI SIZE",{1057,637,80,10},8,Theme::muted);
    }
    void resized() override {
        chain.setBounds(13,90,216,535);
        slot.setBounds(247,90,906,519);
        preset.setBounds(356,27,332,39);
        match.setBounds(696,27,87,37);
        for(int k=0;k<3;k++)mode[(size_t)k].setBounds(796+k*119,27,115,37);
        const int m=(int)processor.state.getRawParameterValue("mode")->load();
        for(int i=0;i<4;i++)macro[(size_t)i].setBounds(m==0?96+i*254:333+i*222,m==0?399:614,m==0?192:120,m==0?49:19);
        input.setBounds(80,648,228,34);mix.setBounds(433,648,241,34);output.setBounds(845,648,220,34);
        sizeSelect.setBounds(1094,650,69,26);
        browser.setBounds(445,77,650,538);
    }
    void refresh(){
        int m=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        chain.setVisible(m==1 || m==2);slot.setVisible(m==1 || m==2);
        preset.setButtonText(processor.presetName().toUpperCase()+"  v");
        for(int i=0;i<3;i++){
            const int mapped=i==2?3:i;
            mode[(size_t)i].setColour(juce::TextButton::buttonColourId,
                mapped==m || (m==2 && i==1)?(i==2?Theme::muted.withAlpha(.28f):Theme::green.withAlpha(.20f)):Theme::panelAlt);
        }
        resized();repaint();
    }
    void timerCallback() override{
        const auto n=processor.presetName().toUpperCase()+"  v";
        if(preset.getButtonText()!=n)preset.setButtonText(n);
        const int m=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        if(slot.isVisible()!=(m==1 || m==2))refresh();
        // Repaint only active signal panels and the two tiny meter strips;
        // a full-frame 30 Hz redraw was costly on integrated GPUs.
        if(m==1){chain.repaint();slot.repaint();}
        repaint(juce::Rectangle<int>(307,651,88,26));
        repaint(juce::Rectangle<int>(1068,651,100,26));
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
    juce::Image bg,logo;
    ChainPanel chain;SlotEditor slot;PresetBrowser browser;
    skellcore::ui::LookAndFeel skin;
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
    setSize(818,482);
}
SkellPresetAudioProcessorEditor::~SkellPresetAudioProcessorEditor(){canvas.reset();}
void SkellPresetAudioProcessorEditor::resized(){
    const float scale=(float)getWidth()/Theme::width;
    canvas->setTransform(juce::AffineTransform::scale(scale));
    canvas->setBounds(0,0,Theme::width,Theme::height);
}

juce::AudioProcessorEditor* SkellPresetAudioProcessor::createEditor(){return new SkellPresetAudioProcessorEditor(*this);}
