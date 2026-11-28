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
#include "ui/SkellModeSwitch.h"
#include "ui/SkellMacroKnob.h"
#include "ui/SkellStereoSeparationKnob.h"
#include "ui/SkellSettingsPanel.h"
#include "ui/SkellIdleDeck.h"
#include "ui/SkellMatchPanel.h"
#include "chain/LiveMatchOptimizer.h"
#include "chain/AlchemistNativePreset.h"
#include "chain/NativeSoundMatchTrials.h"
// Select the bundled UI implementation before headers-only dependency snapshots.
#if __has_include("../SkellCore/include/SkellCore/UI.h")
#include "../SkellCore/include/SkellCore/UI.h"
#else
#include <SkellCore/UI.h>
#endif
#include "ui/SkellMoonButton.h"
#include "ui/SkellRoutingPage.h"
#include "ui/SkellLiveEffectPanel.h"
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include <limits>
#include <optional>

namespace {
// Presentation-only hover handles. Click or deliberate axis-aligned drag.
class WorkspaceEdge final:public juce::Component,public juce::SettableTooltipClient {
public:
    enum Direction { Left,Right,Up,Down };
    explicit WorkspaceEdge(bool vertical=false):verticalDrag(vertical){
        setWantsKeyboardFocus(true);
        setMouseCursor(vertical?juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::LeftRightResizeCursor);
    }
    std::function<void()> onAction;
    void configure(Direction d,const juce::String& title){
        direction=d;setTitle(title);setTooltip(title);repaint();
    }
    void setContextHover(bool show){if(contextHover!=show){contextHover=show;repaint();}}
    void visibilityChanged()override{
        if(!isVisible()){hover=false;pressed=false;contextHover=false;}
    }
    bool testHoverPaint() {
        // Exercise the real mouse callbacks and check software pixels, not
        // merely the logical hover flag. No native cursor warp needed.
        const auto source=juce::Desktop::getInstance().getMainMouseSource();
        const auto pos=getLocalBounds().getCentre().toFloat();
        const auto now=juce::Time::getCurrentTime();
        const juce::MouseEvent ev(source,pos,juce::ModifierKeys(),0.f,0.f,0.f,0.f,0.f,
            this,this,now,pos,now,0,false);
        const auto painted=[this](){
            juce::Image image(juce::Image::ARGB,getWidth(),getHeight(),true,juce::SoftwareImageType{});
            {juce::Graphics g(image);paint(g);}
            for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)
                if(image.getPixelAt(x,y).getAlpha()!=0)return true;
            return false;
        };
        setContextHover(false);mouseExit(ev);const bool resting=!painted();
        mouseEnter(ev);const bool shown=painted();mouseExit(ev);
        return resting&&shown&&!painted();
    }
    bool releaseGesture(int dx,int dy){
        const int along=std::abs(verticalDrag?dy:dx),across=std::abs(verticalDrag?dx:dy);
        const bool correctDirection=!verticalDrag || (direction==Up?dy<0:dy>0);
        if((along<=4&&across<=4)||(along>=10&&along>=across&&correctDirection)){
            if(onAction)onAction();
            return true;
        }
        return false;
    }
    void mouseEnter(const juce::MouseEvent& ev)override{hover=true;pointer=ev.getPosition();repaint();}
    void mouseMove(const juce::MouseEvent& ev)override{pointer=ev.getPosition();repaint();}
    void mouseExit(const juce::MouseEvent&)override{hover=false;repaint();}
    void mouseDown(const juce::MouseEvent& ev)override{pressed=ev.mods.isLeftButtonDown();repaint();}
    void mouseUp(const juce::MouseEvent& ev)override{
        if(!pressed)return;
        pressed=false;
        releaseGesture(ev.getDistanceFromDragStartX(),ev.getDistanceFromDragStartY());repaint();
    }
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::returnKey || key==juce::KeyPress::spaceKey){if(onAction)onAction();return true;}
        return false;
    }
    void focusGained(FocusChangeType)override{repaint();}
    void focusLost(FocusChangeType)override{pressed=false;repaint();}
    void paint(juce::Graphics& g)override{
        if(!(hover||pressed||contextHover))return;
        const auto area=getLocalBounds().toFloat();
        const float w=verticalDrag?juce::jmin(54.f,area.getWidth()):area.getWidth();
        const float h=verticalDrag?area.getHeight():juce::jmin(44.f,area.getHeight());
        const float cx=hover&&verticalDrag?juce::jlimit(w*.5f,area.getWidth()-w*.5f,(float)pointer.x):area.getCentreX();
        const float cy=hover&&!verticalDrag?juce::jlimit(h*.5f,area.getHeight()-h*.5f,(float)pointer.y):area.getCentreY();
        auto r=juce::Rectangle<float>(cx-w*.5f,cy-h*.5f,w,h).reduced(1.f);
        const bool active=hover||pressed;
        // Bare chevrons: no enclosure or background.
        const auto c=r.getCentre();juce::Path arrow;
        if(direction==Left||direction==Right){
            const float d=direction==Right?1.f:-1.f;
            arrow.startNewSubPath(c.x-d*2.4f,c.y-4.f);arrow.lineTo(c.x+d*2.4f,c.y);arrow.lineTo(c.x-d*2.4f,c.y+4.f);
        }else{
            const float d=direction==Down?1.f:-1.f;
            arrow.startNewSubPath(c.x-4.f,c.y-d*2.4f);arrow.lineTo(c.x,c.y+d*2.4f);arrow.lineTo(c.x+4.f,c.y-d*2.4f);
        }
        g.setColour(Theme::bg.withAlpha(active?.84f:.52f));
        g.strokePath(arrow,juce::PathStrokeType(3.7f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(active?Theme::green:Theme::white.withAlpha(.32f));
        g.strokePath(arrow,juce::PathStrokeType(active?1.9f:1.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
private:
    juce::Point<int> pointer;
    Direction direction=Left;bool verticalDrag=false,contextHover=false,hover=false,pressed=false;
};
void setParam(SkellPresetAudioProcessor& p,const juce::String& id,float physical) {
    if(auto* v=p.state.getParameter(id))v->setValueNotifyingHost(v->convertTo0to1(physical));
}
}

class SkellPresetAudioProcessorEditor::Canvas final:public juce::Component,private juce::Timer {
public:
    Canvas(SkellPresetAudioProcessorEditor& owner,SkellPresetAudioProcessor& p)
    : outer(owner),processor(p),chain(p),slot(p),idle(p),routing(p),browser(p),micPanel(p),skin(std::make_unique<skellcore::ui::LookAndFeel>(skellcore::ui::Theme::neon())) {
        Theme::selectFor(processor.state);
        setLookAndFeel(skin.get());
        setSize(Theme::width,Theme::height);
        setWantsKeyboardFocus(true);
        addAndMakeVisible(chain);addAndMakeVisible(slot);
        addAndMakeVisible(idle);
        addChildComponent(routing);
        routing.onClose=[this]{closeRouting();};
        routing.onBeforeChange=[this]{prepareHistory();};
        routing.onAfterChange=[this]{commitHistory();};
        routing.onEdit=[this](int id){routing.setVisible(false);rackHidden=false;macroOpen=false;if(chain.onSidechainEdit)chain.onSidechainEdit(id);};
        chain.onRoutingRequest=[this](int id){openRouting(id);};
        addChildComponent(focusVisual);focusVisual.setInterceptsMouseClicks(false,false);
        for(auto* edge:{&rackOuterEdge,&rackInnerEdge,&visualLeftEdge,&visualRightEdge,&drawerEdge})addAndMakeVisible(*edge);
        rackOuterEdge.onAction=rackInnerEdge.onAction=[this]{toggleRackVisibility();};
        visualLeftEdge.onAction=visualRightEdge.onAction=[this]{if(matchPanel.isVisible())closeMatch();toggleRoutingEdge();};
        drawerEdge.onAction=[this]{if(matchPanel.isVisible())closeMatch();toggleDrawerEdge();};
        addAndMakeVisible(moonButton);
        moonButton.setTooltip("Show / hide master controls");
        moonButton.onClick=[this]{if(matchPanel.isVisible())closeMatch();toggleView();};
        stageButton.setButtonText("STAGE_HOME");
        stageButton.setTooltip("Click SKELLPRESET to return to the stage");
        stageButton.setTitle("Return to the stage");
        style(stageButton);addAndMakeVisible(stageButton);
        stageButton.onClick=[this]{browser.setVisible(false);if(chain.onClearSelection)chain.onClearSelection();};
        chain.setSelectedSlot(-1);
        themeButton.onClick=[this] {setTheme(Theme::nextVariant(selectedTheme));};
        addAndMakeVisible(themeButton);
        previousMode=juce::jlimit(0,1,(int)processor.state.state.getProperty("uiPreviousMode",1));
        rackOnRight=(bool)processor.state.state.getProperty("uiRackOnRight",false);
        chain.setDockedRight(rackOnRight);
        chain.onDockRequest=[this](bool right){dockRack(right);};

        chain.onSelection=[this](int s){
            if(matchPanel.isVisible())closeMatch();
            if(routing.isVisible()){routing.selectTarget(s);chain.setSelectedSlot(s);return;}
            editingIdle=false;macroOpen=false;
            micPanel.setVisible(false); // effect editor must never sit behind MIC
            idle.setVisible(false);
            slot.select(s);chain.setSelectedSlot(s);refresh();
        };
        chain.onSidechainEdit=[this](int s){
            if(chain.onSelection)chain.onSelection(s);
            slot.openSidechainControls();
        };
        chain.onClearSelection=[this]{
            if(matchPanel.isVisible())closeMatch();
            routing.setVisible(false);editingIdle=true;
            slot.setHostedEditorPresentation(false);
            micPanel.setVisible(false); // show complete Home deck, not stale MIC
            slot.dismissTransientOverlays();
            chain.setSelectedSlot(-1);
            idle.setVisible(true);
            idle.toFront(false);refresh();
        };
        slot.onBackgroundClick=[this] { if(chain.onClearSelection)chain.onClearSelection(); };
        chain.onReplaceRequest=[this](int s){
            if(matchPanel.isVisible())closeMatch();
            routing.setVisible(false);editingIdle=false;macroOpen=false;rackHidden=false;idle.setVisible(false);
            micPanel.setVisible(false);
            slot.select(s);
            chain.setSelectedSlot(s);
            refresh();slot.openModulePicker();
        };

        undo.setButtonText(juce::String::charToString(0x21B6));
        redo.setButtonText(juce::String::charToString(0x21B7));
        undo.setTitle("Undo");redo.setTitle("Redo");
        undo.setTooltip("Undo preset load, native rack removal or routing (Ctrl+Z). Hosted VST3 state excluded.");
        redo.setTooltip("Redo preset load, native rack removal or routing (Ctrl+Y or Ctrl+Shift+Z)");
        style(undo);style(redo);
        addAndMakeVisible(undo);addAndMakeVisible(redo);
        undo.onClick=[this]{if(matchPanel.isVisible())closeMatch();restoreHistory(undoStates,redoStates);};
        redo.onClick=[this]{if(matchPanel.isVisible())closeMatch();restoreHistory(redoStates,undoStates);};
        updateHistoryButtons();
        preset.setButtonText(p.presetName().toUpperCase()+"  ▼");
        preset.onClick=[this]{
            if(matchPanel.isVisible())closeMatch();
            micPanel.setVisible(false);
            if(browser.isVisible())browser.setVisible(false);else {slot.setHostedEditorPresentation(false);slot.dismissTransientOverlays();browser.open();}
        };
        style(preset);addAndMakeVisible(preset);
        matchButton.setButtonText("MATCH");matchButton.setTooltip("Analyze isolated dry vocals and create an undoable native rack preset");
        style(matchButton);addAndMakeVisible(matchButton);
        for(auto* b:std::array<juce::Component*,6>{{&preset,&matchButton,&undo,&redo,&settings,&powerButton}})b->setComponentID("header-control");
        addChildComponent(matchPanel);matchPanel.setThemeSource(processor.state);
        matchButton.onClick=[this]{if(matchPanel.isVisible())closeMatch();else beginMatch();};
        idle.onMicrophoneMatch=[this]{if(!matchPanel.isVisible())beginMatch();};
        matchPanel.onClose=[this]{closeMatch();};
        matchPanel.onRetry=[this]{beginMatch();};
        matchPanel.onStart=[this]{startMatchListening();};
        matchPanel.onBeat=[this]{chooseMatchFile(false,true);};
        matchPanel.onRefine=[this]{matchPanel.update(SkellMatchPanel::RefineText,"Describe a change to your current Auto Mix.",1.f,0.f,true);};
        matchPanel.onBack=[this]{matchPanel.update(SkellMatchPanel::Done,matchLastDetail,1.f,0.f,true);};
        matchPanel.onSubmit=[this](juce::String request){
            auto next=matchIntent;if(!matchLastRecipe||!skellpreset::liveMatch::parseMixIntent(request.toStdString(),next)){matchPanel.update(SkellMatchPanel::RefineText,"Try a supported change: more aggressive, clearer, brighter, less reverb, wider or quieter.",1.f,0.f,true);return;}
            finishMatch(matchDry,false,matchAudio,&next);
        };
        matchPanel.onListen=[this]{closeMatch();if(chain.onSelection)chain.onSelection(processor.slotIdAt(0));};
        matchPanel.onUpload=[this]{chooseMatchFile(false);};
        matchPanel.onReference=[this]{chooseMatchFile(true);};
        micButton.setButtonText("MIC");
        micButton.setComponentID("rack-hardware");
        micButton.setTooltip("Open dedicated input microphone modeling panel");
        style(micButton);addAndMakeVisible(micButton);
        micButton.onClick=[this] {
            if(matchPanel.isVisible())closeMatch();
            if(browser.isVisible())browser.setVisible(false);
            if(!micPanel.isVisible()){slot.setHostedEditorPresentation(false);slot.dismissTransientOverlays();}
            micPanel.refresh();
            micPanel.setVisible(!micPanel.isVisible());
            micButton.setToggleState(micPanel.anyActive() || micPanel.isVisible(),juce::dontSendNotification);
            if(micPanel.isVisible())micPanel.toFront(true);
        };
        // Keep the original test-only button adapter, but put the actual
        // microphone trigger at the top right of the effect rack.
        micButton.setVisible(false);
        chain.onMicRequest=[this] {if(micButton.onClick)micButton.onClick();};
        // One workspace; the former mode automation remains for saved sessions.
        style(powerButton);addAndMakeVisible(powerButton);
        powerButton.onClick=[this] {
            if(matchPanel.isVisible())closeMatch();
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
                // Repaint only the dial window, never the whole Simple page.
                if(drawerVisible())repaint(slot.getBounds().withTrimmedTop(slot.getHeight()-206));
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
            s.onValueChange=[this]{if(drawerVisible())repaint(slot.getBounds().withTrimmedTop(slot.getHeight()-206));};
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
        // Output-stage stereo separation. Same APVTS binding path as the other
        // master controls, so automation and host gestures are untouched.
        stereoSeparation.setThemeSource(&p.state);
        addAndMakeVisible(stereoSeparation);
        stereoSeparationAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.state,"stereoSeparation",stereoSeparation);
        // The attachment replaces our formatter with the host one. Re-apply so
        // the dial and its readout keep the percentage + direction wording.
        stereoSeparation.applyReadoutFormat();
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
        for(auto& value:masterValues){
            value.setJustificationType(juce::Justification::centred);
            value.setFont(Theme::font(14.f,true));
            value.setBorderSize(juce::BorderSize<int>(0));
            value.setMinimumHorizontalScale(.75f);
            value.setColour(juce::Label::textColourId,Theme::white);
            value.setInterceptsMouseClicks(false,false);
            addAndMakeVisible(value);
        }
        // SliderAttachment owns its listener; onValueChange is additive.
        for(auto* s:{&input,&mix,&output})s->onValueChange=[this]{syncMasterReadouts();};
        stereoSeparation.onValueChange=[this]{syncMasterReadouts();};
        syncMasterReadouts();
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
        settings.setTooltip("Resize interface");
        style(settings);
        addAndMakeVisible(settings);
        settings.onClick=[this] {
            if(matchPanel.isVisible())closeMatch();
            browser.setVisible(false);
            micPanel.setVisible(false);
            if(settingsPanel.isVisible())settingsPanel.close();
            else {
                slot.setHostedEditorPresentation(false);
                settingsPanel.open(sizeSelect.getSelectedId()-1);
            }
        };
        settingsPanel.onSizeChange=[this](int choice){
            if(choice>=1 && choice<=4)
                sizeSelect.setSelectedId(choice,juce::sendNotificationSync);
        };
        settingsPanel.onClose=[this]{settings.grabKeyboardFocus();};
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
        chain.onBeforeRemove=[this]{if(matchPanel.isVisible())closeMatch();prepareHistory();};
        chain.onAfterRemove=[this]{commitHistory();};
        browser.onBeforeLoad=[this]{if(matchPanel.isVisible())closeMatch();prepareHistory();};
        browser.onLoadFailed=[this]{pendingHistory.reset();updateHistoryButtons();};
        browser.onLoaded=[this]{
            routing.setVisible(false);commitHistory();
            editingIdle=false;idle.setVisible(false);
            slot.select(0);chain.setSelectedSlot(0);refresh();
        };
        browser.onCoverChanged=[this]{repaint();};
        browser.onClose=[this]{preset.repaint();};
        browser.onVisibilityChanged=[this]{syncPresetHeader();};
        addAndMakeVisible(bypassShade);bypassShade.setVisible(false);
        addAndMakeVisible(browser);browser.setVisible(false);
        settingsPanel.toFront(false);
        addAndMakeVisible(micPanel);micPanel.setVisible(false);
        micPanel.onClose=[this]{micButton.repaint();};
        setTheme((int)processor.state.state.getProperty("uiTheme",0),false);
        addMouseListener(this,true); // Outside clicks on child panels dismiss the overlay.
        resized();refresh();startTimerHz(30);
    }
    void syncMasterReadouts(){
        const std::array<juce::Slider*,3> sliders{{&input,&mix,&output}};
        for(size_t i=0;i<sliders.size();++i){
            const auto value=sliders[i]->getTextFromValue(sliders[i]->getValue());
            if(masterValues[i].getText()!=value)masterValues[i].setText(value,juce::dontSendNotification);
        }
        // Sep reads out percentage plus direction, so the neutral detent is
        // unambiguous and a widen or merge move is never colour-only.
        const auto sep=juce::roundToInt(stereoSeparation.getValue());
        const auto text=SkellStereoSeparationKnob::readoutFor((double)sep);
        if(masterValues[3].getText()!=text)
            masterValues[3].setText(text,juce::dontSendNotification);
        // The direction prefix needs more room than a plain dB readout and
        // must never ellipsize at the 70% default scale.
        masterValues[3].setMinimumHorizontalScale(.62f);
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
        steps.emplace_back(&stageButton,"BACK TO THE STAGE",
            "Click the SKELLPRESET wordmark to return to the performer. Click the moon to show or hide sound controls.");
        if(chain.isVisible()) {
            steps.emplace_back(&chain,"YOUR EFFECT CHAIN",
                "Scroll the rack, drag to reorder, double-click to replace or use + to add a processor. Studio Compressor, Studio Limiter and Studio Delay have dedicated effect controls.");
            if(slot.isVisible())steps.emplace_back(&slot,"EDIT A SLOT",
                "Adjust the selected effect here. Rack slots keep their own independent controls, order and automation.");
        }
        steps.emplace_back(&chain,"MIC MODEL + NOISE GATE",
            "Click the rack's MIC icon. MIC MODEL shapes the recorded microphone; NOISE GATE has its own ON switch, threshold, hold and release. Both start independent, and the gate starts OFF.");
        if(idle.isVisible())steps.emplace_back(&moonButton,"SOUND CONTROLS",
            "Click the moon to expose Tone, Punch, Space, Grit, Input, Mix and Output.");
        steps.emplace_back(&powerButton,"POWER / BYPASS",
            "Green means your effect rack is active. Click the power icon to bypass without losing any settings.");
        if(mix.isVisible())steps.emplace_back(&mix,"BLEND YOUR SOUND",
            "The stage drawer includes Input, dry/wet Mix, and Output.");
        steps.emplace_back(&settings,"RESIZE",
            "Choose the interface size from the dropdown.");
        return steps;
    }
    skellcore::ui::GuidedTourPalette tourPalette() const {
        return {Theme::green,Theme::panel,Theme::white,Theme::muted,
                juce::Colour(0xd9000000)};
    }
    ~Canvas()override {stopTimer();closeMatch();matchPool.removeAllJobs(true,-1);processor.closeHostedVST3Editors();setLookAndFeel(nullptr);}
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
        for(juce::TextButton* b:std::array<juce::TextButton*,8>{{
            &preset,&matchButton,&undo,&redo,&powerButton,&settings,&record,&micButton}})
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
        for(auto& value:masterValues)value.setColour(juce::Label::textColourId,Theme::white);
        sizeSelect.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
        sizeSelect.setColour(juce::ComboBox::textColourId,Theme::white);
        sizeSelect.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        style(stageButton);settingsPanel.applyTheme();routing.applyTheme();
        chain.repaint();slot.applyTheme();browser.applyTheme();
        idle.repaint();settingsPanel.repaint();
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
        closeMatch();
        routing.setVisible(false);rackHidden=false;dockRack(false);
        variant=Theme::clampVariant(variant);
        processor.state.state.setProperty("uiTheme",variant,nullptr);
        setTheme(variant,false);
        if(workspaceMode==3){
            previousMode=1;
            processor.state.state.setProperty("uiPreviousMode",previousMode,nullptr);
        }
        macroOpen=workspaceMode==0;
        editingIdle=macroOpen;
        if(macroOpen)chain.setSelectedSlot(-1);
        setParam(processor,"mode",(float)(workspaceMode==3?3:1));
        refresh();
        if(showBrowser)browser.open();
        else browser.setVisible(false);
        repaint();
    }
    bool configureBrowserForScreenshot(int page,int width,int height,bool list) {
        const bool result=browser.configureDirectoryForScreenshot(page,width,height);
        browser.setListViewForScreenshot(list);return result;
    }
    // Actual button, input callback and preset application; only the input
    // stimulus is synthetic. This is not a separate mock component.
    bool smartMatchForScreenshot(int scene){
        processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
        configureForScreenshot(0,scene==0?0:1,false);
        if(scene==0){macroOpen=false;refresh();}
        const auto original=processor.presetName();
        if(scene==0){if(!idle.testMicrophoneMatchTarget())return false;}else if(matchButton.onClick)matchButton.onClick();
        if(!matchPanel.isVisible()||!editingIdle||processor.presetName()!=original)return false;
        if(scene==0)return matchPanel.hasOriginalArtwork()&&matchPanel.testPlacement()&&matchPanel.testChooseStyle()&&!matchListening;
        matchPanel.testStart();
        processor.prepareToPlay(48000.,512);
        juce::MidiBuffer midi;juce::AudioBuffer<float> b(2,512);
        const int blocks=scene==1?1900:scene==4?150:660;
        for(int k=0;k<blocks;++k){
            for(int n=0;n<512;++n){
                const double time=(k*512+n)/48000.;
                const float x=scene==1?0.f:(float)((.09+.025*std::sin(8.*time))*std::sin(1382.3*time)+.014*std::sin(25132.7*time));
                b.setSample(0,n,x);b.setSample(1,n,x);
            }
            processor.processBlock(b,midi);
        }
        tickMatch();
        if(scene==1)return matchPanel.isVisible()&&processor.presetName()==original;
        if(scene==4){updateHeaderMeters(true);idle.setAudioFresh(processor.liveScopeSampleCount()>0);idle.setMeasuredPeaks(processor.getInputPeak(),processor.getOutputPeak());idle.tickAnimation(juce::Time::getMillisecondCounterHiRes(),true);matchPanel.advanceAnimationForScreenshot(1.8f);const auto p95=matchPanel.benchmarkOrbForScreenshot();return std::isfinite(p95)&&p95<33.33&&matchListening&&matchPanel.isVisible()&&processor.presetName()==original;}
        // Only the headless test waits for the owned optimizer. Normal UI
        // polls asynchronously and never blocks the message thread.
        for(int i=0;matchOptimization&&i<3000;++i){juce::Thread::sleep(10);tickMatch();}
        const bool applied=matchPanel.isVisible()&&processor.presetName()=="Match / My Vocal"&&processor.slotType(0)==SkellPresetAudioProcessor::StudioEQFX&&processor.getLatencySamples()==0;
        if(scene==2)return applied;
        if(scene==5)return applied&&matchPanel.testRefine();
        if(undo.onClick)undo.onClick();
        const bool undone=processor.presetName()==original;
        if(redo.onClick)redo.onClick();
        const bool redone=processor.presetName()=="Match / My Vocal";
        closeMatch();processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);refresh();
        return applied&&undone&&redone;
    }
    void setMatchPageForScreenshot(int page) {
        editingIdle=false;idle.setVisible(false);refresh();
        slot.setMatchPageForScreenshot(page);
    }
    void setSuiteForScreenshot(int page,bool stylesOpen) {
        editingIdle=false;idle.setVisible(false);refresh();
        slot.setSuiteForScreenshot(page,stylesOpen);
    }
    void setSuiteSpectrumForScreenshot(bool enabled) {
        editingIdle=false;idle.setVisible(false);refresh();
        slot.setSuiteSpectrumForScreenshot(enabled);
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
    void setSettingsPanelForScreenshot(bool visible) {
        if(visible) {
            browser.setVisible(false);
            micPanel.setVisible(false);
            slot.dismissTransientOverlays();
            settingsPanel.open(sizeSelect.getSelectedId()-1);
        } else {
            // Scene setup must not leave a stale settings overlay on the
            // following actual-editor screenshot.
            settingsPanel.setVisible(false);
        }
    }
    void setModOrbForScreenshot(bool enabled) {
        if(enabled){editingIdle=false;idle.setVisible(false);refresh();}
        slot.setModOrbForScreenshot(enabled);
    }
    void showRoutingForScreenshot(){openRouting(processor.slotIdAt(0));}
    bool updateLiveVisualForScreenshot(){
        updateHeaderMeters(true);if(focusVisual.isVisible()){updateFocusVisual();return focusVisual.hasSignal();}
        return slot.updateLiveVisualForScreenshot();
    }
    void configureEdgesForScreenshot(bool right,bool hide,bool controls,bool sidechain){
        dockRack(right);
        if(hide!=rackHidden)toggleRackVisibility();
        if(controls!=macroOpen)toggleDrawerEdge();
        if(sidechain!=routing.isVisible())toggleRoutingEdge();
        refresh();
    }
    void updateStageForScreenshot(double nowMs){
        updateHeaderMeters(true);
        idle.setAudioFresh(processor.liveScopeSampleCount()>0);
        idle.setMeasuredPeaks(processor.getInputPeak(),processor.getOutputPeak());
        idle.tickAnimation(nowMs,true);
    }
    bool showHistoryForScreenshot(bool redoAvailable){
        undoStates.clear();redoStates.clear();pendingHistory.reset();updateHistoryButtons();
        processor.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
        browser.loadFactoryForHistoryTest(skellpreset::deuce_v25::factoryIndex);
        if(redoAvailable)undo.onClick();
        return redoAvailable?(redo.isEnabled()&&!undo.isEnabled()):(undo.isEnabled()&&!redo.isEnabled());
    }
    void showHomeForScreenshot() {
        if(chain.onClearSelection)chain.onClearSelection();
        refresh();
    }
    bool openRackContextMenuForScreenshot(int visualRow) {
        return chain.openContextMenuForScreenshot(visualRow);
    }
    void openModulePickerForScreenshot() {
        editingIdle=false;idle.setVisible(false);refresh();
        slot.openModulePickerForScreenshot();
    }
    void openInstalledVST3BrowserForScreenshot() {
        editingIdle=false;idle.setVisible(false);refresh();
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
    // Stage drawer is presentation state only, never a host automation gesture.
    void openRouting(int target){
        if(!routing.isVisible())routingReturnIdle=editingIdle;
        browser.setVisible(false);micPanel.setVisible(false);settingsPanel.setVisible(false);
        editingIdle=false;slot.dismissTransientOverlays();slot.setHostedEditorPresentation(false);
        routing.open(target);refresh();
    }
    void toggleView() {
        routing.setVisible(false);browser.setVisible(false);micPanel.setVisible(false);
        editingIdle=true;chain.setSelectedSlot(-1);
        slot.dismissTransientOverlays();macroOpen=!macroOpen;
        refresh();
    }
    void closeRouting(){routing.setVisible(false);editingIdle=routingReturnIdle;refresh();}
    void toggleRoutingEdge(){
        if(browser.isVisible()||settingsPanel.isVisible()||micPanel.isVisible())return;
        if(routing.isVisible())closeRouting();
        else openRouting(editingIdle?processor.slotIdAt(0):slot.selectedSlot());
    }
    bool nativeVisualAvailable()const noexcept{
        const int type=processor.slotType(slot.selectedSlot());
        return type!=SkellPresetAudioProcessor::Empty && type!=SkellPresetAudioProcessor::ExternalVST3;
    }
    bool focusVisible()const noexcept{
        return !editingIdle&&!routing.isVisible()&&nativeVisualAvailable()&&(rackHidden||macroOpen);
    }
    bool drawerVisible()const noexcept{return !routing.isVisible()&&macroOpen&&(editingIdle||nativeVisualAvailable());}
    bool visualNavigationVisible()const noexcept{
        return !routing.isVisible()&&(editingIdle||focusVisible());
    }
    juce::Rectangle<int> liveNavigationBounds()const noexcept{
        return focusVisible()?focusVisual.getBounds():idle.getBounds();
    }
    void updateVisualHover(){
        const bool inside=visualNavigationVisible()&&!browser.isVisible()&&!settingsPanel.isVisible()&&!micPanel.isVisible()
            && isMouseOver(true)&&liveNavigationBounds().contains(getMouseXYRelative());
        for(auto* edge:{&visualLeftEdge,&visualRightEdge,&drawerEdge})edge->setContextHover(inside);
        moonButton.setVisualHover(inside&&editingIdle);
    }
    juce::Rectangle<int> workspaceBounds()const noexcept{
        return rackHidden?juce::Rectangle<int>(14,90,1140,561):juce::Rectangle<int>(rackOnRight?14:247,90,906,561);
    }
    void toggleRackVisibility(){
        if(browser.isVisible()||settingsPanel.isVisible()||micPanel.isVisible())return;
        rackHidden=!rackHidden;slot.dismissTransientOverlays();refresh();
    }
    void toggleDrawerEdge(){
        if(routing.isVisible()||browser.isVisible()||settingsPanel.isVisible()||micPanel.isVisible())return;
        if(!editingIdle&&!nativeVisualAvailable())return;
        macroOpen=!macroOpen;slot.dismissTransientOverlays();refresh();
    }
    void updateFocusVisual(){
        if(!focusVisible())return;
        const bool active=(int)processor.state.getRawParameterValue("mode")->load()!=3 &&
            !browser.isVisible()&&!settingsPanel.isVisible()&&!micPanel.isVisible();
        processor.requestSlotVisual(active?slot.selectedSlot():-1);
        if(focusVisual.update(processor,slot.selectedSlot()))focusVisual.repaint();
    }
    juce::StringArray getScreenshotLayoutIssues() const {
        juce::StringArray issues;
        const auto canvasBounds=juce::Rectangle<int>(0,0,Theme::width,Theme::height);
        const std::array<const juce::Component*,13> all{{&themeButton,&preset,
            &stageButton,&powerButton,&chain,&slot,&input,&mix,&output,&sizeSelect,&settings,&micButton,&stereoSeparation}};
        for(const auto* child:all){
            if(child->getWidth()<=0 || child->getHeight()<=0 || !canvasBounds.contains(child->getBounds()))
                issues.add("Control outside canvas bounds: " + child->getName());
        }
        if(record.isVisible())
            issues.add("Removed REC WAV header button still visible");
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
            &preset,&matchButton,&stageButton,&undo,&redo,&settings,&powerButton}};
        for(size_t i=0;i<top.size();++i)
            for(size_t j=i+1;j<top.size();++j)
                if(top[i]->getBounds().intersects(top[j]->getBounds()))
                    issues.add("Overlapping top-bar controls " + juce::String((int)i)
                               + " / " + juce::String((int)j));
        if(headerMeterBounds().getCentreY()!=preset.getBounds().getCentreY() || headerMeterBounds().getX()<stageButton.getRight() || headerMeterBounds().getRight()>preset.getX() ||
           headerMeterBounds().getBottom()>75 || headerMeterBounds().getWidth()<meterWidth)
            issues.add("Header meters overlap controls or leave the header");
        if(powerButton.getX()<=settings.getRight() ||
           Theme::width-powerButton.getRight()>22 ||
           matchButton.getX()-preset.getRight()!=4 || undo.getX()-matchButton.getRight()!=4 || redo.getX()-undo.getRight()!=4 ||
           settings.getX()-redo.getRight()!=4 || powerButton.getX()-settings.getRight()!=4 ||
           stageButton.getRight()>preset.getX())
            issues.add("Owner header order changed");
        if(undo.getWidth()>34 || redo.getWidth()>34 ||
           undo.getHeight()!=34 || redo.getHeight()!=34 ||
           undo.getWidth()!=undo.getHeight() || redo.getWidth()!=redo.getHeight() ||
           undo.getTitle()!="Undo" || redo.getTitle()!="Redo")
            issues.add("Undo and Redo icons have become oversized again");
        if(!micPanel.hasBeenMoved() &&
           (micPanel.getWidth()>330 || micPanel.getHeight()>325 ||
            micPanel.getWidth()<312 || micPanel.getHeight()<308))
            issues.add("Compact draggable MIC popup is no longer sized correctly");
        if(!canvasBounds.contains(browser.getBounds()))issues.add("Preset browser is clipped");
        if(!settings.isVisible()||sizeSelect.isVisible())issues.add("UI size control not relocated to settings");
        if(!settingsPanel.testLayoutForTest())
            issues.add("Resize dropdown is clipped");
        if(!slot.testPopupLayoutsForTest())
            issues.add("Effect / VST3 / EQ / wet duck popups have clipping or overlay conflicts");
        if(!juce::Rectangle<int>(0,0,Theme::width,Theme::height)
            .contains(settingsPanel.getBounds()))
            issues.add("Themed Settings overlay clipped by canvas");
        if(!rackHidden && chain.getBounds().intersects(slot.getBounds()))
            issues.add("Rack overlaps the module editor");
        if(slot.getHeight()!=chain.getHeight())
            issues.add("Advanced control deck does not fill the rack height");
        if(!rackHidden && rackOnRight!=(chain.getX()>slot.getX()))
            issues.add("Rack dock state disagrees with displayed position");
        const int current=(int)processor.state.getRawParameterValue("mode")->load();
        const bool drawer=drawerVisible();
        for(const auto& control:macro)
            if(control.isVisible()!=drawer)issues.add("Stage macro visibility differs from drawer");
        if(chain.isVisible()==rackHidden || slot.isVisible()!=(!editingIdle && !routing.isVisible() && !focusVisible()) || idle.isVisible()!=editingIdle || focusVisual.isVisible()!=focusVisible())
            issues.add("Unified rack/stage visibility regressed");
        for(const auto* edge:{&rackOuterEdge,&rackInnerEdge,&visualLeftEdge,&visualRightEdge,&drawerEdge})
            if(edge->isVisible()&&!canvasBounds.contains(edge->getBounds()))
                issues.add("Workspace edge outside canvas");
        for(const auto* edge:{&visualLeftEdge,&visualRightEdge,&drawerEdge}){
            if(edge->isVisible()!=visualNavigationVisible())
                issues.add("Visual arrow leaked onto a non-live editor");
            if(edge->isVisible()&&!liveNavigationBounds().contains(edge->getBounds()))
                issues.add("Visual arrow outside live visualization");
        }
        if(focusVisible() && !workspaceBounds().contains(focusVisual.getBounds()))
            issues.add("Focus visualization escapes workspace");
        for(const auto* master:{&input,&mix,&output})
            if(master->isVisible()!=drawer)issues.add("Master knob visibility differs from drawer");
        const std::array<const juce::Slider*,3> masterSliders{{&input,&mix,&output}};
        for(size_t i=0;i<3;++i) {
            const auto& value=masterValues[i];
            const auto& slider=*masterSliders[i];
            const auto name=juce::String(i==0?"Input":i==1?"Mix":"Output");
            if(value.isVisible()!=drawer)issues.add(name+" drawer readout visibility mismatch");
            if(!canvasBounds.contains(value.getBounds()) || value.getBounds().intersects(slider.getBounds()))
                issues.add(name+" drawer readout geometry mismatch");
            // JUCE's getTextFromValue is non-const. The installed formatter
            // is callable from a const slider and preserves the actual sign threshold.
            if(!slider.textFromValueFunction ||
               value.getText()!=slider.textFromValueFunction(slider.getValue()))
                issues.add(name+" drawer readout stale: "+value.getText());
        }
        if(masterValues[3].isVisible())issues.add("Stereo direction text must remain hidden");
        if(current==3 && !bypassShade.isVisible())issues.add("Bypass overlay missing");
        if(powerButton.isBypassed()!=(current==3))issues.add("Power indicator mismatch");
        if(stereoSeparation.isVisible()==rackHidden || micButton.isVisible()==rackHidden ||
           !chain.getBounds().contains(stereoSeparation.getBounds()) ||
           !chain.getBounds().contains(micButton.getBounds()) ||
           stereoSeparation.getRight()>micButton.getX() ||
           micButton.getX()-stereoSeparation.getRight()>6 ||
           stereoSeparation.getWidth()!=micButton.getWidth() ||
           stereoSeparation.getHeight()!=micButton.getHeight())
            issues.add("Rack MIC/stereo hardware grouping regressed");
        if(moonButton.isVisible()!=editingIdle || (editingIdle && !idle.getBounds().contains(moonButton.getBounds())))
            issues.add("Moon trigger is outside live stage");
        if(drawer) {
            for(const auto& control:macro)
                if(control.getY()<workspaceBounds().getBottom()-206 || !workspaceBounds().contains(control.getBounds()))
                    issues.add("Macro escapes stage drawer");
        }
        if(routing.isVisible() && routing.getBounds()!=workspaceBounds())issues.add("Routing page escaped the effect deck");
        if(!preset.getButtonText().contains(processor.presetName().toUpperCase()))
            issues.add("Preset selector no longer displays the loaded preset name");
        if(!slot.moduleSelectorMatchesAudio())
            issues.add("Module dropdown differs from the actual selected DSP type");
        return issues;
    }
    // Message-thread-only metering; the audio callback still publishes the
    // same atomics. Fast attack / ~200ms perceptual release stops strobing.
    // Nonfinite host/driver readings cannot poison the meter.
    static float followMeter(float previous,float incoming) noexcept {
        const float target=std::isfinite(incoming)
            ?juce::jlimit(0.f,4.f,incoming):0.f;
        const float old=std::isfinite(previous)
            ?juce::jlimit(0.f,4.f,previous):0.f;
        return target>=old?target:juce::jmax(target,old*.84f);
    }
    // Normalize the twelve visible bars to an actual -60..0 dBFS scale
    // instead of losing quiet vocals with a linear 0..1 mapping.
    static int meterBars(float amplitude) noexcept {
        if(!std::isfinite(amplitude) || amplitude<=.001f)return 0;
        const float safe=juce::jlimit(.001f,1.f,amplitude);
        return juce::jlimit(0,12,
            (int)std::ceil((20.f*std::log10(safe)+60.f)/5.f));
    }
    static juce::Rectangle<int> headerMeterBounds() noexcept {return {315,26,292,34};}
    static constexpr int meterWidth=238;
    static int meterPixels(float amplitude) noexcept {
        if(!std::isfinite(amplitude) || amplitude<=.001f)return 0;
        const float normal=(20.f*std::log10(juce::jlimit(.001f,1.f,amplitude))+60.f)/60.f;
        return juce::jlimit(0,meterWidth,juce::roundToInt(normal*meterWidth));
    }
    void updateHeaderMeters(bool recent){
        displayedInputPeak=followMeter(displayedInputPeak,recent?processor.getInputPeak():0.f);
        displayedOutputPeak=followMeter(displayedOutputPeak,recent?processor.getOutputPeak():0.f);
        const int in=meterPixels(displayedInputPeak),out=meterPixels(displayedOutputPeak);
        const bool inClip=isMeterOverloaded(displayedInputPeak),outClip=isMeterOverloaded(displayedOutputPeak);
        if(in!=lastInputMeter || out!=lastOutputMeter || inClip!=lastInputOverload || outClip!=lastOutputOverload){
            lastInputMeter=in;lastOutputMeter=out;lastInputOverload=inClip;lastOutputOverload=outClip;
            repaint(headerMeterBounds());
        }
    }
    void paintHeaderMeters(juce::Graphics& g) const {
        const auto region=headerMeterBounds();
        const float x=(float)region.getX()+38.f;
        const auto draw=[&](const char* label,float y,float value){
            Theme::fittedText(g,label,{region.getX(),(int)y-3,30,14},10.5f,Theme::white.withAlpha(.84f),true,juce::Justification::centredRight);
            const juce::Rectangle<float> track(x,y,(float)meterWidth,8.f);
            g.setColour(Theme::panelAlt);g.fillRoundedRectangle(track,2.f);
            g.setColour(Theme::stroke.withAlpha(.70f));g.drawRoundedRectangle(track,2.f,.75f);
            const int pixels=meterPixels(value);
            if(pixels>0){
                juce::Graphics::ScopedSaveState clip(g);
                g.reduceClipRegion(track.withWidth((float)pixels).toNearestInt());
                juce::ColourGradient scale(Theme::green.withAlpha(.83f),x,y,Theme::red,x+meterWidth,y,false);
                scale.addColour(.74,Theme::green);scale.addColour(.90,Theme::amber);
                g.setGradientFill(scale);g.fillRoundedRectangle(track,1.5f);
                g.setColour(Theme::white.withAlpha(.20f));g.drawHorizontalLine((int)y+1,x+1.f,x+meterWidth-1.f);
            }
            for(int db=-55;db<0;db+=5){const float tx=x+meterWidth*(db+60)/60.f;
                g.setColour(Theme::bg.withAlpha(.50f));g.drawLine(tx,y,tx,y+8.f,.65f);}
            g.setColour(isMeterOverloaded(value)?Theme::red:Theme::stroke.withAlpha(.55f));
            g.fillRoundedRectangle(x+meterWidth+4.f,y,5.f,8.f,1.5f);
        };
        // Keep true logarithmic ballistics but no printed units or numeric scale.
        draw("IN",(float)region.getCentreY()-12.f,displayedInputPeak);
        draw("OUT",(float)region.getCentreY()+4.f,displayedOutputPeak);
    }
    static bool isMeterOverloaded(float amplitude) noexcept {
        return std::isfinite(amplitude) && amplitude>=1.f;
    }
    static bool testMeterBallistics() noexcept {
        if(meterPixels(0.f)!=0 || meterPixels(.001f)!=0 || meterPixels(1.f)!=meterWidth ||
           meterPixels(std::numeric_limits<float>::infinity())!=0 ||
           std::abs(meterPixels(.1f)-meterWidth*2/3)>1)return false;
        if(meterBars(0.f)!=0 || meterBars(.001f)!=0 ||
           meterBars(.01f)!=4 || meterBars(.1f)!=8 ||
           meterBars(1.f)!=12 || meterBars(2.f)!=12 ||
           meterBars(std::numeric_limits<float>::quiet_NaN())!=0)
            return false;
        float displayed=followMeter(0.f,1.f);
        if(displayed!=1.f)return false; // immediate transient attack
        for(int tick=0;tick<16;++tick){
            const float next=followMeter(displayed,0.f);
            if(!std::isfinite(next) || next>=displayed || next<0.f)
                return false;
            displayed=next;
        }
        if(displayed>=.1f)return false;
        // Overload colour must update even if a 0.98→1.02 transition
        // leaves all twelve dB-scaled segments illuminated.
        if(meterBars(.98f)!=12 || meterBars(1.02f)!=12 ||
           isMeterOverloaded(.98f) || !isMeterOverloaded(1.02f))
            return false;
        const float bad=followMeter(
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN());
        return bad==0.f && followMeter(.1f,.5f)==.5f;
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
        if(!micPanel.testTubePreampForTest())
            issues.add("MIC PREAMP cannot automate independent enable or drive");
        if(!micPanel.testCompactGeometryForTest())
            issues.add("Movable MIC panel overlaps controls at minimum/default/expanded dimensions");
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
        if(!browser.isVisible()||!preset.getToggleState())issues.add("Preset selector did not open and highlight the browser");
        const bool originalDock=rackOnRight,originalHidden=rackHidden;
        for(bool right:{false,true}){
            dockRack(right);resized();
            if(browser.getBounds()!=workspaceBounds() || browser.getBounds().intersects(chain.getBounds()) || browser.getY()<preset.getBottom())
                issues.add("Preset page is not docked clear of the rack/header");
        }
        rackHidden=true;resized();
        if(browser.getBounds()!=workspaceBounds())issues.add("Preset page did not expand with the hidden rack");
        rackHidden=originalHidden;dockRack(originalDock);resized();
        if(browser.visibleFactoryPresetsForTest()!=2 ||
           !browser.canSetCoverForTest())
            issues.add("Main Vocals must show exactly Alchemist and Deuce");
        if(!browser.testMainVocalsPresetFilterForTest())
            issues.add("Main Vocals, My Presets, Artists, Favorites or manual genre filter regressed");
        if(!browser.testLibrarySnapshotForTest())
            issues.add("Preset library snapshot/copy/cover-cache regression failed");
        if(!browser.testBrowserLayoutForTest())
            issues.add("Preset browser custom category controls or cover preview overlap");
        if(!SkellIdleDeck::testStageGeometryForTest())
            issues.add("Skell full-body performer/scope stage clips at compact or enlarged sizes");
        if(!SkellIdleDeck::testTelemetryScaleForTest())
            issues.add("Live performer input/output and RMS telemetry dBFS scale regressed");
        if(!testMeterBallistics())
            issues.add("Input/output -60dBFS meter scale, attack, release or NaN guard regressed");
        if(!slot.testHomeReturnHitZonesForTest())
            issues.add("Removed VISUAL control still has an active hit target");
        if(!testEmptyGapReturnRegions())
            issues.add("Blank gap/top/bottom Home return zones leak into module or rack controls");
        if(!testWorkspaceEdges())issues.add("Workspace edge interaction or selection regression");
        if(!browser.testBrowserKeyboardForTest())
            issues.add("Preset browser keyboard focus, pagination, bank selection or restoration failed");
        if(!browser.testBrowserSortingForTest())
            issues.add("Preset browser A-Z/Z-A/starred-first sorting changed active preset or failed");
        if(!browser.testCategoryCountsForTest())
            issues.add("Cached category counts diverge from saved presets, artists or favorites");
        if(PresetBrowser::artworkPaletteIndexForTest(0)!=0 ||
           PresetBrowser::artworkPaletteIndexForTest(1)!=1 ||
           PresetBrowser::artworkPaletteIndexForTest(3)!=3 ||
           PresetBrowser::artworkPaletteIndexForTest(4)!=0 ||
           PresetBrowser::artworkPaletteIndexForTest(9)!=1 ||
           PresetBrowser::artworkPaletteIndexForTest(44)!=0 ||
           PresetBrowser::artworkPaletteIndexForTest(45)!=1 ||
           PresetBrowser::artworkPaletteIndexForTest(-1)!=0)
            issues.add("Preset artwork palette index can escape the four-colour array");
        if(!PresetBrowser::presetCardHitForTest(false,250,110,650) ||
           !PresetBrowser::presetCardHitForTest(false,632,140,650) ||
           !PresetBrowser::presetCardHitForTest(false,250,415,650) ||
           PresetBrowser::presetCardHitForTest(true,250,140,650) ||
           PresetBrowser::presetCardHitForTest(false,206,140,650) ||
           PresetBrowser::presetCardHitForTest(false,633,140,650) ||
           PresetBrowser::presetCardHitForTest(false,250,167,650) ||
           PresetBrowser::presetCardHitForTest(false,250,471,650))
            issues.add("Preset cards are selectable through Edit Details, outside edges or row gaps");
        if(processor.presetName()!="Alchemist" ||
           processor.getProgramName(skellpreset::alchemist::factoryIndex)!="Alchemist" ||
           processor.getProgramName(skellpreset::deuce_v25::factoryIndex)!="Deuce")
            issues.add("The two visible factory presets must use the requested names");
        preset.onClick();
        if(browser.isVisible()||preset.getToggleState())issues.add("Preset selector did not close and reset the browser");
        toggleView();
        if(!chain.isVisible() || !drawerVisible())issues.add("Moon drawer must keep rack available");
        toggleView();
        if(!chain.isVisible() || drawerVisible())issues.add("Moon drawer did not close");
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
        const bool automated=correctState() && masterValues[0].getText()=="+3.0 dB" &&
            masterValues[1].getText()=="67%" && masterValues[2].getText()=="-2.0 dB";
        toggleView();
        const bool hiddenInAdvanced=!input.isVisible() && !mix.isVisible() &&
            !output.isVisible() && !masterValues[0].isVisible() && !masterValues[1].isVisible() &&
            !masterValues[2].isVisible() && correctState();
        toggleView();
        const bool recalled=input.isVisible() && mix.isVisible() && output.isVisible() &&
            correctState() && std::abs(input.getValue()-3.0)<.05 &&
            std::abs(mix.getValue()-.67)<.02 && std::abs(output.getValue()+2.0)<.05;
        // Also test host-notified APVTS values at the extreme ends; this is
        // independent of changing a slider through the UI.
        setParam(processor,"input",24.f);setParam(processor,"mix",0.f);setParam(processor,"output",-24.f);
        syncMasterReadouts();
        const bool hostReadouts=masterValues[0].getText()=="+24.0 dB" &&
            masterValues[1].getText()=="0%" && masterValues[2].getText()=="-24.0 dB";
        input.setValue(oldMasters[0],juce::sendNotificationSync);
        mix.setValue(oldMasters[1],juce::sendNotificationSync);
        output.setValue(oldMasters[2],juce::sendNotificationSync);
        if(!simpleShown || !automated || !hiddenInAdvanced || !recalled || !hostReadouts)
            issues.add("Simple Input/Mix/Output visibility or automation recall regressed");
        const double oldStereo=stereoSeparation.getValue();
        stereoSeparation.setValue(-48.,juce::sendNotificationSync);
        const bool stereoSimple=stereoSeparation.isVisible() && micButton.isVisible() &&
            masterValues[3].getText()=="50%  WIDE" &&
            std::abs(processor.state.getRawParameterValue("stereoSeparation")->load()+48.f)<.01f;
        toggleView();
        const bool stereoAdvanced=stereoSeparation.isVisible() && micButton.isVisible() &&
            chain.getBounds().contains(stereoSeparation.getBounds()) &&
            std::abs(stereoSeparation.getValue()+48.)<.01;
        stereoSeparation.setValue(96.,juce::sendNotificationSync);
        toggleView();
        const bool stereoRecall=masterValues[3].getText()=="100%  MONO" &&
            std::abs(stereoSeparation.getValue()-96.)<.01;
        stereoSeparation.setValue(oldStereo,juce::sendNotificationSync);
        if(!stereoSimple || !stereoAdvanced || !stereoRecall)
            issues.add("Stereo MIC grouping or automation across workspaces regressed");
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
        if(!powerButton.isBypassed() || !chain.isVisible())
            issues.add("Simple mode power button changed the view or failed");
        if(powerButton.getButtonText().isNotEmpty() ||
           stageButton.getRight()>preset.getX() ||
           stageButton.getButtonText()!="STAGE_HOME" ||
           matchButton.getX()-preset.getRight()!=4 ||
           undo.getX()-matchButton.getRight()!=4 ||
           redo.getX()-undo.getRight()>12 ||
           settings.getX()<=redo.getRight() || settings.getX()-redo.getRight()>16 ||
           powerButton.getX()-settings.getRight()<0 ||
           powerButton.getX()-settings.getRight()>12 ||
           Theme::width-powerButton.getRight()>22 ||
           settings.getWidth()>37 || powerButton.getWidth()>40)
            issues.add("Header must have no Stage button and use preset / undo / redo / settings / rightmost power");
        powerButton.onClick();
        settings.onClick();
        if(!settingsPanel.isVisible())
            issues.add("Settings button did not open its themed overlay");
        if(!settingsPanel.testKeyboardNavigationForTest())
            issues.add("Resize dropdown selection regressed");
        // Exercise Escape directly; focus-grabbing is verified by native
        // screenshots because a CI smoke editor may not have a window peer.
        if(!settingsPanel.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) ||
           settingsPanel.isVisible())
            issues.add("Settings panel did not dismiss with Escape");
        settings.onClick();
        if(!settingsPanel.isVisible())
            issues.add("Settings button did not reopen its themed overlay");
        settings.onClick();
        if(settingsPanel.isVisible())
            issues.add("Settings button did not close its themed overlay");
        configureForScreenshot(0,1,false);
        openRouting(processor.slotIdAt(0));
        if(!routing.isVisible() || slot.isVisible() || !chain.isVisible() || !routing.testConnectionForTest(processor.slotIdAt(0)))
            issues.add("Sidechain routing preview/connect/disconnect regressed");
        if(chain.onClearSelection)chain.onClearSelection();
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
        if(selectedTheme!=Theme::nextVariant(beforeTheme))
            issues.add("Microphone skeleton theme button did not advance to the other dark skin");
        themeButton.onClick();
        if(selectedTheme!=beforeTheme)
            issues.add("Theme button did not round-trip Obsidian/Crimson");
        if(Theme::clampVariant(Theme::Cream)!=Theme::Obsidian)
            issues.add("Removed cream skin must load old theme=1 states as Obsidian");
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
            issues.add("Rack rail/gutter wheel navigation changed effect order");
        if(!chain.testWheelRegions())
            issues.add("Rack effect, side scrollbar, shift and wet hit zones overlap");
        if(!chain.testRackRailNavigation())
            issues.add("Rack scrollbar dragging changed physical effect order or limits");
        if(!chain.testProgrammaticSelectionReveal())
            issues.add("External rack selection failed to reveal its physical effect without reordering");
        if(!chain.testWheelReordersPhysicalEffect())
            issues.add("Effect-face wheel did not move the same physical FX up/down without changing wet");
        if(!chain.testRackKeyboardNavigation())
            issues.add("Rack keyboard up/down, reorder, Home/End or Page keys failed");
        chain.setScreenshotScrollOffset(0);
        if(!chain.testWheelWetAndRack())
            issues.add("Rack wheel and wet controls are not independent");
        if(!chain.testRemoveExtraSlot())
            issues.add("Removing an extra rack row changes stable physical IDs");
        if(!chain.testRackControlsAndWet())
            issues.add("Rack wet trim and green bypass dot did not update or restore host parameters");
        // Keep this regression independent of the owner's approved Alchemist
        // chain, which deliberately has no sidechain-capable effects. Exercise
        // the actual APVTS link gestures on a throwaway native processor so
        // the UI smoke never changes the loaded factory sound or rack order.
        {
            auto dockFixture=std::make_unique<SkellPresetAudioProcessor>();
            dockFixture->setSlotType(0,SkellPresetAudioProcessor::StudioDelayFX);
            dockFixture->setSlotType(1,SkellPresetAudioProcessor::AdlibDoubler);
            dockFixture->setSlotType(2,SkellPresetAudioProcessor::Reverb);
            dockFixture->setSlotType(3,SkellPresetAudioProcessor::Compressor);
            ChainPanel fixtureDock(*dockFixture);
            if(!fixtureDock.testSidechainDock())
                issues.add("Sidechain dock failed to link/unlink supported physical effects without changing rack wet/order");
        }
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
        if(!slot.testLiveViewForTest())issues.add("Integrated effect visuals or rack wet preservation failed");
        if(!slot.testEffectPageShortcutsForTest())
            issues.add("Effect editor numeric/arrow pages and Ctrl+F/Escape overlay keyboard regression");
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
        processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(25));
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
        processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(42));
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
        // The pre-release catalog has 39 factories after retiring 16 Adlibs.
        // Verify the retained native sound-match identities at their new indexes.
        if(processor.getNumPrograms()!=SkellPresetAudioProcessor::customRackProgramIndex+1 ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(43))!="Alchemist" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(44))!="Deuce D/Max (Native Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(45))!="Alchemist Refined (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(46))!="Deuce" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(47))!="Alchemist Peak Guard (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(48))!="Alchemist Tone Match (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(49))!="Alchemist Lighter Dynamics (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(50))!="Alchemist Fast Compressor Blend (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(51))!="Alchemist Smooth Compressor Blend (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(52))!="Alchemist Serial Dynamics (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(53))!="Alchemist Low-End Contour (Trial)" ||
           processor.getProgramName(skellpreset::factoryIndexForRecipe(54))!="Alchemist Serial Dynamics Blend (Trial)")
            issues.add("Retained factory catalog or Alchemist identity changed");
        if(!slot.testAllNativeEffectLayoutsForTest())
            issues.add("A native effect editor has clipped/invisible inline controls or popup geometry");
        if(!slot.testVocalFeelSuiteUI())
            issues.add("Vocal Feel Suite 16 controls, styles popup, or return automation drag failed");
        if(!slot.testSuiteSpectrumUIForTest())
            issues.add("Vocal Feel Spectrum live input/output UI button or read-only mode failed");
        // Real stereo audio → APVTS slot 43 → measured GR → exact state
        // snapshot/restore. Never prove a new plugin using UI-only graphics.
        processor.loadFactoryPreset(skellpreset::factoryIndexForRecipe(25)); // zero-PDC tracking factory
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
        // Full Vocal Feel Suite: test the actual native rack processing,
        // measured five parallel returns, de-esser and opto, then host recall.
        processor.clearRackToEmpty();
        processor.setSlotType(0,SkellPresetAudioProcessor::VocalFeelSuiteFX);
        processor.setSlotOn(0,true);
        const auto& voiceStyle=skellpreset::dsp::vocalFeelRecipes[9].values;
        for(int control=0;control<16;++control)
            processor.setSlotParam(0,control,voiceStyle[(size_t)control]);
        const bool suiteSelected=processor.slotType(0)==SkellPresetAudioProcessor::VocalFeelSuiteFX
            && processor.state.getParameter("s1_fxExt11")!=nullptr
            && processor.getLatencySamples()==0;
        juce::AudioBuffer<float> suiteSignal(2,256);
        juce::MidiBuffer suiteMidi;
        bool suiteFinite=true;
        for(int block=0;block<160;++block) {
            for(int sample=0;sample<256;++sample) {
                const float time=(float)(block*256+sample)/48000.f;
                const float syllable=((block*256+sample)%15000)<12000?1.f:0.f;
                const float l=syllable*(.32f*std::sin(juce::MathConstants<float>::twoPi*210.f*time)
                    +.13f*std::sin(juce::MathConstants<float>::twoPi*8300.f*time));
                suiteSignal.setSample(0,sample,l);
                suiteSignal.setSample(1,sample,.77f*l);
            }
            processor.processBlock(suiteSignal,suiteMidi);
            for(int n=0;n<256;++n)
                suiteFinite=suiteFinite && std::isfinite(suiteSignal.getSample(0,n))
                                         && std::isfinite(suiteSignal.getSample(1,n));
        }
        bool hasLiveReturns=true;
        for(int returnIndex=0;returnIndex<5;++returnIndex)
            hasLiveReturns=hasLiveReturns &&
                processor.getSuiteReturnRms(0,returnIndex)>.00001f;
        const bool hasLiveDynamics=processor.getSlotGainReductionDb(0)>.05f &&
            processor.getSlotDeessReductionDb(0)>=0.f;
        juce::MemoryBlock suiteSaved;
        processor.getStateInformation(suiteSaved);
        processor.setSlotType(0,SkellPresetAudioProcessor::Empty);
        processor.setStateInformation(suiteSaved.getData(),(int)suiteSaved.getSize());
        const bool suiteRestored=processor.slotType(0)==SkellPresetAudioProcessor::VocalFeelSuiteFX
            && std::abs(processor.getSlotParam(0,10)-voiceStyle[10])<.01f
            && std::abs(processor.getSlotParam(0,14)-voiceStyle[14])<.01f;
        if(!suiteSelected || !suiteFinite || !hasLiveReturns ||
           !hasLiveDynamics || !suiteRestored)
            issues.add("Vocal Feel Suite native processBlock, return meters, dynamics, PDC or saved state failed");
        // Critical compatibility: do not replace the original simple macro
        // processor when adding a catalog, recorder or waveform visualization.
        setParam(processor,"mode",0.f); // legacy host value, not a UI navigation command
        editingIdle=true;macroOpen=true;refresh();
        if(!chain.isVisible() || !macro[0].isVisible())
            issues.add("Legacy mode value must not hide the rack or stage macros");
        macroOpen=false;editingIdle=false;
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
        // Keep the working canvas black. Preset artwork belongs in cards.
        const auto header=juce::Rectangle<float>(14.f,5.f,(float)Theme::width-28.f,69.f);
        Theme::panelBox(g,header,9.f);
        // Layer the same workspace contour over a restrained satin face.
        // Keep the border continuous around the logo and rightmost switch.
        Theme::workspaceFrame(g,header);
        g.setColour(Theme::bg.withAlpha(.44f));g.drawRoundedRectangle(header.reduced(3.f),6.f,.75f);
        g.setColour(Theme::white.withAlpha(.08f));g.drawHorizontalLine(8,header.getX()+12.f,header.getRight()-12.f);
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
        wordmark.createLayout(brand,340.f);
        // Treat the brand as one compact lockup, vertically centred on the
        // logo. Subtitle and version share the same font/baseline and both
        // edges derive from the measured wordmark, not unrelated offsets.
        const float titleX=75.f,titleW=wordmark.getWidth(),titleH=wordmark.getHeight();
        constexpr int subHeight=14;
        constexpr float overlap=4.f;
        const float titleY=44.5f-(titleH+subHeight-overlap)*.5f;
        wordmark.draw(g,{titleX,titleY,titleW,titleH});
        const int subY=juce::roundToInt(titleY+titleH-overlap);
        const int left=juce::roundToInt(titleX),right=juce::roundToInt(titleX+titleW);
        constexpr int editionWidth=29,editionGap=5;
        Theme::fittedText(g,"ONE PLUGIN. EVERY PRESET.",
            {left,subY,juce::jmax(1,right-left-editionWidth-editionGap),subHeight},
            10.5f,Theme::muted,true,juce::Justification::centredLeft);
        Theme::text(g,"v.01",{right-editionWidth,subY,editionWidth,subHeight},
            10.5f,Theme::green,true,juce::Justification::centredRight);
        Theme::text(g,"PRESET",{623,8,268,15},11.f,Theme::muted,true);
        paintHeaderMeters(g);
        if(focusVisible()){
            const auto deck=workspaceBounds();
            const auto frame=(drawerVisible()?deck.withTrimmedBottom(212):deck).toFloat();
            Theme::panelBox(g,frame,9.f);Theme::workspaceFrame(g,frame);
            Theme::fittedText(g,"SLOT "+juce::String(slot.selectedSlot()+1).paddedLeft('0',2),
                {deck.getX()+23,deck.getY()+12,92,22},11.f,Theme::muted,true);
            Theme::fittedText(g,processor.moduleDisplayName(processor.slotType(slot.selectedSlot())),
                {deck.getX()+115,deck.getY()+8,deck.getWidth()-150,30},20.f,Theme::white,true);
        }
        // No footer meter rail; leave the resize grip and window geometry unchanged.
        // One shallow control drawer below the resized live performer.
        if(drawerVisible()) {
            const auto deck=workspaceBounds();
            const int x=deck.getX(),y=deck.getBottom()-206,w=deck.getWidth();
            Theme::panelBox(g,juce::Rectangle<float>((float)x,(float)y,(float)w,206.f),8.f,true);
            Theme::fittedText(g,"SOUND CONTROLS",{x+16,y+8,w-32,20},12.f,Theme::white,true);
            Theme::fittedText(g,"MACROS     /     MASTER",{x+w-220,y+8,204,20},10.f,Theme::muted,true,juce::Justification::centredRight);
            g.setColour(Theme::stroke.withAlpha(.60f));
            g.drawHorizontalLine(y+34,(float)x+16.f,(float)(x+w-16));
            g.setColour(Theme::green.withAlpha(.45f));g.fillRect((float)x+16.f,(float)y+33.f,38.f,1.5f);
            const char* labels[]{"TONE","PUNCH","SPACE","GRIT","INPUT","MIX","OUTPUT"};
            for(int k=0;k<7;++k){
                const int cellLeft=x+10+k*(w-20)/7,cellRight=x+10+(k+1)*(w-20)/7;
                const bool active=k>=4 || !processor.macroTargets(k).isEmpty();
                Theme::fittedText(g,labels[k],{cellLeft,y+38,cellRight-cellLeft,22},13.f,active?Theme::white:Theme::muted,true,juce::Justification::centred);
                if(k<4){
                    const auto raw=processor.state.getRawParameterValue("macro"+juce::String(k+1));
                    Theme::fittedText(g,juce::String(juce::roundToInt(100.f*raw->load()))+"%",
                        {cellLeft,y+160,cellRight-cellLeft,24},17.f,active?Theme::green:Theme::muted,true,juce::Justification::centred);
                    if(!active)Theme::text(g,"UNASSIGNED",{cellLeft,y+184,cellRight-cellLeft,12},8.5f,Theme::muted,true,juce::Justification::centred);
                }
            }
            g.setColour(Theme::stroke.withAlpha(.6f));
            g.drawVerticalLine(x+10+4*(w-20)/7,(float)y+38.f,(float)y+188.f);
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
        chain.setBounds(rackOnRight?939:13,90,216,561);
        // The Advanced footer macro strip is gone: let the module editor
        // fill the space down to the shared master-control footer.
        const auto workspace=workspaceBounds();
        slot.setBounds(workspace.withSizeKeepingCentre(906,561));
        routing.setBounds(workspace);
        const auto visuals=drawerVisible()?workspace.withTrimmedBottom(212):workspace;
        idle.setBounds(visuals);focusVisual.setBounds(visuals.reduced(18,12).withTrimmedTop(34));
        // Cover the actual visible rack border, not just the empty lane.
        // Four pixels inside the right border leaves the 208..212 scrollbar
        // intact; the remaining hit area extends out into the adjacent gap.
        const auto rack=chain.getBounds();
        const int outsideBorder=rackOnRight?(rackHidden?workspace.getRight():rack.getRight()):
            (rackHidden?workspace.getX():rack.getX());
        const int outsideStart=outsideBorder-(rackHidden?8:(rackOnRight?4:12));
        rackOuterEdge.setBounds(juce::jlimit(0,Theme::width-16,outsideStart),
            rack.getY(),16,rack.getHeight());
        rackInnerEdge.setBounds(rackOnRight?rack.getX()-12:rack.getRight()-4,
            rack.getY(),16,rack.getHeight());
        rackOuterEdge.configure(rackHidden?(rackOnRight?WorkspaceEdge::Left:WorkspaceEdge::Right):
            (rackOnRight?WorkspaceEdge::Right:WorkspaceEdge::Left),rackHidden?"Show effect rack":"Hide effect rack");
        rackInnerEdge.configure(rackOnRight?WorkspaceEdge::Right:WorkspaceEdge::Left,"Hide effect rack");
        rackOuterEdge.setVisible(true);rackInnerEdge.setVisible(!rackHidden);
        // No visual handles over effect knobs, empty slots or routing pages.
        const bool visualNav=visualNavigationVisible();
        const auto live=liveNavigationBounds();
        visualLeftEdge.setBounds(live.getX()+2,live.getY()+20,15,juce::jmax(44,live.getHeight()-36));
        visualRightEdge.setBounds(live.getRight()-17,live.getY()+20,15,juce::jmax(44,live.getHeight()-36));
        visualLeftEdge.configure(WorkspaceEdge::Right,"Click or drag: sidechain");
        visualRightEdge.configure(WorkspaceEdge::Left,"Click or drag: sidechain");
        drawerEdge.setBounds(live.getX()+18,live.getBottom()-14,live.getWidth()-36,14);
        drawerEdge.configure(drawerVisible()?WorkspaceEdge::Down:WorkspaceEdge::Up,"Click or drag vertically: sound controls");
        for(auto* edge:{&visualLeftEdge,&visualRightEdge,&drawerEdge}){
            edge->setVisible(visualNav);edge->setContextHover(false);
        }
        moonButton.setBounds(idle.moonBounds().translated(idle.getX(),idle.getY()));
        moonButton.setVisible(editingIdle);moonButton.expanded=drawerVisible();
        moonButton.setVisualHover(false);
        if(editingIdle){idle.toFront(false);moonButton.toFront(false);}
        if(focusVisible())focusVisual.toFront(false);
        if(routing.isVisible())routing.toFront(false);
        moonButton.repaint();
        // Smaller movable mic panel opens on the LEFT; manual drag/resize
        // remains under the user's control and survives editor refresh.
        constexpr int micWidth=324,micHeight=320;
        if(!micPanel.hasBeenMoved())
            micPanel.setBounds(rackOnRight?18:238,107,micWidth,micHeight);
        else {
            const int w=juce::jlimit(312,440,micPanel.getWidth());
            const int h=juce::jlimit(308,400,micPanel.getHeight());
            micPanel.setBounds(
                juce::jlimit(0,Theme::width-w,micPanel.getX()),
                juce::jlimit(0,Theme::height-h,micPanel.getY()),w,h);
        }
        preset.setBounds(623,26,268,34);
        matchButton.setBounds(895,26,100,34);
        // No dedicated Live Stage button: the wordmark is the return target.
        stageButton.setBounds(73,15,225,52);
        undo.setBounds(999,26,34,34);
        redo.setBounds(1037,26,34,34);
        micButton.setBounds(chain.getX()+174,chain.getY()+6,34,34);
        // Owner correction: power is the rightmost header control.
        powerButton.setBounds(1113,26,34,34);
        const bool drawer=drawerVisible();
        const auto deck=workspaceBounds();
        const int y=deck.getBottom()-206;
        const auto dialBounds=[&](int k){const int l=deck.getX()+10+k*(deck.getWidth()-20)/7;
            const int r=deck.getX()+10+(k+1)*(deck.getWidth()-20)/7;
            return juce::Rectangle<int>((l+r)/2-43,y+66,86,86);};
        for(int i=0;i<4;++i){
            macro[(size_t)i].setBounds(dialBounds(i));
            macro[(size_t)i].setVisible(drawer);
        }
        input.setBounds(dialBounds(4));mix.setBounds(dialBounds(5));output.setBounds(dialBounds(6));
        micButton.setVisible(!rackHidden);
        stereoSeparation.setBounds(chain.getX()+136,chain.getY()+6,34,34);
        stereoSeparation.setVisible(!rackHidden);
        for(auto* control:{&input,&mix,&output})control->setVisible(drawer);
        for(size_t i=0;i<masterValues.size();++i){
            const auto b=dialBounds(4+juce::jmin(2,(int)i));
            masterValues[i].setBounds(b.getX()-15,y+160,b.getWidth()+30,24);
            masterValues[i].setVisible(i<3 && drawer);
        }
        // The rack's two matching hardware controls remain above its painted face.
        micButton.toFront(false);stereoSeparation.toFront(false);
        syncMasterReadouts();
        sizeSelect.setBounds(1094,664,69,22);
        settings.setBounds(1075,26,34,34);
        settingsPanel.setBounds(settings.getRight()-230,78,230,98);
        record.setBounds(0,0,1,1);
        record.setVisible(false);
        // Presets are a workspace page, not a floating window. The header,
        // meters and rack stay visible on either dock.
        browser.dockTo(workspace);
        bypassShade.setBounds(workspace.withTrimmedBottom(26));
        if(bypassShade.isVisible())bypassShade.toFront(false);
        // Rack presentation stays reachable even with a bypassed effect.
        for(auto* edge:{&visualLeftEdge,&visualRightEdge,&drawerEdge,&rackOuterEdge,&rackInnerEdge})edge->toFront(false);
        if(browser.isVisible())browser.toFront(false);
        if(settingsPanel.isVisible())settingsPanel.toFront(false);
        if(micPanel.isVisible())micPanel.toFront(false);
        matchPanel.placeWithin(getLocalBounds(),rackOnRight);
        if(matchPanel.isVisible())matchPanel.toFront(false);
    }
    void syncPresetHeader(){
        const bool open=browser.isVisible();
        preset.setToggleState(open,juce::dontSendNotification);
        const auto label=processor.presetName().toUpperCase()+(open?"  ▲":"  ▼");
        if(preset.getButtonText()!=label)preset.setButtonText(label);
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
        const bool drawer=drawerVisible();
        chain.setVisible(!rackHidden);slot.setVisible(!editingIdle && !routing.isVisible() && !focusVisible());
        focusVisual.setVisible(focusVisible());
        routing.setEnabled(!bypass);
        idle.setVisible(editingIdle);
        chain.setAlpha(bypass?.30f:1.f);slot.setAlpha(bypass?.30f:1.f);
        chain.setEnabled(!bypass);slot.setEnabled(!bypass);
        // Keep the actual performer and its BYPASS lamp readable. The
        // disabled rack/controls and master shade already show bypass.
        idle.setEnabled(!bypass);idle.setAlpha(bypass?.72f:1.f);
        if(idle.isShowing())idle.repaint();
        for(auto& s:macro){
            s.setVisible(drawer);
            s.setEnabled(!bypass);
            s.setAlpha(bypass?.35f:1.f);
        }
        for(auto* s:{&input,&mix,&output}) {
            s->setVisible(drawer);
            s->setEnabled(!bypass);
            s->setAlpha(bypass?.35f:1.f);
        }
        stereoSeparation.setEnabled(!bypass);
        stereoSeparation.setAlpha(bypass?.45f:1.f);
        micButton.setEnabled(!bypass);
        moonButton.setEnabled(!bypass);
        for(size_t i=0;i<masterValues.size();++i){masterValues[i].setVisible(i<3 && drawer);masterValues[i].setAlpha(bypass?.50f:1.f);}
        syncMasterReadouts();
        bypassShade.setVisible(bypass);
        if(bypass)bypassShade.toFront(false);
        if(browser.isVisible())browser.toFront(false);
        if(settingsPanel.isVisible())settingsPanel.toFront(true);
        if(micPanel.isVisible())micPanel.toFront(true);
        micButton.setToggleState(micPanel.anyActive() || micPanel.isVisible(),juce::dontSendNotification);
        micButton.setColour(juce::TextButton::buttonColourId,
            micPanel.anyActive()?Theme::green.withAlpha(.21f):Theme::panelAlt);
        micButton.setColour(juce::TextButton::textColourOffId,
            micPanel.anyActive()?Theme::green:Theme::white);
        syncPresetHeader();
        powerButton.setBypassed(bypass);
        matchButton.setEnabled(!bypass);
        // UI state must reflect the current host/program state *now*, not at
        // the next 30Hz timer tick. This also prevents stale module names
        // during screenshot capture and fast A/B preset auditioning.
        chain.refreshIfChanged();
        slot.setHostedEditorPresentation(!bypass && !editingIdle && !routing.isVisible() && !focusVisible() &&
            !browser.isVisible() && !settingsPanel.isVisible() && !micPanel.isVisible());
        if(!focusVisible())slot.refreshIfChanged();
        resized();updateFocusVisual();repaint();
    }
    void timerCallback() override{
        updateVisualHover();
        processor.refreshHostedLatency();
        if(micPanel.isVisible())micPanel.refreshIfChanged();
        const bool micActive=micPanel.anyActive();
        micButton.setToggleState(micActive || micPanel.isVisible(),juce::dontSendNotification);
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
        syncPresetHeader();
        const int m=juce::jlimit(0,3,(int)processor.state.getRawParameterValue("mode")->load());
        if(m==3&&matchPanel.isVisible())closeMatch();
        if(bypassShade.isVisible()!=(m==3))refresh();
        if(focusVisual.isVisible()!=focusVisible()){refresh();return;}
        // Do not request -1 then immediately request the selected slot every
        // timer tick: that would reset the audio capture generation at 30 Hz.
        if(!focusVisible())slot.setHostedEditorPresentation(m!=3 && !editingIdle && !routing.isVisible() &&
            !browser.isVisible() && !settingsPanel.isVisible() && !micPanel.isVisible());
        if(m!=3){if(!rackHidden)chain.refreshIfChanged();if(!focusVisible())slot.refreshIfChanged();if(routing.isVisible())routing.refresh();}
        if(focusVisible())updateFocusVisual();
        if(drawerVisible())syncMasterReadouts();
        const auto now=juce::Time::getMillisecondCounter();
        const unsigned count=processor.liveScopeSampleCount();
        const bool advancing=count!=lastScopeSampleCount;
        const bool recent=advancing || (lastScopeFrameTimeMs!=0u &&
            (unsigned)(now-lastScopeFrameTimeMs)<400u);
        // Never retain a frozen peak when the host stops calling processBlock.
        updateHeaderMeters(recent);
        tickMatch();
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
        if(advancing) {
            lastScopeSampleCount=count;
            lastScopeFrameTimeMs=now;
            idle.setMeasuredPeaks(processor.getInputPeak(),processor.getOutputPeak());
            idle.setAudioFresh(count>0);
            // The former top LIVE status was removed; do not repaint the
            // static SKELL title/version every incoming scope update.
            if(drawerVisible())repaint(slot.getBounds().withTrimmedTop(slot.getHeight()-206));
        } else if(lastScopeFrameTimeMs!=0u &&
                  (unsigned)(juce::Time::getMillisecondCounter()-
                             lastScopeFrameTimeMs)>=400u) {
            // The host can stop sending blocks without resetting its scope
            // buffer. Clear the Stage LIVE indicator and drawn trace once;
            // the visible stage has a separate elapsed-time animation clock.
            if(idle.hasFreshAudio()){
                idle.setAudioFresh(false);
                if(drawerVisible())repaint(slot.getBounds().withTrimmedTop(slot.getHeight()-206));
            }
        }
        // Motion does not depend on audio block cadence. Hidden views do not
        // paint and resume without a time jump. Existing UI timer stays 30 Hz.
        idle.tickAnimation(juce::Time::getMillisecondCounterHiRes());
    }
    // The rack and effect face never contain an implicit click-away area:
    // only the empty lane separating those boxes or the slim top/bottom
    // breathing room is a Home hit target. This callback receives only
    // clicks that passed through all child components.
    bool isEmptyGapReturn(juce::Point<int> point) const noexcept {
        const int laneLeft=rackOnRight?slot.getRight():chain.getRight();
        const int laneRight=rackOnRight?chain.getX():slot.getX();
        const bool between=(!rackHidden && point.x>laneLeft+1 && point.x<laneRight-1 &&
                            point.y>=chain.getY() && point.y<chain.getBottom());
        const bool above=(point.x>=14 && point.x<Theme::width-14 &&
                          point.y>=78 && point.y<89);
        const bool below=(point.x>=14 && point.x<Theme::width-14 &&
                          point.y>=chain.getBottom()+2 && point.y<662);
        return between || above || below;
    }
    bool testEmptyGapReturnRegions() const noexcept {
        const int left=rackOnRight?slot.getRight():chain.getRight();
        const int right=rackOnRight?chain.getX():slot.getX();
        return right-left>=12 &&
            isEmptyGapReturn({(left+right)/2,chain.getY()+190}) &&
            isEmptyGapReturn({700,82}) &&
            isEmptyGapReturn({700,656}) &&
            !isEmptyGapReturn(chain.getBounds().getCentre()) &&
            !isEmptyGapReturn(slot.getBounds().getCentre()) &&
            !isEmptyGapReturn({1000,50}) &&
            !isEmptyGapReturn({600,500});
    }
    bool testWorkspaceEdges(){
        const int selected=slot.selectedSlot(),program=processor.getCurrentProgram();
        const bool savedIdle=editingIdle,savedMacro=macroOpen,savedDock=rackOnRight;
        const bool savedBrowser=browser.isVisible();
        browser.setVisible(false);
        const auto parameters=processor.getParameters();
        std::vector<float> before;before.reserve((size_t)parameters.size());
        for(auto* parameter:parameters)before.push_back(parameter->getValue());
        editingIdle=true;macroOpen=false;refresh();bool ok=true;
        for(bool right:{false,true}){
            dockRack(right);
            ok &= rackInnerEdge.releaseGesture(24,0)&&rackHidden&&!chain.isVisible();
            ok &= workspaceBounds().getWidth()==1140;
            ok &= !rackOuterEdge.releaseGesture(2,25)&&rackHidden;
            ok &= rackOuterEdge.releaseGesture(0,0)&&!rackHidden&&chain.isVisible();
            ok &= visualLeftEdge.releaseGesture(22,0)&&routing.isVisible();
            ok &= !visualLeftEdge.isVisible()&&!visualRightEdge.isVisible()&&!drawerEdge.isVisible();
            closeRouting();ok &= !routing.isVisible();
            ok &= !drawerEdge.releaseGesture(0,26)&&!drawerVisible();
            ok &= drawerEdge.releaseGesture(0,-26)&&drawerVisible();
            ok &= !drawerEdge.releaseGesture(0,-26)&&drawerVisible();
            ok &= drawerEdge.releaseGesture(0,26)&&!drawerVisible();
        }
        editingIdle=false;macroOpen=false;refresh();
        // Check hit testing on the physical rack borders while an ordinary
        // effect is selected, rather than only calling a hidden handler.
        for(bool right:{false,true}){
            dockRack(right);
            ok &= !visualLeftEdge.isVisible()&&!visualRightEdge.isVisible()&&!drawerEdge.isVisible();
            const auto rack=chain.getBounds();
            for(int y:{rack.getY()+70,rack.getCentreY(),rack.getBottom()-30}){
                ok &= getComponentAt(right?rack.getX():rack.getRight()-1,y)==&rackInnerEdge;
                ok &= getComponentAt(right?rack.getRight()-1:rack.getX(),y)==&rackOuterEdge;
                // Right-edge rack scroll rail remains usable with either dock.
                ok &= getComponentAt(rack.getX()+210,y)==&chain;
            }
            ok &= rackInnerEdge.testHoverPaint()&&rackOuterEdge.testHoverPaint();
            ok &= rackInnerEdge.releaseGesture(0,0)&&rackHidden&&!chain.isVisible();
            ok &= rackOuterEdge.testHoverPaint();
            if(nativeVisualAvailable()){
                ok &= focusVisual.isVisible()&&!slot.isVisible()&&drawerEdge.isVisible();
                ok &= drawerEdge.testHoverPaint()&&visualLeftEdge.testHoverPaint()&&visualRightEdge.testHoverPaint();
                ok &= drawerEdge.releaseGesture(0,-26)&&focusVisual.isVisible()&&drawerVisible();
                ok &= drawerEdge.releaseGesture(0,26)&&focusVisual.isVisible()&&!drawerVisible();
            }
            ok &= rackOuterEdge.releaseGesture(0,0)&&slot.isVisible()&&!focusVisual.isVisible();
        }
        dockRack(savedDock);editingIdle=savedIdle;macroOpen=savedMacro;refresh();
        browser.setVisible(savedBrowser);if(savedBrowser)browser.toFront(false);
        for(int i=0;i<parameters.size();++i)ok &= parameters[i]->getValue()==before[(size_t)i];
        return ok&&slot.selectedSlot()==selected&&processor.getCurrentProgram()==program;
    }
    void mouseDown(const juce::MouseEvent& e)override {
        for(const auto* edge:{&rackOuterEdge,&rackInnerEdge,&visualLeftEdge,&visualRightEdge,&drawerEdge})
            if(e.eventComponent==edge)return;
        const auto point=e.getEventRelativeTo(this).getPosition();
        const bool overlayWasVisible=micPanel.isVisible() ||
            browser.isVisible() || settingsPanel.isVisible();
        const auto micTrigger=micButton.getBounds();
        if(micPanel.isVisible() && !micPanel.getBounds().contains(point)
           && !(chain.isVisible() && micTrigger.contains(point))
           && !(micButton.isVisible() && micButton.getBounds().contains(point)))
            micPanel.setVisible(false);
        if(browser.isVisible() && !browser.getBounds().contains(point) && !preset.getBounds().contains(point)) {
            browser.setVisible(false);preset.repaint();
        }
        if(settingsPanel.isVisible() && !settingsPanel.getBounds().contains(point)
           && !settings.getBounds().contains(point))
            settingsPanel.close();
        const int current=(int)processor.state.getRawParameterValue("mode")->load();
        if(!overlayWasVisible && !e.mods.isPopupMenu() &&
           current!=3 && !editingIdle && isEmptyGapReturn(point) &&
           chain.onClearSelection)
            chain.onClearSelection();
    }
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::escapeKey && matchPanel.isVisible()){closeMatch();return true;}
        if(key==juce::KeyPress::escapeKey && micPanel.isVisible()) {
            micPanel.setVisible(false);return true;
        }
        // Keep the local refinement text editor's editing shortcuts.
        if(matchPanel.isVisible()&&matchPanel.editingPrompt())return false;
        // Never steal Ctrl+Z from the browser's active search/editor field.
        if(!browser.isVisible() && !settingsPanel.isVisible() &&
           !micPanel.isVisible() && key.getModifiers().isCommandDown()) {
            if(matchPanel.isVisible())closeMatch();
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
        if(key==juce::KeyPress::escapeKey && routing.isVisible()){closeRouting();return true;}
        if(key==juce::KeyPress::escapeKey && rackHidden){toggleRackVisibility();return true;}
        if(key==juce::KeyPress::escapeKey && drawerVisible()){toggleDrawerEdge();return true;}
        if(key==juce::KeyPress::escapeKey && slot.isVisible()){
            editingIdle=true;slot.setHostedEditorPresentation(false);idle.setVisible(true);chain.setSelectedSlot(-1);
            idle.toFront(false);refresh();return true;
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
    void closeMatch(){
        ++matchSession;
        processor.cancelLiveMatch();matchListening=false;
        if(matchFile)matchFile->cancel.store(true,std::memory_order_relaxed);
        matchFile.reset();
        if(matchOptimization)matchOptimization->cancel.store(true,std::memory_order_relaxed);
        matchOptimization.reset();matchAudio.reset();matchDry={};matchBeat={};matchIntent={};matchLastRecipe.reset();matchLastDetail.clear();matchPanel.setBeatReady(false);matchPanel.setRefinementAvailable(false);
        matchPanel.setVisible(false);matchButton.setToggleState(false,juce::dontSendNotification);
    }
    void beginMatch(){
        closeMatch();matchDry={};
        if((int)processor.state.getRawParameterValue("mode")->load()==3)return;
        browser.setVisible(false);settingsPanel.close();micPanel.setVisible(false);
        if(chain.onClearSelection)chain.onClearSelection();
        macroOpen=false;rackHidden=false;matchPanel.resetPosition();refresh();
        matchPanel.setVisible(true);matchPanel.toFront(true);
        matchButton.setToggleState(true,juce::dontSendNotification);
        matchPanel.update(SkellMatchPanel::ChooseStyle,"Choose a style before listening.",0.f,0.f,false);
    }
    void startMatchListening(){
        if(!matchPanel.isVisible()||matchListening)return;
        matchListening=true;matchStarted=juce::Time::getMillisecondCounterHiRes();
        if(!processor.startLiveMatch()){matchListening=false;matchPanel.update(SkellMatchPanel::Problem,"Not enough memory for local analysis. Close other plugins and retry.",0.f,0.f,false);return;}
        matchPanel.update(SkellMatchPanel::Listening,juce::String(skellpreset::liveMatch::guideFor(matchPanel.selectedStyle()).name)+" auto mix. Play an isolated vocal; your rack is unchanged until validation.",0.f,0.f,false);
    }
    struct MatchOptimizationResult {std::atomic<bool> cancel{false},done{false};std::atomic<int> progress{0};skellpreset::liveMatch::Optimization result;skellpreset::liveMatch::Profile target;skellpreset::liveMatch::MixStyle style=skellpreset::liveMatch::MixStyle::Balanced;bool reference=false,refinement=false;skellpreset::liveMatch::MixIntent intent;skellpreset::liveMatch::Profile beat;std::optional<skellpreset::liveMatch::Recipe> previous;};
    void finishMatch(const skellpreset::liveMatch::Profile& profile,bool reference,std::shared_ptr<skellpreset::liveMatch::Audio> audio={},const skellpreset::liveMatch::MixIntent* refinement=nullptr){
        processor.cancelLiveMatch();matchListening=false;
        if(!profile.valid()){
            if(!reference){matchDry={};matchAudio.reset();}
            if(profile.invalid>0.f){matchPanel.update(SkellMatchPanel::Problem,"Invalid audio samples detected. Choose a clean recording or retry.",0.f,0.f,matchDry.valid());return;}
            matchPanel.update(SkellMatchPanel::Problem,"Not enough clean audio. Use at least 6 seconds with 4 seconds of audible vocals; avoid clipping.",0.f,0.f,matchDry.valid());return;
        }
        if(!reference&&!refinement){matchDry=audio?profile:skellpreset::liveMatch::Profile{};matchAudio=std::move(audio);matchLastRecipe.reset();matchIntent={};matchPanel.setRefinementAvailable(false);}
        if(reference&&!audio){matchPanel.update(SkellMatchPanel::Problem,"Reference audio could not be retained. Choose another isolated file.",0.f,0.f,matchDry.valid());return;}
        if(!matchAudio){matchPanel.update(SkellMatchPanel::Problem,"No usable dry sample. Retry or choose a dry file.",0.f,0.f,false);return;}
        auto job=std::make_shared<MatchOptimizationResult>();job->reference=reference;job->target=profile;job->style=matchPanel.selectedStyle();job->intent=refinement?*refinement:matchIntent;job->beat=matchBeat;job->refinement=refinement!=nullptr;if(refinement)job->previous=matchLastRecipe;
        matchOptimization=job;
        const auto source=matchAudio;const auto dry=matchDry;
        matchPanel.update(SkellMatchPanel::Refining,"Rendering native candidates and checking a separate vocal phrase. Your rack is unchanged.",0.f,0.f,true);
        matchPool.addJob(std::function<void()>([job,source,dry,referenceAudio=reference?std::move(audio):nullptr]{
            try{job->result=job->reference?skellpreset::liveMatch::optimize(*source,dry,&job->target,&job->cancel,&job->progress,referenceAudio.get()):skellpreset::liveMatch::optimizeAutoMix(*source,dry,job->style,&job->cancel,&job->progress,job->intent,job->beat.valid()?&job->beat:nullptr,job->previous?&*job->previous:nullptr);}catch(const std::bad_alloc&){job->result={};}
            job->done.store(true,std::memory_order_release);
        }));
    }
    void applyOptimizedMatch(const MatchOptimizationResult& job){
        const auto& o=job.result;
        if(!o.valid&&job.refinement){matchPanel.update(SkellMatchPanel::Done,"Request did not improve both phrases. Current rack kept. Try another refinement.",1.f,0.f,true);return;}
        if(!o.valid){matchPanel.update(SkellMatchPanel::Problem,o.referenceRejected?"Reference level, tone, space or stereo character is outside this rack match. Your rack is unchanged.":"No verified improvement on both vocal phrases. Try a longer or clearer dry vocal.",1.f,0.f,matchDry.valid());return;}
        prepareHistory();
        if(!pendingHistory){matchPanel.update(SkellMatchPanel::Problem,"Save and remove hosted effects before replacing this rack.",0.f,0.f,matchDry.valid());return;}
        const auto result=processor.applyLiveMatch(matchDry,job.reference?&job.target:nullptr,&o.recipe);
        if(result.failed()){pendingHistory.reset();matchPanel.update(SkellMatchPanel::Problem,result.getErrorMessage(),0.f,0.f,matchDry.valid());return;}
        commitHistory();refresh();
        const auto detail=juce::String(job.reference?"Reference":skellpreset::liveMatch::guideFor(job.style).name)+" / "+juce::String(o.tested)+" candidates. Feature score "+juce::String(o.dryHoldout,1)+" to "+juce::String(o.holdoutFinal,1)+". Audition in your DAW.";
        if(!job.reference){matchLastRecipe=o.recipe;matchIntent=job.intent;}else matchLastRecipe.reset();
        matchLastDetail=detail;matchPanel.setRefinementAvailable(matchLastRecipe.has_value());
        matchPanel.update(SkellMatchPanel::Done,detail,1.f,0.f,true);
        matchPanel.toFront(true);
    }
    void tickMatch(){
        if(!matchPanel.isVisible())return;
        if(matchOptimization){
            if(matchOptimization->done.load(std::memory_order_acquire)){auto job=std::move(matchOptimization);if(!job->cancel.load(std::memory_order_relaxed))applyOptimizedMatch(*job);}
            else {const int n=matchOptimization->progress.load(std::memory_order_relaxed);matchPanel.update(SkellMatchPanel::Refining,"Comparing native tone and space candidates: "+juce::String(n)+". Checking held-out phrase before applying.",std::min(.95f,n/(float)(matchOptimization->reference?skellpreset::liveMatch::referenceBudget:skellpreset::liveMatch::autoMixBudget)),0.f,true);}
            return;
        }
        if(matchFile&&matchFile->done.load(std::memory_order_acquire)){auto completed=std::move(matchFile);if(!completed->cancel.load(std::memory_order_relaxed)){
            if(completed->beat){matchBeat=completed->profile.valid()?completed->profile:skellpreset::liveMatch::Profile{};matchPanel.setBeatReady(matchBeat.valid());matchPanel.update(SkellMatchPanel::ChooseStyle,matchBeat.valid()?"Instrumental context loaded.":"Beat not usable; choose another or continue.",0.f,0.f,false);}
            else finishMatch(completed->profile,completed->reference,completed->audio);}return;}
        if(!matchListening)return;
        skellpreset::liveMatch::Profile profile;bool done=false;
        processor.readLiveMatch(profile,done);
        if(done){
            std::shared_ptr<skellpreset::liveMatch::Audio> audio;
            if(profile.valid())try{audio=std::make_shared<skellpreset::liveMatch::Audio>();if(!processor.copyLiveMatchAudio(*audio))audio.reset();}catch(const std::bad_alloc&){audio.reset();}
            finishMatch(profile,false,std::move(audio));return;
        }
        const auto seconds=(juce::Time::getMillisecondCounterHiRes()-matchStarted)*.001;
        if(seconds>45.){processor.cancelLiveMatch();matchListening=false;matchPanel.update(SkellMatchPanel::Problem,"No usable vocal input. Play audio in your DAW or choose Dry File.",0.f,0.f,false);return;}
        const bool more=seconds>6.&&profile.active<4.f;
        matchPanel.update(more?SkellMatchPanel::More:SkellMatchPanel::Listening,
            more?"Need more audio — keep the isolated vocal playing.":juce::String(profile.active,1)+" / 4.0 s of audible input. Your rack is unchanged.",
            std::min(profile.active/4.f,profile.elapsed/6.f),processor.getInputPeak(),false);
    }
    void chooseMatchFile(bool reference,bool beat=false){
        if(reference&&!matchDry.valid())return;
        matchChooser=std::make_unique<juce::FileChooser>(beat?"Choose instrumental beat file":reference?"Choose isolated reference vocal":"Choose dry isolated vocal",juce::File{},"*.wav;*.aif;*.aiff;*.flac");
        const auto session=matchSession;
        matchChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [safe=juce::Component::SafePointer<Canvas>(this),reference,beat,session](const juce::FileChooser& chooser){
                if(auto* self=safe.getComponent()){auto file=chooser.getResult();if(file.existsAsFile()&&self->matchPanel.isVisible()&&self->matchSession==session)self->analyzeMatchFile(file,reference,beat);}
            });
    }
    void analyzeMatchFile(juce::File file,bool reference,bool beat=false){
        processor.cancelLiveMatch();matchListening=false;
        if(matchFile)matchFile->cancel.store(true,std::memory_order_relaxed);
        if(matchOptimization)matchOptimization->cancel.store(true,std::memory_order_relaxed);
        matchOptimization.reset();
        if(!reference&&!beat){matchDry={};matchAudio.reset();}
        auto task=std::make_shared<MatchFileResult>();task->reference=reference;task->beat=beat;matchFile=task;
        matchPanel.update(SkellMatchPanel::Reading,"Reading up to 20 seconds locally. No audio is uploaded.",0.f,0.f,matchDry.valid());
        matchPool.addJob(std::function<void()>([file,task]{
            try {
            skellpreset::liveMatch::Profile profile;juce::AudioFormatManager formats;formats.registerBasicFormats();
            if(file.getSize()>0&&file.getSize()<=128*1024*1024){
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                if(reader&&reader->numChannels>=1&&reader->numChannels<=2&&reader->sampleRate>=8000.&&reader->sampleRate<=192000.){
                    skellpreset::liveMatch::Analyzer analyzer;analyzer.prepare(reader->sampleRate,task->beat);juce::AudioBuffer<float> chunk((int)reader->numChannels,4096);
                    if(!task->beat){task->audio=std::make_shared<skellpreset::liveMatch::Audio>();task->audio->rate=reader->sampleRate;task->audio->stereo.reserve((size_t)std::min<juce::int64>(reader->lengthInSamples,(juce::int64)(reader->sampleRate*20))*2);}
                    for(juce::int64 offset=0;offset<reader->lengthInSamples&&!analyzer.finished()&&!task->cancel.load(std::memory_order_relaxed);){
                        const int n=(int)std::min<juce::int64>(4096,reader->lengthInSamples-offset);
                        if(!reader->read(&chunk,0,n,offset,true,reader->numChannels>1)){profile={};break;}
                        const auto before=analyzer.frameCount();analyzer.push(chunk.getReadPointer(0),reader->numChannels>1?chunk.getReadPointer(1):nullptr,n);profile=analyzer.result();
                        if(task->audio){for(int i=0;i<(int)(analyzer.frameCount()-before);++i){const float l=chunk.getSample(0,i);task->audio->stereo.push_back(l);task->audio->stereo.push_back(reader->numChannels>1?chunk.getSample(1,i):l);}}
                        offset+=n;
                    }
                }
            }
            task->profile=profile;
            }catch(const std::bad_alloc&){task->audio.reset();task->profile={};}
            task->done.store(true,std::memory_order_release);
        }));
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
        const auto presentation=processor.presentation();
        processor.setStateInformation(target.state.getData(),
                                      (int)target.state.getSize());
        processor.setPresentation(presentation); // audio undo/redo never changes presentation
        browser.restoreHistorySelection(target.selectedPreset);
        pushHistory(to,std::move(current));
        pendingHistory.reset();
        updateHistoryButtons();
        refresh();
    }
    SkellPresetAudioProcessorEditor& outer;
    SkellPresetAudioProcessor& processor;
    ChainPanel chain;SlotEditor slot;SkellIdleDeck idle;SkellRoutingPage routing;
    SkellLiveEffectPanel focusVisual;
    WorkspaceEdge rackOuterEdge,rackInnerEdge,visualLeftEdge,visualRightEdge,drawerEdge{true};
    PresetBrowser browser;SkellMicModelPanel micPanel;
    SkellSettingsPanel settingsPanel;
    SkellMoonButton moonButton;
    SkellActionButton stageButton;
    SkullThemeButton themeButton;
    BypassShade bypassShade;
    std::unique_ptr<skellcore::ui::LookAndFeel> skin;
    int selectedTheme=Theme::Obsidian,previousMode=1;
    bool rackOnRight=false,editingIdle=true,macroOpen=false,rackHidden=false,routingReturnIdle=true;
    int lastInputMeter=-1,lastOutputMeter=-1;
    float displayedInputPeak=0.f,displayedOutputPeak=0.f;
    bool lastInputOverload=false,lastOutputOverload=false;
    bool lastMicOn=false;
    int lastRecordingState=-1;
    unsigned lastScopeSampleCount=~0u;
    unsigned lastScopeFrameTimeMs=0u;
    SkellActionButton preset,matchButton,undo,redo,record,micButton;
    SkellMatchPanel matchPanel;
    std::unique_ptr<juce::FileChooser> matchChooser;
    std::uint64_t matchSession=0;
    skellpreset::liveMatch::Profile matchBeat;
    skellpreset::liveMatch::MixIntent matchIntent;
    std::optional<skellpreset::liveMatch::Recipe> matchLastRecipe;
    juce::String matchLastDetail;
    struct MatchFileResult {std::atomic<bool> cancel{false},done{false};skellpreset::liveMatch::Profile profile;std::shared_ptr<skellpreset::liveMatch::Audio> audio;bool reference=false,beat=false;};
    std::shared_ptr<MatchOptimizationResult> matchOptimization;
    std::shared_ptr<skellpreset::liveMatch::Audio> matchAudio;
    std::shared_ptr<MatchFileResult> matchFile;
    juce::ThreadPool matchPool{1};
    skellpreset::liveMatch::Profile matchDry;
    bool matchListening=false;
    double matchStarted=0.;
    SkellSettingsButton settings;
    std::unique_ptr<juce::FileChooser> recordChooser;
    juce::File lastCaptureFile;
    SkellPowerButton powerButton;
    std::array<SkellMacroKnob,4> macro;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> macroAttachment;
    juce::Slider input,mix,output;
    // Bipolar output-stage separation dial beside the master hardware group.
    SkellStereoSeparationKnob stereoSeparation;
    std::array<juce::Label,4> masterValues;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment,mixAttachment,outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoSeparationAttachment;
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
bool SkellPresetAudioProcessorEditor::configureBrowserForScreenshot(int page,int width,int height,bool list) {
    return canvas->configureBrowserForScreenshot(page,width,height,list);
}
bool SkellPresetAudioProcessorEditor::smartMatchForScreenshot(int scene){return canvas->smartMatchForScreenshot(scene);}
void SkellPresetAudioProcessorEditor::setMatchPageForScreenshot(int page) {
    canvas->setMatchPageForScreenshot(page);
}
void SkellPresetAudioProcessorEditor::setSuiteForScreenshot(int page,bool stylesOpen) {
    canvas->setSuiteForScreenshot(page,stylesOpen);
}
void SkellPresetAudioProcessorEditor::setSuiteSpectrumForScreenshot(bool enabled) {
    canvas->setSuiteSpectrumForScreenshot(enabled);
}
void SkellPresetAudioProcessorEditor::setMicPanelForScreenshot(bool visible) {
    canvas->setMicPanelForScreenshot(visible);
}
void SkellPresetAudioProcessorEditor::setSettingsPanelForScreenshot(bool visible) {
    canvas->setSettingsPanelForScreenshot(visible);
}
void SkellPresetAudioProcessorEditor::setModOrbForScreenshot(bool enabled) {
    canvas->setModOrbForScreenshot(enabled);
}

void SkellPresetAudioProcessorEditor::dockRackForScreenshot(bool right) {
    canvas->dockRackForScreenshot(right);
}

void SkellPresetAudioProcessorEditor::showRoutingForScreenshot(){canvas->showRoutingForScreenshot();}
void SkellPresetAudioProcessorEditor::configureEdgesForScreenshot(bool right,bool hide,bool controls,bool sidechain){canvas->configureEdgesForScreenshot(right,hide,controls,sidechain);}
bool SkellPresetAudioProcessorEditor::updateLiveVisualForScreenshot(){return canvas->updateLiveVisualForScreenshot();}
void SkellPresetAudioProcessorEditor::updateStageForScreenshot(double nowMs){canvas->updateStageForScreenshot(nowMs);}
bool SkellPresetAudioProcessorEditor::showHistoryForScreenshot(bool redoAvailable){return canvas->showHistoryForScreenshot(redoAvailable);}
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
