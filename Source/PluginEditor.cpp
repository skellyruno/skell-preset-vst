#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/ChainPanel.h"
#include "ui/SlotEditor.h"
#include "ui/PresetBrowser.h"
#include "ui/SkullThemeButton.h"
#include "ui/SkellPowerButton.h"
#include "ui/SkellSettingsButton.h"
#include "ui/SkellActionButton.h"
#include "ui/SkellMacroKnob.h"
#include "chain/AlchemistNativePreset.h"
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
        chain.onReplaceRequest=[this](int s){
            slot.select(s);
            chain.setSelectedSlot(s);
            slot.openModulePicker();
        };

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
        style(viewSwitch);style(powerButton);
        addAndMakeVisible(viewSwitch);addAndMakeVisible(powerButton);
        viewSwitch.onClick=[this] {
            browser.setVisible(false);
            const int current=(int)processor.state.getRawParameterValue("mode")->load();
            const int view=current==3?previousMode:(current==0?0:1);
            previousMode=view==0?1:0;
            processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
            setParam(processor,"mode",(float)previousMode);
            refresh();
        };
        powerButton.onClick=[this] {
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
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            constexpr float pi=juce::MathConstants<float>::pi;
            s.setRotaryParameters(1.25f*pi,2.75f*pi,true);
            s.setDoubleClickReturnValue(true,.5);
            const juce::StringArray macroLabels{"Tone","Punch","Space","Grit"};
            s.setTooltip(macroLabels[k]+"  |  0-100  |  double-click resets to 50");
            s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            s.setColour(juce::Slider::trackColourId,Theme::green);
            s.setColour(juce::Slider::thumbColourId,Theme::white);
            addAndMakeVisible(s);
            macroAttachment[(size_t)k]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                p.state,"macro"+juce::String(k+1),s);
            // APVTS installs its own decimal formatter. Our 0-100 formatter
            // must be applied AFTER binding to retain the host's original
            // parameter normalization while showing integer percentages.
            s.textFromValueFunction=[](double n) {
                return juce::String(juce::roundToInt(juce::jlimit(0.,1.,n)*100.))+"%";
            };
            s.valueFromTextFunction=[](const juce::String& value) {
                return juce::jlimit(0.,1.,value.getDoubleValue()/100.);
            };
            s.updateText();
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
        // Host attachments otherwise show opaque raw decimals such as 0.000
        // and 1.000. Display physical units without changing DSP precision
        // or the existing automated parameter normalization.
        auto decibels=[](double value) {
            return juce::String(juce::roundToInt(value))+" dB";
        };
        auto readDb=[](const juce::String& text) {
            return juce::jlimit(-24.,24.,text.getDoubleValue());
        };
        input.textFromValueFunction=decibels;
        output.textFromValueFunction=decibels;
        input.valueFromTextFunction=readDb;
        output.valueFromTextFunction=readDb;
        mix.textFromValueFunction=[](double value) {
            return juce::String(juce::roundToInt(juce::jlimit(0.,1.,value)*100.))+"%";
        };
        mix.valueFromTextFunction=[](const juce::String& text) {
            const double value=text.getDoubleValue();
            return juce::jlimit(0.,1.,(text.contains("%")||value>1.)?value/100.:value);
        };
        input.updateText();output.updateText();mix.updateText();
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
        sizeSelect.setVisible(false); // Internal compatibility adapter only.
        // Use the recessed, engraved settings switch from Skell Silence.
        settings.setTooltip("Settings  /  Interface Size");
        style(settings);
        addAndMakeVisible(settings);
        settings.onClick=[this] {
            // An actual native rounded-rectangle settings menu, using the
            // SkellCore LookAndFeel; do not add another cramped dropdown.
            juce::PopupMenu menu;
            menu.addSectionHeader("INTERFACE SIZE");
            const juce::StringArray labels{"70%","85%","100%","125%"};
            for(int i=0;i<labels.size();++i)
                menu.addItem(i+1,labels[i],true,sizeSelect.getSelectedId()==i+1);
            menu.addSeparator();
            menu.addSectionHeader("RECORDINGS");
            menu.addItem(100,"Reveal last WAV in folder",lastCaptureFile.existsAsFile());
            menu.showMenuAsync(
                juce::PopupMenu::Options().withTargetComponent(&settings),
                [safe=juce::Component::SafePointer<Canvas>(this)](int choice) {
                    if(safe==nullptr)return;
                    if(choice>=1 && choice<=4)
                        safe->sizeSelect.setSelectedId(choice,juce::sendNotificationSync);
                    else if(choice==100 && safe->lastCaptureFile.existsAsFile())
                        safe->lastCaptureFile.revealToUser();
                });
        };
        record.setButtonText("REC WAV");
        record.setTooltip("Capture processed stereo track audio to a 24-bit WAV. FL Studio owns the microphone and transport.");
        style(record);
        addAndMakeVisible(record);
        record.onClick=[this] {
            if(processor.isCapturing()) {
                processor.stopCapture();
                record.setButtonText("REC WAV");
                if(processor.droppedCaptureSamples()>0)
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                        "Recording finished with gaps",
                        "The recorder dropped "+juce::String((juce::int64)processor.droppedCaptureSamples())+
                        " samples because the storage writer could not keep up.");
                return;
            }
            const auto defaultPath=juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory).getChildFile("SkellPreset-Capture.wav");
            recordChooser=std::make_unique<juce::FileChooser>(
                "Save processed track audio as WAV",defaultPath,"*.wav");
            recordChooser->launchAsync(
                juce::FileBrowserComponent::saveMode |
                juce::FileBrowserComponent::canSelectFiles |
                juce::FileBrowserComponent::warnAboutOverwriting,
                [safe=juce::Component::SafePointer<Canvas>(this)](const juce::FileChooser& chosen) {
                    if(safe==nullptr)return;
                    auto file=chosen.getResult();
                    if(file.getFullPathName().isEmpty())return;
                    if(!file.hasFileExtension(".wav"))file=file.withFileExtension(".wav");
                    const auto result=safe->processor.startCapture(file);
                    if(result.failed())
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Cannot record",result.getErrorMessage());
                    else {
                        safe->lastCaptureFile=file;
                        safe->record.setButtonText("STOP REC");
                    }
                });
        };
        browser.onLoaded=[this]{slot.select(0);chain.setSelectedSlot(0);refresh();};
        browser.onCoverChanged=[this]{repaint();};
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
        steps.emplace_back(&viewSwitch,"SIMPLE / ADVANCED",
            "Use the single view button to switch between four macros and the full effect rack.");
        if(chain.isVisible() && slot.isVisible()) {
            steps.emplace_back(&chain,"YOUR EFFECT CHAIN",
                "Double-click a row to replace its effect. Drag its small ring for wet blend; click the rightmost green dot to bypass.");
            steps.emplace_back(&slot,"EDIT A SLOT",
                "Adjust the selected effect here. Rack slots keep their own independent controls, order and automation.");
        }
        steps.emplace_back(&powerButton,"POWER / BYPASS",
            "Green means your effect rack is active. Click the power icon to bypass without losing any settings.");
        steps.emplace_back(&mix,"BLEND YOUR SOUND",
            "Use Mix to balance the processed chain against your original audio.");
        steps.emplace_back(&settings,"INTERFACE SIZE",
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
            Theme::text(g,"CLICK POWER TO RESTORE",{centre.x-220,centre.y+15,440,23},12,Theme::muted,true,juce::Justification::centred);
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
        for(juce::TextButton* b:std::array<juce::TextButton*,7>{{
            &preset,&match,&viewSwitch,&powerButton,&help,&settings,&record}})
            style(*b);
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
    void openInstalledVST3BrowserForScreenshot() {
        slot.openInstalledVST3BrowserForScreenshot();
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
        const std::array<const juce::Component*,14> all{{&themeButton,&preset,&match,
            &viewSwitch,&powerButton,&help,&chain,&slot,&input,&mix,&output,&sizeSelect,&settings,&record}};
        for(const auto* child:all){
            if(child->getWidth()<=0 || child->getHeight()<=0 || !canvasBounds.contains(child->getBounds()))
                issues.add("Control outside canvas bounds: " + child->getName());
        }
        if(record.getBounds().intersects(output.getBounds()) ||
           record.getBounds().intersects(mix.getBounds()) ||
           record.getBounds().getRight()>=773)
            issues.add("Recorder button overlaps the bottom output label or master slider");
        if(!input.textFromValueFunction || input.textFromValueFunction(0.)!="0 dB"
           || !output.textFromValueFunction || output.textFromValueFunction(0.)!="0 dB"
           || !mix.textFromValueFunction || mix.textFromValueFunction(1.)!="100%")
            issues.add("Master strip displays raw values rather than physical dB/% labels");
        const std::array<const juce::Component*,5> top{{&preset,&match,&viewSwitch,&settings,&powerButton}};
        for(size_t i=0;i<top.size();++i)
            for(size_t j=i+1;j<top.size();++j)
                if(top[i]->getBounds().intersects(top[j]->getBounds()))
                    issues.add("Overlapping top-bar controls " + juce::String((int)i)
                               + " / " + juce::String((int)j));
        if(!canvasBounds.contains(browser.getBounds()))issues.add("Preset browser is clipped");
        if(!settings.isVisible()||sizeSelect.isVisible())issues.add("UI size control not relocated to settings");
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
        if(powerButton.isBypassed()!=(current==3))
            issues.add("Power indicator disagrees with host bypass state");
        if(!advanced){
            for(int i=0;i<4;++i) {
                const auto face=juce::Rectangle<int>(78+i*254,339,220,214);
                if(!face.contains(macro[(size_t)i].getBounds()))
                    issues.add("Simple macro dial escapes its card");
                for(int j=i+1;j<4;++j)
                    if(macro[(size_t)i].getBounds().intersects(macro[(size_t)j].getBounds()))
                        issues.add("Simple macro dials overlap");
            }
        }
        if(!slot.moduleSelectorMatchesAudio())
            issues.add("Module dropdown differs from the actual selected DSP type");
        return issues;
    }
    juce::StringArray runScreenshotInteractionTests(){
        juce::StringArray issues;
        configureForScreenshot(0,1,false);
        // The headless renderer reuses a processor that may have loaded a
        // legacy factory sound for compatibility tests. Select Alchemist
        // explicitly before asserting its editable factory cover.
        processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
        browser.onLoaded();
        preset.onClick();
        if(!browser.isVisible())issues.add("Preset selector did not open browser");
        if(browser.visibleFactoryPresetsForTest()!=1 ||
           !browser.canSetCoverForTest())
            issues.add("Browser must show only Alchemist and allow changing its cover");
        preset.onClick();
        if(browser.isVisible())issues.add("Preset selector did not close browser");
        viewSwitch.onClick();
        if(chain.isVisible())issues.add("Simple mode failed to hide the rack");
        viewSwitch.onClick();
        if(!chain.isVisible())issues.add("Advanced mode toggle failed");
        powerButton.onClick();
        if(!bypassShade.isVisible() || !powerButton.isBypassed())
            issues.add("Power button did not bypass and illuminate OFF state");
        powerButton.onClick();
        if(bypassShade.isVisible() || powerButton.isBypassed())
            issues.add("Power button did not restore the processing ON state");
        // The Simple macro dials must be real automatable controls, not
        // decorative painted knobs. Verify 0-100 labels and both view modes.
        configureForScreenshot(0,0,false);
        for(int i=0;i<4;++i) {
            if(macro[(size_t)i].getSliderStyle()!=
               juce::Slider::RotaryHorizontalVerticalDrag)
                issues.add("Simple macro is not a functional rotary");
            if(!macro[(size_t)i].textFromValueFunction ||
               macro[(size_t)i].getTextFromValue(.371)!="37%")
                issues.add("Simple macro does not show whole-number 0-100");
        }
        powerButton.onClick();
        if(!powerButton.isBypassed() || chain.isVisible())
            issues.add("Simple mode power button changed the view or failed");
        if(powerButton.getButtonText().isNotEmpty() ||
           settings.getX()<=powerButton.getX() ||
           settings.getX()-powerButton.getRight()>8 ||
           settings.getWidth()>37 || powerButton.getWidth()>37)
            issues.add("Header controls must be compact Power then Settings");
        powerButton.onClick();
        configureForScreenshot(0,1,false);
        for(const auto& control:macro)
            if(control.getSliderStyle()!=juce::Slider::LinearHorizontal)
                issues.add("Advanced macro strip was unexpectedly replaced");
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
        processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
        chain.setScreenshotScrollOffset(2);
        if(!chain.canScrollForTest() || chain.scrollPositionForTest()!=2 ||
           !chain.testMouseWheelScroll())
            issues.add("Default rack wheel-down/wheel-up scrolling failed");
        chain.setScreenshotScrollOffset(0);
        if(!chain.testRackControlsAndWet())
            issues.add("Rack wet trim and green bypass dot did not update or restore host parameters");
        if(!chain.triggerReplaceForTest(0) || !slot.modulePickerVisibleForTest())
            issues.add("Rack double-click replacement did not open effect library");
        slot.openModulePicker();
        if(slot.modulePickerVisibleForTest())
            issues.add("Rack double-click effect library could not be closed");
        if(!slot.testModulePickerInteraction())
            issues.add("Module browser did not toggle, filter or load Ring Mod");
        if(!slot.testEightBandGraphAndQ())
            issues.add("8-Band EQ graph did not edit the eighth band and its Q");
        if(!slot.testKnobReadouts())
            issues.add("APVTS overwrote physical Hz/dB/Q knob readouts");
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
        // The new Alchemist module has an independent fourth-generation
        // choice. Earlier published type normalizations must remain exact.
        processor.setSlotType(0,SkellPresetAudioProcessor::AlchemistMatchFinishFX);
        {
            const auto* modern=processor.state.getRawParameterValue("s1_fxExt4");
            const auto* legacy=processor.state.getRawParameterValue("s1_type");
            const auto* a=processor.state.getRawParameterValue("s1_fxExt");
            const auto* b=processor.state.getRawParameterValue("s1_fxExt2");
            const auto* c=processor.state.getRawParameterValue("s1_fxExt3");
            if(processor.slotType(0)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX
               || !modern || (int)modern->load()!=1 || !legacy || (int)legacy->load()!=0
               || !a || (int)a->load()!=0 || !b || (int)b->load()!=0
               || !c || (int)c->load()!=0 || !slot.moduleSelectorMatchesAudio())
                issues.add("Alchemist match finish changed older automation extension IDs");
        }
        processor.setSlotType(0,SkellPresetAudioProcessor::RingModFX);
        if(auto* modern=processor.state.getRawParameterValue("s1_fxExt4"))
            if((int)modern->load()!=0)
                issues.add("Legacy module failed to clear Alchemist extension");
        if(auto* extension=processor.state.getRawParameterValue("s1_fxExt"))
            if((int)extension->load()!=0)
                issues.add("Legacy module did not clear extended host parameter");
        // Latest effect extensions must not renormalize previously-published
        // 23-choice type or the earlier three-choice effect extension.
        for(int type=SkellPresetAudioProcessor::EightBandToneFX;
                type<=SkellPresetAudioProcessor::StudioPitchFormantFX;++type){
            processor.setSlotType(0,type);
            slot.select(0);
            const auto* legacy=processor.state.getRawParameterValue("s1_type");
            const auto* previous=processor.state.getRawParameterValue("s1_fxExt");
            const auto* advanced=processor.state.getRawParameterValue("s1_fxExt2");
            const auto* studio=processor.state.getRawParameterValue("s1_fxExt3");
            const bool isStudio=type==SkellPresetAudioProcessor::StudioPitchFormantFX;
            if(processor.slotType(0)!=type || !slot.moduleSelectorMatchesAudio()
                || !legacy || (int)legacy->load()!=SkellPresetAudioProcessor::Empty
                || !previous || (int)previous->load()!=0 || !advanced || !studio
                || (int)advanced->load()!=(isStudio?0:type-SkellPresetAudioProcessor::VocalVibratoFX)
                || (int)studio->load()!=(isStudio?1:0))
                issues.add("Advanced effect changed legacy slot-type automation mapping");
        }
        // The recording-oriented Live Lead preset has no limiter PDC.
        // Selecting the studio phase vocoder must add exactly 768 samples;
        // removing the slot must return to zero host latency.
        processor.loadFactoryPreset(25);
        processor.setSlotType(8,SkellPresetAudioProcessor::StudioPitchFormantFX);
        processor.setSlotOn(8,true);
        if(processor.getLatencySamples()!=
           skellpreset::dsp::SpectralPitchFormant::latencySamples)
            issues.add("Studio pitch/formant did not report its 768-sample PDC");
        processor.setSlotType(8,SkellPresetAudioProcessor::Empty);
        processor.setSlotOn(8,false);
        if(processor.getLatencySamples()!=0)
            issues.add("Studio pitch/formant removal did not clear host PDC");
        // The 322 source has six *effect classes* but no named 322 preset
        // collection. The new native Studio Six factory chain must actually
        // instantiate all six without deleting the old Simple macro mixer.
        processor.loadFactoryPreset(42);
        const int sixTypes[]{SkellPresetAudioProcessor::EQ,
            SkellPresetAudioProcessor::Compressor,
            SkellPresetAudioProcessor::Saturator,
            SkellPresetAudioProcessor::Delay,
            SkellPresetAudioProcessor::Reverb,
            SkellPresetAudioProcessor::VocalChorusFX};
        const int sixSlots[]{0,1,3,4,5,7};
        for(int i=0;i<6;++i){
            auto* on=processor.state.getRawParameterValue(
                "s"+juce::String(sixSlots[i]+1)+"_on");
            if(processor.slotType(sixSlots[i])!=sixTypes[i] ||
               !on || on->load()<.5f)
                issues.add("Studio Six native effect chain is missing a stage");
        }
        if(processor.getNumPrograms()!=44)
            issues.add("Alchemist factory program did not append at index 43");
        // Critical compatibility: do not replace the original simple macro
        // processor when adding a catalog, recorder or waveform visualization.
        setParam(processor,"mode",0.f);
        refresh();
        if(chain.isVisible() || macro[0].getWidth()<=0)
            issues.add("322 integration broke the original Simple macro mixer");
        processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
        slot.select(0);
        chain.setSelectedSlot(0);
        refresh(); // Restore actual Advanced workspace after Simple-mode QA.
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
        // Selected preset artwork as a full-UI, low-contrast wallpaper.
        // The real modules and labels remain foreground components.
        const auto cover=browser.selectedCover();
        if(cover.isValid()) {
            juce::Graphics::ScopedSaveState art(g);
            g.setOpacity(.16f);
            g.drawImageWithin(cover,0,0,Theme::width,Theme::height,
                juce::RectanglePlacement::fillDestination);
            g.setOpacity(1.f);
            g.setColour(Theme::bg.withAlpha(.57f));
            g.fillRect(0,0,Theme::width,Theme::height);
        }
        g.setColour(Theme::panel);g.fillRect(0,0,Theme::width,76);
        g.setColour(Theme::stroke);g.drawHorizontalLine(75,0.f,(float)Theme::width);
        // One typographic run avoids the clipped hyphen and inconsistent gaps.
        // PRESET stays neon green in the main Obsidian skin.
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
        // The new tagline lives directly under SKELLPRESET, rather than
        // fighting the old waveform strip for the same 14 px.
        Theme::text(g,"ONE RACK. EVERY PRESET.",{76,54,222,15},
                    10.3f,Theme::muted,true);
        const float inputLevel=juce::jlimit(0.f,1.f,processor.getInputPeak());
        const bool recording=processor.isCapturing();
        const auto liveColour=recording?Theme::red:Theme::green;
        g.setColour(liveColour.withAlpha(.23f+.65f*inputLevel));
        g.fillEllipse(304.f,58.f,6.f,6.f);
        Theme::text(g,recording?"REC":"LIVE",{315,53,37,17},9.5f,
                    liveColour,true);
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
            // Expanded live scope in the Simple view: this is actual
            // post-effects output, not a random waveform drawn for decoration.
            Theme::text(g,"LIVE AUDIO",{79,306,110,20},11,
                processor.isCapturing()?Theme::red:Theme::green,true);
            g.setColour(Theme::stroke.withAlpha(.48f));
            g.drawHorizontalLine(317,202.f,1077.f);
            std::array<float,128> waveform{};
            processor.getLiveWaveform(waveform);
            juce::Path simpleScope;
            for(std::size_t n=0;n<waveform.size();++n) {
                const float x=205.f+(float)n*867.f/(waveform.size()-1);
                const float y=317.f-juce::jlimit(-1.f,1.f,waveform[n])*10.f;
                if(n==0)simpleScope.startNewSubPath(x,y);
                else simpleScope.lineTo(x,y);
            }
            g.setColour(processor.isCapturing()?Theme::red:Theme::green.withAlpha(.9f));
            g.strokePath(simpleScope,juce::PathStrokeType(1.7f));
            const juce::StringArray labels{"TONE","PUNCH","SPACE","GRIT"};
            const juce::StringArray hints{"COLOR & AIR","TRANSIENT","DEPTH & TAILS","EDGE & TEXTURE"};
            for(int k=0;k<4;k++){
                const float x=78.f+254.f*k;
                const auto raw=processor.state.getRawParameterValue(
                    "macro"+juce::String(k+1));
                const float amount=juce::jlimit(0.f,1.f,raw?raw->load():.5f);
                const auto face=juce::Rectangle<float>(x,339.f,220.f,214.f);
                g.setGradientFill(juce::ColourGradient(
                    Theme::panelAlt.brighter(.055f),face.getTopLeft(),
                    Theme::panelAlt.darker(.12f),face.getBottomRight(),false));
                g.fillRoundedRectangle(face,9.f);
                g.setColour(Theme::stroke.withAlpha(.91f));
                g.drawRoundedRectangle(face.reduced(.5f),9.f,1.f);
                // One status tick and a hardware dial; all motion below is
                // derived from the actual host-automated macro, not a mock meter.
                g.setColour(Theme::green.withAlpha(.22f+.60f*amount));
                g.fillRoundedRectangle(x+17.f,354.f,3.f,20.f,1.5f);
                Theme::text(g,labels[k],{(int)x+28,349,177,27},
                            16,Theme::white,true);
                Theme::text(g,hints[k],{(int)x+20,377,180,18},
                            10.5f,Theme::muted,true,
                            juce::Justification::centred);
                // Mini hardware scale stays subtle and never competes with
                // the real rotary control drawn by SkellCore LookAndFeel.
                constexpr float pi=juce::MathConstants<float>::pi;
                const float centreX=x+109.f,centreY=454.f,rad=62.f;
                for(int tick=0;tick<=10;++tick){
                    const float a=(1.25f+1.5f*(float)tick/10.f)*pi;
                    const float dx=std::sin(a),dy=-std::cos(a);
                    const float tickInner=rad-1.5f,tickOuter=rad+2.5f;
                    g.setColour(tick<=juce::roundToInt(amount*10.f)
                        ?Theme::green.withAlpha(.20f+.37f*amount)
                        :Theme::muted.withAlpha(.21f));
                    g.drawLine(centreX+dx*tickInner,centreY+dy*tickInner,
                               centreX+dx*tickOuter,centreY+dy*tickOuter,1.4f);
                }
                Theme::text(g,juce::String(juce::roundToInt(amount*100.f)),
                    {(int)x+55,510,110,29},21,Theme::green,true,
                    juce::Justification::centred);
                Theme::text(g,"/ 100",{(int)x+149,516,58,17},
                    10,Theme::muted,false);
                const auto meter=juce::Rectangle<float>(x+19.f,543.f,182.f,2.f);
                g.setColour(Theme::stroke);g.fillRoundedRectangle(meter,1.f);
                if(amount>.0001f){
                    g.setColour(Theme::green.withAlpha(.53f));
                    g.fillRoundedRectangle(meter.withWidth(182.f*amount),1.f);
                }
            }
        } else {
            const auto macroDeck=juce::Rectangle<float>(rackOnRight?14.f:247.f,588.f,906.f,46.f);
            g.setColour(Theme::panel);g.fillRoundedRectangle(macroDeck,7.f);
            g.setColour(Theme::stroke.withAlpha(.75f));g.drawRoundedRectangle(macroDeck,7.f,1.f);
            const juce::StringArray labels{"TONE","PUNCH","SPACE","GRIT"};
            for(int k=0;k<4;k++){
                const int x=(rackOnRight?30:265)+222*k;
                Theme::text(g,labels[k],{x,590,105,17},12,Theme::muted,true);
                const auto* value=processor.state.getRawParameterValue(
                    "macro"+juce::String(k+1));
                Theme::text(g,juce::String(juce::roundToInt(100.f*
                    juce::jlimit(0.f,1.f,value?value->load():.5f))),
                    {x+108,590,76,17},12,Theme::green,true,
                    juce::Justification::centredRight);
            }
        }
    }
    void paintOverChildren(juce::Graphics& g) override {
        // A subtle second wallpaper pass keeps cover art visible across the
        // whole plugin, including panel surfaces, without hiding controls.
        if(browser.isVisible())return;
        const auto cover=browser.selectedCover();
        if(!cover.isValid())return;
        juce::Graphics::ScopedSaveState art(g);
        g.reduceClipRegion(juce::Rectangle<int>(14,78,1140,555));
        g.setOpacity(.045f);
        g.drawImageWithin(cover,14,78,1140,555,
            juce::RectanglePlacement::fillDestination);
    }
    void resized() override {
        themeButton.setBounds(15,12,53,56);
        chain.setBounds(rackOnRight?939:13,90,216,535);
        slot.setBounds(rackOnRight?14:247,90,906,496);
        preset.setBounds(356,27,332,39);
        match.setBounds(696,27,87,37);
        viewSwitch.setBounds(796,27,237,37);
        // Same approved header direction as AdlibLab: POWER then SETTINGS.
        // Compact 36px controls on one baseline, visually grouped.
        powerButton.setBounds(1069,28,36,36);
        help.setBounds(rackOnRight?906:1129,604,24,23);
        const int raw=(int)processor.state.getRawParameterValue("mode")->load();
        const int display=raw==3?previousMode:(raw==0?0:1);
        for(int i=0;i<4;i++) {
            auto& knob=macro[(size_t)i];
            const auto desired=display==0?juce::Slider::RotaryHorizontalVerticalDrag:
                                           juce::Slider::LinearHorizontal;
            if(knob.getSliderStyle()!=desired)knob.setSliderStyle(desired);
            if(display==0)
                knob.setBounds(127+i*254,398,120,113);
            else
                knob.setBounds(rackOnRight?97+i*222:333+i*222,
                               607,120,24);
        }
        input.setBounds(80,648,228,34);
        mix.setBounds(433,648,185,34);
        output.setBounds(845,648,172,34);
        sizeSelect.setBounds(1094,648,69,26);
        settings.setBounds(1110,28,36,36);
        record.setBounds(634,648,115,30);
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
        viewSwitch.setButtonText(display==0?"GO ADVANCED":"GO SIMPLE");
        viewSwitch.setTooltip(display==0?"Open full effect chain":"Switch to four macros");
        powerButton.setBypassed(bypass);
        viewSwitch.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
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
        const bool armed=processor.isCapturing();
        record.setButtonText(armed?"STOP REC":"REC WAV");
        record.setColour(juce::TextButton::buttonColourId,
            armed?Theme::red.withAlpha(.27f):Theme::panelAlt);
        // Only the small scope strip needs refresh at 30 Hz.
        repaint(juce::Rectangle<int>(73,51,271,25));
        const int activeView=(int)processor.state.getRawParameterValue("mode")->load();
        if(activeView==0 || (activeView==3 && previousMode==0))
            repaint(juce::Rectangle<int>(78,303,1010,29));
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
    SkellActionButton preset,match,help,record;
    SkellSettingsButton settings;
    std::unique_ptr<juce::FileChooser> recordChooser;
    juce::File lastCaptureFile;
    SkellActionButton viewSwitch;
    SkellPowerButton powerButton;
    std::array<SkellMacroKnob,4> macro;
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

void SkellPresetAudioProcessorEditor::openInstalledVST3BrowserForScreenshot() {
    canvas->openInstalledVST3BrowserForScreenshot();
}
