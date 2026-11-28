#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "ModulePicker.h"
#include "SkellActionButton.h"
#include "VocalFeelStyles.h"
#include "InstalledVST3Browser.h"
#include "SkellEQNodeMenu.h"
#include <array>
#include <memory>
#include <functional>
#include "Theme.h"
#include "SkellLiveEffectPanel.h"

// The actual DSP parameter value is always painted beside the dial by
// SlotEditor. The old normalized 0-100 hover badge was contradictory (e.g.
// "68" over a -19.4 dBFS threshold) and is intentionally removed.
// Retain precise host automation and mouse-wheel behavior.
class SkellHoverPercentKnob final:public juce::Slider {
public:
    void mouseEnter(const juce::MouseEvent& e) override {
        hovered=true;juce::Slider::mouseEnter(e);repaint();
    }
    void mouseExit(const juce::MouseEvent& e) override {
        hovered=false;juce::Slider::mouseExit(e);repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        editing=true;juce::Slider::mouseDown(e);repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        juce::Slider::mouseDrag(e);repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override {
        juce::Slider::mouseUp(e);editing=false;repaint();
    }
    bool showsValueForTest() const noexcept {return hovered||editing;}
protected:
    void paintOverChildren(juce::Graphics& g) override {
        juce::Slider::paintOverChildren(g);
        // Physical-unit readout is permanently visible beside the dial;
        // no competing normalized percentage bubble on hover/drag.
    }
private:
    void paintHardware(juce::Graphics&,int type);
    void bindLimiterControls();
    bool hovered=false,editing=false;
};
// Single corner light controls the exact sN_on automation used by the rack.
// Drawn as a real button with a generous hit target, not a decorative LED.
class SkellSlotPowerDot final:public juce::Button {
public:
    SkellSlotPowerDot():juce::Button("Toggle this rack slot") {
        setTooltip("Click to bypass or enable this effect in both the editor and rack");
    }
    void setStatus(bool active,bool available) {
        if(on!=active || isEnabled()!=available) {
            on=active;setEnabled(available);repaint();
        }
    }
    void paintButton(juce::Graphics& g,bool hover,bool pressed) override {
        const float cx=getWidth()*.5f,cy=getHeight()*.5f;
        if(on && isEnabled()) {
            g.setColour(Theme::green.withAlpha(hover?.32f:.15f));
            g.fillEllipse(cx-13.f,cy-13.f,26.f,26.f);
        }
        g.setColour(Theme::bg);g.fillEllipse(cx-9.f,cy-9.f,18.f,18.f);
        g.setColour(on && isEnabled()?Theme::green:Theme::stroke);
        g.drawEllipse(cx-9.f,cy-9.f,18.f,18.f,1.7f);
        g.setColour(on && isEnabled()?Theme::green.brighter(pressed?0.f:.18f):
                    Theme::muted.withAlpha(.55f));
        g.fillEllipse(cx-5.f,cy-5.f,10.f,10.f);
        if(hasKeyboardFocus(true)) {
            g.setColour(Theme::blue.withAlpha(.92f));
            g.drawEllipse(cx-12.f,cy-12.f,24.f,24.f,1.5f);
        }
    }
private:
    bool on=false;
};
// Compact JUCE popup with real APVTS attachments and no audio-thread GUI.
class SkellWetDuckPanel final:public juce::Component {
public:
    void setReduction(float db) {
        const int step=juce::jlimit(0,120,juce::roundToInt(db*2.f));
        if(step!=reduction){reduction=step;repaint(10,getHeight()-27,getWidth()-20,23);}
    }
    void paint(juce::Graphics& g) override {
        Theme::panelBox(g,getLocalBounds().toFloat().reduced(1.f),8.f,true);
        Theme::text(g,"SIDECHAIN / EFFECT CONTRIBUTION",
                    {16,11,getWidth()-57,24},13.f,Theme::green,true);
        Theme::text(g,"DUCKING",{17,84,86,20},11,Theme::white,true);
        Theme::text(g,"THRESHOLD",{17,125,86,20},11,Theme::white,true);
        Theme::text(g,"ATTACK",{17,166,86,20},11,Theme::white,true);
        Theme::text(g,"RELEASE",{17,207,86,20},11,Theme::white,true);
        Theme::fittedText(g,"MEASURED DUCK: "+juce::String(reduction*.5f,1)+" dB",
                          {17,getHeight()-23,getWidth()-33,18},10,reduction>0?Theme::green:Theme::muted);
    }
private:
    int reduction=0;
};
class SlotEditor final:public juce::Component {
public:
    explicit SlotEditor(SkellPresetAudioProcessor& p);
    ~SlotEditor() override;
    std::function<void()> onBackgroundClick; // legacy callback; home is reached by Escape or canvas gap
    static juce::Rectangle<int> homeReturnBounds() noexcept {return {};}
    bool testHomeReturnHitZonesForTest() const noexcept {return homeReturnBounds().isEmpty();}
    void paint(juce::Graphics&) override;
    void resized() override;
    void select(int newSlot);
    bool updateLiveVisualForScreenshot(){livePanel.update(processor,slot);return livePanel.hasSignal();}
    void applyTheme();
    void refreshIfChanged(); // only redraw when DSP controls or type change
    int selectedSlot()const noexcept{return slot;}
    bool moduleSelectorMatchesAudio() const noexcept {
        return moduleBox.getSelectedId()==processor.slotType(slot)+1;
    }
    void testGraphGesture(juce::Point<float> point) {updateGraphNode(point);}
    // Rack double-click opens this search overlay; no duplicate selector.
    void openModulePicker();
    void openSidechainControls() {
        if(!SkellPresetAudioProcessor::supportsEffectSidechain(processor.slotType(slot)))return;
        dismissTransientOverlays();
        duckPanel.setVisible(true);duckPanel.toFront(true);
    }
    void openModulePickerForScreenshot() {openModulePicker();}
    // UI-only: prevent overlapping modal effect selectors when opening the MIC panel.
    void dismissTransientOverlays() {
        vocalFeelStyles.close();
        modulePicker.hide();
        installedBrowser.hide();
        eqNodeMenu.close();
        endGraphGesture();
        duckPanel.setVisible(false);
        livePanel.setVisible(false);processor.requestSlotVisual(-1);
    }
    bool modulePickerVisibleForTest() const noexcept {return modulePicker.isVisible();}
    bool hasEmbeddedEditorForTest() const noexcept {return embeddedEditor!=nullptr;}
    void setHostedEditorPresentation(bool allowed) {
        hostedPresentation=allowed;
        if(!allowed){hideEmbeddedEditor();processor.requestSlotVisual(-1);}
        else syncPresentation();
    }
    void openInstalledVST3BrowserForScreenshot() {loadExternal.onClick();}
    bool testModulePickerInteraction();
    bool testExternalBrowseCancel();
    bool testEightBandGraphAndQ();
    bool testStudioEqSeventhBand();
    bool testStudioEqAddAndFilterTypes();
    bool testStudioEqGraphDoubleClick();
    bool testKnobReadouts();
    bool testClassicDynamicsUnits();
    bool testMatchFinishPages();
    bool testStudioLimiterPages();
    bool testStudioDelayPages();
    bool testVocalFeelSuiteUI();
    bool testSuiteSpectrumUIForTest();
    bool testAdlibControls();
    bool testWetSidechainPanel();
    bool testMicModelCards();
    bool testPowerDotSync();
    bool testIntegratedEffectLayout() const;
    bool testPopupLayoutsForTest() const;
    bool testAllNativeEffectLayoutsForTest();
    bool testModOrbToggle();
    void setModOrbForScreenshot(bool enabled);
    void setMatchPageForScreenshot(int page) {showMatchPage(page);}
    void setSuiteForScreenshot(int page,bool stylesOpen) {
        showSuitePage(page);
        if(stylesOpen && vocalFeelStyleButton.onClick)
            vocalFeelStyleButton.onClick();
    }
    void setSuiteSpectrumForScreenshot(bool enabled) {
        if(processor.slotType(slot)==SkellPresetAudioProcessor::VocalFeelSuiteFX
           && suiteSpectrumView!=enabled && suiteSpectrumButton.onClick)
            suiteSpectrumButton.onClick();
    }
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool testEffectPageShortcutsForTest();
    bool testLiveViewForTest();
private:
    void paintHardware(juce::Graphics&,int type);
    std::array<juce::String,4> titlesForHardware(int type) const;
    void bindLimiterControls();
    SkellPresetAudioProcessor& processor;
    int slot=0;
    int eqBank=0; // two editable EQ bands per knob row; graph shows all eight
    int activeEqBand=0;
    int matchPage=0; // 0: sound shape / 1: calibration / 2: native reference match
    int limiterPage=0; // local UI pages only; existing p1..p9 host IDs
    bool limiterDetailsVisible=false; // historical renderer compatibility, never a visible drawer
    void updateLimiterCompactVisibility();
    int delayPage=0; // opt-in Studio Delay: 16 controls across four UI pages
    int suitePage=0; // Vocal Feel Suite: source 16 controls in four pages
    int gestureSuiteParameter=-1; // return p11..p15 graph gestures
    bool suiteSpectrumView=false;
    int suiteSpectrumTick=0;
    skellpreset::visual::LiveSpectrumAnalyzer suiteSpectrumAnalyzer;
    void showEqBank(int firstBand);
    void showMatchPage(int page);
    void showLimiterPage(int page);
    void showDelayPage(int page);
    void showSuitePage(int page);
    void selectSuiteRecipe(int index);
    void applyKnobReadouts();
    void layoutParameterControls(int type);
    void endGraphGesture();
    // ComboBox remains as a noninteractive compatibility/synchronization
    // adapter; the user-facing selector is the searchable native overlay.
    juce::ComboBox moduleBox;
    SkellSlotPowerDot slotPower;
    SkellModulePicker modulePicker;
    SkellActionButton browseEffects{"BROWSE ALL EFFECTS"},browseExternal{"LOAD A VST3"};
    class CategoryTile final:public juce::Button {
    public:
        CategoryTile():juce::Button("Effect category") {}
        int index=0;
        void paintButton(juce::Graphics& g,bool over,bool down) override {
            constexpr const char* names[]{"DYNAMICS","TONE","SPACE","MOTION","PITCH","VOCAL"};
            constexpr const char* detail[]{"Compression, gates, limiting","EQ, saturation, colour",
                "Delay, reverb, doubling","Chorus, flanger, modulation","Pitch and formant","Complete vocal processing"};
            const auto area=getLocalBounds().toFloat().reduced(.8f);
            Theme::panelBox(g,area,7.f);
            const auto colour=index==0?Theme::amber:index==2||index==4?Theme::blue:index==3?Theme::pink:Theme::green;
            if(over || down || hasKeyboardFocus(true)){
                g.setColour(colour.withAlpha(down?.16f:.075f));
                g.fillRoundedRectangle(area,7.f);
                g.setColour(colour.withAlpha(.74f));
                g.drawRoundedRectangle(area,7.f,1.3f);
            }
            const float x=24.f,y=area.getCentreY();
            g.setColour(colour.withAlpha(.11f));
            g.fillRoundedRectangle(x-12.f,y-18.f,44.f,36.f,6.f);
            g.setColour(colour.withAlpha(.95f));
            juce::Path glyph;
            switch(index){
                case 0:glyph.startNewSubPath(x-5.f,y+10.f);glyph.lineTo(x+5.f,y-3.f);
                    glyph.quadraticTo(x+12.f,y-11.f,x+24.f,y-11.f);break;
                case 1:glyph.startNewSubPath(x-6.f,y+7.f);glyph.cubicTo(x+2.f,y+7.f,x+2.f,y-12.f,x+9.f,y-12.f);
                    glyph.cubicTo(x+15.f,y-12.f,x+16.f,y+6.f,x+25.f,y+6.f);break;
                case 2:for(int i=0;i<4;++i)g.drawEllipse(x+6.f*i-8.f,y-(11.f-2.f*i),22.f-4.f*i,22.f-4.f*i,1.2f);break;
                case 3:for(int i=0;i<=30;++i){const float xx=x-6.f+(float)i;
                    const float yy=y+8.f*std::sin((float)i*.42f);if(i==0)glyph.startNewSubPath(xx,yy);else glyph.lineTo(xx,yy);}break;
                case 4:glyph.startNewSubPath(x-4.f,y+9.f);glyph.lineTo(x+5.f,y-8.f);
                    glyph.lineTo(x+12.f,y+2.f);glyph.lineTo(x+23.f,y-11.f);break;
                default:g.drawRoundedRectangle(x+3.f,y-13.f,9.f,17.f,4.f,1.6f);
                    glyph.startNewSubPath(x-2.f,y-1.f);glyph.cubicTo(x-2.f,y+12.f,x+17.f,y+12.f,x+17.f,y-1.f);
                    glyph.startNewSubPath(x+7.5f,y+8.f);glyph.lineTo(x+7.5f,y+15.f);break;
            }
            g.strokePath(glyph,juce::PathStrokeType(1.7f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            Theme::fittedText(g,names[index],{70,18,getWidth()-85,25},16.f,Theme::white,true);
            Theme::fittedText(g,detail[index],{70,48,getWidth()-85,21},12.f,Theme::muted);
        }
    };
    std::array<CategoryTile,6> effectCategories;
    static constexpr std::array<const char*,6> categoryNames{{"Dynamics","Tone","Space","Motion","Pitch","Vocal"}};
    void openEffectCategory(const juce::String& category);
    void syncEmptyActions();
    SkellInstalledVST3Browser installedBrowser;
    juce::TextButton loadExternal{"LOAD VST3"},editExternal{"OPEN PLUGIN UI"};
    juce::TextButton upgradeEq{"UPGRADE TO PARAMETRIC EQ"};
    juce::TextButton addEqNode{"+ ADD NODE"};
    SkellEQNodeMenu eqNodeMenu;
    int contextEqBand=0;
    void setStudioEqBandType(int band,int type);
    void setStudioEqBandEnabled(int band,bool enabled);
    int studioEqBandType(int band)const;
    bool studioEqBandEnabled(int band)const;
    void syncStudioEqNodeButtons();
    bool addStudioEqNodeAt(juce::Point<float> point);
    juce::TextButton modOrbButton{"ORB"};
    bool modOrbView=false; // UI-only; never serialized or used in DSP
    std::unique_ptr<juce::FileChooser> externalChooser;
    bool externalBrowsePending=false; // UI-only, not audio-thread state
    // Owned UI view, never created in the audio callback. Each installed
    // plugin may expose a JUCE wrapper editor; unsupported editors use
    // the original separate-window path without affecting DSP processing.
    std::unique_ptr<juce::AudioProcessorEditor> embeddedEditor;
    juce::AudioPluginInstance* embeddedHost=nullptr;
    bool hostedPresentation=false,hostedClassMenuOpen=false;
    void hideEmbeddedEditor(){if(embeddedEditor)embeddedEditor->setVisible(false);}
    bool embeddedFitRequested=false;
    bool embeddedFitRejected=false;
    void releaseEmbeddedEditor();
    void syncEmbeddedEditor();
    void layoutEmbeddedEditor();
    juce::Slider eqQSlider;
    std::array<juce::Slider,5> eqDetails;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,5> eqDetailAttachments;
    std::array<juce::Slider,2> eqCutSliders;
    std::array<juce::TextButton,8> eqBandButtons;
    std::array<juce::TextButton,3> matchPageButtons;
    std::array<juce::TextButton,4> delayPageButtons;
    juce::ToggleButton rightFirst{"RIGHT FIRST (PING PONG)"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> rightFirstAttachment;
    std::array<juce::TextButton,4> suitePageButtons;
    juce::TextButton vocalFeelStyleButton{"VOICE STYLES  ▼"};
    SkellVocalFeelStyles vocalFeelStyles;
    juce::TextButton suiteSpectrumButton{"SPECTRUM"};
    juce::Slider studioGateFloor;
    juce::Slider studioCompKnee;
    juce::TextButton duckOpen{"SIDECHAIN"},duckClose{"X"};
    SkellWetDuckPanel duckPanel;
    juce::ComboBox duckSource;
    juce::ToggleButton parallelReturn{"PARALLEL RETURN (POST MIC)"},duckExternal{"EXTERNAL HOST KEY"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> duckExternalAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> duckSourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> parallelReturnAttachment;
    juce::TextButton liveView{"VISUAL"};
    SkellLiveEffectPanel livePanel;
    void syncPresentation();
    juce::ToggleButton duckOn{"DUCK WHEN VOCAL IS PRESENT"};
    juce::Slider duckDepth,duckThreshold,duckAttack,duckRelease;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> duckOnAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> duckAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateFloorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> studioCompKneeAttachment;
    std::array<SkellHoverPercentKnob,4> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqQAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,2> eqCutAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> knobAttachment;
    bool draggingGraph=false;
    int gestureSlot=-1,gestureType=-1;
    int gestureEqBand=-1; // same parameter IDs receive begin/end gestures
    bool graphSnapshotValid=false;
    int lastType=-1;
    int lastPeakLevel=-1,lastGainReductionStep=-1,lastDeessStep=-1;
    std::array<int,5> lastSuiteLevels{{-1,-1,-1,-1,-1}};
    bool lastPowerOn=false;
    int lastHostedLatency=-1;
    bool lastHostedLoaded=false;
    std::array<float,6> lastControls{};
    std::array<float,16> lastExtendedControls{};
    std::array<float,98> lastEqControls{}; // includes eight new filter types/enables
    bool updateMicCard(juce::Point<float> point);
    bool updateSkelldizerCard(juce::Point<float> point);
    void updateGraphNode(juce::Point<float>);
    void chooseVST3File();
    void loadVST3FromFile(const juce::File& file);
};
