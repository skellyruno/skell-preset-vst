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
        themeButton.onClick=[this] {setTheme((selectedTheme+1)%3);};
        help.setButtonText("?");
        help.setTooltip("Replay the Skell-Preset guided tour");
        style(help);addAndMakeVisible(help);
        help.onClick=[this]{outer.showGuidedTour(false);};
        addAndMakeVisible(themeButton);
        previousMode=juce::jlimit(0,1,(int)processor.state.state.getProperty("uiPreviousMode",1));
        rackOnRight=(bool)processor.state.state.getProperty("uiRackOnRight",false);
        chain.setDockedRight(rackOnRight);
        chain.onDockRequest=[this](bool right){dockRack(right);};

        chain.onSelection=[this](int s){slot.select(s);chain.setSelectedSlot(s);};

        preset.setButtonText(p.presetName().toUpperCase()+"  ▼");
        preset.onClick=[this]{if(browser.isVisible())browser.setVisible(false);else browser.open();};
        style(preset);addAndMakeVisible(preset);
        match.setButtonText("MATCH");style(match);addAndMakeVisible(match);
        match.onClick=[this]{
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"MATCH | DEVELOPMENT STAGE",
                "The deterministic MATCH alignment + level test harness is implemented and passing. "
                "Dry/Wet WAV file capture, optimization, and full-chain reconstruction are not yet wired into this editor. "
                "No match percentage is claimed for real vocal material.","OK");
        };
        // One view-switch control plus one independent bypass toggle.
        for(auto& button:mode){style(button);addAndMakeVisible(button);}
        mode[0].onClick=[this] {
            browser.setVisible(false);
            const int current=(int)processor.state.getRawParameterValue("mode")->load();
            const int view=current==3?previousMode:(current==0?0:1);
            previousMode=view==0?1:0;
            processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
            setParam(processor,"mode",(float)previousMode);
            refresh();
        };
        mode[1].onClick=[this] {
            browser.setVisible(false);
            const int current=juce::jlimit(0,3,
                (int)processor.state.getRawParameterValue("mode")->load());
            if(current==3)
                setParam(processor,"mode",(float)previousMode);
            else {
                previousMode=current==0?0:1;
                processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
                setParam(processor,"mode",3.f);
            }
            refresh();
        };
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
    void syncSizeControl(float scale) {
        constexpr float sizes[]{.70f,.85f,1.f,1.25f};
        int nearest=0;
        for(int i=1;i<4;++i)
            if(std::abs(scale-sizes[i])<std::abs(scale-sizes[nearest]))nearest=i;
        if(sizeSelect.getSelectedId()!=nearest+1)
            sizeSelect.setSelectedId(nearest+1,juce::dontSendNotification);
    }
    std::vector<skellcore::ui::GuidedTourStep> tourSteps() {
        std::vector<skellcore::ui::GuidedTourStep> steps;
        steps.emplace_back(&preset,"PRESET BROWSER",
            "Choose a starting sound from the preset browser. You can always make your own changes.");
        steps.emplace_back(&mode[0],"SIMPLE / ADVANCED",
            "Use the single view button to switch between four macros and the full effect rack.");
        if(chain.isVisible() && slot.isVisible()) {
            steps.emplace_back(&chain,"YOUR EFFECT CHAIN",
                "Select an effect slot or use its add control to build your processing chain.");
            steps.emplace_back(&slot,"EDIT A SLOT",
                "Adjust the selected effect here. Rack slots keep their own controls and order.");
        }
        steps.emplace_back(&mix,"BLEND YOUR SOUND",
            "Use Mix to balance the processed chain against your original audio.");
        steps.emplace_back(&sizeSelect,"INTERFACE SIZE",
            "Resize the interface here. Click the skull to cycle themes or ? to replay this tour.");
        return steps;
    }
    skellcore::ui::GuidedTourPalette tourPalette() const {
        return {Theme::green,Theme::panel,Theme::white,Theme::muted,
                juce::Colour(0xd9000000)};
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
        for(auto* b:{&preset,&match,&mode[0],&mode[1],&help})style(*b);
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
    void dockRack(bool right) {
        if(rackOnRight==right)return;
        rackOnRight=right;
        chain.setDockedRight(right);
        processor.state.state.setProperty("uiRackOnRight",right,nullptr);
        resized();repaint();
    }
    void dockRackForScreenshot(bool right) {dockRack(right);refresh();}
    void configureForScreenshot(int variant,int workspaceMode,bool showBrowser) {
        dockRack(false);
        variant=Theme::clampVariant(variant);
        processor.state.state.setProperty("uiTheme",variant,nullptr);
        setTheme(variant,false);
        if(workspaceMode==3){
            previousMode=1;
            processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
        }
        setParam(processor,"mode",(float)(workspaceMode==3?3:juce::jlimit(0,1,workspaceMode)));
        refresh();
        if(showBrowser)browser.open();
        else browser.setVisible(false);
        repaint();
    }
    void openModulePickerForScreenshot() {
        slot.openModulePickerForScreenshot();
    }
    void selectRackSlotForScreenshot(int physicalSlot,int scrollOffset) {
        chain.setScreenshotScrollOffset(scrollOffset);
        chain.setSelectedSlot(physicalSlot);
        slot.select(physicalSlot);
        refresh();
    }
    juce::StringArray getScreenshotLayoutIssues() const {
        juce::StringArray issues;
        const auto canvasBounds=juce::Rectangle<int>(0,0,Theme::width,Theme::height);
        const std::array<const juce::Component*,12> all{{&themeButton,&preset,&match,
            &mode[0],&mode[1],&help,&chain,&slot,&input,&mix,&output,&sizeSelect}};
        for(const auto* child:all){
            if(child->getWidth()<=0 || child->getHeight()<=0 || !canvasBounds.contains(child->getBounds()))
                issues.add("Control outside canvas bounds: " + child->getName());
        }
        const std::array<const juce::Component*,4> top{{&preset,&match,&mode[0],&mode[1]}};
        for(size_t i=0;i<top.size();++i)
            for(size_t j=i+1;j<top.size();++j)
                if(top[i]->getBounds().intersects(top[j]->getBounds()))
                    issues.add("Overlapping top-bar controls " + juce::String((int)i)
                               + " / " + juce::String((int)j));
        if(!canvasBounds.contains(browser.getBounds()))issues.add("Preset browser is clipped");
        if(chain.getBounds().intersects(slot.getBounds()))
            issues.add("Rack overlaps the module editor");
        if(rackOnRight!=(chain.getX()>slot.getX()))
            issues.add("Rack dock state disagrees with displayed position");
        const int currentMode=(int)processor.state.getRawParameterValue("mode")->load();
        if(currentMode==1 || (currentMode==3 && previousMode==1))
            for(const auto& control:macro)
                if(control.getBounds().intersects(chain.getBounds()))
                    issues.add("Rack hides an Advanced-mode macro");
        const int current=(int)processor.state.getRawParameterValue("mode")->load();
        const bool advanced=current==1 || (current==3 && previousMode==1);
        if(chain.isVisible()!=advanced || slot.isVisible()!=advanced)
            issues.add("Mode visibility did not match the requested workspace");
        if(current==3 && !bypassShade.isVisible())issues.add("Bypass overlay missing");
        if(!slot.moduleSelectorMatchesAudio())
            issues.add("Module dropdown differs from the actual selected DSP type");
        return issues;
    }
    juce::StringArray runScreenshotInteractionTests(){
        juce::StringArray issues;
        configureForScreenshot(0,1,false);
        preset.onClick();
        if(!browser.isVisible())issues.add("Preset selector did not open browser");
        preset.onClick();
        if(browser.isVisible())issues.add("Preset selector did not close browser");
        mode[0].onClick();
        if(chain.isVisible())issues.add("Simple mode failed to hide the rack");
        mode[0].onClick();
        if(!chain.isVisible())issues.add("Advanced mode toggle failed");
        mode[1].onClick();
        if(!bypassShade.isVisible())issues.add("Bypass did not enable overlay");
        mode[1].onClick();
        if(bypassShade.isVisible())issues.add("Bypass toggle failed to restore workspace");
        const int beforeTheme=selectedTheme;
        themeButton.onClick();
        if(selectedTheme!=(beforeTheme+1)%3)
            issues.add("Skull click didn't cycle themes immediately");
        setTheme(beforeTheme);
        dockRack(true);
        if(chain.getX()<=slot.getX()
           || !(bool)processor.state.state.getProperty("uiRackOnRight",false))
            issues.add("Rack failed to dock on the right");
        dockRack(false);
        if(chain.getX()>=slot.getX())
            issues.add("Rack failed to return left");
        if(!slot.testModulePickerInteraction())
            issues.add("Module browser did not toggle, filter or load Ring Mod");
        // This drives the same XY mutation code as real mouse dragging:
        // verify the graphical insert is an actual parameter editor.
        processor.loadFactoryPreset(0);
        processor.setSlotType(0,SkellPresetAudioProcessor::Saturator);
        slot.select(0);
        const float oldValue=processor.getSlotParam(0,0);
        slot.testGraphGesture({650.f,220.f});
        if(std::abs(processor.getSlotParam(0,0)-oldValue)<.05f)
            issues.add("Saturator graph drag failed to update host automation");
        // New inserts use the same host-automatable drag gesture.
        processor.setSlotType(0,SkellPresetAudioProcessor::Radio);
        processor.setSlotParam(0,0,.16f);
        slot.select(0);
        slot.testGraphGesture({680.f,220.f});
        if(processor.getSlotParam(0,0)<.50f)
            issues.add("Radio visual graph did not change the actual cutoff");
        processor.setSlotType(0,SkellPresetAudioProcessor::ThreeBandDynamics);
        processor.setSlotParam(0,0,.1f);
        slot.select(0);
        slot.testGraphGesture({680.f,220.f});
        if(processor.getSlotParam(0,0)<.50f)
            issues.add("Three-band graph did not change the actual compression");
        processor.setSlotType(0,SkellPresetAudioProcessor::VoxRackRoom);
        processor.setSlotParam(0,0,.12f);
        slot.select(0);
        slot.testGraphGesture({685.f,220.f});
        if(processor.getSlotParam(0,0)<.5f)
            issues.add("VoxRack Room drag failed to change its room size");
        processor.setSlotType(0,SkellPresetAudioProcessor::VoxRackSmoothComp);
        processor.setSlotParam(0,0,.13f);
        slot.select(0);
        slot.testGraphGesture({685.f,220.f});
        if(processor.getSlotParam(0,0)<.5f)
            issues.add("VoxRack compressor drag failed to change threshold");
        // Extended modules must not change the 23-value legacy VST3
        // choice parameter: old FL Studio automation keeps its normalization.
        for(int type=SkellPresetAudioProcessor::VocalRiderFX;
                type<=SkellPresetAudioProcessor::VocalVibratoFX;++type) {
            processor.setSlotType(0,type);
            slot.select(0);
            const auto* legacy=processor.state.getRawParameterValue("s1_type");
            const auto* extension=processor.state.getRawParameterValue("s1_fxExt");
            if(processor.slotType(0)!=type || !slot.moduleSelectorMatchesAudio()
                || !legacy || (int)legacy->load()!=SkellPresetAudioProcessor::Empty
                || !extension
                || (int)extension->load()!=type-SkellPresetAudioProcessor::RingModFX)
                issues.add("Extended rack module was not mapped to separate host parameter");
        }
        processor.setSlotType(0,SkellPresetAudioProcessor::RingModFX);
        if(auto* extension=processor.state.getRawParameterValue("s1_fxExt"))
            if((int)extension->load()!=0)
                issues.add("Legacy module did not clear extended host parameter");
        processor.loadFactoryPreset(0);
        slot.select(0);
        chain.setSelectedSlot(0);
        return issues;
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
        // One typographic run avoids the clipped hyphen and inconsistent gaps.
        // PRESET stays true neon green across all three colour skins.
        juce::AttributedString brand;
        brand.setJustification(juce::Justification::centredLeft);
        brand.append("SKELL",Theme::white);
        brand.append("PRESET",juce::Colour(0xff39ff3a));
        // AttributedString::setFont styles existing text; calling it before
        // append() left the new runs at JUCE's tiny default font.
        brand.setFont(Theme::font(27.f,true));
        juce::TextLayout wordmark;
        wordmark.createLayout(brand,271.f);
        wordmark.draw(g,juce::Rectangle<float>(75.f,18.f,271.f,38.f));
        Theme::text(g,"ONE PLUGIN. EVERY SOUND.",{76,54,239,17},11,Theme::muted,true);
        Theme::text(g,processor.state.state.getProperty("presetBank","Main Vocals").toString().toUpperCase(),{356,12,332,14},10,Theme::muted,true);
        const double sampleRate=juce::jmax(8000.,processor.preparedAudioRate());
        const double latencyMs=1000.*processor.getLatencySamples()/sampleRate;
        Theme::text(g,"PDC "+juce::String(latencyMs,1)+" ms",
                    {695,12,91,14},9,Theme::muted,true);
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
            Theme::panelBox(g,{14.f,90.f,1140.f,507.f},11.f);
            Theme::text(g,"SIMPLE  /  MACRO MIXER",{52,115,520,25},14,Theme::green,true);
            Theme::text(g,processor.presetName(),{74,164,988,69},47,Theme::white,true);
            Theme::text(g,"YOUR SOUND, WITHOUT THE CLUTTER",{79,248,900,24},14,Theme::muted,true);
            Theme::text(g,"Four macros, one full effect rack. Open Advanced to edit every module.",{79,278,960,24},13,Theme::muted);
            g.setColour(Theme::stroke);g.drawHorizontalLine(320,74.f,1084.f);
            const juce::StringArray labels{"TONE","PUNCH","SPACE","GRIT"};
            const juce::StringArray hints{"COLOR & AIR","TRANSIENT IMPACT","DEPTH & TAILS","EDGE & TEXTURE"};
            for(int k=0;k<4;k++){
                const float x=78.f+254.f*k;
                auto face=juce::Rectangle<float>(x,339.f,220.f,214.f);
                g.setColour(Theme::panelAlt);g.fillRoundedRectangle(face,9.f);
                g.setColour(Theme::stroke);g.drawRoundedRectangle(face,9.f,1.f);
                g.setColour(Theme::green.withAlpha(.78f));g.fillRoundedRectangle(x+18.f,354.f,3.f,21.f,1.5f);
                Theme::text(g,labels[k],{(int)x+30,349,176,30},17,Theme::white,true);
                Theme::text(g,hints[k],{(int)x+20,386,179,18},11,Theme::muted,true);
                const float value=processor.state.getRawParameterValue("macro"+juce::String(k+1))->load();
                Theme::text(g,juce::String(juce::roundToInt(value*100.f))+"%",
                    {(int)x+20,501,175,28},19,Theme::green,true,juce::Justification::centred);
                g.setColour(Theme::stroke.withAlpha(.6f));
                g.drawLine(x+18.f,533.f,x+201.f,533.f,.9f);
            }
        } else {
            const auto macroDeck=juce::Rectangle<float>(rackOnRight?14.f:247.f,588.f,906.f,46.f);
            g.setColour(Theme::panel);g.fillRoundedRectangle(macroDeck,7.f);
            g.setColour(Theme::stroke.withAlpha(.75f));g.drawRoundedRectangle(macroDeck,7.f,1.f);
            const juce::StringArray labels{"TONE","PUNCH","SPACE","GRIT"};
            for(int k=0;k<4;k++){
                const int x=(rackOnRight?30:265)+222*k;
                Theme::text(g,labels[k],{x,590,171,17},12,Theme::muted,true);
            }
        }
    }
    void resized() override {
        themeButton.setBounds(15,12,53,56);
        chain.setBounds(rackOnRight?939:13,90,216,535);
        slot.setBounds(rackOnRight?14:247,90,906,496);
        preset.setBounds(356,27,332,39);
        match.setBounds(696,27,87,37);
        mode[0].setBounds(796,27,176,37);
        mode[1].setBounds(982,27,171,37);
        help.setBounds(rackOnRight?906:1129,604,24,23);
        const int raw=(int)processor.state.getRawParameterValue("mode")->load();
        const int display=raw==3?previousMode:(raw==0?0:1);
        for(int i=0;i<4;i++)
            macro[(size_t)i].setBounds(display==0?96+i*254:
                (rackOnRight?97+i*222:333+i*222),
                display==0?438:607,display==0?192:120,display==0?47:24);
        input.setBounds(80,648,228,34);
        mix.setBounds(433,648,241,34);
        output.setBounds(845,648,172,34);
        sizeSelect.setBounds(1094,648,69,26);
        // Keep the rack unobstructed when the user docks it on the right.
        browser.setBounds(rackOnRight?76:445,77,650,538);
        bypassShade.setBounds(display==0?14:(rackOnRight?14:247),90,
                              display==0?1140:906,display==0?507:496);
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
        preset.setButtonText(processor.presetName().toUpperCase()+"  ▼");
        mode[0].setButtonText(display==0?"GO ADVANCED":"GO SIMPLE");
        mode[0].setTooltip(display==0?"Open full effect chain":"Switch to four macros");
        mode[1].setButtonText(bypass?"RESTORE":"BYPASS");
        mode[1].setColour(juce::TextButton::buttonColourId,
                          bypass?Theme::muted.withAlpha(.32f):Theme::panelAlt);
        mode[0].setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        // UI state must reflect the current host/program state *now*, not at
        // the next 30Hz timer tick. This also prevents stale module names
        // during screenshot capture and fast A/B preset auditioning.
        chain.refreshIfChanged();
        slot.refreshIfChanged();
        resized();repaint();
    }
    void timerCallback() override{
        processor.refreshHostedLatency();
        const int newTheme=Theme::clampVariant((int)processor.state.state.getProperty("uiTheme",0));
        if(newTheme!=selectedTheme){setTheme(newTheme,false);return;}
        syncSizeControl((float)outer.getWidth()/Theme::width);
        const auto n=processor.presetName().toUpperCase()+"  ▼";
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
    bool rackOnRight=false;
    int lastInputMeter=-1,lastOutputMeter=-1;
    juce::TextButton preset,match,help;
    std::array<juce::TextButton,2> mode;
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
    guidedTour=std::make_unique<skellcore::ui::GuidedTourController>(
        *this,"skell-preset-vst",1);
    showGuidedTour(true);
}
SkellPresetAudioProcessorEditor::~SkellPresetAudioProcessorEditor(){
    guidedTour.reset(); // destroy spotlight before its Canvas target controls
    canvas.reset();
}
void SkellPresetAudioProcessorEditor::showGuidedTour(bool firstRun) {
    if(!guidedTour || !canvas)return;
    guidedTour->setPalette(canvas->tourPalette());
    guidedTour->setSteps(canvas->tourSteps());
    if(firstRun)guidedTour->startIfFirstRun();
    else guidedTour->replay();
}
void SkellPresetAudioProcessorEditor::dismissGuidedTourForScreenshot() {
    if(guidedTour)guidedTour->dismiss();
}
bool SkellPresetAudioProcessorEditor::showGuidedTourForScreenshot() {
    showGuidedTour(false);
    return guidedTour && guidedTour->isShowing();
}
void SkellPresetAudioProcessorEditor::resized(){
    const float scale=(float)getWidth()/Theme::width;
    const float previous=(float)processor.state.state.getProperty("uiScale",.70f);
    if(std::abs(previous-scale)>.001f)
        processor.state.state.setProperty("uiScale",scale,nullptr);
    canvas->setTransform(juce::AffineTransform::scale(scale));
    canvas->setBounds(0,0,Theme::width,Theme::height);
    canvas->syncSizeControl(scale);
}

juce::AudioProcessorEditor* SkellPresetAudioProcessor::createEditor(){return new SkellPresetAudioProcessorEditor(*this);}

void SkellPresetAudioProcessorEditor::configureForScreenshot(int theme,int mode,bool browserOpen){
    canvas->configureForScreenshot(theme,mode,browserOpen);
}
juce::StringArray SkellPresetAudioProcessorEditor::getScreenshotLayoutIssues() const {
    return canvas->getScreenshotLayoutIssues();
}
juce::StringArray SkellPresetAudioProcessorEditor::runScreenshotInteractionTests(){
    return canvas->runScreenshotInteractionTests();
}

void SkellPresetAudioProcessorEditor::selectRackSlotForScreenshot(int physicalSlot,int scrollOffset) {
    canvas->selectRackSlotForScreenshot(physicalSlot,scrollOffset);
}

void SkellPresetAudioProcessorEditor::dockRackForScreenshot(bool right) {
    canvas->dockRackForScreenshot(right);
}

void SkellPresetAudioProcessorEditor::openModulePickerForScreenshot() {
    canvas->openModulePickerForScreenshot();
}
