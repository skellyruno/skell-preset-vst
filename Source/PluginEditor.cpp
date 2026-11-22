#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/ChainPanel.h"
#include "ui/SlotEditor.h"
#include "ui/PresetBrowser.h"
#include "ui/MicModelPanel.h"
#include "ui/SkullThemeButton.h"
#include "ui/SkellPowerButton.h"
#include "ui/SkellSettingsButton.h"
#include "ui/SkellActionButton.h"
#include "ui/SkellMacroKnob.h"
#include "ui/SkellSettingsPanel.h"
#include "ui/SkellIdleDeck.h"
#include "chain/AlchemistNativePreset.h"
#include "chain/NativeSoundMatchTrials.h"
#include <SkellCore/UI.h>
#include <SkellCore/WorldLink.h>
#include <array>
#include <memory>
#include <vector>

namespace {
void setParam(SkellPresetAudioProcessor& p,const juce::String& id,float physical) {
    if(auto* v=p.state.getParameter(id))v->setValueNotifyingHost(v->convertTo0to1(physical));
}
}

class SkellPresetAudioProcessorEditor::Canvas final:public juce::Component,private juce::Timer {
public:
    Canvas(SkellPresetAudioProcessorEditor& owner,SkellPresetAudioProcessor& p)
    : outer(owner),processor(p),chain(p),slot(p),idle(p),browser(p),micPanel(p),skin(std::make_unique<skellcore::ui::LookAndFeel>(skellcore::ui::Theme::neon())) {
        Theme::selectFor(processor.state);
        setLookAndFeel(skin.get());
        setSize(Theme::width,Theme::height);
        setWantsKeyboardFocus(true);
        addAndMakeVisible(chain);addAndMakeVisible(slot);
        addAndMakeVisible(idle);
        chain.setSelectedSlot(-1);
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

        chain.onSelection=[this](int s){
            editingIdle=false;
            micPanel.setVisible(false); // effect editor must never sit behind MIC
            idle.setVisible(false);
            slot.select(s);chain.setSelectedSlot(s);
        };
        chain.onClearSelection=[this]{
            editingIdle=true;
            micPanel.setVisible(false); // show complete Home deck, not stale MIC
            slot.dismissTransientOverlays();
            chain.setSelectedSlot(-1);
            idle.setVisible(slot.isVisible());
            idle.toFront(false);
        };
        slot.onBackgroundClick=[this] { if(chain.onClearSelection)chain.onClearSelection(); };
        chain.onReplaceRequest=[this](int s){
            editingIdle=false;idle.setVisible(false);
            micPanel.setVisible(false);
            slot.select(s);
            chain.setSelectedSlot(s);
            slot.openModulePicker();
        };

        undo.setButtonText("UNDO");
        redo.setButtonText("REDO");
        undo.setTooltip("Undo a preset load (Ctrl+Z). New edits and external hosted VST3s are not in history yet.");
        redo.setTooltip("Redo a preset load (Ctrl+Y or Ctrl+Shift+Z)");
        style(undo);style(redo);
        addAndMakeVisible(undo);addAndMakeVisible(redo);
        undo.onClick=[this]{restoreHistory(undoStates,redoStates);};
        redo.onClick=[this]{restoreHistory(redoStates,undoStates);};
        updateHistoryButtons();
        preset.setButtonText(p.presetName().toUpperCase()+"  ▼");
        preset.onClick=[this]{
            micPanel.setVisible(false);
            if(browser.isVisible())browser.setVisible(false);else browser.open();
        };
        style(preset);addAndMakeVisible(preset);
        micButton.setButtonText("MIC");
        micButton.setTooltip("Open dedicated input microphone modeling panel");
        style(micButton);addAndMakeVisible(micButton);
        micButton.onClick=[this] {
            if(browser.isVisible())browser.setVisible(false);
            if(!micPanel.isVisible())slot.dismissTransientOverlays();
            micPanel.refresh();
            micPanel.setVisible(!micPanel.isVisible());
            if(micPanel.isVisible())micPanel.toFront(true);
        };
        // Keep the original test-only button adapter, but put the actual
        // microphone trigger at the top right of the effect rack.
        micButton.setVisible(false);
        chain.onMicRequest=[this] {if(micButton.onClick)micButton.onClick();};
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
            browser.setVisible(false);micPanel.setVisible(false);
            const int current=(int)processor.state.getRawParameterValue("mode")->load();
            const int view=current==3?previousMode:(current==0?0:1);
            previousMode=view==0?1:0;
            processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
            setParam(processor,"mode",(float)previousMode);
            refresh();
        };
        powerButton.onClick=[this] {
            browser.setVisible(false);micPanel.setVisible(false);
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
            s.onReadoutVisibilityChange=[this] {
                // Values live on the Canvas behind the hosted slider.
                // Repaint the footer only on interaction transitions.
                repaint(juce::Rectangle<int>(14,480,1144,160));
            };
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
            // Three true rotary controls, with larger percent readouts painted
            // separately to avoid JUCE text-box clipping at 70% scale.
            // Preserve the exact physical APVTS parameters/automation IDs.
            s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s->setRotaryParameters(1.25f*juce::MathConstants<float>::pi,
                                    2.75f*juce::MathConstants<float>::pi,true);
            s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            s->setScrollWheelEnabled(true);
            s->setDoubleClickReturnValue(true, s==&mix?1.:0.);
            s->setColour(juce::Slider::trackColourId,Theme::green);
            s->setColour(juce::Slider::thumbColourId,Theme::white);
            s->setColour(juce::Slider::textBoxTextColourId,Theme::white);
            s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            addAndMakeVisible(s);
        }
        inputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"input",input);
        mixAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"mix",mix);
        outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"output",output);
        // Input and Output drive real -24..+24 dB gain stages. Show actual
        // dB instead of a pseudo-percent where 50% ambiguously meant unity.
        // This is PRESENTATION ONLY: physical ranges, APVTS IDs and DSP
        // are unchanged. Accept legacy typed percentages as a fallback.
        auto gainDb=[](double db) {
            return juce::String(db>0.001?"+":"")+juce::String(db,1)+" dB";
        };
        auto parseGainDb=[](const juce::String& text) {
            const double displayed=text.getDoubleValue();
            const double db=text.contains("%")
                ?(displayed-50.)*48./100.:displayed;
            return juce::jlimit(-24.,24.,db);
        };
        input.textFromValueFunction=gainDb;
        output.textFromValueFunction=gainDb;
        input.valueFromTextFunction=parseGainDb;
        output.valueFromTextFunction=parseGainDb;
        mix.textFromValueFunction=[](double value) {
            return juce::String(juce::roundToInt(juce::jlimit(0.,1.,value)*100.))+"%";
        };
        mix.valueFromTextFunction=[](const juce::String& text) {
            const double value=text.getDoubleValue();
            return juce::jlimit(0.,1.,(text.contains("%")||value>1.)?value/100.:value);
        };
        input.updateText();output.updateText();mix.updateText();
        input.setTooltip("Input gain -24 to +24 dB · 0 dB is unity");
        mix.setTooltip("Global dry / wet mix · 100% is fully processed");
        output.setTooltip("Output gain -24 to +24 dB · 0 dB is unity");
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
        settings.setTooltip("Settings / Interface Size / Optional SkellWorld Presence");
        style(settings);
        addAndMakeVisible(settings);
        settings.onClick=[this] {
            browser.setVisible(false);
            micPanel.setVisible(false);
            if(settingsPanel.isVisible())settingsPanel.close();
            else {
                refreshWorldLinkStatus();
                settingsPanel.open(sizeSelect.getSelectedId()-1);
            }
        };
        settingsPanel.onWorldLinkToggle=[this] {
            if(worldLink.requested())worldLink.leave();
            else worldLink.join();
            refreshWorldLinkStatus();
        };
        settingsPanel.onSizeChange=[this](int choice){
            if(choice>=1 && choice<=4)
                sizeSelect.setSelectedId(choice,juce::sendNotificationSync);
        };
        addAndMakeVisible(settingsPanel);
        settingsPanel.setVisible(false);
        record.setButtonText("REC WAV");
        record.setTooltip("Capture processed stereo track audio to a 24-bit WAV. FL Studio owns the microphone and transport.");
        style(record);
        addAndMakeVisible(record);
        record.setVisible(false); // no REC WAV on the main interface
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
        browser.onBeforeLoad=[this]{prepareHistory();};
        browser.onLoadFailed=[this]{pendingHistory.reset();updateHistoryButtons();};
        browser.onLoaded=[this]{
            commitHistory();
            editingIdle=false;idle.setVisible(false);
            slot.select(0);chain.setSelectedSlot(0);refresh();
        };
        browser.onCoverChanged=[this]{repaint();};
        browser.onClose=[this]{preset.repaint();};
        addAndMakeVisible(bypassShade);bypassShade.setVisible(false);
        addAndMakeVisible(browser);browser.setVisible(false);
        settingsPanel.toFront(false);
        addAndMakeVisible(micPanel);micPanel.setVisible(false);
        micPanel.onClose=[this]{micButton.repaint();};
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
            "Switch between the uncluttered sound overview and the full effect rack.");
        if(chain.isVisible() && slot.isVisible()) {
            steps.emplace_back(&chain,"YOUR EFFECT CHAIN",
                "Scroll the rack, drag to reorder, double-click to replace or use + to add a processor. Studio Compressor, Studio Limiter and Studio Delay have dedicated effect controls.");
            steps.emplace_back(&slot,"EDIT A SLOT",
                "Adjust the selected effect here. Rack slots keep their own independent controls, order and automation.");
        }
        steps.emplace_back(&chain,"MIC MODEL + NOISE GATE",
            "Click the rack's MIC icon. MIC MODEL shapes the recorded microphone; NOISE GATE has its own ON switch, threshold, hold and release. Both start independent, and the gate starts OFF.");
        steps.emplace_back(&idle,"START HERE",
            "The home deck shows your real output signal. Choose a preset or select any rack slot to begin; the microphone controls are available separately.");
        steps.emplace_back(&powerButton,"POWER / BYPASS",
            "Green means your effect rack is active. Click the power icon to bypass without losing any settings.");
        steps.emplace_back(&mix,"BLEND YOUR SOUND",
            "The three bottom-right knobs control Input, dry/wet Mix, and Output.");
        steps.emplace_back(&settings,"SETTINGS / SKELLWORLD",
            "Resize the interface or optionally join SkellWorld presence. This sends no audio, preset, chain or automation data.");
        return steps;
    }
    skellcore::ui::GuidedTourPalette tourPalette() const {
        return {Theme::green,Theme::panel,Theme::white,Theme::muted,
                juce::Colour(0xd9000000)};
    }
    ~Canvas()override {worldLink.leave();stopTimer();setLookAndFeel(nullptr);}
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
        micPanel.refresh();
        setLookAndFeel(nullptr);
        skin=std::make_unique<skellcore::ui::LookAndFeel>(skellcore::ui::Theme{
            Theme::bg,Theme::panel,Theme::green,Theme::white,Theme::muted});
        setLookAndFeel(skin.get());
        for(juce::TextButton* b:std::array<juce::TextButton*,10>{{
            &preset,&match,&viewSwitch,&undo,&redo,&powerButton,&help,&settings,&record,&micButton}})
            style(*b);
        for(auto& s:macro){
            s.setColour(juce::Slider::trackColourId,Theme::green);
            s.setColour(juce::Slider::thumbColourId,Theme::white);
        }
        for(auto* s:{&input,&mix,&output}){
            s->setColour(juce::Slider::trackColourId,Theme::green);
            s->setColour(juce::Slider::thumbColourId,Theme::white);
            s->setColour(juce::Slider::textBoxTextColourId,Theme::white);
            s->setColour(juce::Slider::textBoxBackgroundColourId,Theme::panelAlt);
            s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        }
        sizeSelect.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
        sizeSelect.setColour(juce::ComboBox::textColourId,Theme::white);
        sizeSelect.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        chain.repaint();slot.applyTheme();browser.applyTheme();
        idle.repaint();settingsPanel.repaint();
        refresh();
    }
    void refreshWorldLinkStatus() {
        settingsPanel.setWorldLinkState(
            worldLink.requested(),worldLink.online(),worldLink.hubOnline());
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
    void setMatchPageForScreenshot(int page) {
        editingIdle=false;idle.setVisible(false);
        slot.setMatchPageForScreenshot(page);
    }
    void setMicPanelForScreenshot(bool visible) {
        // A capture of the closed mic panel must not close a completely
        // independent open preset browser scene. Only opening MIC owns
        // the overlay-dismiss action.
        if(visible){
            browser.setVisible(false);
            slot.dismissTransientOverlays();
        }
        micPanel.refresh();
        micPanel.setVisible(visible);
        if(visible)micPanel.toFront(true);
    }
    void setModOrbForScreenshot(bool enabled) {
        editingIdle=false;idle.setVisible(false);
        slot.setModOrbForScreenshot(enabled);
    }
    void showHomeForScreenshot() {
        if(chain.onClearSelection)chain.onClearSelection();
        refresh();
    }
    bool openRackContextMenuForScreenshot(int visualRow) {
        return chain.openContextMenuForScreenshot(visualRow);
    }
    void openModulePickerForScreenshot() {
        editingIdle=false;idle.setVisible(false);
        slot.openModulePickerForScreenshot();
    }
    void openInstalledVST3BrowserForScreenshot() {
        editingIdle=false;idle.setVisible(false);
        slot.openInstalledVST3BrowserForScreenshot();
    }
    void selectRackSlotForScreenshot(int physicalSlot,int scrollOffset) {
        // The idle screen is correct at startup, but local Quick4 must
        // capture the REAL requested effect once explicitly selected.
        editingIdle=false;idle.setVisible(false);
        micPanel.setVisible(false); // same dismissal as real rack selection
        chain.setScreenshotScrollOffset(scrollOffset);
        chain.setSelectedSlot(physicalSlot);
        slot.select(physicalSlot);
        refresh();
    }
    juce::StringArray getScreenshotLayoutIssues() const {
        juce::StringArray issues;
        const auto canvasBounds=juce::Rectangle<int>(0,0,Theme::width,Theme::height);
        const std::array<const juce::Component*,13> all{{&themeButton,&preset,&match,
            &viewSwitch,&powerButton,&help,&chain,&slot,&input,&mix,&output,&sizeSelect,&settings}};
        for(const auto* child:all){
            if(child->getWidth()<=0 || child->getHeight()<=0 || !canvasBounds.contains(child->getBounds()))
                issues.add("Control outside canvas bounds: " + child->getName());
        }
        if(record.isVisible() || micButton.isVisible())
            issues.add("Removed REC WAV and legacy MIC header buttons still visible");
        if(input.getBounds().intersects(mix.getBounds()) ||
           mix.getBounds().intersects(output.getBounds()))
            issues.add("Footer master knobs overlap");
        if(!input.textFromValueFunction || input.textFromValueFunction(0.)!="0.0 dB"
           || input.textFromValueFunction(6.)!="+6.0 dB"
           || !output.textFromValueFunction || output.textFromValueFunction(-6.)!="-6.0 dB"
           || !mix.textFromValueFunction || mix.textFromValueFunction(1.)!="100%"
           || !input.valueFromTextFunction
           || std::abs(input.valueFromTextFunction("50%"))>1.e-5
           || std::abs(input.valueFromTextFunction("+6 dB")-6.)>1.e-5)
            issues.add("Master rotary readout or legacy percent input incorrectly maps physical dB");
        const std::array<const juce::Component*,7> top{{
            &preset,&match,&viewSwitch,&undo,&redo,&settings,&powerButton}};
        for(size_t i=0;i<top.size();++i)
            for(size_t j=i+1;j<top.size();++j)
                if(top[i]->getBounds().intersects(top[j]->getBounds()))
                    issues.add("Overlapping top-bar controls " + juce::String((int)i)
                               + " / " + juce::String((int)j));
        if(!canvasBounds.contains(browser.getBounds()))issues.add("Preset browser is clipped");
        if(!settings.isVisible()||sizeSelect.isVisible())issues.add("UI size control not relocated to settings");
        if(!settingsPanel.testLayoutForTest())
            issues.add("Settings scale cards or close/presence targets are clipped");
        if(!slot.testPopupLayoutsForTest())
            issues.add("Effect / VST3 / EQ / wet duck popups have clipping or overlay conflicts");
        if(!juce::Rectangle<int>(0,0,Theme::width,Theme::height)
            .contains(settingsPanel.getBounds()))
            issues.add("Themed Settings overlay clipped by canvas");
        if(chain.getBounds().intersects(slot.getBounds()))
            issues.add("Rack overlaps the module editor");
        if(slot.getHeight()!=chain.getHeight())
            issues.add("Advanced control deck does not fill the rack height");
        if(rackOnRight!=(chain.getX()>slot.getX()))
            issues.add("Rack dock state disagrees with displayed position");
        const int current=(int)processor.state.getRawParameterValue("mode")->load();
        const bool advanced=current==1 || (current==3 && previousMode==1);
        for(const auto& control:macro)
            if(control.isVisible()==advanced)
                issues.add(advanced?"Advanced mode still shows a legacy macro":
                                  "Simple mode lost a macro control");
        if(chain.isVisible()!=advanced || slot.isVisible()!=advanced)
            issues.add("Mode visibility did not match the requested workspace");
        for(const auto* master:{&input,&mix,&output})
            if(master->isVisible()==advanced)
                issues.add("Input/Mix/Output are not restricted to Simple mode");
        if(current==3 && !bypassShade.isVisible())issues.add("Bypass overlay missing");
        if(powerButton.isBypassed()!=(current==3))
            issues.add("Power indicator disagrees with host bypass state");
        if(!advanced) {
            for(int i=0;i<4;++i) {
                const auto card=juce::Rectangle<int>(78+i*254,339,220,214);
                if(!card.contains(macro[(size_t)i].getBounds()))
                    issues.add("Simple macro dial escapes its card");
                for(int j=i+1;j<4;++j)
                    if(macro[(size_t)i].getBounds().intersects(macro[(size_t)j].getBounds()))
                        issues.add("Simple macro dials overlap");
            }
            if(input.getWidth()<44 || mix.getWidth()<44 || output.getWidth()<44 ||
               input.getHeight()<39 || mix.getHeight()<39 || output.getHeight()<39 ||
               input.getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag ||
               mix.getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag ||
               output.getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag)
                issues.add("Master hardware rotary controls too small or incorrect");
        }
        if(!preset.getButtonText().contains(processor.presetName().toUpperCase()))
            issues.add("Preset selector no longer displays the loaded preset name");
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
        if(!chain.triggerMicForTest())
            issues.add("Rack MIC icon has no click action");
        if(!micPanel.isVisible())
            issues.add("Dedicated MIC panel did not open from rack icon");
        if(!micPanel.testInputAndBypass())
            issues.add("Dedicated MIC panel cannot automate target or bypass");
        if(!micPanel.testNoiseGateAndBypass())
            issues.add("Mic NOISE GATE tab does not automate threshold or independent bypass");
        chain.triggerMicForTest();
        if(micPanel.isVisible())
            issues.add("Dedicated MIC panel failed to close");
        // Opening MIC after the effect library must dismiss the old overlay,
        // without changing the selected effect or any DSP parameter.
        slot.openModulePickerForScreenshot();
        if(!slot.modulePickerVisibleForTest())
            issues.add("Effect library could not open for MIC dismissal test");
        chain.triggerMicForTest();
        if(slot.modulePickerVisibleForTest() || !micPanel.isVisible())
            issues.add("MIC panel did not dismiss overlapping effect library");
        chain.triggerMicForTest();
        preset.onClick();
        if(!browser.isVisible())issues.add("Preset selector did not open browser");
        if(browser.visibleFactoryPresetsForTest()!=2 ||
           !browser.canSetCoverForTest())
            issues.add("Main Vocals must show exactly Alchemist and Deuce");
        if(!browser.testMainVocalsPresetFilterForTest())
            issues.add("Main Vocals, My Presets, Artists, Favorites or manual genre filter regressed");
        if(!browser.testBrowserLayoutForTest())
            issues.add("Preset browser genre buttons, seven banks or cover preview layout overlap");
        if(processor.presetName()!="Alchemist" ||
           processor.getProgramName(skellpreset::alchemist::factoryIndex)!="Alchemist" ||
           processor.getProgramName(skellpreset::deuce_v25::factoryIndex)!="Deuce")
            issues.add("The two visible factory presets must use the requested names");
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
        // Exercise the real APVTS sliders through both workspace modes.
        // Restore the prior values so later factory regressions stay clean.
        configureForScreenshot(0,0,false);
        const std::array<double,3> oldMasters{input.getValue(),mix.getValue(),output.getValue()};
        const bool simpleShown=input.isVisible() && mix.isVisible() && output.isVisible();
        input.setValue(3.0,juce::sendNotificationSync);
        mix.setValue(.67,juce::sendNotificationSync);
        output.setValue(-2.0,juce::sendNotificationSync);
        const auto correctState=[this]{
            return std::abs(processor.state.getRawParameterValue("input")->load()-3.f)<.05f &&
                std::abs(processor.state.getRawParameterValue("mix")->load()-.67f)<.02f &&
                std::abs(processor.state.getRawParameterValue("output")->load()+2.f)<.05f;
        };
        const bool automated=correctState();
        viewSwitch.onClick();
        const bool hiddenInAdvanced=!input.isVisible() && !mix.isVisible() &&
            !output.isVisible() && correctState();
        viewSwitch.onClick();
        const bool recalled=input.isVisible() && mix.isVisible() && output.isVisible() &&
            correctState() && std::abs(input.getValue()-3.0)<.05 &&
            std::abs(mix.getValue()-.67)<.02 && std::abs(output.getValue()+2.0)<.05;
        input.setValue(oldMasters[0],juce::sendNotificationSync);
        mix.setValue(oldMasters[1],juce::sendNotificationSync);
        output.setValue(oldMasters[2],juce::sendNotificationSync);
        if(!simpleShown || !automated || !hiddenInAdvanced || !recalled)
            issues.add("Simple Input/Mix/Output visibility or automation recall regressed");
        for(int i=0;i<4;++i) {
            if(!macro[(size_t)i].isVisible()
               || macro[(size_t)i].getSliderStyle()!=
                   juce::Slider::RotaryHorizontalVerticalDrag)
                issues.add("Simple rotary Tone/Punch/Space/Grit macro missing");
            if(!macro[(size_t)i].textFromValueFunction
               || macro[(size_t)i].getTextFromValue(.371)!="37%")
                issues.add("Simple macro percentage display regressed");
            if(!processor.state.getParameter("macro"+juce::String(i+1)))
                issues.add("Legacy macro automation ID was removed");
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
        settings.onClick();
        if(!settingsPanel.isVisible())
            issues.add("Settings button did not open its themed overlay");
        if(!settingsPanel.hasWorldLinkControlForTest())
            issues.add("Optional SkellWorld presence control is missing from themed Settings");
        settings.onClick();
        if(settingsPanel.isVisible())
            issues.add("Settings button did not close its themed overlay");
        configureForScreenshot(0,1,false);
        for(const auto& control:macro)
            if(control.isVisible())
                issues.add("Legacy macro appears in advanced mode");
        // The real browser's native preset commands must be undoable with a
        // bounded state journal, and Redo must reconstruct the selected sound.
        processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
        browser.loadFactoryForHistoryTest(skellpreset::deuce_v25::factoryIndex);
        const bool loadedDeuce=processor.getCurrentProgram()==skellpreset::deuce_v25::factoryIndex &&
            undo.isEnabled() && !redo.isEnabled();
        undo.onClick();
        const bool undoRecalled=processor.getCurrentProgram()==skellpreset::alchemist::factoryIndex &&
            redo.isEnabled();
        redo.onClick();
        const bool redoRecalled=processor.getCurrentProgram()==skellpreset::deuce_v25::factoryIndex &&
            undo.isEnabled();
        undo.onClick();
        // A new browser operation after undo invalidates stale redo history.
        browser.loadFactoryForHistoryTest(skellpreset::deuce_v25::factoryIndex);
        const bool newLoadClearsRedo=!redo.isEnabled();
        if(!loadedDeuce || !undoRecalled || !redoRecalled || !newLoadClearsRedo)
            issues.add("Header Undo/Redo does not restore native factory preset state");
        undoStates.clear();redoStates.clear();pendingHistory.reset();updateHistoryButtons();
        const int beforeTheme=selectedTheme;
        themeButton.onClick();
        if(selectedTheme!=(beforeTheme+1)%3)
            issues.add("Skell mark click didn't cycle themes immediately");
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
           !chain.testWheelScrollDoesNotReorder())
            issues.add("Rack wheel must navigate without changing effect order");
        chain.setScreenshotScrollOffset(0);
        if(!chain.testWheelWetAndRack())
            issues.add("Rack wheel and wet controls are not independent");
        if(!chain.testRemoveExtraSlot())
            issues.add("Removing an extra rack row changes stable physical IDs");
        if(!chain.testRackControlsAndWet())
            issues.add("Rack wet trim and green bypass dot did not update or restore host parameters");
        if(!chain.triggerReplaceForTest(0) || !slot.modulePickerVisibleForTest())
            issues.add("Rack double-click replacement did not open effect library");
        slot.openModulePicker();
        if(slot.modulePickerVisibleForTest())
            issues.add("Rack double-click effect library could not be closed");
        if(!slot.testModulePickerInteraction())
            issues.add("Module browser did not toggle, filter or load Ring Mod");
        if(!slot.testExternalBrowseCancel())
            issues.add("Canceled external-VST3 browser altered the active effect or bypass");
        if(!slot.testEightBandGraphAndQ())
            issues.add("8-Band EQ graph did not edit the eighth band and its Q");
        if(!slot.testKnobReadouts())
            issues.add("APVTS overwrote physical Hz/dB/Q knob readouts");
        if(!slot.testClassicDynamicsUnits())
            issues.add("Classic dynamics controls do not display or parse their physical units");
        if(!slot.testStudioEqSeventhBand())
            issues.add("Studio seven-band EQ last page does not edit B7 frequency, gain or width");
        if(!slot.testStudioEqAddAndFilterTypes())
            issues.add("Studio EQ Add Node, eighth-band controls, HP/LP or Remove not operational");
        if(!slot.testStudioEqGraphDoubleClick())
            issues.add("Studio EQ double-click on the graph did not create/edit a real eighth node");
        if(!slot.testMatchFinishPages())
            issues.add("Alchemist Shape/Calibration tabs do not expose all eight real parameters");
        if(!slot.testStudioLimiterPages())
            issues.add("Studio Limiter pages or dedicated p9 gate floor do not control host DSP");
        if(!slot.testStudioDelayPages())
            issues.add("Studio Delay effect pages failed to map p1..p16 host parameters");
        if(!slot.testAdlibControls())
            issues.add("AdlibFX UI knob units, inert controls or parameter bindings failed");
        if(!slot.testWetSidechainPanel())
            issues.add("Sidechain editor controls, host bindings or layout failed");
        if(!slot.testIntegratedEffectLayout())
            issues.add("Unified effect UI has overlapping/missing inline parameter controls");
        if(!slot.testPowerDotSync())
            issues.add("Effect power light does not toggle the same APVTS state as the rack");
        if(!slot.testMicModelCards())
            issues.add("Mic Modeling profile cards failed to update stable host parameters");
        if(!slot.testModOrbToggle())
            issues.add("Modulation orb/graph toggle is not reversible");
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
        slot.select(0); // Refresh the real selector; don't test stale prior UI.
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
        // The append-only catalog now contains 55 programs. Preserve the
        // original Alchemist index and assert every later sound-match trial
        // stays at its documented factory ID as new candidates are appended.
        if(processor.getNumPrograms()!=SkellPresetAudioProcessor::customRackProgramIndex+1 ||
           processor.getProgramName(43)!="Alchemist" ||
           processor.getProgramName(44)!="Deuce D/Max (Native Trial)" ||
           processor.getProgramName(45)!="Alchemist Refined (Trial)" ||
           processor.getProgramName(46)!="Deuce" ||
           processor.getProgramName(47)!="Alchemist Peak Guard (Trial)" ||
           processor.getProgramName(48)!="Alchemist Tone Match (Trial)" ||
           processor.getProgramName(49)!="Alchemist Lighter Dynamics (Trial)" ||
           processor.getProgramName(50)!="Alchemist Fast Compressor Blend (Trial)" ||
           processor.getProgramName(51)!="Alchemist Smooth Compressor Blend (Trial)" ||
           processor.getProgramName(52)!="Alchemist Serial Dynamics (Trial)" ||
           processor.getProgramName(53)!="Alchemist Low-End Contour (Trial)" ||
           processor.getProgramName(54)!="Alchemist Serial Dynamics Blend (Trial)")
            issues.add("Append-only factory catalog or legacy Alchemist index changed");
        if(!slot.testAllNativeEffectLayoutsForTest())
            issues.add("A native effect editor has clipped/invisible inline controls or popup geometry");
        // Real stereo audio → APVTS slot 43 → measured GR → exact state
        // snapshot/restore. Never prove a new plugin using UI-only graphics.
        processor.loadFactoryPreset(25); // zero-PDC tracking factory
        processor.setSlotType(0,SkellPresetAudioProcessor::VocalFeelOptoFX);
        processor.setSlotOn(0,true);
        processor.setSlotParam(0,0,.8f);
        processor.setSlotParam(0,1,.5f); // WARM
        processor.setSlotParam(0,2,.5f); // MALE 2
        processor.setSlotParam(0,3,1.f);
        const bool validNativeType=processor.slotType(0)==SkellPresetAudioProcessor::VocalFeelOptoFX
            && processor.state.getParameter("s1_fxExt10")!=nullptr
            && processor.getLatencySamples()==0;
        juce::AudioBuffer<float> optoSignal(2,256);
        juce::MidiBuffer optoMidi;
        bool validAudio=true;
        for(int block=0;block<110;++block) {
            for(int sample=0;sample<256;++sample) {
                const float t=(float)(block*256+sample)/48000.f;
                const float wave=.36f*std::sin(juce::MathConstants<float>::twoPi*250.f*t);
                optoSignal.setSample(0,sample,wave);
                optoSignal.setSample(1,sample,wave*.8f);
            }
            processor.processBlock(optoSignal,optoMidi);
            for(int sample=0;sample<256;++sample)
                validAudio=validAudio && std::isfinite(optoSignal.getSample(0,sample))
                            && std::isfinite(optoSignal.getSample(1,sample));
        }
        const bool realReduction=processor.getSlotGainReductionDb(0)>1.f
            && processor.getSlotGainReductionDb(0)<40.f;
        juce::MemoryBlock optoSnapshot;
        processor.getStateInformation(optoSnapshot);
        processor.setSlotType(0,SkellPresetAudioProcessor::Empty);
        processor.setStateInformation(optoSnapshot.getData(),(int)optoSnapshot.getSize());
        const bool optoRecalled=processor.slotType(0)==SkellPresetAudioProcessor::VocalFeelOptoFX
            && std::abs(processor.getSlotParam(0,0)-.8f)<.01f
            && std::abs(processor.getSlotParam(0,1)-.5f)<.01f;
        if(!validNativeType || !validAudio || !realReduction || !optoRecalled)
            issues.add("Vocal Feel Opto real audio, 0-PDC, GR meter or host state roundtrip failed");
        // Critical compatibility: do not replace the original simple macro
        // processor when adding a catalog, recorder or waveform visualization.
        setParam(processor,"mode",0.f);
        refresh();
        if(chain.isVisible() || !macro[0].isVisible())
            issues.add("Simple mode must show macro cards and hide the rack");
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
        // Subtle studio-console lighting gives the flat canvas depth while
        // keeping neon-green labels legible in every approved theme.
        g.setGradientFill(juce::ColourGradient(
            Theme::panel.brighter(.12f),0.f,0.f,
            Theme::bg,0.f,(float)Theme::height,false));
        g.fillRect(0,0,Theme::width,Theme::height);
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
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(.08f),0.f,0.f,
            Theme::panel.darker(.06f),0.f,76.f,false));
        g.fillRect(0,0,Theme::width,76);
        g.setColour(Theme::white.withAlpha(.065f));
        g.drawHorizontalLine(1,10.f,(float)Theme::width-10.f);
        g.setColour(Theme::stroke.withAlpha(.95f));
        g.drawHorizontalLine(75,0.f,(float)Theme::width);
        g.setColour(Theme::green.withAlpha(.42f));
        g.fillRect(76,74,110,1);
        // One typographic run avoids the clipped hyphen and inconsistent gaps.
        // PRESET stays neon green in the main Obsidian skin.
        juce::AttributedString brand;
        brand.setJustification(juce::Justification::centredLeft);
        brand.append("SKELL",Theme::white);
        brand.append("PRESET",Theme::green);
        // AttributedString::setFont styles existing text; calling it before
        // append() left the new runs at JUCE's tiny default font.
        brand.setFont(Theme::font(27.f,true));
        juce::TextLayout wordmark;
        wordmark.createLayout(brand,271.f);
        wordmark.draw(g,juce::Rectangle<float>(75.f,18.f,271.f,38.f));
        // The new tagline lives directly under SKELLPRESET, rather than
        // fighting the old waveform strip for the same 14 px.
        Theme::text(g,"ONE PLUGIN. EVERY PRESET.",{76,54,190,15},
                    10.3f,Theme::muted,true);
        // UI edition marker belongs with the title, not an isolated LIVE
        // badge. Binary package version stays 0.7.0 for old host projects.
        Theme::text(g,"V 0.1",{282,30,64,18},11.f,
                    Theme::green.withAlpha(.86f),true);
        Theme::text(g,"PRESET",{356,12,332,14},10,Theme::muted,true);
        const double sampleRate=juce::jmax(8000.,processor.preparedAudioRate());
        const double latencyMs=1000.*processor.getLatencySamples()/sampleRate;
        Theme::text(g,"PDC "+juce::String(latencyMs,1)+" ms",
                    {695,12,91,14},9,Theme::muted,true);
        g.setColour(Theme::stroke);g.drawHorizontalLine(634,0.f,(float)Theme::width);
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.withMultipliedBrightness(.88f),0.f,635.f,
            Theme::panel.darker(.07f),0.f,688.f,false));
        g.fillRect(0,635,Theme::width,53);
        g.setColour(Theme::white.withAlpha(.055f));
        g.drawHorizontalLine(636,12.f,(float)Theme::width-12.f);
        const int masterMode=(int)processor.state.getRawParameterValue("mode")->load();
        if(masterMode==0 || (masterMode==3 && previousMode==0)) {
            g.setColour(Theme::stroke.withAlpha(.50f));
            g.drawVerticalLine(801,644.f,681.f);
        }
        Theme::text(g,"IN",{26,646,42,18},11,Theme::muted,true);
        Theme::text(g,"OUT",{214,646,48,18},11,Theme::muted,true);
        const float inPeak=juce::jlimit(0.f,1.f,processor.getInputPeak());
        const float outPeak=juce::jlimit(0.f,1.f,processor.getOutputPeak());
        for(int ix=0;ix<12;++ix){
            g.setColour(ix<12.f*inPeak?Theme::green:Theme::stroke);
            g.fillRect(68+ix*10,651,7,11);
            g.setColour(ix<12.f*outPeak?Theme::green:Theme::stroke);
            g.fillRect(263+ix*10,651,7,11);
        }
        // Hardware dials show the true physical gain and mix readouts.
        // The actual gain stages operate in dB; only dry/wet uses %.
        const int currentView=(int)processor.state.getRawParameterValue("mode")->load();
        const bool masterControlsInSimple=(currentView==0 ||
            (currentView==3 && previousMode==0));
        if(masterControlsInSimple) {
        static constexpr const char* masterNames[]{"INPUT","MIX","OUTPUT"};
        const float values[]{(float)input.getValue(),(float)mix.getValue(),
                             (float)output.getValue()};
        for(int i=0;i<3;++i) {
            const int x=807+i*116;
            const auto card=juce::Rectangle<float>((float)x,639.f,110.f,46.f);
            Theme::panelBox(g,card,5.f);
            g.setColour(Theme::green.withAlpha(.78f));
            g.fillRoundedRectangle((float)x+7.f,645.f,2.f,9.f,1.f);
            Theme::text(g,masterNames[i],{x+52,642,54,14},11.4f,Theme::muted,true);
            const auto readout=i==1
                ?mix.getTextFromValue(values[i])
                :(i==0?input.getTextFromValue(values[i])
                     :output.getTextFromValue(values[i]));
            Theme::fittedText(g,readout,
                        {x+50,658,57,23},15.8f,Theme::white,true);
        }
        }
        const int current=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        const int viewMode=current==3?previousMode:(current==0?0:1);
        if(viewMode==0){
            Theme::panelBox(g,{14.f,90.f,1140.f,507.f},11.f);
            Theme::text(g,"SIMPLE  /  MACRO MIXER",{52,115,520,25},15.8f,Theme::green,true);
            Theme::fittedText(g,processor.presetName(),{74,164,988,69},47,Theme::white,true);
            Theme::text(g,"YOUR SOUND, WITHOUT THE CLUTTER",{79,248,900,24},16.0f,Theme::white.withAlpha(.86f),true);
            Theme::text(g,"FOUR MACROS  /  OPEN ADVANCED TO EDIT EACH EFFECT",{79,278,960,24},14.6f,Theme::muted);
            // Expanded live scope in the Simple view: this is actual
            // post-effects output, not a random waveform drawn for decoration.
            Theme::text(g,"LIVE AUDIO",{79,306,110,20},13,
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
            static constexpr const char* labels[]{"TONE","PUNCH","SPACE","GRIT"};
            static constexpr const char* hints[]{"COLOR & AIR","TRANSIENT","DEPTH & TAILS","EDGE & TEXTURE"};
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
                // Macro value is useful even when not hovering. At 70%
                // scale the old hover-only readout left four expensive
                // looking but opaque knobs with no numeric feedback.
                Theme::text(g,juce::String(juce::roundToInt(amount*100.f))+"%",
                    {(int)x+55,510,110,29},21,Theme::green,true,
                    juce::Justification::centred);
                const auto meter=juce::Rectangle<float>(x+19.f,543.f,182.f,2.f);
                g.setColour(Theme::stroke);g.fillRoundedRectangle(meter,1.f);
                if(amount>.0001f){
                    g.setColour(Theme::green.withAlpha(.53f));
                    g.fillRoundedRectangle(meter.withWidth(182.f*amount),1.f);
                }
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
        // The Advanced footer macro strip is gone: let the module editor
        // fill the space down to the shared master-control footer.
        slot.setBounds(rackOnRight?14:247,90,906,535);
        idle.setBounds(slot.getBounds());
        // Smaller movable mic panel opens on the LEFT; manual drag/resize
        // remains under the user's control and survives editor refresh.
        constexpr int micWidth=354,micHeight=363;
        if(!micPanel.hasBeenMoved())
            micPanel.setBounds(rackOnRight?18:238,107,micWidth,micHeight);
        else {
            const int w=juce::jlimit(340,490,micPanel.getWidth());
            const int h=juce::jlimit(345,430,micPanel.getHeight());
            micPanel.setBounds(
                juce::jlimit(0,Theme::width-w,micPanel.getX()),
                juce::jlimit(0,Theme::height-h,micPanel.getY()),w,h);
        }
        preset.setBounds(356,27,332,39);
        match.setBounds(696,27,87,37);
        viewSwitch.setBounds(796,27,176,37);
        // Bounded message-thread preset history in the top bar.
        // The two small actions never obscure the Power / Settings group.
        undo.setBounds(978,28,41,36);
        redo.setBounds(1023,28,41,36);
        micButton.setBounds(0,0,44,36);
        micButton.setVisible(false);
        // Same approved header direction as AdlibLab: POWER then SETTINGS.
        // Compact 36px controls on one baseline, visually grouped.
        powerButton.setBounds(1069,28,36,36);
        help.setBounds(rackOnRight?906:1129,604,24,23);
        const int raw=(int)processor.state.getRawParameterValue("mode")->load();
        const int display=raw==3?previousMode:(raw==0?0:1);
        for(int i=0;i<4;++i) {
            auto& knob=macro[(size_t)i];
            // Original Simple-mode hardware cards and rotary controls;
            // Advanced hides them without changing automatable macro IDs.
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setBounds(127+i*254,398,120,113);
            knob.setVisible(display==0);
        }
        // Keep all three original APVTS controls Simple-only and usable
        // even when the plugin is displayed at its default 70% scale.
        input.setBounds(810,642,45,40);
        mix.setBounds(926,642,45,40);
        output.setBounds(1042,642,45,40);
        // Keep published input/mix/output APVTS values active in both modes,
        // but expose their studio gain staging only in the Simple workspace.
        for(auto* control:{&input,&mix,&output})
            control->setVisible(display==0);
        sizeSelect.setBounds(1094,648,69,26);
        settings.setBounds(1110,28,36,36);
        settingsPanel.setBounds(743,78,388,326);
        record.setBounds(0,0,1,1);
        record.setVisible(false);
        // Keep the rack unobstructed when the user docks it on the right.
        browser.setBounds(rackOnRight?76:445,77,650,538);
        bypassShade.setBounds(display==0?14:(rackOnRight?14:247),90,
                              display==0?1140:906,display==0?507:535);
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
        idle.setVisible(display==1 && editingIdle);
        chain.setAlpha(bypass?.30f:1.f);slot.setAlpha(bypass?.30f:1.f);
        chain.setEnabled(!bypass);slot.setEnabled(!bypass);
        idle.setEnabled(!bypass);idle.setAlpha(bypass?.30f:1.f);
        for(auto& s:macro){
            s.setVisible(display==0);
            s.setEnabled(!bypass);
            s.setAlpha(bypass?.35f:1.f);
        }
        for(auto* s:{&input,&mix,&output}) {
            s->setVisible(display==0);
            s->setEnabled(!bypass);
            s->setAlpha(bypass?.35f:1.f);
        }
        bypassShade.setVisible(bypass);
        if(bypass)bypassShade.toFront(false);
        if(browser.isVisible())browser.toFront(false);
        if(settingsPanel.isVisible())settingsPanel.toFront(true);
        if(micPanel.isVisible())micPanel.toFront(true);
        micButton.setColour(juce::TextButton::buttonColourId,
            micPanel.anyActive()?Theme::green.withAlpha(.21f):Theme::panelAlt);
        micButton.setColour(juce::TextButton::textColourOffId,
            micPanel.anyActive()?Theme::green:Theme::white);
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
        if(settingsPanel.isVisible())refreshWorldLinkStatus();
        if(micPanel.isVisible())micPanel.refreshIfChanged();
        const bool micActive=micPanel.anyActive();
        if(micActive!=lastMicOn) {
            lastMicOn=micActive;
            micButton.setColour(juce::TextButton::buttonColourId,
                micActive?Theme::green.withAlpha(.21f):Theme::panelAlt);
            micButton.setColour(juce::TextButton::textColourOffId,
                micActive?Theme::green:Theme::white);
            micButton.repaint();
        }
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
        if(inLevel!=lastInputMeter){lastInputMeter=inLevel;repaint(juce::Rectangle<int>(68,649,125,20));}
        if(outLevel!=lastOutputMeter){lastOutputMeter=outLevel;repaint(juce::Rectangle<int>(263,649,125,20));}
        const bool armed=processor.isCapturing();
        // Avoid invalidating the capture button 30 times per second when its
        // state has not changed; scope and meters still repaint as needed.
        if(lastRecordingState!=(armed?1:0)) {
            lastRecordingState=armed?1:0;
            record.setButtonText(armed?"STOP REC":"REC WAV");
            record.setColour(juce::TextButton::buttonColourId,
                armed?Theme::red.withAlpha(.27f):Theme::panelAlt);
        }
        // Do not invalidate the scope and activity strip while the audio
        // producer has not advanced. During playback these still refresh at
        // 30 Hz, preserving animation timing and UI responsiveness.
        const unsigned count=processor.liveScopeSampleCount();
        if(count!=lastScopeSampleCount) {
            lastScopeSampleCount=count;
            if(idle.isVisible())idle.repaint();
            // The former top LIVE status was removed; do not repaint the
            // static SKELL title/version every incoming scope update.
            const int activeView=(int)processor.state.getRawParameterValue("mode")->load();
            if(activeView==0 || (activeView==3 && previousMode==0))
                // Original Simple waveform lives around y=317; repaint the
                // small live strip, not the entire row of rotary cards.
                repaint(juce::Rectangle<int>(79,300,1000,34));
        }
    }
    void mouseDown(const juce::MouseEvent& e)override {
        const auto point=e.getEventRelativeTo(this).getPosition();
        const auto micTrigger=juce::Rectangle<int>(chain.getX()+158,chain.getY()+5,42,38);
        if(micPanel.isVisible() && !micPanel.getBounds().contains(point)
           && !micTrigger.contains(point))
            micPanel.setVisible(false);
        if(browser.isVisible() && !browser.getBounds().contains(point) && !preset.getBounds().contains(point)) {
            browser.setVisible(false);preset.repaint();
        }
        if(settingsPanel.isVisible() && !settingsPanel.getBounds().contains(point)
           && !settings.getBounds().contains(point))
            settingsPanel.close();
    }
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::escapeKey && micPanel.isVisible()) {
            micPanel.setVisible(false);return true;
        }
        // Never steal Ctrl+Z from the browser's active search/editor field.
        if(!browser.isVisible() && !settingsPanel.isVisible() &&
           !micPanel.isVisible() && key.getModifiers().isCommandDown()) {
            const auto character=juce::CharacterFunctions::toLowerCase(key.getKeyCode());
            if(character=='z') {
                if(key.getModifiers().isShiftDown())
                    restoreHistory(redoStates,undoStates);
                else
                    restoreHistory(undoStates,redoStates);
                return true;
            }
            if(character=='y') {
                restoreHistory(redoStates,undoStates);
                return true;
            }
        }
        if(key==juce::KeyPress::escapeKey && settingsPanel.isVisible()){
            settingsPanel.close();return true;
        }
        if(key==juce::KeyPress::escapeKey&&browser.isVisible()){browser.setVisible(false);return true;}
        if(key==juce::KeyPress::escapeKey && slot.isVisible()){
            editingIdle=true;idle.setVisible(true);chain.setSelectedSlot(-1);
            idle.toFront(false);return true;
        }
        return false;
    }
private:
    // Preset-level history is intentionally separate from host automation
    // gestures: JUCE's APVTS is not given a new UndoManager, and no published
    // automation ID changes. All snapshots/restores run on the UI thread.
    struct HistorySnapshot {
        juce::MemoryBlock state;
        juce::File selectedPreset;
    };
    static constexpr size_t historyLimit=12;
    std::vector<HistorySnapshot> undoStates,redoStates;
    std::unique_ptr<HistorySnapshot> pendingHistory;
    bool makeSnapshot(HistorySnapshot& snapshot) {
        // Hosted VST3 state cannot currently be restored atomically with its
        // external native editor; skip it rather than claiming safe undo.
        for(int slotId=0;slotId<SkellPresetAudioProcessor::slotCount;++slotId)
            if(processor.slotType(slotId)==SkellPresetAudioProcessor::ExternalVST3)
                return false;
        processor.getStateInformation(snapshot.state);
        if(snapshot.state.getSize()==0 || snapshot.state.getSize()>2u*1024u*1024u)
            return false;
        snapshot.selectedPreset=browser.historySelection();
        return true;
    }
    void updateHistoryButtons() {
        undo.setEnabled(!undoStates.empty());
        redo.setEnabled(!redoStates.empty());
    }
    void pushHistory(std::vector<HistorySnapshot>& stack,HistorySnapshot snapshot) {
        if(stack.size()>=historyLimit)stack.erase(stack.begin());
        stack.push_back(std::move(snapshot));
    }
    void prepareHistory() {
        pendingHistory.reset();
        HistorySnapshot snapshot;
        if(makeSnapshot(snapshot))
            pendingHistory=std::make_unique<HistorySnapshot>(std::move(snapshot));
    }
    void commitHistory() {
        if(!pendingHistory)return;
        pushHistory(undoStates,std::move(*pendingHistory));
        pendingHistory.reset();
        redoStates.clear();
        updateHistoryButtons();
    }
    void restoreHistory(std::vector<HistorySnapshot>& from,
                        std::vector<HistorySnapshot>& to) {
        if(from.empty())return;
        HistorySnapshot current;
        if(!makeSnapshot(current))return;
        auto target=std::move(from.back());
        from.pop_back();
        // These snapshots were produced by getStateInformation itself.
        // In particular, no stale factory ID or slot-order renumbering occurs.
        processor.setStateInformation(target.state.getData(),
                                      (int)target.state.getSize());
        browser.restoreHistorySelection(target.selectedPreset);
        pushHistory(to,std::move(current));
        pendingHistory.reset();
        updateHistoryButtons();
        refresh();
    }
    SkellPresetAudioProcessorEditor& outer;
    SkellPresetAudioProcessor& processor;
    ChainPanel chain;SlotEditor slot;SkellIdleDeck idle;
    PresetBrowser browser;SkellMicModelPanel micPanel;
    SkellSettingsPanel settingsPanel;
    skellcore::world::WorldLink worldLink{"skell.preset","Skell Preset"};
    SkullThemeButton themeButton;
    BypassShade bypassShade;
    std::unique_ptr<skellcore::ui::LookAndFeel> skin;
    int selectedTheme=Theme::Obsidian,previousMode=1;
    bool rackOnRight=false,editingIdle=true;
    int lastInputMeter=-1,lastOutputMeter=-1;
    bool lastMicOn=false;
    int lastRecordingState=-1;
    unsigned lastScopeSampleCount=~0u;
    SkellActionButton preset,match,undo,redo,help,record,micButton;
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
        *this,"skell-preset-vst",2);
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
void SkellPresetAudioProcessorEditor::setMatchPageForScreenshot(int page) {
    canvas->setMatchPageForScreenshot(page);
}
void SkellPresetAudioProcessorEditor::setMicPanelForScreenshot(bool visible) {
    canvas->setMicPanelForScreenshot(visible);
}
void SkellPresetAudioProcessorEditor::setModOrbForScreenshot(bool enabled) {
    canvas->setModOrbForScreenshot(enabled);
}

void SkellPresetAudioProcessorEditor::dockRackForScreenshot(bool right) {
    canvas->dockRackForScreenshot(right);
}

void SkellPresetAudioProcessorEditor::showHomeForScreenshot() {
    canvas->showHomeForScreenshot();
}
bool SkellPresetAudioProcessorEditor::openRackContextMenuForScreenshot(int visualRow) {
    return canvas->openRackContextMenuForScreenshot(visualRow);
}
void SkellPresetAudioProcessorEditor::openModulePickerForScreenshot() {
    canvas->openModulePickerForScreenshot();
}

void SkellPresetAudioProcessorEditor::openInstalledVST3BrowserForScreenshot() {
    canvas->openInstalledVST3BrowserForScreenshot();
}
