#include "SlotEditor.h"
#include "Theme.h"
#include "HardwareLayout.h"
#include "PhysicalReadouts.h"
#include "../chain/EffectRegistry.h"
#include <cmath>
#include <cstdio>
#include <limits>
namespace {
bool isPagedEQ(int type) noexcept {
    return type==SkellPresetAudioProcessor::EightBandToneFX
        || type==SkellPresetAudioProcessor::StudioEQFX;
}
// The actual face geometry follows the instrument, not a single generic row.
enum class ControlFace { eqPairs, dynamicsChannel, rotaryBank, vocalChannel };
ControlFace controlFace(int type) noexcept {
    using P=SkellPresetAudioProcessor;
    if(isPagedEQ(type) || type==P::EQ || type==P::AdlibFilter || type==P::Radio || type==P::MicModelFX)
        return ControlFace::eqPairs;
    const auto family=std::string_view(skellpreset::registry::at(type).category);
    if(family=="Dynamics")return ControlFace::dynamicsChannel;
    if(family=="Vocal")return ControlFace::vocalChannel;
    return ControlFace::rotaryBank;
}
bool rotaryControl(int type,int index) noexcept {
    if(skellpreset::face::hardware(type))return type!=30 || index<2;
    const auto face=controlFace(type);
    // Dynamics: threshold/frequency and ratio/level are precision faders;
    // timing/amount knobs use rotary motion. Vocal: four matching knobs.
    if(type==SkellPresetAudioProcessor::ThreeBandDynamics)return true; // three bands and timing
    if(type==SkellPresetAudioProcessor::SerialDynamicsFX)return true; // FET/Opto mix controls
    return face==ControlFace::rotaryBank || face==ControlFace::vocalChannel ||
        (face==ControlFace::dynamicsChannel && index>=2);
}
juce::Rectangle<int> parameterCardBounds(int type,int index,bool compactLimiter=false) noexcept {
    if(compactLimiter && type==SkellPresetAudioProcessor::StudioLimiterFX && index<2)
        return {33+index*423,401,412,120};
    if(controlFace(type)==ControlFace::eqPairs)
        return {33+(index%2)*423,401+(index/2)*61,412,55};
    return {33+index*211,401,202,120};
}
juce::Rectangle<int> parameterControlBounds(int type,int index,bool compactLimiter=false) noexcept {
    if(skellpreset::face::hardware(type))return skellpreset::face::control(type,index);
    const auto card=parameterCardBounds(type,index,compactLimiter);
    if(controlFace(type)==ControlFace::eqPairs)
        return {card.getX()+126,card.getY()+8,card.getWidth()-137,38};
    if(rotaryControl(type,index))
        return {card.getCentreX()-69,card.getY()+26,138,91};
    return {card.getX()+12,card.getY()+33,card.getWidth()-24,72};
}
float eqGainAtY(int type,float y) noexcept {
    return juce::jlimit(0.f,1.f,.5f+(254.f-y)/206.f*
        (type==SkellPresetAudioProcessor::StudioEQFX?2.f:2.4f));
}
float eqVisualFrequency(const SkellPresetAudioProcessor& p,int slot,int band,int type) {
    const float x=p.getSlotParam(slot,band*2);
    if(type==SkellPresetAudioProcessor::StudioEQFX)
        return 20.f*std::pow(1000.f,x);
    const float lo=skellpreset::dsp::EightBandTone::edges[(size_t)band];
    const float hi=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
    return lo*std::pow(hi/lo,x);
}
bool isModulationOrbEffect(int type) noexcept {
    using P=SkellPresetAudioProcessor;
    return type==P::Tremolo || type==P::StereoWidth ||
           type==P::VocalChorusFX || type==P::VocalFlangerFX ||
           type==P::RingModFX || type==P::VocalVibratoFX;
}
const juce::StringArray names=[] {
    juce::StringArray result;
    for(const auto& effect:skellpreset::registry::effects)result.add(effect.name);
    return result;
}();
std::array<juce::String,4> titles(int type) {
    switch(type){
        case 1:return {"Frequency","Gain","Q","Low Cut"};
        case 2:return {"Threshold","Ratio","Attack","Release"};
        case 3:return {"Ceiling","Drive","Knee","Release"};
        case 4:return {"Threshold","Release","Hold","Hysteresis"};
        case 5:return {"Drive","Character","Tone","Bias"};
        case 6:return {"Decay","Damping","Size","Tone"};
        case 7:return {"Time","Feedback","Filter","Stereo"};
        case 8:return {"Excite","Colour","Focus","Mix"};
        case 9:return {"Throw Time","Feedback","Tone","Trigger"};
        case 10:return {"Width","Timing","Motion","Depth"};
        case 11:return {"Sib. Freq","Threshold","Reduction","Release"};
        case 12:return {"Plugin","Settings","State","PDC"};
        case 13:return {"Low Cut","High Cut","Nasal","Drive"};
        case 14:return {"Bit Depth","Sample Rate","Drive","Dither"};
        case 15:return {"Rate","Depth","Shape","Stereo"};
        case 16:return {"Width","Mono Bass","Balance","Bass Mix"};
        case 17:return {"Low Dyn","Mid Dyn","High Dyn","Release"};
        case 18:return {"Room Size","Damping","Pre-delay","Low Cut"};
        case 19:return {"Threshold","Ratio","Soft Knee","Release"};
        case 20:return {"Rate","Depth","Voices","Stereo"};
        case 21:return {"Rate","Sweep","Feedback","Stereo"};
        case 22:return {"Carrier Hz","Depth","Shape","Stereo"};
        case 23:return {"Amount","Target","Range","Warmth"};
        case 24:return {"Division","Feedback","Ducking","Tone"};
        case 25:return {"Rate","Depth","Stereo","Shape"};
        case 26:return {"B1 Freq","B1 Gain","B2 Freq","B2 Gain"};
        case 27:return {"Threshold","Ratio","FET Speed","Parallel"};
        case 28:return {"Frequency","Threshold","Reduction","Release"};
        case 29:return {"Threshold","Strength","Hold","Release"};
        case 30:return {"Pitch","Formant","Pitch Range","Formant Range"};
        case 31:return {"Harmonic","Envelope","Slow Contour","Fast Attack"};
        case 32:return {"Input Mic","Simulate","Amount","Trim"};
        case 33:return {"Amount","Character","Output","Level Match"};
        case 34:return {"Band 1 Freq","Band 1 Gain","Band 2 Freq","Band 2 Gain"};
        case 35:return {"Ceiling","Drive","Comp Thresh","Comp Ratio"};
        case 36:return {"Threshold","Ratio","Attack","Release"};
        case 37:return {"Pre Gain","Threshold","Type A/B","Effect Mix"};
        case 38:return {"Time","Feedback","Stereo Mode","R/L Offset"};
        case 39:return {"Depth","Beat Division","Gate Tightness","Mix"};
        case 40:return {"Stereo Width","LFO Rate","Unused","Mix"};
        case 41:return {"Amount","HP Blend","Cutoff","Mix"};
        case 42:return {"Drive","Unused","Colour","Mix"};
        case 43:return {"Amount","Colour","Voice","Parallel"};
        case 44:return {"Voice","Sens","Low","Mid"};
        default:return {"Level","Tone","Time","Blend"};
    }
}
}
SlotEditor::SlotEditor(SkellPresetAudioProcessor& p):
    processor(p),modulePicker(p),installedBrowser(p) {
    setWantsKeyboardFocus(true);
    for(int i=0;i<names.size();i++)moduleBox.addItem(names[i],i+1);
    moduleBox.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
    moduleBox.setColour(juce::ComboBox::textColourId,Theme::white);
    moduleBox.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    addChildComponent(moduleBox); // retain selected IDs for host/UI assertions
    moduleBox.setVisible(false);
    slotPower.onClick=[this] {
        if(processor.slotType(slot)==SkellPresetAudioProcessor::Empty)return;
        const auto* current=processor.state.getRawParameterValue(
            "s"+juce::String(slot+1)+"_on");
        processor.setSlotOn(slot,!(current && current->load()>.5f));
        graphSnapshotValid=false;
        refreshIfChanged();
    };
    slotPower.setWantsKeyboardFocus(true);
    addAndMakeVisible(slotPower);
    modulePicker.onSelect=[this](int type){
        const bool wasEmpty=processor.slotType(slot)==SkellPresetAudioProcessor::Empty;
        processor.setSlotType(slot,type);
        // New blank instances must make an inserted effect audible. Preserve
        // an intentionally bypassed existing effect on replacement.
        if(type==SkellPresetAudioProcessor::Empty)
            processor.setSlotOn(slot,false);
        else if(wasEmpty)
            processor.setSlotOn(slot,true);
        moduleBox.setSelectedId(type+1,juce::dontSendNotification);
        graphSnapshotValid=false;
        refreshIfChanged();
        repaint();
    };
    modulePicker.onExternal=[this] {
        if(slot<0)return;
        // Choosing the external *browser* is not a processor replacement.
        // Keep the current audio effect intact until a third-party bundle
        // successfully instantiates; cancel/failure must not mute the chain.
        externalBrowsePending=true;
        hideEmbeddedEditor();
        installedBrowser.open();
        installedBrowser.toFront(true);
    };
    addAndMakeVisible(modulePicker);
    modulePicker.setVisible(false);
    for(auto* button:{&loadExternal,&editExternal}){
        button->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        button->setColour(juce::TextButton::textColourOffId,Theme::green);
        addAndMakeVisible(*button);
        button->setVisible(false);
    }
    loadExternal.setTooltip("Search installed VST3 effects or browse for a VST3 file");
    editExternal.setTooltip("Open the hosted VST3 editor in a separate window");
    // Directory discovery is file-only. The third-party library is opened
    // exclusively after choosing a bundle or using the manual picker.
    addAndMakeVisible(installedBrowser);
    installedBrowser.setVisible(false);
    installedBrowser.onLoad=[this](const juce::File& path){
        externalBrowsePending=false;
        loadVST3FromFile(path);
    };
    installedBrowser.onDismiss=[this]{externalBrowsePending=false;};
    installedBrowser.onBrowseFile=[this]{chooseVST3File();};
    loadExternal.onClick=[this] {
        if(installedBrowser.isVisible())installedBrowser.hide();
        else {
            if(modulePicker.isVisible())modulePicker.hide();
            hideEmbeddedEditor();
            installedBrowser.open();
        }
    };
    editExternal.onClick=[this] {
        // Explicit detach/fallback for native editors that prefer a normal
        // top-level window. Never open a second view of one processor.
        releaseEmbeddedEditor();
        processor.openHostedVST3Editor(slot);
    };
    editExternal.setButtonText("DETACH UI");
    editExternal.setTooltip("Detach third-party UI into its own window if needed");
    upgradeEq.setTooltip("Convert this classic single-bell EQ into an editable eight-band parametric EQ; previous presets are unchanged.");
    upgradeEq.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.18f));
    upgradeEq.setColour(juce::TextButton::textColourOffId,Theme::green);
    addAndMakeVisible(upgradeEq);
    upgradeEq.setVisible(false);
    modOrbButton.setTooltip("Digital Bones: actual modulation controls and post-FX activity; press GRAPH to return.");
    addAndMakeVisible(modOrbButton);
    modOrbButton.setVisible(false);
    modOrbButton.onClick=[this]{setModOrbForScreenshot(!modOrbView);};
    upgradeEq.onClick=[this] {
        if(processor.slotType(slot)!=SkellPresetAudioProcessor::EQ)return;
        const float oldFreq=processor.getSlotParam(slot,0);
        const float oldGain=processor.getSlotParam(slot,1);
        const float oldQ=processor.getSlotParam(slot,2);
        const float oldHp=processor.getSlotParam(slot,3);
        const float frequency=40.f*std::pow(300.f,oldFreq);
        int target=0;
        for(int i=1;i<8;++i)
            if(frequency>=skellpreset::dsp::EightBandTone::edges[(size_t)i])
                target=i;
        const float lo=skellpreset::dsp::EightBandTone::edges[(size_t)target];
        const float hi=skellpreset::dsp::EightBandTone::edges[(size_t)target+1];
        const float freqNorm=juce::jlimit(0.f,1.f,
            (float)(std::log(frequency/lo)/std::log(hi/lo)));
        // The classic EQ is a single peaking filter plus a high pass.
        // Translate it to one of eight neutral bells and a cutoff rather
        // than losing edits or mutating earlier factory programs.
        for(int i=0;i<8;++i){
            processor.setSlotParam(slot,i*2,.5f);
            processor.setSlotParam(slot,i*2+1,.5f);
        }
        processor.setSlotParam(slot,target*2,freqNorm);
        processor.setSlotParam(slot,target*2+1,
            juce::jlimit(0.f,1.f,.5f+(oldGain-.5f)*.8f));
        const auto prefix="s"+juce::String(slot+1)+"_";
        if(auto* value=processor.state.getParameter(prefix+"eqQ"+juce::String(target+1)))
            value->setValueNotifyingHost(
                skellpreset::dsp::EightBandTone::normalizedFromQ(.3f+7.f*oldQ));
        const float cutoff=20.f+250.f*oldHp;
        if(auto* value=processor.state.getParameter(prefix+"eqLowCut"))
            value->setValueNotifyingHost(juce::jlimit(0.f,1.f,
                (float)(std::log(cutoff/20.f)/std::log(20.f))));
        if(auto* value=processor.state.getParameter(prefix+"eqHighCut"))
            value->setValueNotifyingHost(0.f);
        processor.setSlotType(slot,SkellPresetAudioProcessor::EightBandToneFX);
        select(slot);
        activeEqBand=target;
        showEqBank(target);
    };
    eqQSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    eqQSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,59,22);
    eqQSlider.setRange(0.,1.,.001);
    eqQSlider.setTooltip("Select an EQ node to edit its independent bandwidth (Q 0.4–8)");
    addAndMakeVisible(eqQSlider);
    eqQSlider.setVisible(false);
    for(int index=0;index<8;++index) {
        auto& button=eqBandButtons[(size_t)index];
        button.setButtonText(juce::String(index+1));
        button.setTooltip("Edit parametric EQ band "+juce::String(index+1));
        button.setWantsKeyboardFocus(true);
        button.onClick=[this,index]{
            activeEqBand=index;
            showEqBank(index);
        };
        addAndMakeVisible(button);
        button.setVisible(false);
    }
    addEqNode.setTooltip("Add another independent Studio EQ band (maximum eight)");
    addEqNode.onClick=[this]{
        if(processor.slotType(slot)!=SkellPresetAudioProcessor::StudioEQFX)return;
        for(int i=0;i<8;++i)if(!studioEqBandEnabled(i)){
            setStudioEqBandEnabled(i,true);
            activeEqBand=i;showEqBank(i);syncStudioEqNodeButtons();
            return;
        }
    };
    addAndMakeVisible(addEqNode);addEqNode.setVisible(false);
    eqNodeMenu.onFilterType=[this](int kind){
        if(processor.slotType(slot)==SkellPresetAudioProcessor::StudioEQFX &&
           studioEqBandEnabled(contextEqBand))setStudioEqBandType(contextEqBand,kind);
    };
    eqNodeMenu.onRemove=[this]{
        if(processor.slotType(slot)!=SkellPresetAudioProcessor::StudioEQFX ||
           !studioEqBandEnabled(contextEqBand))return;
        setStudioEqBandEnabled(contextEqBand,false);
        if(contextEqBand==activeEqBand){
            for(int i=0;i<8;++i)if(studioEqBandEnabled(i)){
                activeEqBand=i;break;
            }
        }
        showEqBank(activeEqBand);syncStudioEqNodeButtons();
    };
    addAndMakeVisible(eqNodeMenu);eqNodeMenu.setVisible(false);
    for(int page=0;page<3;++page) {
        auto& button=matchPageButtons[(size_t)page];
        button.setButtonText(page==0?"SHAPE":page==1?"CALIBRATION":"REFERENCE");
        button.setTooltip(page==0
            ?"Edit learned harmonic and envelope weight controls"
            :page==1?"Edit source-level calibration, reference amplitude, trim and smoothing"
                   :"Edit native reference-fitted vocal level, air and trim; live DSP parameters");
        button.onClick=[this,page]{showMatchPage(page);};
        button.setWantsKeyboardFocus(true);
        addAndMakeVisible(button);
        button.setVisible(false);
    }
    for(int page=0;page<4;++page) {
        auto& button=suitePageButtons[(size_t)page];
        constexpr const char* captions[]{"VOICE + TONE","DE-ESS + COMP",
                                         "COLOUR + SPACE","ENERGY + OUT"};
        button.setButtonText(captions[page]);
        button.setTooltip("Vocal Feel Suite: 16 real parameters across four pages");
        button.onClick=[this,page]{showSuitePage(page);};
        button.setWantsKeyboardFocus(true);
        addAndMakeVisible(button);button.setVisible(false);
    }
    vocalFeelStyleButton.setTooltip("Eleven vocal styles; preserve your Sens and Master levels");
    vocalFeelStyleButton.onClick=[this] {
        if(processor.slotType(slot)!=SkellPresetAudioProcessor::VocalFeelSuiteFX)return;
        if(vocalFeelStyles.isVisible()) {vocalFeelStyles.close();return;}
        int current=-1;
        for(int recipe=0;recipe<11;++recipe){
            bool matching=true;
            for(int parameter=0;parameter<16;++parameter) {
                if(parameter==1 || parameter==15)continue;
                const float a=processor.getSlotParam(slot,parameter);
                const float b=skellpreset::dsp::vocalFeelRecipes[(size_t)recipe].values[(size_t)parameter];
                if(std::abs(a-b)>.006f){matching=false;break;}
            }
            if(matching){current=recipe;break;}
        }
        vocalFeelStyles.open(current);
    };
    addAndMakeVisible(vocalFeelStyleButton);
    vocalFeelStyleButton.setVisible(false);
    vocalFeelStyles.onChoose=[this](int index){selectSuiteRecipe(index);};
    addAndMakeVisible(vocalFeelStyles);
    vocalFeelStyles.setVisible(false);
    suiteSpectrumButton.setTooltip(
        "Show real pre-rack / post-master audio spectra; return mix bars remain on MIX view");
    suiteSpectrumButton.onClick=[this] {
        if(processor.slotType(slot)!=SkellPresetAudioProcessor::VocalFeelSuiteFX)return;
        suiteSpectrumView=!suiteSpectrumView;
        suiteSpectrumButton.setButtonText(suiteSpectrumView?"RETURN MIX":"SPECTRUM");
        suiteSpectrumButton.setColour(juce::TextButton::textColourOffId,
                                      suiteSpectrumView?Theme::green:Theme::white);
        graphSnapshotValid=false;repaint();
    };
    addAndMakeVisible(suiteSpectrumButton);
    suiteSpectrumButton.setVisible(false);
    for(int page=0;page<4;++page) {
        auto& button=delayPageButtons[(size_t)page];
        button.setButtonText(page==0?"TIME":page==1?"TONE":
                             page==2?"MOD + MIX":"SYNC + DIFF");
        button.setTooltip("Studio Delay timing, filtering, feedback and modulation");
        button.onClick=[this,page]{showDelayPage(page);};
        button.setWantsKeyboardFocus(true);
        addAndMakeVisible(button);button.setVisible(false);
    }
    studioGateFloor.setSliderStyle(juce::Slider::LinearHorizontal);
    studioGateFloor.setTextBoxStyle(juce::Slider::TextBoxRight,false,54,20);
    studioGateFloor.setScrollWheelEnabled(true);
    studioGateFloor.setTooltip("Studio gate floor: 0% = maximum reduction; 100% = no gate reduction");
    addAndMakeVisible(studioGateFloor);
    studioGateFloor.setVisible(false);
    studioCompKnee.setSliderStyle(juce::Slider::LinearHorizontal);
    studioCompKnee.setTextBoxStyle(juce::Slider::TextBoxRight,false,54,20);
    studioCompKnee.setTooltip("Studio compressor soft-knee intensity (independent p5 automation)");
    studioCompKnee.textFromValueFunction=[](double v){
        return juce::String(juce::roundToInt(v*100.))+"%";
    };
    addAndMakeVisible(studioCompKnee);
    studioCompKnee.setVisible(false);
    rightFirst.setTooltip("Ping Pong only: mono send, first echo right, then left. Other modes unchanged.");
    addAndMakeVisible(rightFirst);rightFirst.setVisible(false);
    duckOpen.setTooltip("Adjust the selected effect sidechain detector and wet contribution");
    duckOpen.onClick=[this] {
        if(!SkellPresetAudioProcessor::supportsEffectSidechain(processor.slotType(slot)))return;
        const bool opening=!duckPanel.isVisible();
        if(opening){modulePicker.setVisible(false);installedBrowser.setVisible(false);livePanel.setVisible(false);liveView.setButtonText("VISUAL");}
        duckPanel.setVisible(opening);
        if(opening)duckPanel.toFront(true);
    };
    addAndMakeVisible(duckOpen);duckOpen.setVisible(false);
    addAndMakeVisible(duckPanel);duckPanel.setVisible(false);
    duckClose.onClick=[this]{duckPanel.setVisible(false);};
    duckPanel.addAndMakeVisible(duckClose);
    duckOn.setTooltip("Reduce the selected effect contribution using the chosen detector; dry audio stays on its aligned path.");
    duckPanel.addAndMakeVisible(duckOn);
    duckSource.addItem("POST MIC",1);duckSource.addItem("PRE MIC",2);duckSource.addItem("SLOT INPUT",3);
    duckSource.setTooltip("Detector source: after MIC, before MIC, or this effect input");
    duckPanel.addAndMakeVisible(duckSource);
    duckExternal.setTooltip("Use the host auxiliary input instead of the internal source. Unconnected key is silent; no automatic vocal fallback.");
    duckPanel.addAndMakeVisible(duckExternal);
    parallelReturn.setTooltip("Independent post-MIC send, summed after the serial rack. Change while stopped.");
    duckPanel.addAndMakeVisible(parallelReturn);
    addAndMakeVisible(liveView);liveView.setVisible(false);
    addChildComponent(livePanel);
    // Legacy presentation state remains readable for old sessions; there
    // is no separate clean/visual face. Telemetry is drawn inside each face.
    liveView.setVisible(false);
    for(auto* slider:{&duckDepth,&duckThreshold,&duckAttack,&duckRelease}) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,78,22);
        slider->setScrollWheelEnabled(true);
        duckPanel.addAndMakeVisible(*slider);
    }
    duckDepth.setTooltip("0% = no ducking, 100% = attenuate wet return during speech");
    duckThreshold.setTooltip("Vocal detector threshold, dBFS");
    duckAttack.setTooltip("Ducking attack: 2 ms = tight fast cut; 80 ms = gradual natural duck");
    duckRelease.setTooltip("Rise of the reverb/delay tail when vocal stops, ms");
    for(int index=0;index<2;++index) {
        auto& control=eqCutSliders[(size_t)index];
        control.setSliderStyle(juce::Slider::LinearHorizontal);
        control.setTextBoxStyle(juce::Slider::TextBoxRight,false,74,21);
        control.setRange(0.,1.,.001);
        control.setTooltip(index==0
            ?"High-pass cut: OFF or 20-400 Hz, smoothed 12ms"
            :"Low-pass cut: OFF or 20k-1.8k Hz, smoothed 12ms");
        addAndMakeVisible(control);
        control.setVisible(false);
    }
    for(int i=0;i<4;++i){
        auto& k=knobs[(size_t)i];
        // In-place studio parameter sliders. These are bound to the same
        // published sN_p1..p4 automation as the removed four-knob footer.
        k.setSliderStyle(juce::Slider::LinearHorizontal);
        k.setScrollWheelEnabled(true);
        k.setTextBoxStyle(juce::Slider::TextBoxRight,false,92,24);
        k.setDoubleClickReturnValue(true,.5);
        k.setTooltip("Drag or type the value directly in this effect panel; double-click resets.");
        // Human-readable display only; hosts still automate the exact stable
        // normalized parameter range. Never make display conversions in DSP.
        k.setColour(juce::Slider::rotarySliderFillColourId,Theme::green);
        k.setColour(juce::Slider::rotarySliderOutlineColourId,Theme::stroke);
        k.setColour(juce::Slider::thumbColourId,Theme::white);
        k.setColour(juce::Slider::textBoxTextColourId,Theme::white);
        k.setColour(juce::Slider::textBoxBackgroundColourId,Theme::panelAlt);
        k.setColour(juce::Slider::textBoxOutlineColourId,Theme::stroke);
        addAndMakeVisible(k);
    }
    moduleBox.onChange=[this] {
        const int next=moduleBox.getSelectedId()-1;
        if(next>=0)processor.setSlotType(slot,next);
        graphSnapshotValid=false;
        refreshIfChanged();
        repaint();
    };
    applyTheme();
    select(0);
}
// AudioProcessorValueTreeState attachments replace slider value/text formatters.
// Reapply our physical-unit formatting AFTER each (re)attachment, including
// EQ band-page switches. This affects presentation only, not automatable values.
void SlotEditor::applyKnobReadouts() {
    eqCutSliders[0].textFromValueFunction=[](double v){
        if(v<=.0001)return juce::String("OFF");
        return juce::String(juce::roundToInt(
            skellpreset::dsp::EightBandTone::lowCutFrequency((float)v)))+" Hz";
    };
    eqCutSliders[1].textFromValueFunction=[](double v){
        if(v<=.0001)return juce::String("OFF");
        const float freq=skellpreset::dsp::EightBandTone::highCutFrequency((float)v);
        return freq>=1000.f ? juce::String(freq/1000.f,1)+" kHz":
                               juce::String(juce::roundToInt(freq))+" Hz";
    };
    eqCutSliders[0].valueFromTextFunction=[](const juce::String& text){
        const float hz=text.getFloatValue();
        if(!text.containsAnyOf("0123456789") || hz<=0.f)return 0.0;
        return (double)juce::jlimit(0.f,1.f,
            (float)(std::log(juce::jlimit(20.f,400.f,hz)/20.f)/std::log(20.f)));
    };
    eqCutSliders[1].valueFromTextFunction=[](const juce::String& input){
        const auto text=input.trim().toLowerCase();
        const float hz=text.getFloatValue()*(text.contains("khz")?1000.f:1.f);
        if(!text.containsAnyOf("0123456789") || hz<=0.f)return 0.0;
        return (double)juce::jlimit(0.f,1.f,
            (float)(std::log(juce::jlimit(1800.f,20000.f,hz)/20000.f)/std::log(.09f)));
    };
    eqCutSliders[0].updateText();eqCutSliders[1].updateText();
    studioGateFloor.textFromValueFunction=[](double v) {
        return juce::String(juce::roundToInt(juce::jlimit(0.,1.,v)*100.))+"%";
    };
    studioGateFloor.valueFromTextFunction=[](const juce::String& text) {
        return juce::jlimit(0.,1.,text.getDoubleValue()/100.);
    };
    studioGateFloor.updateText();
    // SliderAttachment installs host formatters; restore the knee's physical
    // presentation after every rebind, just like the gate floor.
    studioCompKnee.textFromValueFunction=[](double v) {
        return juce::String(juce::roundToInt(juce::jlimit(0.,1.,v)*100.))+"%";
    };
    studioCompKnee.valueFromTextFunction=[](const juce::String& text) {
        return juce::jlimit(0.,1.,text.getDoubleValue()/100.);
    };
    studioCompKnee.updateText();
    eqQSlider.textFromValueFunction=[this](double normalized) {
        // The dedicated label above already says BAND N WIDTH/Q.
        // Redundant "WIDTH 0.63" could not fit the 59px text box and
        // was visibly truncated in the user's real Studio EQ screenshot.
        if(processor.slotType(slot)==SkellPresetAudioProcessor::StudioEQFX)
            return juce::String(normalized,2);
        const float q=skellpreset::dsp::EightBandTone::qFromNormalized((float)normalized);
        return juce::String(q,2);
    };
    eqQSlider.valueFromTextFunction=[this](const juce::String& text) {
        if(processor.slotType(slot)==SkellPresetAudioProcessor::StudioEQFX)
            return (double)juce::jlimit(0.f,1.f,
                text.retainCharacters("0123456789.-").getFloatValue());
        return (double)skellpreset::dsp::EightBandTone::normalizedFromQ(
            text.retainCharacters("0123456789.-").getFloatValue());
    };
    for(int i=0;i<4;++i) {
        auto& k=knobs[(size_t)i];
        k.textFromValueFunction=[this,i](double v)->juce::String {
            const int selectedType=processor.slotType(slot);
            if(!(skellpreset::face::limiter(selectedType) && (i==1 || i==2)))v=juce::jlimit(0.,1.,v);
            const auto physical=skellpreset::readout::forParameter(selectedType,i,processor.preparedAudioRate());
            if(physical.valid)return physical.format(v);
            if(selectedType==SkellPresetAudioProcessor::EQ) {
                if(i==0) {
                    const double hz=40.*std::pow(300.,v);
                    return hz>=1000.?juce::String(hz/1000.,1)+"kHz":
                                    juce::String(juce::roundToInt(hz))+"Hz";
                }
                if(i==1)return juce::String((v-.5)*24.,1)+"dB";
                if(i==2)return juce::String(.3+7.*v,2)+" Q";
                return juce::String(juce::roundToInt(20.+250.*v))+"Hz";
            }
            const int module=selectedType;
            if(skellpreset::face::limiter(module)) {
                if(i==0)return juce::String(20.*std::log10(.12+.87*v),1)+" dBFS";
                return juce::String(v,i==2?2:0)+" ms";
            }
            if(isPagedEQ(module)) {
                const int band=eqBank+i/2;
                if(module==SkellPresetAudioProcessor::StudioEQFX) {
                    if(i%2==0) {
                        const double hz=20.*std::pow(1000.,v);
                        return hz>=1000.?juce::String(hz/1000.,1)+"kHz":
                            juce::String(juce::roundToInt(hz))+"Hz";
                    }
                    return juce::String((v-.5)*36.,1)+"dB";
                }
                if(i%2==0){
                    const double lo=skellpreset::dsp::EightBandTone::edges[(size_t)band];
                    const double hi=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
                    const double hz=lo*std::pow(hi/lo,v);
                    return hz>=1000.?juce::String(hz/1000.,1)+"kHz":
                           juce::String(juce::roundToInt(hz))+"Hz";
                }
                return juce::String((v-.5)*30.,1)+"dB";
            }
            // Native dynamic processors display the same physical units as
            // their real DSP coefficient mapping, not misleading percentages.
            if(module==SkellPresetAudioProcessor::Compressor) {
                const auto* p=processor.state.getRawParameterValue("macro2");
                const double punch=p?p->load():.5;
                if(i==0)return juce::String(-48.+42.*v-12.*(punch-.5),1)+" dBFS";
                if(i==1)return juce::String(1.+11.*v,1)+":1";
                if(i==2)return juce::String(2.+98.*v,1)+" ms";
                return juce::String(20.+480.*v,0)+" ms";
            }
            if(module==SkellPresetAudioProcessor::Gate) {
                if(i==0)return juce::String(-75.+70.*v,1)+" dBFS";
                if(i==1)return juce::String(40.+600.*v,0)+" ms";
                if(i==2)return juce::String(120.*v,0)+" ms";
                return juce::String(8.*v,1)+" dB";
            }
            if(module==SkellPresetAudioProcessor::DeEsser) {
                if(i==0) {
                    const double hz=3500.+8000.*v;
                    return juce::String(hz/1000.,2)+" kHz";
                }
                if(i==1)return juce::String(-53.+39.*v,1)+" dBFS";
                if(i==2)return juce::String(2.+20.*v,1)+" dB";
                return juce::String(35.+245.*v,0)+" ms";
            }
            if(module==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
                const int parameter=suitePage*4+i;
                if(parameter==0)return v<.25?"MALE":v<.75?"MALE 2":"FEMALE";
                if(parameter==8)return v<.25?"FLAT":v<.75?"WARM":"TUBE";
                if(parameter==1 || (parameter>=2 && parameter<=4))
                    return juce::String((v-.5)*24.,1)+" dB";
                if(parameter==6)
                    return juce::String((3000.+9000.*v)/1000.,2)+" kHz";
                if(parameter==5 || parameter==7)
                    return juce::String(juce::roundToInt(v*100.))+"%";
                return juce::String(v*10.,1);
            }
            if(module==SkellPresetAudioProcessor::VocalFeelOptoFX) {
                if(i==0 || i==3)return juce::String(juce::roundToInt(v*100.))+"%";
                if(i==1)return v<.25?"FLAT":v<.75?"WARM":"TUBE";
                return v<.25?"MALE":v<.75?"MALE 2":"FEMALE";
            }
            if(module==SkellPresetAudioProcessor::StudioCompressorFX) {
                if(i==0) {
                    const float threshold=skellpreset::studio::detail::level(
                        juce::roundToInt(v*1000.),7.);
                    return juce::String(20.f*std::log10(juce::jmax(.001f,threshold)),1)+" dBFS";
                }
                if(i==1)return juce::String(1.+.38*(std::pow(51.,v)-1.),1)+":1";
                if(i==2)return juce::String(skellpreset::studio::detail::timeMs(
                    juce::roundToInt(v*10000.)),1)+" ms";
                return juce::String(skellpreset::studio::detail::timeMs(
                    juce::roundToInt(v*10000.))*2.,1)+" ms";
            }
            if(module==SkellPresetAudioProcessor::StudioLimiterFX) {
                const int parameter=limiterPage*4+i;
                if(parameter==0)
                    return juce::String(20.*std::log10(.12+.87*v),1)+" dBFS";
                if(parameter==1)return juce::String(18.*v,1)+" dB";
                if(parameter==2 || parameter==4) {
                    // Same discrete Studio level law used by Compressor/Gate,
                    // including the -60 dB compressor floor and gate OFF.
                    if(parameter==4 && v<=.0001)return "OFF";
                    const float amplitude=skellpreset::studio::detail::level(
                        juce::roundToInt(v*1000.),7.);
                    const float floor=parameter==2?.001f:1.e-6f;
                    return juce::String(20.f*std::log10(
                        juce::jmax(floor,amplitude)),1)+" dBFS";
                }
                if(parameter==3) {
                    const double ratio=1.+.38*(std::pow(51.,v)-1.);
                    return juce::String(ratio,1)+":1";
                }
                if(parameter==5 || parameter==6)
                    return juce::String(skellpreset::studio::detail::timeMs(
                        juce::roundToInt(v*10000.)),1)+" ms";
                return juce::String(juce::roundToInt(v*100.))+"%";
            }
            if(module==SkellPresetAudioProcessor::StudioDelayFX) {
                const int parameter=delayPage*4+i;
                switch(parameter) {
                    case 0:return juce::String(25.*std::pow(80.,v),0)+" ms";
                    case 1:return juce::String(94.*v,0)+"%";
                    case 2:return juce::StringArray{"MONO","STEREO","PING PONG","OFF"}[
                                 juce::jlimit(0,3,(int)(v*3.999))];
                    case 3:return juce::String(50.*v,1)+" ms";
                    case 4:return juce::String(12.+1600.*v*v,0)+" Hz";
                    case 5:return juce::String(400.*std::pow(45.,v),0)+" Hz";
                    case 6:return juce::String(1.+7.*v,1)+" x";
                    case 7:return juce::String(100.*v,0)+"%";
                    case 8:return juce::String(.05+5.*v,2)+" Hz";
                    case 9:return juce::String(12.*v,1)+" ms";
                    case 10:case 11:case 14:case 15:return juce::String(100.*v,0)+"%";
                    case 12:return v>=.5?"ON":"OFF";
                    case 13: {
                        constexpr const char* divisions[]{"4 beats","2 beats","1 beat","1/2 beat",
                            "1/4 beat","1/8 beat","1/16 beat","1/32 beat"};
                        return divisions[juce::jlimit(0,7,(int)(v*7.999))];
                    }
                }
            }
            if(module>=SkellPresetAudioProcessor::AdlibChop &&
               module<=SkellPresetAudioProcessor::AdlibSaturate) {
                if((module==SkellPresetAudioProcessor::AdlibMotion && i==2) ||
                   (module==SkellPresetAudioProcessor::AdlibSaturate && i==1))
                    return "N/A";
                if(module==SkellPresetAudioProcessor::AdlibChop && i==1)
                    return juce::String(1<<juce::jlimit(0,3,(int)(v*3.999)))+"x / beat";
                if(module==SkellPresetAudioProcessor::AdlibMotion && i==1)
                    return juce::String(.08+9.92*v,2)+" Hz";
                if(module==SkellPresetAudioProcessor::AdlibFilter && i==2) {
                    const double hz=80.*std::pow(225.,v);
                    return hz>=1000.?juce::String(hz/1000.,2)+" kHz":
                        juce::String(hz,0)+" Hz";
                }
                return juce::String(100.*v,0)+"%";
            }
            if(module==SkellPresetAudioProcessor::MicModelFX) {
                if(i==0)return skellpreset::dsp::MicModelTone::inputName(
                    skellpreset::dsp::MicModelTone::index((float)v));
                if(i==1)return skellpreset::dsp::MicModelTone::targetName(
                    skellpreset::dsp::MicModelTone::index((float)v));
                if(i==2)return juce::String(juce::roundToInt(v*100.))+"%";
                return juce::String(-6.+12.*v,1)+" dB";
            }
            if(module==SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
                if(matchPage==0) {
                    if(i==0)return juce::String((v-.5)*6.,2)+"% cubic";
                    if(i==1)return juce::String(juce::roundToInt(v*100.))+"%";
                    return juce::String((v-.5)*.7,3)+" x";
                }
                if(matchPage==2) {
                    if(i==0)return v>.75?"ON":"OFF";
                    if(i==1 || i==2)return juce::String(
                        juce::roundToInt(juce::jlimit(0.,1.,(v-.5)*2.)*100.))+"%";
                    return juce::String(4.*v,1)+" dB";
                }
                if(i==0)return juce::String(-40.+30.*v,1)+" dBFS";
                if(i==1)return juce::String(.05+.75*v,3)+" amp";
                if(i==2)return juce::String(-1.+2.*v,2)+" dB";
                return juce::String(2.+48.*v,1)+" ms";
            }
            if(module==SkellPresetAudioProcessor::StudioPitchFormantFX) {
                if(i==0)return juce::String((v-.5)*2.*
                    (3.+9.*processor.getSlotParam(slot,2)),1)+" st";
                if(i==1)return juce::String((v-.5)*2.*
                    (2.+10.*processor.getSlotParam(slot,3)),1)+" st";
                if(i==2)return juce::String(3.+9.*v,0)+" st max";
                return juce::String(2.+10.*v,0)+" st max";
            }
            if(module==SkellPresetAudioProcessor::SerialDynamicsFX) {
                if(i==0)return juce::String(-48.+42.*v,0)+"dB";
                if(i==1)return juce::String(2.+10.*v,1)+":1";
                if(i==2)return juce::String(.35+8.*v,1)+"ms";
            } else if(module==SkellPresetAudioProcessor::DynamicHarshnessFX) {
                if(i==0)return juce::String(1800.*std::pow(4.2,v)/1000.,1)+"kHz";
                if(i==1)return juce::String(-53.+43.*v,0)+"dB";
                if(i==2)return juce::String(10.*v,1)+"dB";
                return juce::String(30.+350.*v,0)+"ms";
            } else if(module==SkellPresetAudioProcessor::AdaptiveNoiseFX) {
                if(i==0)return juce::String(-75.+55.*v,0)+"dB";
                if(i==1)return juce::String(36.*v,0)+"dB";
                if(i==2)return juce::String(250.*v,0)+"ms";
                return juce::String(25.+425.*v,0)+"ms";
            }
            if(module==SkellPresetAudioProcessor::VocalRiderFX){
                if(i==1)return juce::String(-26.+14.*v,0)+"dB";
                if(i==2)return juce::String(3.+12.*v,0)+"dB";
            } else if(module==SkellPresetAudioProcessor::DuckingPingPongFX && i==0) {
                return v<.25?"1/16":v<.5?"1/8":v<.75?"1/8D":"1/4";
            } else if(module==SkellPresetAudioProcessor::VocalVibratoFX) {
                if(i==0)return juce::String(.3+11.7*v,1)+"Hz";
                if(i==1)return juce::String(3.5*v,1)+"ms";
            }
            return juce::String(juce::roundToInt(v*100.))+"%";
        };
        k.valueFromTextFunction=[this,i](const juce::String& input)->double {
            const auto text=input.trim().toLowerCase();
            const double value=text.getDoubleValue();
            const int selectedType=processor.slotType(slot);
            const auto physical=skellpreset::readout::forParameter(selectedType,i,processor.preparedAudioRate());
            if(physical.valid) {
                if(text.contains("%") && juce::String(physical.unit)!="%")return juce::jlimit(0.,1.,value/100.);
                return physical.parse(text);
            }
            if(selectedType==SkellPresetAudioProcessor::EQ) {
                if(i==0) {
                    const double hz=juce::jmax(1.,value*(text.contains("khz")?1000.:1.));
                    return juce::jlimit(0.,1.,std::log(hz/40.)/std::log(300.));
                }
                if(i==1)return juce::jlimit(0.,1.,value/24.+.5);
                if(i==2)return juce::jlimit(0.,1.,(value-.3)/7.);
                return juce::jlimit(0.,1.,(value-20.)/250.);
            }
            const int module=selectedType;
            if(skellpreset::face::limiter(module)) {
                if(i==0)return juce::jlimit(0.,1.,(std::pow(10.,value/20.)-.12)/.87);
                return i==1?juce::jlimit(5.,500.,value):juce::jlimit(0.,3.,value);
            }
            if(isPagedEQ(module)) {
                if(module==SkellPresetAudioProcessor::StudioEQFX) {
                    if(i%2==1)return juce::jlimit(0.,1.,value/36.+.5);
                    const double hz=juce::jmax(1.,value*(text.contains("khz")?1000.:1.));
                    return juce::jlimit(0.,1.,std::log(hz/20.)/std::log(1000.));
                }
                if(i%2==1)return juce::jlimit(0.,1.,value/30.+.5);
                const int band=eqBank+i/2;
                const double lo=skellpreset::dsp::EightBandTone::edges[(size_t)band];
                const double hi=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
                const double hz=juce::jmax(1.,value*(text.contains("khz")?1000.:1.));
                return juce::jlimit(0.,1.,std::log(hz/lo)/std::log(hi/lo));
            }
            if(module==SkellPresetAudioProcessor::Compressor) {
                if(text.contains("%"))return juce::jlimit(0.,1.,value/100.);
                const auto* p=processor.state.getRawParameterValue("macro2");
                const double punch=p?p->load():.5;
                if(i==0)return juce::jlimit(0.,1.,
                    (value+48.+12.*(punch-.5))/42.);
                if(i==1)return juce::jlimit(0.,1.,(value-1.)/11.);
                if(i==2)return juce::jlimit(0.,1.,(value-2.)/98.);
                return juce::jlimit(0.,1.,(value-20.)/480.);
            }
            if(module==SkellPresetAudioProcessor::Gate) {
                if(text.contains("%"))return juce::jlimit(0.,1.,value/100.);
                if(i==0)return juce::jlimit(0.,1.,(value+75.)/70.);
                if(i==1)return juce::jlimit(0.,1.,(value-40.)/600.);
                if(i==2)return juce::jlimit(0.,1.,value/120.);
                return juce::jlimit(0.,1.,value/8.);
            }
            if(module==SkellPresetAudioProcessor::DeEsser) {
                if(text.contains("%"))return juce::jlimit(0.,1.,value/100.);
                if(i==0) {
                    const double hz=value*(text.contains("khz")?1000.:1.);
                    return juce::jlimit(0.,1.,(hz-3500.)/8000.);
                }
                if(i==1)return juce::jlimit(0.,1.,(value+53.)/39.);
                if(i==2)return juce::jlimit(0.,1.,(value-2.)/20.);
                return juce::jlimit(0.,1.,(value-35.)/245.);
            }
            if(module==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
                const int parameter=suitePage*4+i;
                if(parameter==0) {
                    if(text.contains("female"))return 1.;
                    if(text.contains("male 2")||text=="deep")return .5;
                    return 0.;
                }
                if(parameter==8) {
                    if(text.contains("tube"))return 1.;
                    if(text.contains("warm"))return .5;
                    return 0.;
                }
                if(parameter==1 || (parameter>=2 && parameter<=4))
                    return juce::jlimit(0.,1.,value/24.+.5);
                if(parameter==6) {
                    const double hz=value*(text.contains("khz")?1000.:1.);
                    return juce::jlimit(0.,1.,(hz-3000.)/9000.);
                }
                if(parameter==5 || parameter==7)
                    return juce::jlimit(0.,1.,value/100.);
                return juce::jlimit(0.,1.,value/10.);
            }
            if(module==SkellPresetAudioProcessor::StudioDelayFX) {
                // The visible units are editable physical units, NOT a 0..100
                // value disguised as Hz/ms. Host parameter IDs/ranges unchanged.
                const int parameter=delayPage*4+i;
                const double hz=value*(text.contains("khz")?1000.:1.);
                const auto unit=[](double n){return juce::jlimit(0.,1.,n);};
                switch(parameter) {
                    case 0:return unit(std::log(juce::jmax(25.,value)/25.)/std::log(80.));
                    case 1:return unit(value/94.);
                    case 2:{
                        if(text.contains("ping"))return .625;
                        if(text.contains("mono"))return .125;
                        if(text.contains("stereo"))return .375;
                        if(text=="off")return .875;
                        return unit(value/3.);
                    }
                    case 3:return unit(value/50.);
                    case 4:return unit(std::sqrt(juce::jmax(0.,(hz-12.)/1600.)));
                    case 5:return unit(std::log(juce::jmax(400.,hz)/400.)/std::log(45.));
                    case 6:return unit((value-1.)/7.);
                    case 7:return unit(value/100.);
                    case 8:return unit((value-.05)/5.);
                    case 9:return unit(value/12.);
                    case 10:case 11:case 14:case 15:return unit(value/100.);
                    case 12:return text=="on" || value>=1.?1.:0.;
                    case 13:{
                        constexpr const char* divisions[]{"4 beats","2 beats","1 beat",
                            "1/2 beat","1/4 beat","1/8 beat","1/16 beat","1/32 beat"};
                        for(int k=0;k<8;++k)
                            if(text==divisions[k])return (k+.5)/8.;
                        return unit(value);
                    }
                }
            }
            if(module>=SkellPresetAudioProcessor::AdlibChop &&
               module<=SkellPresetAudioProcessor::AdlibSaturate) {
                const auto unit=[](double x){return juce::jlimit(0.,1.,x);};
                if(module==SkellPresetAudioProcessor::AdlibChop && i==1)
                    return (juce::jlimit(0,3,(int)std::round(
                        std::log2(juce::jmax(1.,value))))+.5)/4.;
                if(module==SkellPresetAudioProcessor::AdlibMotion && i==1)
                    return unit((value-.08)/9.92);
                if(module==SkellPresetAudioProcessor::AdlibFilter && i==2) {
                    const double hz=juce::jmax(80.,value*(text.contains("khz")?1000.:1.));
                    return unit(std::log(hz/80.)/std::log(225.));
                }
                return unit(value/100.);
            }
            if(module==SkellPresetAudioProcessor::MicModelFX) {
                if(i==0 || i==1) {
                    for(int n=0;n<4;++n) {
                        const juce::String name=i==0
                            ?skellpreset::dsp::MicModelTone::inputName(n)
                            :skellpreset::dsp::MicModelTone::targetName(n);
                        if(name.toLowerCase()==text)return n/3.;
                    }
                    return juce::jlimit(0.,1.,value);
                }
                if(i==2)return juce::jlimit(0.,1.,value/100.);
                return juce::jlimit(0.,1.,(value+6.)/12.);
            }
            if(module==SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
                if(matchPage==0) {
                    if(i==0)return juce::jlimit(0.,1.,.5+value/6.);
                    if(i==1)return juce::jlimit(0.,1.,value/100.);
                    return juce::jlimit(0.,1.,.5+value/.7);
                }
                if(matchPage==2) {
                    if(i==0)return (text=="on" || value>75.)?1.:0.;
                    if(i==1 || i==2)
                        return juce::jlimit(.5,1.,.5+value/200.);
                    return juce::jlimit(0.,1.,value/4.);
                }
                if(i==0)return juce::jlimit(0.,1.,(value+40.)/30.);
                if(i==1)return juce::jlimit(0.,1.,(value-.05)/.75);
                if(i==2)return juce::jlimit(0.,1.,(value+1.)/2.);
                return juce::jlimit(0.,1.,(value-2.)/48.);
            }
            if(module==SkellPresetAudioProcessor::StudioPitchFormantFX) {
                if(i==0)return juce::jlimit(0.,1.,.5+value/
                    (2.*(3.+9.*processor.getSlotParam(slot,2))));
                if(i==1)return juce::jlimit(0.,1.,.5+value/
                    (2.*(2.+10.*processor.getSlotParam(slot,3))));
                if(i==2)return juce::jlimit(0.,1.,(value-3.)/9.);
                return juce::jlimit(0.,1.,(value-2.)/10.);
            }
            if(module==SkellPresetAudioProcessor::SerialDynamicsFX) {
                if(i==0)return juce::jlimit(0.,1.,(value+48.)/42.);
                if(i==1)return juce::jlimit(0.,1.,(value-2.)/10.);
                if(i==2)return juce::jlimit(0.,1.,(value-.35)/8.);
            } else if(module==SkellPresetAudioProcessor::DynamicHarshnessFX) {
                if(i==0)return juce::jlimit(0.,1.,std::log(
                    juce::jmax(1.,value*(text.contains("khz")?1000.:1.))/1800.)/std::log(4.2));
                if(i==1)return juce::jlimit(0.,1.,(value+53.)/43.);
                if(i==2)return juce::jlimit(0.,1.,value/10.);
                return juce::jlimit(0.,1.,(value-30.)/350.);
            } else if(module==SkellPresetAudioProcessor::AdaptiveNoiseFX) {
                if(i==0)return juce::jlimit(0.,1.,(value+75.)/55.);
                if(i==1)return juce::jlimit(0.,1.,value/36.);
                if(i==2)return juce::jlimit(0.,1.,value/250.);
                return juce::jlimit(0.,1.,(value-25.)/425.);
            }
            if(module==SkellPresetAudioProcessor::VocalRiderFX){
                if(i==1)return juce::jlimit(0.,1.,(value+26.)/14.);
                if(i==2)return juce::jlimit(0.,1.,(value-3.)/12.);
            } else if(module==SkellPresetAudioProcessor::DuckingPingPongFX && i==0) {
                if(text=="1/16")return .125;
                if(text=="1/8")return .375;
                if(text=="1/8d")return .625;
                if(text=="1/4")return .875;
            } else if(module==SkellPresetAudioProcessor::VocalVibratoFX) {
                if(i==0)return juce::jlimit(0.,1.,(value-.3)/11.7);
                if(i==1)return juce::jlimit(0.,1.,value/3.5);
            }
            return juce::jlimit(0.,1.,(text.contains("%")||value>1.)?value/100.:value);
        };
    }
    // APVTS attachments already populated each text box using their default
    // formatting. Reassigning textFromValueFunction alone leaves that cached
    // raw decimal visible until the user moves a knob. Update the visible
    // contents immediately, including after selecting a different EQ bank.
    eqQSlider.updateText();
    for(auto& knob:knobs)knob.updateText();
}
void SlotEditor::releaseEmbeddedEditor(){
    if(!embeddedEditor)return;
    removeChildComponent(embeddedEditor.get());
    // Destroy before the hosted processor, on the JUCE GUI/message thread.
    embeddedEditor.reset();
    embeddedHost=nullptr;
    embeddedFitRequested=false;embeddedFitRejected=false;
}
void SlotEditor::layoutEmbeddedEditor(){
    if(!embeddedEditor)return;
    if(!hostedPresentation || !isShowing() || modulePicker.isVisible() ||
       installedBrowser.isVisible() || hostedClassMenuOpen || duckPanel.isVisible()) {
        hideEmbeddedEditor();return;
    }
    auto& view=*embeddedEditor;
    const auto area=getLocalBounds().withTrimmedTop(85).reduced(18,6);
    if(area.isEmpty())return;
    // Native HWND editors do not reliably follow a JUCE affine transform.
    // Negotiate size with their wrapper instead of scaling the child window.
    if(!embeddedFitRequested && (view.getWidth()>area.getWidth() ||
                                  view.getHeight()>area.getHeight())) {
        embeddedFitRequested=true;
        const float ratio=juce::jmin(float(area.getWidth())/float(juce::jmax(1,view.getWidth())),
                                    float(area.getHeight())/float(juce::jmax(1,view.getHeight())));
        if(view.isResizable())
            view.setBoundsConstrained({area.getX(),area.getY(),
                juce::jmax(1,juce::roundToInt(view.getWidth()*ratio)),
                juce::jmax(1,juce::roundToInt(view.getHeight()*ratio))});
        else view.setScaleFactor(juce::jlimit(.1f,1.f,ratio));
    }
    embeddedFitRejected=view.getWidth()>area.getWidth() || view.getHeight()>area.getHeight();
    if(embeddedFitRejected){view.setVisible(false);return;}
    view.setCentrePosition(area.getCentre());
    if(!view.isVisible()){
        view.setVisible(true);
        // Native attachment may renegotiate the dimensions; do not let the
        // child cover the footer or the surrounding host controls.
        embeddedFitRejected=view.getWidth()>area.getWidth() || view.getHeight()>area.getHeight();
        if(embeddedFitRejected){view.setVisible(false);return;}
        view.setCentrePosition(area.getCentre());
    }
}
void SlotEditor::syncEmbeddedEditor(){
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::ExternalVST3){
        releaseEmbeddedEditor();return;
    }
    auto* instance=processor.hostedVST3InstanceForUI(slot);
    if(embeddedHost==instance && embeddedEditor){layoutEmbeddedEditor();return;}
    releaseEmbeddedEditor();
    if(!hostedPresentation || !isShowing() || modulePicker.isVisible() ||
       installedBrowser.isVisible() || hostedClassMenuOpen)return;
    if(!instance || !instance->hasEditor())return;
    // Do not take an editor already displayed by a floating host window.
    if(instance->getActiveEditor()!=nullptr)return;
    auto* view=instance->createEditorIfNeeded();
    if(!view)return; // unsupported editor remains usable via window fallback
    embeddedHost=instance;
    embeddedEditor.reset(view);
    view->setVisible(false);
    addChildComponent(*view);
    layoutEmbeddedEditor();
}
void SlotEditor::loadVST3FromFile(const juce::File& path) {
    if(path.getFullPathName().isEmpty())return;
    using Host=skellpreset::host::HostedVST3;
    std::vector<Host::EffectChoice> choices;
    const auto inspected=Host::inspectEffects(path,choices);
    if(inspected.failed()){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "VST3 could not load",inspected.getErrorMessage());return;
    }
    const int target=slot;
    const int originalType=processor.slotType(target);
    juce::Component::SafePointer<SlotEditor> safe(this);
    auto finish=[safe,target,originalType,path,choices](int id){
        if(!safe)return;
        safe->hostedClassMenuOpen=false;
        if(safe->selectedSlot()!=target ||
           safe->processor.slotType(target)!=originalType ||
           id<=0 || id>(int)choices.size())return;
        const auto result=safe->processor.loadHostedVST3(target,path,choices[(size_t)id-1].identifier,&choices[(size_t)id-1].description);
        if(result.failed())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "VST3 could not load",result.getErrorMessage());
        else safe->syncEmbeddedEditor();
        safe->graphSnapshotValid=false;safe->refreshIfChanged();safe->repaint();
    };
    if(choices.size()==1){finish(1);return;}
    juce::PopupMenu menu;
    menu.addSectionHeader("Choose the exact VST3 effect");
    for(int i=0;i<(int)choices.size();++i){
        const auto& choice=choices[(size_t)i];
        menu.addItem(i+1,choice.name+(choice.vendor.isNotEmpty()?"  /  "+choice.vendor:juce::String{}));
    }
    hostedClassMenuOpen=true;hideEmbeddedEditor();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),std::move(finish));
}
void SlotEditor::chooseVST3File() {
    // Keep the asynchronous chooser alive until it returns.
    externalChooser=std::make_unique<juce::FileChooser>(
        "Choose an installed VST3 effect",juce::File{},"*.vst3",true);
    juce::Component::SafePointer<SlotEditor> safe(this);
    externalChooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::canSelectDirectories,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return;
            const auto path=chooser.getResult();
            if(path.getFullPathName().isEmpty())return;
            safe->loadVST3FromFile(path);
        });
}
void SlotEditor::applyTheme() {
    Theme::selectFor(processor.state);
    moduleBox.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
    moduleBox.setColour(juce::ComboBox::textColourId,Theme::white);
    moduleBox.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    modulePicker.applyTheme();
    installedBrowser.applyTheme();
    for(auto* button:{&loadExternal,&editExternal}) {
        button->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        button->setColour(juce::TextButton::textColourOffId,Theme::green);
    }
    for(juce::Slider* s:std::array<juce::Slider*,13>{{
        &eqQSlider,&eqCutSliders[0],&eqCutSliders[1],&studioGateFloor,
        &studioCompKnee,&knobs[0],&knobs[1],&knobs[2],&knobs[3],
        &duckDepth,&duckThreshold,&duckAttack,&duckRelease}}) {
        s->setColour(juce::Slider::rotarySliderFillColourId,Theme::green);
        s->setColour(juce::Slider::rotarySliderOutlineColourId,Theme::stroke);
        s->setColour(juce::Slider::thumbColourId,Theme::white);
        s->setColour(juce::Slider::textBoxTextColourId,Theme::white);
        s->setColour(juce::Slider::textBoxBackgroundColourId,Theme::panelAlt);
        s->setColour(juce::Slider::textBoxOutlineColourId,Theme::stroke);
    }
    upgradeEq.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.18f));
    upgradeEq.setColour(juce::TextButton::textColourOffId,Theme::green);
    addEqNode.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.14f));
    addEqNode.setColour(juce::TextButton::textColourOffId,Theme::green);
    eqNodeMenu.repaint();
    duckOpen.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.17f));
    duckOpen.setColour(juce::TextButton::textColourOffId,Theme::green);
    duckClose.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
    duckClose.setColour(juce::TextButton::textColourOffId,Theme::white);
    duckOn.setColour(juce::ToggleButton::textColourId,Theme::white);
    duckExternal.setColour(juce::ToggleButton::textColourId,Theme::white);
    rightFirst.setColour(juce::ToggleButton::textColourId,Theme::white);
    duckPanel.repaint();
    for(int page=0;page<4;++page) {
        auto& button=suitePageButtons[(size_t)page];
        button.setColour(juce::TextButton::buttonColourId,
            page==suitePage?Theme::green.withAlpha(.31f):Theme::panelAlt);
        button.setColour(juce::TextButton::textColourOffId,
            page==suitePage?Theme::green:Theme::white);
    }
    vocalFeelStyleButton.setColour(juce::TextButton::buttonColourId,
                                  Theme::green.withAlpha(.16f));
    vocalFeelStyleButton.setColour(juce::TextButton::textColourOffId,Theme::green);
    vocalFeelStyles.repaint();
    suiteSpectrumButton.setColour(juce::TextButton::buttonColourId,
                                  suiteSpectrumView?Theme::green.withAlpha(.18f):Theme::panelAlt);
    suiteSpectrumButton.setColour(juce::TextButton::textColourOffId,
                                  suiteSpectrumView?Theme::green:Theme::white);
    modOrbButton.setColour(juce::TextButton::buttonColourId,
        modOrbView?Theme::green.withAlpha(.22f):Theme::panelAlt);
    modOrbButton.setColour(juce::TextButton::textColourOffId,
        modOrbView?Theme::green:Theme::white);
    for(int page=0;page<3;++page) {
        auto& button=matchPageButtons[(size_t)page];
        button.setColour(juce::TextButton::buttonColourId,
            page==matchPage?Theme::green.withAlpha(.32f):Theme::panelAlt);
        button.setColour(juce::TextButton::textColourOffId,
            page==matchPage?Theme::green:Theme::white);
    }
    for(int i=0;i<8;++i) {
        eqBandButtons[(size_t)i].setColour(juce::TextButton::buttonColourId,
            i==activeEqBand?Theme::green.withAlpha(.36f):Theme::panelAlt);
        eqBandButtons[(size_t)i].setColour(juce::TextButton::textColourOffId,
            i==activeEqBand?Theme::green:Theme::white);
    }
    repaint();
}
int SlotEditor::studioEqBandType(int band)const{
    if(band<0||band>=8)return 0;
    const auto id="s"+juce::String(slot+1)+"_studioEqKind"+juce::String(band+1);
    if(const auto* p=processor.state.getRawParameterValue(id))
        return juce::jlimit(0,4,(int)p->load());
    return band==0?3:(band==6?4:0);
}
bool SlotEditor::studioEqBandEnabled(int band)const{
    if(band<0||band>=8)return false;
    const auto id="s"+juce::String(slot+1)+"_studioEqOn"+juce::String(band+1);
    if(const auto* p=processor.state.getRawParameterValue(id))return p->load()>.5f;
    return band<7;
}
void SlotEditor::setStudioEqBandType(int band,int type){
    if(band<0||band>=8)return;
    const auto id="s"+juce::String(slot+1)+"_studioEqKind"+juce::String(band+1);
    if(auto* p=processor.state.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1((float)juce::jlimit(0,4,type)));
    graphSnapshotValid=false;refreshIfChanged();
}
void SlotEditor::setStudioEqBandEnabled(int band,bool enabled){
    if(band<0||band>=8)return;
    const auto id="s"+juce::String(slot+1)+"_studioEqOn"+juce::String(band+1);
    if(auto* p=processor.state.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(enabled?1.f:0.f));
    graphSnapshotValid=false;refreshIfChanged();
}
void SlotEditor::syncStudioEqNodeButtons(){
    const bool studio=processor.slotType(slot)==SkellPresetAudioProcessor::StudioEQFX;
    int count=0;
    if(studio){
        for(int band=0;band<8;++band){
            const bool enabled=studioEqBandEnabled(band);
            eqBandButtons[(size_t)band].setVisible(enabled);
            if(enabled)++count;
        }
    }
    addEqNode.setVisible(studio);
    addEqNode.setButtonText(count==8?"8 / 8 NODES":"+ ADD NODE");
    addEqNode.setEnabled(studio&&count<8);
}
void SlotEditor::showEqBank(int firstBand) {
    const auto module=processor.slotType(slot);
    if(!isPagedEQ(module))return;
    // Seven bands need an overlapping final page: B6+B7. Previously the
    // last button silently displayed B5+B6, leaving B7 rotary automation
    // unreachable even though its graph point and WIDTH control existed.
    eqBank=module==SkellPresetAudioProcessor::StudioEQFX
        ? (firstBand>=7?6:(firstBand>=6?5:juce::jlimit(0,2,firstBand/2)*2))
        : juce::jlimit(0,3,firstBand/2)*2;
    const auto prefix="s"+juce::String(slot+1)+"_";
    for(auto& a:knobAttachment)a.reset();
    for(int k=0;k<4;++k){
        const int param=eqBank*2+k+1;
        knobAttachment[(size_t)k]=
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"p"+juce::String(param),knobs[(size_t)k]);
    }
    eqQAttachment.reset();
    eqQAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.state,prefix+"eqQ"+juce::String(activeEqBand+1),eqQSlider);
    applyKnobReadouts();
    for(int i=0;i<8;++i) {
        const bool active=i==activeEqBand;
        auto& b=eqBandButtons[(size_t)i];
        b.setColour(juce::TextButton::buttonColourId,
            active?Theme::green.withAlpha(.36f):Theme::panelAlt);
        b.setColour(juce::TextButton::textColourOffId,active?Theme::green:Theme::white);
        b.repaint();
    }
    graphSnapshotValid=false;
    repaint();
}
void SlotEditor::showMatchPage(int page) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX)
        return;
    matchPage=juce::jlimit(0,2,page);
    const auto prefix="s"+juce::String(slot+1)+"_";
    for(auto& attachment:knobAttachment)attachment.reset();
    for(int k=0;k<4;++k)
        knobAttachment[(size_t)k]=
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"p"+juce::String(matchPage*4+k+1),
                knobs[(size_t)k]);
    for(int i=0;i<3;++i) {
        auto& b=matchPageButtons[(size_t)i];
        b.setColour(juce::TextButton::buttonColourId,
            i==matchPage?Theme::green.withAlpha(.32f):Theme::panelAlt);
        b.setColour(juce::TextButton::textColourOffId,
            i==matchPage?Theme::green:Theme::white);
        b.repaint();
    }
    applyKnobReadouts();
    graphSnapshotValid=false;
    repaint();
}
void SlotEditor::updateLimiterCompactVisibility() {
    studioGateFloor.setVisible(false);
    const int type=processor.slotType(slot);
    if(skellpreset::face::limiter(type))for(int i=0;i<4;++i)knobs[(size_t)i].setVisible(i<3);
}
void SlotEditor::bindLimiterControls() {
    if(!skellpreset::face::limiter(processor.slotType(slot)))return;
    const auto pre="s"+juce::String(slot+1)+"_";
    for(auto& a:knobAttachment)a.reset();
    const juce::String ids[]{pre+"p1",pre+"limitRelease",pre+"limitAttack"};
    for(int i=0;i<3;++i)knobAttachment[(size_t)i]=
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,ids[i],knobs[(size_t)i]);
    updateLimiterCompactVisibility();applyKnobReadouts();
}
void SlotEditor::showLimiterPage(int) {limiterPage=0;bindLimiterControls();}
void SlotEditor::showSuitePage(int page) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::VocalFeelSuiteFX)return;
    suitePage=juce::jlimit(0,3,page);
    for(auto& attachment:knobAttachment)attachment.reset();
    const auto prefix="s"+juce::String(slot+1)+"_";
    for(int i=0;i<4;++i)
        knobAttachment[(size_t)i]=
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"p"+juce::String(suitePage*4+i+1),
                knobs[(size_t)i]);
    for(int i=0;i<4;++i) {
        auto& button=suitePageButtons[(size_t)i];
        button.setColour(juce::TextButton::buttonColourId,
            i==suitePage?Theme::green.withAlpha(.31f):Theme::panelAlt);
        button.setColour(juce::TextButton::textColourOffId,
            i==suitePage?Theme::green:Theme::white);
    }
    applyKnobReadouts();graphSnapshotValid=false;repaint();
}
void SlotEditor::selectSuiteRecipe(int index) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::VocalFeelSuiteFX
       || index<0 || index>=(int)skellpreset::dsp::vocalFeelRecipes.size())return;
    const auto& values=skellpreset::dsp::vocalFeelRecipes[(size_t)index].values;
    for(int param=0;param<16;++param) {
        // Source presets preserve input sensitivity and master output.
        if(param==1||param==15)continue;
        processor.setSlotParam(slot,param,values[(size_t)param]);
    }
    graphSnapshotValid=false;refreshIfChanged();repaint();
}
bool SlotEditor::testSuiteSpectrumUIForTest() {
    const int original=slot,oldType=processor.slotType(slot);
    processor.setSlotType(slot,SkellPresetAudioProcessor::VocalFeelSuiteFX);select(slot);
    const bool ok=suiteSpectrumView && !suiteSpectrumButton.isVisible() && testIntegratedEffectLayout();
    processor.setSlotType(slot,oldType);select(original);return ok;
}
bool SlotEditor::testVocalFeelSuiteUI() {
    const int originalSlot=slot,originalType=processor.slotType(slot);
    std::array<float,16> previous{};
    for(int i=0;i<16;++i)previous[(size_t)i]=processor.getSlotParam(slot,i);
    processor.setSlotType(slot,SkellPresetAudioProcessor::VocalFeelSuiteFX);
    processor.setSlotOn(slot,true);select(slot);
    bool result=suitePage==0 && suitePageButtons[0].isVisible()
                 && vocalFeelStyleButton.isVisible();
    showSuitePage(1);
    result=result && knobs[2].getTextFromValue(.5)=="7.50 kHz";
    showSuitePage(3);
    knobs[0].setValue(.73,juce::sendNotificationSync);
    result=result && std::abs(processor.getSlotParam(slot,12)-.73f)<.015f;
    vocalFeelStyles.open(-1);
    result=result && vocalFeelStyles.isVisible() &&
        vocalFeelStyles.testLayoutForTest();
    const float beforeSens=processor.getSlotParam(slot,1);
    const float beforeMaster=processor.getSlotParam(slot,15);
    vocalFeelStyles.selectForTest(6); // Female Air
    result=result && !vocalFeelStyles.isVisible() &&
        std::abs(processor.getSlotParam(slot,0)-1.f)<.002f &&
        std::abs(processor.getSlotParam(slot,1)-beforeSens)<.002f &&
        std::abs(processor.getSlotParam(slot,15)-beforeMaster)<.002f;
    const float beforeAttack=processor.getSlotParam(slot,12);
    gestureSuiteParameter=12;updateGraphNode({405.f,208.f});
    gestureSuiteParameter=-1;
    result=result && std::abs(processor.getSlotParam(slot,12)-beforeAttack)>.01f;
    processor.setSlotType(slot,originalType);
    for(int i=0;i<16;++i)processor.setSlotParam(slot,i,previous[(size_t)i]);
    select(originalSlot);
    return result;
}
void SlotEditor::showDelayPage(int page) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::StudioDelayFX)return;
    delayPage=juce::jlimit(0,3,page);
    for(auto& attachment:knobAttachment)attachment.reset();
    const auto prefix="s"+juce::String(slot+1)+"_";
    for(int i=0;i<4;++i)
        knobAttachment[(size_t)i]=
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"p"+juce::String(delayPage*4+i+1),
                knobs[(size_t)i]);
    for(int i=0;i<4;++i) {
        auto& button=delayPageButtons[(size_t)i];
        button.setColour(juce::TextButton::buttonColourId,
            i==delayPage?Theme::green.withAlpha(.31f):Theme::panelAlt);
        button.setColour(juce::TextButton::textColourOffId,
            i==delayPage?Theme::green:Theme::white);
    }
    applyKnobReadouts();graphSnapshotValid=false;repaint();
}
bool SlotEditor::testStudioDelayPages() {
    const int physical=slot,oldType=processor.slotType(slot);
    std::array<float,16> saved{};
    for(int i=0;i<16;++i)saved[(size_t)i]=processor.getSlotParam(slot,i);
    processor.setSlotType(physical,SkellPresetAudioProcessor::StudioDelayFX);
    processor.setSlotOn(physical,true);
    select(physical);
    bool ok=delayPage==0 && delayPageButtons[0].isVisible();
    processor.setSlotParam(physical,12,1.f);
    processor.setSlotParam(physical,13,.76f);
    showDelayPage(3);
    ok=ok && delayPage==3 && knobs[0].getTextFromValue(1.)=="ON" &&
         knobs[1].getTextFromValue(.76).isNotEmpty() &&
         knobs[1].valueFromTextFunction &&
         std::abs(knobs[1].valueFromTextFunction("1/8 beat")-.6875)<.002 &&
         knobs[0].valueFromTextFunction &&
         knobs[0].valueFromTextFunction("OFF")==0.;
    showDelayPage(0);
    ok=ok && knobs[0].valueFromTextFunction &&
         std::abs(knobs[0].valueFromTextFunction("250 ms")-
                  std::log(10.)/std::log(80.))<.003 &&
         knobs[1].valueFromTextFunction &&
         std::abs(knobs[1].valueFromTextFunction("47%")-.5)<.002 &&
         knobs[2].valueFromTextFunction &&
         std::abs(knobs[2].valueFromTextFunction("PING PONG")-.625)<.002;
    showDelayPage(1);
    ok=ok && knobs[0].valueFromTextFunction &&
         std::abs(knobs[0].valueFromTextFunction("412 Hz")-.5)<.002 &&
         knobs[1].valueFromTextFunction &&
         std::abs(knobs[1].valueFromTextFunction("6 kHz")-
                  std::log(15.)/std::log(45.))<.003;
    showDelayPage(2);
    knobs[2].setValue(.61,juce::sendNotificationSync);
    ok=ok && std::abs(processor.getSlotParam(physical,10)-.61f)<.01f;
    processor.setSlotType(physical,oldType);
    for(int i=0;i<16;++i)processor.setSlotParam(physical,i,saved[(size_t)i]);
    select(physical);
    return ok;
}
bool SlotEditor::testWetSidechainPanel() {
    const int physical=slot,originalType=processor.slotType(slot);
    const auto prefix="s"+juce::String(slot+1)+"_";
    auto* on=processor.state.getParameter(prefix+"duckOn");
    auto* amount=processor.state.getParameter(prefix+"duckDepth");
    auto* threshold=processor.state.getParameter(prefix+"duckThreshold");
    auto* release=processor.state.getParameter(prefix+"duckRelease");
    auto* attack=processor.state.getParameter(prefix+"duckAttack");
    if(!on||!amount||!threshold||!release||!attack)return false;
    const float saved[]{processor.state.getRawParameterValue(prefix+"duckOn")->load(),
                        processor.state.getRawParameterValue(prefix+"duckDepth")->load(),
                        processor.state.getRawParameterValue(prefix+"duckThreshold")->load(),
                        processor.state.getRawParameterValue(prefix+"duckRelease")->load(),
                        processor.state.getRawParameterValue(prefix+"duckAttack")->load()};
    processor.setSlotType(slot,SkellPresetAudioProcessor::Reverb);
    select(slot);
    bool ok=duckOpen.isVisible()&&!duckPanel.isVisible();
    duckOpen.onClick();
    ok=ok&&duckPanel.isVisible()&&duckDepth.isVisible()&&duckOn.isVisible()
         &&duckPanel.getRight()<=getWidth()&&duckPanel.getBottom()<=getHeight();
    duckOn.setToggleState(true,juce::sendNotificationSync);
    duckDepth.setValue(.77,juce::sendNotificationSync);
    duckThreshold.setValue(-34.,juce::sendNotificationSync);
    duckRelease.setValue(420.,juce::sendNotificationSync);
    duckAttack.setValue(5.,juce::sendNotificationSync);
    ok=ok&&processor.state.getRawParameterValue(prefix+"duckOn")->load()>.5f &&
       std::abs(processor.state.getRawParameterValue(prefix+"duckDepth")->load()-.77f)<.02f &&
       duckThreshold.getTextFromValue(-34.).contains("dBFS") &&
       duckRelease.getTextFromValue(420.).contains("ms") &&
       duckAttack.getTextFromValue(5.).contains("ms") &&
       std::abs(processor.state.getRawParameterValue(prefix+"duckAttack")->load()-5.f)<.1f;
    duckClose.onClick();
    ok=ok&&!duckPanel.isVisible();
    on->setValueNotifyingHost(on->convertTo0to1(saved[0]));
    amount->setValueNotifyingHost(amount->convertTo0to1(saved[1]));
    threshold->setValueNotifyingHost(threshold->convertTo0to1(saved[2]));
    release->setValueNotifyingHost(release->convertTo0to1(saved[3]));
    attack->setValueNotifyingHost(attack->convertTo0to1(saved[4]));
    processor.setSlotType(physical,originalType);
    select(physical);
    return ok;
}
bool SlotEditor::testAdlibControls() {
    const int physical=slot,oldType=processor.slotType(slot);
    const auto* oldOn=processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_on");
    const bool wasOn=oldOn&&oldOn->load()>.5f;
    std::array<float,16> old{};
    for(int i=0;i<16;++i)old[(size_t)i]=processor.getSlotParam(slot,i);
    bool ok=true;
    for(int type=SkellPresetAudioProcessor::AdlibChop;
        type<=SkellPresetAudioProcessor::AdlibSaturate;++type) {
        processor.setSlotType(slot,type);
        processor.setSlotOn(slot,true);
        select(slot);
        ok=ok&&moduleSelectorMatchesAudio()&&knobs[0].isEnabled()&&knobs[3].isEnabled();
        if(type==SkellPresetAudioProcessor::AdlibChop)
            ok=ok&&titles(type)[1]=="Beat Division"&&
                knobs[1].getTextFromValue(.625).contains("4x")&&
                knobs[1].valueFromTextFunction&&
                std::abs(knobs[1].valueFromTextFunction("8x")-.875)<.003;
        else if(type==SkellPresetAudioProcessor::AdlibMotion)
            ok=ok&&!knobs[2].isEnabled()&&
                knobs[2].getTextFromValue(.5)=="N/A"&&
                knobs[1].valueFromTextFunction&&
                std::abs(knobs[1].valueFromTextFunction("5.04 Hz")-.5)<.005;
        else if(type==SkellPresetAudioProcessor::AdlibFilter)
            ok=ok&&titles(type)[1]=="HP Blend"&&
                knobs[2].valueFromTextFunction&&
                std::abs(knobs[2].valueFromTextFunction("1200 Hz")-
                         std::log(15.)/std::log(225.))<.003;
        else
            ok=ok&&!knobs[1].isEnabled()&&
                knobs[1].getTextFromValue(.5)=="N/A";
    }
    processor.setSlotType(physical,oldType);
    processor.setSlotOn(physical,wasOn);
    for(int i=0;i<16;++i)processor.setSlotParam(physical,i,old[(size_t)i]);
    select(physical);
    return ok;
}
bool SlotEditor::testStudioLimiterPages() {
    juce::MemoryBlock saved;processor.getStateInformation(saved);
    const int original=slot;bool ok=true;
    for(int type:{SkellPresetAudioProcessor::Limiter,SkellPresetAudioProcessor::StudioLimiterFX}) {
        processor.setSlotType(slot,type);select(slot);
        std::array<float,8> hidden{};
        for(int i=1;i<9;++i)hidden[(size_t)i-1]=processor.getSlotParam(slot,i);
        const auto pre="s"+juce::String(slot+1)+"_";
        ok=ok && !studioGateFloor.isVisible() && !knobs[3].isVisible();
        for(int i=0;i<3;++i)ok=ok && knobs[(size_t)i].isVisible() && knobs[(size_t)i].getWidth()>190;
        knobs[0].setValue(.72,juce::sendNotificationSync);
        knobs[1].setValue(227.,juce::sendNotificationSync);
        knobs[2].setValue(1.25,juce::sendNotificationSync);
        ok=ok && std::abs(processor.getSlotParam(slot,0)-.72f)<.002f &&
            std::abs(processor.state.getRawParameterValue(pre+"limitRelease")->load()-227.f)<.01f &&
            std::abs(processor.state.getRawParameterValue(pre+"limitAttack")->load()-1.25f)<.01f &&
            knobs[0].getTextFromValue(.72).contains("dBFS") &&
            knobs[1].getTextFromValue(227.)=="227 ms" &&
            knobs[2].getTextFromValue(1.25)=="1.25 ms";
        for(int i=1;i<9;++i)ok=ok && processor.getSlotParam(slot,i)==hidden[(size_t)i-1];
    }
    processor.setStateInformation(saved.getData(),(int)saved.getSize());select(original);return ok;
}
bool SlotEditor::testMatchFinishPages() {
    processor.setSlotType(0,SkellPresetAudioProcessor::AlchemistMatchFinishFX);
    processor.setSlotOn(0,true);
    processor.setSlotParam(0,0,.20f);
    processor.setSlotParam(0,4,.67f);
    processor.setSlotParam(0,5,.335f);
    processor.setSlotParam(0,8,1.f);
    processor.setSlotParam(0,9,.95f);
    processor.setSlotParam(0,10,.85f);
    processor.setSlotParam(0,11,.50f);
    select(0);
    const bool shape=matchPage==0 && matchPageButtons[0].isVisible() &&
        matchPageButtons[1].isVisible() && matchPageButtons[2].isVisible() &&
        std::abs(knobs[0].getValue()-.20)<.002;
    showMatchPage(1);
    const bool calibrated=matchPage==1 &&
        std::abs(knobs[0].getValue()-.67)<.002 &&
        std::abs(knobs[1].getValue()-.335)<.002 &&
        knobs[0].getTextFromValue(.67).contains("dBFS") &&
        knobs[3].getTextFromValue(.271).contains("ms");
    showMatchPage(2);
    const bool reference=matchPage==2 &&
        std::abs(knobs[0].getValue()-1.)<.002 &&
        std::abs(knobs[1].getValue()-.95)<.002 &&
        std::abs(knobs[2].getValue()-.85)<.002 &&
        std::abs(knobs[3].getValue()-.50)<.002 &&
        knobs[0].getTextFromValue(1.).containsIgnoreCase("ON");
    // Prove the REFERENCE knob edits the host's ninth existing parameter.
    knobs[1].setValue(.72,juce::sendNotificationSync);
    const bool routed=std::abs(processor.getSlotParam(0,9)-.72f)<.005f;
    showMatchPage(0);
    return shape && calibrated && reference && routed &&
        std::abs(knobs[0].getValue()-.20)<.002 &&
        std::abs(processor.getSlotParam(0,4)-.67f)<.002f;
}
void SlotEditor::openModulePicker() {
    installedBrowser.hide();
    livePanel.setVisible(false);liveView.setButtonText("VISUAL");
    if(modulePicker.isVisible())modulePicker.hide();
    else {hideEmbeddedEditor();modulePicker.open(processor.slotType(slot));}
}
SlotEditor::~SlotEditor(){processor.requestSlotVisual(-1);endGraphGesture();releaseEmbeddedEditor();}
void SlotEditor::select(int newSlot) {
    endGraphGesture();
    eqNodeMenu.close();
    if(slot!=newSlot)releaseEmbeddedEditor();
    slot=juce::jlimit(0,SkellPresetAudioProcessor::slotCount-1,newSlot);
    eqBank=0;activeEqBand=0;matchPage=0;limiterPage=0;
    lastPeakLevel=-1;lastGainReductionStep=-1;lastDeessStep=-1;
    lastSuiteLevels.fill(-1);vocalFeelStyles.close();
    suiteSpectrumView=false;suiteSpectrumTick=0;
    suiteSpectrumButton.setButtonText("SPECTRUM");
    graphSnapshotValid=false; // never leave the previous slot's power state
    if(modulePicker.isVisible())modulePicker.hide();
    installedBrowser.hide();
    duckPanel.setVisible(false);livePanel.setVisible(false);liveView.setButtonText("VISUAL");
    graphSnapshotValid=false;
    eqQAttachment.reset();
    duckSourceAttachment.reset();parallelReturnAttachment.reset();duckExternalAttachment.reset();
    gateFloorAttachment.reset();
    duckOnAttachment.reset();
    rightFirstAttachment.reset();
    for(auto& a:duckAttachments)a.reset();
    for(auto& a:eqCutAttachments)a.reset();
    externalChooser.reset();
    for(auto& a:knobAttachment)a.reset();
    const auto prefix="s"+juce::String(slot+1)+"_";
    duckOnAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.state,prefix+SkellPresetAudioProcessor::sidechainEnableSuffix(processor.slotType(slot)),duckOn);
    rightFirstAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.state,prefix+"rightFirst",rightFirst);
    duckSourceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,prefix+"duckSource",duckSource);
    parallelReturnAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,prefix+"parallelReturn",parallelReturn);
    duckExternalAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,prefix+"duckExternal",duckExternal);
    duckAttachments[0]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.state,prefix+"duckDepth",duckDepth);
    duckAttachments[1]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.state,prefix+"duckThreshold",duckThreshold);
    duckAttachments[2]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.state,prefix+"duckAttack",duckAttack);
    duckAttachments[3]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.state,prefix+"duckRelease",duckRelease);
    // Restore physical readouts after APVTS attachment creates the slider ranges.
    duckDepth.textFromValueFunction=[](double v){return juce::String(v*100.,0)+"%";};
    duckThreshold.textFromValueFunction=[](double v){return juce::String(v,1)+" dBFS";};
    duckAttack.textFromValueFunction=[](double v){return juce::String(v,0)+" ms";};
    duckRelease.textFromValueFunction=[](double v){return juce::String(v,0)+" ms";};
    duckDepth.updateText();duckThreshold.updateText();duckAttack.updateText();
    duckRelease.updateText();
    moduleBox.setSelectedId(processor.slotType(slot)+1,juce::dontSendNotification);
    if(isPagedEQ(processor.slotType(slot))) {
        eqQAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,prefix+"eqQ1",eqQSlider);
        if(processor.slotType(slot)==SkellPresetAudioProcessor::EightBandToneFX) {
        eqCutAttachments[0]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,prefix+"eqLowCut",eqCutSliders[0]);
        eqCutAttachments[1]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,prefix+"eqHighCut",eqCutSliders[1]);
        }
    }
    for(int k=0;k<4;k++)knobAttachment[(size_t)k]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,prefix+"p"+juce::String(k+1),knobs[(size_t)k]);
    refreshIfChanged();
    if(skellpreset::face::limiter(processor.slotType(slot)))bindLimiterControls();
    applyKnobReadouts();
    if(isPagedEQ(processor.slotType(slot)))
        showEqBank(0);
    if(processor.slotType(slot)==SkellPresetAudioProcessor::AlchemistMatchFinishFX)
        showMatchPage(0);
    if(processor.slotType(slot)==SkellPresetAudioProcessor::StudioLimiterFX)
        showLimiterPage(0);
    if(processor.slotType(slot)==SkellPresetAudioProcessor::StudioDelayFX)
        showDelayPage(0);
    if(processor.slotType(slot)==SkellPresetAudioProcessor::VocalFeelSuiteFX)
        showSuitePage(0);
    repaint();
}
void SlotEditor::syncPresentation() {
    const int type=processor.slotType(slot);
    const bool eligible=type!=SkellPresetAudioProcessor::Empty && type!=SkellPresetAudioProcessor::ExternalVST3;
    liveView.setVisible(false);
    livePanel.setVisible(false);
    processor.requestSlotVisual(eligible && hostedPresentation && isShowing()?slot:-1);
}
void SlotEditor::refreshIfChanged() {
    const auto prefix="s"+juce::String(slot+1)+"_";
    const int type=processor.slotType(slot);
    if(eqNodeMenu.isVisible() &&
       (type!=SkellPresetAudioProcessor::StudioEQFX || !studioEqBandEnabled(contextEqBand)))
        eqNodeMenu.close();
    std::array<float,6> current{};
    for(int i=0;i<4;++i)current[(size_t)i]=processor.getSlotParam(slot,i);
    if(const auto* macro=processor.state.getRawParameterValue("macro1"))current[4]=macro->load();
    if(const auto* macro=processor.state.getRawParameterValue("macro2"))current[5]=macro->load();
    const auto* power=processor.state.getRawParameterValue(prefix+"on");
    const bool powerOn=power && power->load()>.5f && type!=SkellPresetAudioProcessor::Empty;
    slotPower.setStatus(powerOn,type!=SkellPresetAudioProcessor::Empty);
    slotPower.setVisible(type!=SkellPresetAudioProcessor::Empty);
    bool changed=!graphSnapshotValid || type!=lastType || powerOn!=lastPowerOn;
    // 0.5 dB steps bound GUI repaints. The meter is measured on the audio
    // callback and published atomically; no synthetic GR animation.
    if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX && suiteSpectrumView) {
        // At most 10 spectrum repaints per second with the existing 30Hz UI timer.
        // The Goertzel transform itself is computed on the GUI paint thread.
        if(++suiteSpectrumTick>=3) {
            suiteSpectrumTick=0;
            repaint(juce::Rectangle<int>(33,408,835,106));
        }
    }
    const bool vocalMeter=skellpreset::face::hardware(type);
    const int grStep=vocalMeter
        ?juce::roundToInt(2.f*processor.getSlotGainReductionDb(slot)):0;
    changed=changed || grStep!=lastGainReductionStep;
    lastGainReductionStep=grStep;
    const int deessStep=type==SkellPresetAudioProcessor::VocalFeelSuiteFX
        ?juce::roundToInt(2.f*processor.getSlotDeessReductionDb(slot)):0;
    changed=changed || deessStep!=lastDeessStep;lastDeessStep=deessStep;
    for(int i=0;i<5;++i){
        // The graph uses the same -55..0 dB scale; a linear-RMS
        // quantizer made quiet real vocal returns appear frozen.
        const float returnRms=type==SkellPresetAudioProcessor::VocalFeelSuiteFX
            ?processor.getSuiteReturnRms(slot,i):0.f;
        const int quant=type==SkellPresetAudioProcessor::VocalFeelSuiteFX
            ?juce::jlimit(0,16,juce::roundToInt(16.f*
                juce::jlimit(0.f,1.f,
                    (skellpreset::dsp::gainToDb(returnRms)+55.f)/55.f))):0;
        changed=changed || quant!=lastSuiteLevels[(size_t)i];
        lastSuiteLevels[(size_t)i]=quant;
    }
    lastPowerOn=powerOn;
    for(int i=0;i<6;++i)changed=changed || std::abs(current[(size_t)i]-lastControls[(size_t)i])>1.e-5f;
    // Every published extended control may be automated by the host.
    // Include high-numbered controls in the paint snapshot even if their
    // page is hidden; the main graph reads their current values.
    for(int i=4;i<16;++i){
        const float value=processor.getSlotParam(slot,i);
        changed=changed||std::abs(value-lastExtendedControls[(size_t)i])>1.e-5f;
        lastExtendedControls[(size_t)i]=value;
    }
    const float rawPeak=processor.getSlotPeak(slot);
    const float measured=std::isfinite(rawPeak)?juce::jlimit(0.f,1.f,rawPeak):0.f;
    const int level=juce::jlimit(0,14,juce::roundToInt(measured*14.f));
    const bool peakChanged=lastPeakLevel!=level;
    lastPeakLevel=level;
    if(type==SkellPresetAudioProcessor::ExternalVST3) {
        const int latency=processor.hostedVST3LatencySamples(slot);
        const bool loaded=processor.hasHostedVST3(slot);
        changed=changed || latency!=lastHostedLatency || loaded!=lastHostedLoaded;
        lastHostedLatency=latency;
        lastHostedLoaded=loaded;
    }
    if(isPagedEQ(type)) {
        for(int k=0;k<16;++k) {
            const float v=processor.getSlotParam(slot,k);
            changed=changed || std::abs(v-lastEqControls[(size_t)k])>1.e-5f;
            lastEqControls[(size_t)k]=v;
        }
        for(int k=0;k<8;++k){
            const auto* value=processor.state.getRawParameterValue(
                prefix+"eqQ"+juce::String(k+1));
            const float v=value?value->load():.5f;
            changed=changed || std::abs(v-lastEqControls[(size_t)(16+k)])>1.e-5f;
            lastEqControls[(size_t)(16+k)]=v;
        }
        for(int k=0;k<2;++k) {
            const auto* value=processor.state.getRawParameterValue(
                prefix+(k==0?"eqLowCut":"eqHighCut"));
            const float v=value?value->load():0.f;
            changed=changed||std::abs(v-lastEqControls[(size_t)(24+k)])>1.e-5f;
            lastEqControls[(size_t)(24+k)]=v;
        }
    }
    if(type==SkellPresetAudioProcessor::StudioEQFX) {
        for(int k=0;k<8;++k){
            const auto* kind=processor.state.getRawParameterValue(
                prefix+"studioEqKind"+juce::String(k+1));
            const auto* enabled=processor.state.getRawParameterValue(
                prefix+"studioEqOn"+juce::String(k+1));
            const float vKind=kind?kind->load():0.f;
            const float vOn=enabled?enabled->load():0.f;
            changed=changed || std::abs(vKind-lastEqControls[(size_t)(26+k)])>1.e-5f ||
                               std::abs(vOn-lastEqControls[(size_t)(34+k)])>1.e-5f;
            lastEqControls[(size_t)(26+k)]=vKind;
            lastEqControls[(size_t)(34+k)]=vOn;
        }
    }
    if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
        for(int k=0;k<12;++k) {
            const float v=processor.getSlotParam(slot,k);
            changed=changed || std::abs(v-lastEqControls[(size_t)k])>1.e-5f;
            lastEqControls[(size_t)k]=v;
        }
    }
    if(type==SkellPresetAudioProcessor::ExternalVST3){
        syncEmbeddedEditor();
        const bool inlineShown=embeddedEditor && embeddedEditor->isVisible();
        const auto caption=inlineShown?"DETACH UI":"OPEN UI";
        if(editExternal.getButtonText()!=caption)editExternal.setButtonText(caption);
    }
    syncPresentation();
    if(hostedPresentation && isShowing() && type!=SkellPresetAudioProcessor::Empty &&
       type!=SkellPresetAudioProcessor::ExternalVST3 && livePanel.update(processor,slot))
        repaint(19,142,868,378);
    if(duckPanel.isVisible()){
        duckPanel.setReduction(processor.getSlotDuckReductionDb(slot));
        duckSource.setEnabled(!duckExternal.getToggleState());
    }
    if(!changed){
        if(peakChanged){
            repaint(juce::Rectangle<int>(178,75,195,22));
            if(skellpreset::face::hardware(type))repaint(19,142,868,378);
            if(type==SkellPresetAudioProcessor::Delay ||
               (isModulationOrbEffect(type) && modOrbView))
                repaint(juce::Rectangle<int>(33,151,835,206));
        }
        return;
    }
    if(type!=lastType){
        duckOnAttachment.reset();
        duckOnAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.state,prefix+SkellPresetAudioProcessor::sidechainEnableSuffix(type),duckOn);
        endGraphGesture();
        layoutParameterControls(type);
        // A new effect resets the EQ bank, preserving all saved parameter
        // values while reattaching only the controls visible on this page.
        eqBank=0;activeEqBand=0;matchPage=0;limiterPage=0;delayPage=0;suitePage=0;
        vocalFeelStyles.close();
        suiteSpectrumView=type==SkellPresetAudioProcessor::VocalFeelSuiteFX;suiteSpectrumTick=0;
        suiteSpectrumButton.setButtonText("SPECTRUM");
        gateFloorAttachment.reset();
        studioCompKneeAttachment.reset();
        modOrbView=false;
        modOrbButton.setButtonText("BONES");
        eqQAttachment.reset();
        for(auto& attachment:eqCutAttachments)attachment.reset();
        if(isPagedEQ(type)) {
            eqQAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"eqQ1",eqQSlider);
            if(type==SkellPresetAudioProcessor::EightBandToneFX) {
            eqCutAttachments[0]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"eqLowCut",eqCutSliders[0]);
            eqCutAttachments[1]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"eqHighCut",eqCutSliders[1]);
            }
        }
        for(auto& attachment:knobAttachment)attachment.reset();
        for(int k=0;k<4;++k)
            knobAttachment[(size_t)k]=
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor.state,prefix+"p"+juce::String(k+1),knobs[(size_t)k]);
        moduleBox.setSelectedId(type+1,juce::dontSendNotification);
        if(isPagedEQ(type))
            showEqBank(activeEqBand);
        else if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX)
            showMatchPage(0);
        else if(type==SkellPresetAudioProcessor::StudioCompressorFX)
            studioCompKneeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"p5",studioCompKnee);
        else if(type==SkellPresetAudioProcessor::StudioDelayFX)
            showDelayPage(0);
        else if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX)
            showSuitePage(0);
        else if(type==SkellPresetAudioProcessor::StudioLimiterFX) {
            gateFloorAttachment=
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor.state,prefix+"p9",studioGateFloor);
            showLimiterPage(0);
        }
    }
    if(skellpreset::face::limiter(type))bindLimiterControls();
    // Selection may reuse a type with a different physical slot. Attach
    // the ninth control even when lastType coincidentally remains the same.
    if(type==SkellPresetAudioProcessor::StudioLimiterFX && !gateFloorAttachment)
        gateFloorAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,prefix+"p9",studioGateFloor);
    if(type==SkellPresetAudioProcessor::StudioCompressorFX && !studioCompKneeAttachment)
        studioCompKneeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state,prefix+"p5",studioCompKnee);
    applyKnobReadouts();
    lastType=type;lastControls=current;graphSnapshotValid=true;
    const bool external=type==SkellPresetAudioProcessor::ExternalVST3;
    if(!external && !externalBrowsePending && installedBrowser.isVisible())
        installedBrowser.hide();
    syncEmbeddedEditor();
    loadExternal.setVisible(external);
    editExternal.setVisible(external && processor.hasHostedVST3(slot)
                            && processor.hostedVST3InstanceForUI(slot)->hasEditor());
    // No competing "upgrade EQ" button: modern EQ is the single library
    // choice, while older saved EQ instances remain editable and recallable.
    upgradeEq.setVisible(false);
    modOrbButton.setVisible(false);modOrbButton.setBounds({});
    const bool hasSidechain=SkellPresetAudioProcessor::supportsWetSidechain(type);
    liveView.setVisible(false);livePanel.setVisible(false);
    parallelReturn.setEnabled(SkellPresetAudioProcessor::supportsParallelReturn(type));
    duckOpen.setVisible(hasSidechain);
    if(!SkellPresetAudioProcessor::supportsEffectSidechain(type))duckPanel.setVisible(false);
    eqQSlider.setVisible(isPagedEQ(type));
    for(auto& control:eqCutSliders)control.setVisible(type==SkellPresetAudioProcessor::EightBandToneFX);
    for(int index=0;index<8;++index)
        eqBandButtons[(size_t)index].setVisible(isPagedEQ(type) &&
            (type==SkellPresetAudioProcessor::EightBandToneFX ||
             studioEqBandEnabled(index)));
    syncStudioEqNodeButtons();
    for(auto& button:matchPageButtons)button.setVisible(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX);
    for(auto& button:delayPageButtons)button.setVisible(type==SkellPresetAudioProcessor::StudioDelayFX);
    rightFirst.setVisible(type==SkellPresetAudioProcessor::StudioDelayFX);
    for(auto& button:suitePageButtons)button.setVisible(type==SkellPresetAudioProcessor::VocalFeelSuiteFX);
    vocalFeelStyleButton.setVisible(type==SkellPresetAudioProcessor::VocalFeelSuiteFX);
    suiteSpectrumButton.setVisible(false);suiteSpectrumButton.setBounds({});
    if(type!=SkellPresetAudioProcessor::VocalFeelSuiteFX)vocalFeelStyles.close();
    studioGateFloor.setVisible(false);
    studioCompKnee.setVisible(type==SkellPresetAudioProcessor::StudioCompressorFX);
    for(int i=0;i<4;++i) {
        auto& knob=knobs[(size_t)i];
        const bool unused=(type==SkellPresetAudioProcessor::AdlibMotion && i==2)
            ||(type==SkellPresetAudioProcessor::AdlibSaturate && i==1);
        knob.setEnabled(!external && !unused);
        knob.setVisible(skellpreset::face::visible(type,i));
        knob.setTooltip(unused
            ?juce::String("Reserved automation parameter; no audible DSP")
            :juce::String("Drag, wheel or type the displayed value. Double-click to reset."));
    }
    updateLimiterCompactVisibility();
    repaint();
}
void SlotEditor::resized() {
    // The rack row now owns wet, bypass and module replacement.
    moduleBox.setBounds(489,25,211,29);
    modulePicker.positionWithin({405,66,470,400});
    eqQSlider.setBounds(672,114,186,26);
    eqCutSliders[0].setBounds(68,125,270,23);
    eqCutSliders[1].setBounds(425,125,215,23);
    for(int k=0;k<8;++k)eqBandButtons[(size_t)k].setBounds(219+k*49,96,40,26);
    matchPageButtons[0].setBounds(512,72,105,23);
    matchPageButtons[1].setBounds(623,72,125,23);
    matchPageButtons[2].setBounds(754,72,112,23);
    for(int page=0;page<4;++page) {
        delayPageButtons[(size_t)page].setBounds(45+page*150,106,142,26);
        suitePageButtons[(size_t)page].setBounds(45+page*144,106,136,27);
    }
    vocalFeelStyleButton.setBounds(626,105,238,29);
    vocalFeelStyles.setBounds(628,139,235,340);
    suiteSpectrumButton.setBounds(721,161,132,27);
    rightFirst.setBounds(47,196,242,25);
    duckOpen.setBounds(708,106,157,28);
    liveView.setBounds({});
    livePanel.setBounds(33,151,835,206);
    duckPanel.setBounds(505,136,353,356);
    duckClose.setBounds(318,11,25,23);
    duckOn.setBounds(16,41,326,31);
    duckDepth.setBounds(108,80,232,30);
    duckThreshold.setBounds(108,121,232,30);
    duckAttack.setBounds(108,162,232,30);
    duckRelease.setBounds(108,203,232,30);
    duckSource.setBounds(17,237,323,28);
    duckExternal.setBounds(17,267,323,27);
    parallelReturn.setBounds(17,298,323,28);
    studioGateFloor.setBounds(548,104,298,27);
    studioCompKnee.setBounds(548,104,298,27);
    loadExternal.setBounds(542,33,164,32);
    installedBrowser.setBounds(85,62,735,385);
    addEqNode.setBounds(74,96,127,26);
    eqNodeMenu.setBounds(610,156,205,241);
    // Leave the top-right synchronized power dot fully uncovered.
    editExternal.setBounds(714,33,121,32);
    layoutEmbeddedEditor();
    upgradeEq.setBounds(650,97,212,31);
    modOrbButton.setBounds({});
    // A common bounded parameter area for every native effect. Individual
    // family treatments are painted behind these real APVTS controls.
    slotPower.setBounds(843,22,40,40);
    layoutParameterControls(processor.slotType(slot));
}
void SlotEditor::layoutParameterControls(int type){
    const bool paired=!skellpreset::face::hardware(type) && controlFace(type)==ControlFace::eqPairs;
    for(int k=0;k<4;++k){
        auto& control=knobs[(size_t)k];
        control.setDoubleClickReturnValue(true,skellpreset::face::limiter(type)?(k==1?70.:k==2?0.:.5):.5);
        const bool rotary=rotaryControl(type,k);
        const auto style=rotary?juce::Slider::RotaryHorizontalVerticalDrag:
            juce::Slider::LinearHorizontal;
        if(control.getSliderStyle()!=style)control.setSliderStyle(style);
        const auto textPosition=paired?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow;
        if(control.getTextBoxPosition()!=textPosition ||
           control.getTextBoxWidth()!=(paired?112:132) || control.getTextBoxHeight()!=26)
            control.setTextBoxStyle(textPosition,false,paired?112:132,26);
        control.setBounds(parameterControlBounds(type,k,!limiterDetailsVisible));
    }
}
bool SlotEditor::testPopupLayoutsForTest() const {
    const auto bounds=getLocalBounds();
    if(getWidth()<906 || getHeight()<535)return false;
    const juce::Component* popups[]{&modulePicker,&installedBrowser,&eqNodeMenu,
                                    &duckPanel,&vocalFeelStyles};
    for(const auto* popup:popups)
        if(!bounds.contains(popup->getBounds()) ||
           popup->getWidth()<190 || popup->getHeight()<200)return false;
    // Overlays may occupy the same coordinates, but only one popup from
    // the mutually-exclusive effect-picker / VST3 browser pair can be visible.
    if(modulePicker.isVisible() && installedBrowser.isVisible())return false;
    const juce::Component* duckControls[]{&duckClose,&duckOn,&duckExternal,&duckSource,&parallelReturn,
        &duckDepth,&duckThreshold,&duckAttack,&duckRelease};
    for(const auto* child:duckControls)
        if(!duckPanel.getLocalBounds().contains(child->getBounds()))return false;
    if(vocalFeelStyleButton.getBounds().intersects(suitePageButtons[3].getBounds()))
        return false;
    const auto metadata=juce::Rectangle<int>{47,383,811,17};
    const auto graph=juce::Rectangle<int>{33,151,835,206};
    if(!bounds.contains(metadata) || metadata.intersects(graph) ||
       (!skellpreset::face::hardware(processor.slotType(slot)) && metadata.intersects(knobs[0].getBounds())) ||

       eqQSlider.getBounds().intersects(eqBandButtons[7].getBounds()))
        return false;
    for(int i=0;i<4;++i)
        for(int j=i+1;j<4;++j)
            if(suitePageButtons[(size_t)i].getBounds().intersects(
                   suitePageButtons[(size_t)j].getBounds()))return false;
    return modulePicker.testLayoutForTest() &&
           installedBrowser.testLayoutForTest() && eqNodeMenu.testLayoutForTest() &&
           vocalFeelStyles.testLayoutForTest() &&
           getLocalBounds().contains(suiteSpectrumButton.getBounds()) &&
           !suiteSpectrumButton.getBounds().intersects(vocalFeelStyleButton.getBounds());
}
std::array<juce::String,4> SlotEditor::titlesForHardware(int type) const {
    auto labels=titles(type);
    if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX && matchPage==1)
        labels={"Envelope Centre","Reference Level","Output Trim","Gain Smoothing"};
    if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX && matchPage==2)
        labels={"Reference On","Level Match","Air Match","Base Gain"};
    if(type==SkellPresetAudioProcessor::StudioLimiterFX && limiterPage==1)
        labels={"Gate Threshold","Release","Attack","Knee"};
    if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
        constexpr const char* captions[4][4]{
            {"VOICE","SENS","LOW EQ","MID EQ"},
            {"HIGH EQ","DE-ESS AMOUNT","DE-ESS FREQ","OPTO AMOUNT"},
            {"COLOUR","MAIN","MAGIC","SPACE"},
            {"ATTACK","ATTITUDE","PRESENCE","MASTER"}
        };
        for(int i=0;i<4;++i)labels[(size_t)i]=captions[suitePage][i];
    }
    if(type==SkellPresetAudioProcessor::StudioDelayFX) {
        constexpr const char* captions[4][4]{
            {"TIME","FEEDBACK","STEREO MODE","R/L OFFSET"},
            {"LOW CUT","HIGH CUT","DRIVE","SATURATION"},
            {"LFO RATE","MOD DEPTH","ECHO WET","DRY LEVEL"},
            {"TEMPO SYNC","BEAT DIVISION","TONE","DIFFUSION"}
        };
        for(int i=0;i<4;++i)labels[(size_t)i]=captions[delayPage][i];
    }
    if(isPagedEQ(type)) {
        labels={ "B"+juce::String(eqBank+1)+" FREQ", "B"+juce::String(eqBank+1)+" GAIN",
                 "B"+juce::String(eqBank+2)+" FREQ", "B"+juce::String(eqBank+2)+" GAIN" };
    }
    if(skellpreset::face::limiter(type))labels={"Threshold","Release","Attack",""};
    return labels;
}
void SlotEditor::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    // ONE uninterrupted studio effect face. Controls and graph are inside
    // it; the former second box and duplicate knob panel are gone.
    Theme::panelBox(g,{0,0,906.f,535.f},9.f);
    g.setColour(Theme::stroke.withAlpha(.60f));
    g.drawHorizontalLine(65,20.f,886.f);
    const int type=processor.slotType(slot);
    if(type==SkellPresetAudioProcessor::Empty) {
        Theme::text(g,"SLOT "+juce::String(slot+1).paddedLeft('0',2),
            {21,10,200,18},10,Theme::muted,true);
        const auto add=juce::Rectangle<int>(333,204,240,90);
        g.setColour(Theme::green.withAlpha(.10f));
        g.fillRoundedRectangle(add.toFloat(),9.f);
        g.setColour(Theme::green.withAlpha(.54f));
        g.drawRoundedRectangle(add.toFloat(),9.f,1.f);
        g.drawLine(445.f,231.f,461.f,231.f,1.8f);
        g.drawLine(453.f,223.f,453.f,239.f,1.8f);
        Theme::fittedText(g,"ADD EFFECT",{333,248,240,30},
            17.f,Theme::white,true,juce::Justification::centred);
        return;
    }
    const auto family=juce::String(skellpreset::registry::at(type).category);
    const auto familyAccent=family.equalsIgnoreCase("Dynamics")?Theme::amber:
        family.equalsIgnoreCase("Motion")?Theme::pink:
        family.equalsIgnoreCase("Space")?Theme::blue:
        family.equalsIgnoreCase("Pitch")?Theme::blue:
        family.equalsIgnoreCase("Vocal")?Theme::green:
        family.equalsIgnoreCase("External")?Theme::muted:Theme::green;
    Theme::text(g,"SLOT "+juce::String(slot+1).paddedLeft('0',2),{21,10,200,18},10,Theme::muted,true);
    const auto hostedName=type==SkellPresetAudioProcessor::ExternalVST3
        ?processor.hostedVST3Name(slot):juce::String{};
    Theme::fittedText(g,hostedName.isNotEmpty()?hostedName:processor.moduleDisplayName(type),
                      {21,29,500,38},25,Theme::white,true);
    if(type==SkellPresetAudioProcessor::ExternalVST3) {
        // Do not paint a fake effect face behind a real native child window.
        if(!embeddedEditor || !embeddedEditor->isVisible()) {
            const auto hint=embeddedFitRejected?"FIXED-SIZE NATIVE EDITOR / OPEN UI":
                processor.hasHostedVST3(slot)?"OPEN PLUGIN UI":"LOAD A VST3 EFFECT";
            Theme::fittedText(g,hint,{25,240,856,40},15.f,Theme::muted,true,juce::Justification::centred);
        }
        return;
    }
    // The hosted editor has its own top action buttons. Do not paint the
    // native selector prompt, category chip or signal dots underneath those
    // controls; it clips the third-party view at small UI scales.
    if(type!=SkellPresetAudioProcessor::ExternalVST3) {
        // Remove the duplicate module dropdown and ON/WET controls. One action
        // surface lives in the rack, including for external hosted VST3 effects.
        // Power is the single interactive status dot at the upper right.
        // It mirrors the rack's sN_on state, including host automation.
        const auto category=type==SkellPresetAudioProcessor::StudioLimiterFX
            ?juce::String("LIMITER")
            :type==SkellPresetAudioProcessor::AlchemistMatchFinishFX
                ?juce::String("MATCH")
                :juce::String(skellpreset::registry::at(type).category).toUpperCase();
        // Category occupies the dedicated metadata row above the title.
        // The former y=70 rectangle intersected the Match page buttons.
        Theme::fittedText(g,category,{528,10,294,18},10.8f,
                          familyAccent.withAlpha(.92f),true,
                          juce::Justification::centredRight);
        // Segment animation is bound to the real per-slot DSP peak. Bone-shaped
        // vertebrae respond to the signal while leaving parameter graphs readable.
        Theme::text(g,"SLOT OUTPUT",{22,73,158,18},11.2f,Theme::muted,true);
        const float level=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
        for(int rib=0;rib<14;++rib) {
            const float x=178.f+13.5f*(float)rib;
            const bool alive=level>((float)rib/14.f);
            g.setColour(alive?Theme::green.withAlpha(.33f+.55f*level)
                             :Theme::stroke.withAlpha(.67f));
            g.fillRoundedRectangle(x+2.f,79.1f,6.7f,7.4f,2.8f);
            g.fillEllipse(x,80.2f,4.5f,4.5f);
            g.fillEllipse(x+6.f,80.2f,4.5f,4.5f);
            if(alive) {
                g.setColour(Theme::white.withAlpha(.15f));
                g.drawLine(x+3.f,79.5f,x+8.f,79.5f,.8f);
            }
        }
    
    }
    if(skellpreset::face::hardware(type)){paintHardware(g,type);return;}
    const auto plot=juce::Rectangle<float>(33,151,835,206);
    const bool echoFace=type==SkellPresetAudioProcessor::Delay ||
        type==SkellPresetAudioProcessor::StudioDelayFX;
    const bool eqFace=type==SkellPresetAudioProcessor::EQ ||
        type==SkellPresetAudioProcessor::StudioEQFX ||
        type==SkellPresetAudioProcessor::EightBandToneFX ||
        type==SkellPresetAudioProcessor::DeEsser;
    // The empty slot has no graph to plot, so it does not draw an empty graph
    // frame: the call-to-action card is the whole surface.
    const bool boxedFace=type!=SkellPresetAudioProcessor::Empty;
    if(boxedFace) {
        g.setGradientFill(juce::ColourGradient(
            Theme::plot,plot.getTopLeft(),
            echoFace?Theme::bg:Theme::panelAlt.darker(.44f),plot.getBottomRight(),false));
        g.fillRoundedRectangle(plot,6.f);
        g.setColour(familyAccent.withAlpha(.31f));
        g.drawRoundedRectangle(plot.reduced(.6f),6.f,1.1f);
    }
    // Distinct instrument families use the same precise clickable graph
    // bounds, but restrained different structural accents.
    g.setColour(familyAccent.withAlpha(.35f));
    g.fillRoundedRectangle(plot.getX()+2.f,plot.getY()+12.f,
                           2.3f,plot.getHeight()-24.f,1.f);
    g.saveState();g.reduceClipRegion(plot.toNearestInt());
    constexpr float frequencies[]{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f};
    const bool emptyFace=type==SkellPresetAudioProcessor::Empty;
    if(eqFace) {
        // Actual pre/post spectrum behind editable response nodes; graph
        // gestures remain on the same coordinate system.
        livePanel.paintEqSpectrum(g,plot);
        for(float freq:frequencies){
            const auto x=plot.getX()+plot.getWidth()*std::log(freq/20.f)/std::log(1000.f);
            g.setColour(Theme::stroke.withAlpha(.48f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());
        }
    } else if(!echoFace && !emptyFace) {
        for(int i=0;i<=8;i++) {
            const auto x=plot.getX()+plot.getWidth()*i/8.f;
            g.setColour(Theme::stroke.withAlpha(.75f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());
        }
    }
    if(!echoFace && !emptyFace && type!=SkellPresetAudioProcessor::DeEsser)for(int i=0;i<=6;i++){
        const auto y=plot.getY()+plot.getHeight()*i/6.f;
        g.setColour(i==3 && eqFace ?Theme::muted.withAlpha(.65f):
            Theme::stroke.withAlpha(eqFace?.48f:.72f));
        g.drawHorizontalLine((int)y,plot.getX(),plot.getRight());
    }
    if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
        if(matchPage==2) {
            // Exact static target-gain function from AlchemistReferenceContour,
            // drawn versus incoming RMS level. This is neither a spectrum
            // analyser nor a generated waveform: dynamic attack/release
            // and the 14.5kHz shelf remain separate causal processing.
            const bool enabled=processor.getSlotParam(slot,8)>.75f;
            const float intensity=juce::jlimit(0.f,1.f,
                (processor.getSlotParam(slot,9)-.5f)*2.f);
            const float air=juce::jlimit(0.f,1.f,
                (processor.getSlotParam(slot,10)-.5f)*2.f);
            const float baseDb=2.f+4.f*(processor.getSlotParam(slot,11)-.5f);
            const float xLeft=plot.getX()+23.f;
            const float xRight=plot.getRight()-23.f;
            const float baseline=plot.getBottom()-51.f;
            const float scale=plot.getHeight()/13.2f;
            g.setColour(Theme::stroke);
            g.drawHorizontalLine((int)baseline,xLeft,xRight);
            juce::Path targetGain;
            for(int index=0;index<=140;++index) {
                const float db=-60.f+60.f*index/140.f;
                const float speech=juce::jlimit(0.f,1.f,(db+59.f)/13.f);
                const float expected=enabled?juce::jlimit(-1.f,10.5f,
                    (baseDb+.38f*(-20.f-db))*speech*intensity):0.f;
                const float x=xLeft+(xRight-xLeft)*index/140.f;
                const float y=baseline-expected*scale;
                if(index==0)targetGain.startNewSubPath(x,y);
                else targetGain.lineTo(x,y);
            }
            g.setColour(Theme::green);
            g.strokePath(targetGain,juce::PathStrokeType(2.7f));
            Theme::text(g,"REFERENCE CONTOUR / RMS INPUT  -60 TO 0 dBFS",
                        {48,165,680,19},11,Theme::green,true);
            Theme::text(g,enabled?"REFERENCE MATCH ACTIVE":"REFERENCE MATCH OFF",
                        {48,193,460,23},13,
                        enabled?Theme::white:Theme::muted,true);
            Theme::text(g,"LEVEL "+juce::String(intensity*100.f,0)+"%  /  AIR "+
                        juce::String(air*100.f,0)+"%  /  BASE "+
                        juce::String(baseDb,1)+" dB",
                        {48,327,710,19},10,Theme::muted);
            Theme::text(g,"35 ms CAUSAL LEVELER / 14.5k AIR / SOFT PEAK SAFETY",
                        {48,350,760,16},9,Theme::muted);
        } else {
        // This is the ACTUAL static cubic transfer implied by the host
        // parameters, not a pretend live waveform or reference null score.
        const float amount=(processor.getSlotParam(slot,0)-.5f)*.06f;
        const float reference=.05f+.75f*processor.getSlotParam(slot,5);
        const float denom=std::max(.0025f,reference*reference);
        const float trim=std::pow(10.f,
            (-1.f+2.f*processor.getSlotParam(slot,6))/20.f);
        juce::Path neutral,curve;
        for(int n=0;n<=180;++n) {
            const float x=-.42f+.84f*(float)n/180.f;
            const float shaped=(x+amount*x*x*x/denom)*trim;
            const float px=plot.getX()+plot.getWidth()*(float)n/180.f;
            const float lineY=plot.getCentreY()-x*plot.getHeight()*.91f;
            const float shapedY=plot.getCentreY()
                -std::clamp(shaped,-.56f,.56f)*plot.getHeight()*.91f;
            if(n==0) {
                neutral.startNewSubPath(px,lineY);
                curve.startNewSubPath(px,shapedY);
            } else {
                neutral.lineTo(px,lineY);
                curve.lineTo(px,shapedY);
            }
        }
        g.setColour(Theme::muted.withAlpha(.56f));
        g.strokePath(neutral,juce::PathStrokeType(1.35f));
        g.setColour(Theme::green.withAlpha(.96f));
        g.strokePath(curve,juce::PathStrokeType(2.4f));
        g.setColour(Theme::green.withAlpha(.08f));
        g.fillRoundedRectangle(plot.reduced(13.f),4.f);
        Theme::text(g,"CAUSAL HARMONIC TRANSFER / SOLID = ACTIVE / DIM = CLEAN",
            {56,166,620,19},10,Theme::green,true);
        Theme::text(g,"20 / 120 / 280ms DETECTORS / +/-0.6 dB CONTOUR / 0ms LOOKAHEAD",
            {56,325,700,20},10,Theme::muted);
        }
    }
    if(type==SkellPresetAudioProcessor::SkelldizerFX) {
        // Character pads change real p2 automation; this is not an FFT meter.
        const int mode=skellpreset::dsp::Skelldizer::characterIndex(
            processor.getSlotParam(slot,1));
        Theme::text(g,"SKELLDIZER / ORIGINAL THREE-BAND ENHANCER",
                    {54,161,705,20},12,Theme::green,true);
        for(int k=0;k<4;++k) {
            const juce::Rectangle<float> card(69.f+k*188.f,190.f,170.f,48.f);
            const bool selected=k==mode;
            g.setColour(selected?Theme::green.withAlpha(.20f):Theme::panelAlt);
            g.fillRoundedRectangle(card,7.f);
            g.setColour(selected?Theme::green:Theme::stroke);
            g.drawRoundedRectangle(card,7.f,selected?1.6f:.9f);
            Theme::text(g,skellpreset::dsp::Skelldizer::characterName(k),
                {(int)card.getX()+7,201,156,22},14,
                selected?Theme::green:Theme::white,true,
                juce::Justification::centred);
        }
        constexpr float profiles[4][3]={{1.06f,1.02f,.96f},
                                         {1.19f,1.03f,.77f},
                                         {.82f,1.02f,1.23f},
                                         {1.03f,1.09f,1.03f}};
        constexpr const char* labels[]={"LOW","MID","HIGH"};
        for(int band=0;band<3;++band) {
            const float x=181.f+234.f*band;
            const float height=24.f+processor.getSlotParam(slot,0)*
                                    std::abs(profiles[mode][band]-1.f)*190.f;
            g.setColour(Theme::stroke.withAlpha(.70f));
            g.fillRoundedRectangle(x,260.f,85.f,68.f,5.f);
            g.setColour(Theme::green.withAlpha(.55f));
            g.fillRoundedRectangle(x,328.f-height,85.f,height,5.f);
            Theme::text(g,labels[band],{(int)x,330,85,19},10,Theme::muted,
                        true,juce::Justification::centred);
        }
    }
    if(type==SkellPresetAudioProcessor::MicModelFX) {
        // Compact Mic Modeling controls INSIDE the existing effect page.
        // Models are generic response profiles, not proprietary brand IRs.
        const int input=skellpreset::dsp::MicModelTone::index(processor.getSlotParam(slot,0));
        const int output=skellpreset::dsp::MicModelTone::index(processor.getSlotParam(slot,1));
        const float strength=processor.getSlotParam(slot,2);
        Theme::text(g,"INPUT MIC",{54,162,210,20},11,Theme::muted,true);
        for(int i=0;i<4;++i) {
            const juce::Rectangle<float> card(52.f+i*91.f,188.f,85.f,33.f);
            const bool chosen=i==input;
            g.setColour(chosen?Theme::green.withAlpha(.17f):Theme::panelAlt);
            g.fillRoundedRectangle(card,5.f);
            g.setColour(chosen?Theme::green:Theme::stroke);
            g.drawRoundedRectangle(card,5.f,chosen?1.55f:.8f);
            Theme::text(g,skellpreset::dsp::MicModelTone::inputName(i),
                {(int)card.getX()+2,195,81,19},10.5f,
                chosen?Theme::green:Theme::white,true,juce::Justification::centred);
        }
        Theme::text(g,"SIMULATE",{54,229,210,20},11,Theme::muted,true);
        for(int i=0;i<4;++i) {
            const int column=i%2,row=i/2;
            const juce::Rectangle<float> card(52.f+column*183.f,
                                              255.f+row*43.f,174.f,37.f);
            const bool chosen=i==output;
            g.setColour(chosen?Theme::green.withAlpha(.16f):Theme::panelAlt);
            g.fillRoundedRectangle(card,5.f);
            g.setColour(chosen?Theme::green:Theme::stroke);
            g.drawRoundedRectangle(card,5.f,chosen?1.55f:.8f);
            g.setColour(chosen?Theme::green:Theme::muted);
            g.fillEllipse(card.getX()+9.f,card.getY()+15.f,6.f,6.f);
            Theme::text(g,skellpreset::dsp::MicModelTone::targetName(i),
                {(int)card.getX()+23,(int)card.getY()+9,139,21},13,
                chosen?Theme::green:Theme::white,true);
        }
        const auto response=skellpreset::dsp::MicModelTone::bandDeltaDb(input,output,strength);
        const float x1=452.f,x2=629.f,x3=840.f,base=262.f,scale=7.5f;
        const float a=base-juce::jlimit(-8.f,8.f,response[0])*scale;
        const float b=base-juce::jlimit(-8.f,8.f,response[1])*scale;
        const float c=base-juce::jlimit(-8.f,8.f,response[2])*scale;
        Theme::text(g,"TONAL PROFILE",{449,167,328,20},12,Theme::green,true);
        g.setColour(Theme::muted.withAlpha(.40f));
        g.drawLine(x1,base,x3,base,1.3f);
        juce::Path curve;
        curve.startNewSubPath(x1,a);
        curve.cubicTo(x1+84.f,a,x2-88.f,b,x2,b);
        curve.cubicTo(x2+99.f,b,x3-91.f,c,x3,c);
        g.setColour(Theme::green.withAlpha(.92f));
        g.strokePath(curve,juce::PathStrokeType(2.7f));
        const float xs[]{x1,x2,x3},ys[]{a,b,c};
        for(int i=0;i<3;++i) {
            g.setColour(Theme::bg);
            g.fillEllipse(xs[i]-4.f,ys[i]-4.f,8.f,8.f);
            g.setColour(Theme::green);
            g.drawEllipse(xs[i]-4.f,ys[i]-4.f,8.f,8.f,1.5f);
        }
        Theme::text(g,"LOW",{442,325,65,17},10,Theme::muted);
        Theme::text(g,"MID",{615,325,65,17},10,Theme::muted);
        Theme::text(g,"AIR",{793,325,50,17},10,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::SkelldizerFX) {
        // Custom character pads occupy the visualization above.
    } else if(type==SkellPresetAudioProcessor::EQ) {
        const float normalizedF=processor.getSlotParam(slot,0);
        const auto* tone=processor.state.getRawParameterValue("macro1");
        const float macroTone=tone?tone->load():.5f;
        const float gain=(processor.getSlotParam(slot,1)-.5f)*24.f+8.f*(macroTone-.5f);
        const float freq=40.f*std::pow(300.f,normalizedF);
        const float x0=(float)(plot.getX()+plot.getWidth()*std::log(freq/20.f)/std::log(1000.f));
        const float y0=plot.getCentreY()-gain*(plot.getHeight()/72.f);
        const double sr=juce::jmax(8000.,processor.getSampleRate());
        const float q=.3f+7.f*processor.getSlotParam(slot,2);
        skellpreset::dsp::Biquad bell,cut;
        bell.setPeak(sr,freq,gain,q);
        cut.setHighPass(sr,20.+processor.getSlotParam(slot,3)*250.);
        juce::Path line;
        for(int px=0;px<=835;px+=3){
            const double f=20.*std::pow(1000.,(double)px/835.);
            const double response=bell.magnitudeDb(f,sr)+cut.magnitudeDb(f,sr);
            const float curveY=juce::jlimit(plot.getY(),plot.getBottom(),
                plot.getCentreY()-(float)response*(plot.getHeight()/72.f));
            if(px==0)line.startNewSubPath(plot.getX(),curveY);
            else line.lineTo(plot.getX()+px,curveY);
        }
        g.setColour(Theme::green.withAlpha(.10f));
        juce::Path filled=line;filled.lineTo(plot.getRight(),plot.getBottom());filled.lineTo(plot.getX(),plot.getBottom());filled.closeSubPath();g.fillPath(filled);
        g.setColour(Theme::blue);g.strokePath(line,juce::PathStrokeType(2.8f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(Theme::blue);g.fillEllipse(x0-6,y0-6,12,12);
        g.setColour(Theme::white);g.drawEllipse(x0-6,y0-6,12,12,1.3f);
        Theme::text(g,juce::String(freq,0)+" Hz  |  "+juce::String(gain,1)+" dB",{540,120,285,20},12,Theme::green,false,juce::Justification::centredRight);
    } else if(type==SkellPresetAudioProcessor::DeEsser) {
        const float cutoff=3500.f+processor.getSlotParam(slot,0)*8000.f;
        const float reduction=2.f+processor.getSlotParam(slot,2)*20.f;
        const double sr=juce::jlimit(8000.,192000.,processor.preparedAudioRate());
        const double actualCutoff=juce::jlimit(1500.,sr*.45,(double)cutoff);
        const double pole=std::exp(-2.*juce::MathConstants<double>::pi*actualCutoff/sr);
        const double gain=skellpreset::dsp::dbToGain(-reduction);
        auto xFor=[&](float hz){return plot.getX()+plot.getWidth()*std::log(hz/20.f)/std::log(1000.f);};
        auto attenuation=[&](double hz){
            const double w=2.*juce::MathConstants<double>::pi*juce::jmin(hz,sr*.5)/sr;
            const double real=1.-pole*std::cos(w),imag=pole*std::sin(w);
            const double denominator=real*real+imag*imag;
            const double lowReal=(1.-pole)*real/denominator,lowImag=-(1.-pole)*imag/denominator;
            return 20.*std::log10(juce::jmax(1.e-8,std::hypot(gain+(1.-gain)*lowReal,(1.-gain)*lowImag)));
        };
        const auto dbY=[&](double db){return plot.getY()+25.f-(float)db*(plot.getHeight()-50.f)/24.f;};
        const auto responseY=[&](double hz){return dbY(attenuation(hz));};
        for(int db=0;db>=-24;db-=6){
            const int y=juce::roundToInt(dbY(db));
            g.setColour(Theme::stroke.withAlpha(db==0?.85f:.48f));
            g.drawHorizontalLine(y,plot.getX(),plot.getRight());
        }
        const float markerX=xFor((float)actualCutoff),markerY=responseY(actualCutoff);
        g.setGradientFill(juce::ColourGradient(Theme::green.withAlpha(.035f),markerX,plot.getY(),
            Theme::green.withAlpha(.14f),plot.getRight(),plot.getY(),false));
        g.fillRect(markerX,plot.getY(),plot.getRight()-markerX,plot.getHeight());
        g.setColour(Theme::amber.withAlpha(.72f));
        g.drawVerticalLine(juce::roundToInt(markerX),plot.getY(),plot.getBottom());
        juce::Path response;
        for(int ix=0;ix<=835;ix+=4){
            const float hz=20.f*std::pow(1000.f,(float)ix/835.f);
            if(ix==0)response.startNewSubPath(plot.getX(),responseY(hz));
            else response.lineTo(plot.getX()+ix,responseY(hz));
        }
        response.lineTo(plot.getRight(),responseY(20000.));
        g.setColour(Theme::green);g.strokePath(response,juce::PathStrokeType(2.5f));
        g.setColour(Theme::bg);g.fillEllipse(markerX-7.f,markerY-7.f,14.f,14.f);
        g.setColour(Theme::green);g.drawEllipse(markerX-7.f,markerY-7.f,14.f,14.f,2.f);
        Theme::text(g,"MAX REDUCTION RESPONSE / LEVEL-DEPENDENT",
            {47,337,700,15},10.f,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX && suiteSpectrumView) {
        // Actual INPUT/OUTPUT spectral values from lock-free audio buffers,
        // analyzed only on JUCE's message/render thread. No invented peaks.
        std::array<float,skellpreset::visual::LiveSpectrumCapture::capacity> input{},output{};
        processor.getLiveSpectrumInput(input);
        processor.getLiveSpectrumOutput(output);
        const double sr=processor.preparedAudioRate();
        const auto inLevels=suiteSpectrumAnalyzer.analyze(input,sr);
        const auto outLevels=suiteSpectrumAnalyzer.analyze(output,sr);
        Theme::text(g,"REAL INPUT / OUTPUT SPECTRUM",
                    {47,162,560,21},13.2f,Theme::green,true);
        for(int marker=0;marker<5;++marker) {
            const float y=207.f+marker*28.f;
            g.setColour(Theme::stroke.withAlpha(.60f));
            g.drawHorizontalLine((int)y,52.f,845.f);
        }
        for(int band=0;band<skellpreset::visual::LiveSpectrumAnalyzer::bands;++band) {
            const float x=55.f+band*(787.f/35.f);
            g.setColour(Theme::stroke.withAlpha(.36f));
            if(band%7==0)g.drawVerticalLine((int)x,209.f,320.f);
        }
        juce::Path before,after;
        for(int band=0;band<skellpreset::visual::LiveSpectrumAnalyzer::bands;++band) {
            const float x=55.f+band*(787.f/35.f);
            const auto toY=[](float magnitude){
                const float db=20.f*std::log10(std::max(magnitude,1.e-6f));
                return 320.f-112.f*juce::jlimit(0.f,1.f,(db+84.f)/84.f);
            };
            const float yIn=toY(inLevels[(size_t)band]);
            const float yOut=toY(outLevels[(size_t)band]);
            if(band==0){before.startNewSubPath(x,yIn);after.startNewSubPath(x,yOut);}
            else {before.lineTo(x,yIn);after.lineTo(x,yOut);}
        }
        g.setColour(Theme::blue.withAlpha(.85f));
        g.strokePath(before,juce::PathStrokeType(1.7f));
        g.setColour(Theme::green);
        g.strokePath(after,juce::PathStrokeType(2.2f));
        // The copper trace is calculated from the EXACT low shelf / mid
        // bell / high shelf design used by the native Suite's realtime DSP.
        // It is a STATIC tonal response, clearly distinct from live spectra.
        const int voice=juce::jlimit(0,2,
            juce::roundToInt(processor.getSlotParam(slot,0)*2.f));
        const auto& profile=skellpreset::dsp::vocalFeelVoices[(size_t)voice];
        skellpreset::dsp::Biquad lowTone,midTone,highTone;
        skellpreset::dsp::vfShelf(lowTone,sr,profile.low,
            (processor.getSlotParam(slot,2)-.5f)*24.f,false);
        midTone.setPeak(sr,profile.mid,
            (processor.getSlotParam(slot,3)-.5f)*24.f,.9);
        skellpreset::dsp::vfShelf(highTone,sr,profile.high,
            (processor.getSlotParam(slot,4)-.5f)*24.f,true);
        juce::Path tone;
        for(int band=0;band<skellpreset::visual::LiveSpectrumAnalyzer::bands;++band) {
            const float x=55.f+band*(787.f/35.f);
            const float hz=skellpreset::visual::LiveSpectrumAnalyzer::frequency(band);
            const double db=lowTone.magnitudeDb(hz,sr)
                           +midTone.magnitudeDb(hz,sr)
                           +highTone.magnitudeDb(hz,sr);
            const float y=juce::jlimit(208.f,320.f,263.f-(float)db*2.25f);
            if(band==0)tone.startNewSubPath(x,y);
            else tone.lineTo(x,y);
        }
        g.setColour(Theme::amber.withAlpha(.90f));
        g.strokePath(tone,juce::PathStrokeType(1.9f));
        // Real dynamically-selected de-esser cutoff; this guide shows
        // where sibilance detection begins, not a fake attenuation trace.
        const float cutoff=3000.f+9000.f*processor.getSlotParam(slot,6);
        const float cutoffX=55.f+787.f*juce::jlimit(0.f,1.f,
            std::log(cutoff/80.f)/std::log(112.5f));
        g.setColour(Theme::blue.withAlpha(.36f));
        g.drawVerticalLine((int)cutoffX,210.f,320.f);
        Theme::text(g,"INPUT",{47,326,96,18},10.3f,Theme::blue,true);
        Theme::text(g,"OUTPUT",{156,326,105,18},10.3f,Theme::green,true);
        Theme::text(g,"EQ CURVE",{276,326,122,18},10.3f,Theme::amber,true);
        Theme::text(g,"DE-ESS REGION",{413,326,175,18},10.3f,Theme::muted,true);
        Theme::text(g,"MEASURED / 80 Hz - 9 kHz",
                    {614,326,233,18},9.6f,Theme::muted,false,
                    juce::Justification::centredRight);
    } else if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
        // Actual per-chain RMS from callback-owned DSP: no invented signal
        // graphics. Dragging a bar uses the original p11..p15 automation ID.
        constexpr const char* returnNames[]{"MAGIC","SPACE","ATTACK","ATTITUDE","PRESENCE"};
        const float compressorGr=juce::jlimit(0.f,48.f,
            processor.getSlotGainReductionDb(slot));
        const float deessGr=juce::jlimit(0.f,24.f,
            processor.getSlotDeessReductionDb(slot));
        Theme::text(g,"VOCAL FEEL  /  FIVE LIVE PARALLEL RETURNS",
                    {47,161,660,22},13.4f,Theme::green,true);
        Theme::text(g,"OPTO  "+juce::String(compressorGr,1)+" dB",
                    {48,185,220,21},11.4f,Theme::white,true);
        Theme::text(g,"DE-ESS  "+juce::String(deessGr,1)+" dB",
                    {300,185,260,21},11.4f,Theme::white,true);
        for(int chain=0;chain<5;++chain) {
            const float x=49.f+chain*163.f;
            const float rms=processor.getSuiteReturnRms(slot,chain);
            const float measured=juce::jlimit(0.f,1.f,
                (skellpreset::dsp::gainToDb(rms)+55.f)/55.f);
            const float level=processor.getSlotParam(slot,10+chain);
            g.setColour(Theme::panelAlt);
            g.fillRoundedRectangle({x,220.f,145.f,69.f},5.f);
            g.setColour(Theme::stroke);
            g.fillRoundedRectangle({x+12.f,239.f,120.f,10.f},4.f);
            g.setColour(Theme::green.withAlpha(.82f));
            if(measured>0.f)
                g.fillRoundedRectangle({x+12.f,239.f,120.f*measured,10.f},4.f);
            Theme::fittedText(g,returnNames[chain],
                {(int)x+9,262,126,19},10.6f,Theme::white,true,
                juce::Justification::centred);
            Theme::text(g,juce::String(level*10.f,1),
                {(int)x+49,294,50,18},11,Theme::green,true,
                juce::Justification::centred);
        }
        Theme::text(g,"LIVE RETURN RMS / DRAG A COLUMN VERTICALLY TO CHANGE ITS MIX",
                    {45,326,780,17},10.5f,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::VocalFeelOptoFX) {
        // Measured gain reduction; sampled from the actual per-slot engine.
        // No fake FFT or imagined loudness score.
        const float gr=juce::jlimit(0.f,24.f,processor.getSlotGainReductionDb(slot));
        const float x=64.f,y=242.f,width=730.f;
        const float proportion=gr/24.f;
        Theme::text(g,"VOCAL FEEL  /  LINKED OPTO DETECTOR",
                    {48,165,680,22},14,Theme::green,true);
        Theme::text(g,"GAIN REDUCTION  "+juce::String(gr,1)+" dB",
                    {49,198,630,25},15,Theme::white,true);
        g.setColour(Theme::stroke);
        g.fillRoundedRectangle({x,y,width,24.f},5.f);
        g.setColour(Theme::green.withAlpha(.90f));
        g.fillRoundedRectangle({x,y,width*proportion,24.f},5.f);
        for(int tick=1;tick<6;++tick) {
            const float xx=x+width*(float)tick/6.f;
            g.setColour(Theme::white.withAlpha(.13f));
            g.drawVerticalLine((int)xx,y+1.f,y+23.f);
        }
        Theme::text(g,"0 dB",{61,275,95,18},10.7f,Theme::muted);
        Theme::text(g,"24 dB",{732,275,69,18},10.7f,Theme::muted,
                    false,juce::Justification::centredRight);
        Theme::text(g,"VOICE-KEYED  /  FLAT · WARM · TUBE  /  ZERO ADDED LATENCY",
                    {49,326,770,18},10,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::Compressor){
        // Steady-state input/output transfer, derived from compressor controls.
        const auto* punch=processor.state.getRawParameterValue("macro2");
        const float punchValue=punch?punch->load():.5f;
        const float threshold=-48.f+processor.getSlotParam(slot,0)*42.f-12.f*(punchValue-.5f);
        const float ratio=1.f+processor.getSlotParam(slot,1)*11.f;
        auto levelToY=[&](float db){return plot.getBottom()-(db+60.f)/60.f*plot.getHeight();};
        juce::Path unity,compressed;
        for(int x=0;x<=835;x+=5){
            const float inDb=-60.f+60.f*x/835.f;
            const float outDb=inDb>threshold?threshold+(inDb-threshold)/ratio:inDb;
            if(x==0){unity.startNewSubPath(plot.getX(),levelToY(inDb));compressed.startNewSubPath(plot.getX(),levelToY(outDb));}
            else {unity.lineTo(plot.getX()+x,levelToY(inDb));compressed.lineTo(plot.getX()+x,levelToY(outDb));}
        }
        g.setColour(Theme::muted.withAlpha(.38f));g.strokePath(unity,juce::PathStrokeType(1.4f));
        g.setColour(Theme::green);g.strokePath(compressed,juce::PathStrokeType(2.6f));
        const float tx=plot.getX()+(threshold+60.f)/60.f*plot.getWidth();
        g.setColour(Theme::amber.withAlpha(.7f));g.drawVerticalLine((int)tx,plot.getY(),plot.getBottom());
        Theme::text(g,"STEADY-STATE TRANSFER  /  "+juce::String(threshold,1)+" dB  /  "+juce::String(ratio,1)+":1",
                    {45,163,670,18},11,Theme::muted,true);
    } else if(type==SkellPresetAudioProcessor::StudioCompressorFX){
        // The same static mapping as the adapted Studio RMS compressor.
        // The real gain also depends on its time-smoothed RMS detector.
        const float p0=processor.getSlotParam(slot,0);
        const float p1=processor.getSlotParam(slot,1);
        const float p4=processor.getSlotParam(slot,4);
        const float thresholdAmp=skellpreset::studio::detail::level(
            juce::roundToInt(p0*1000.f),7.);
        const float threshold=20.f*std::log10(juce::jmax(.001f,thresholdAmp));
        const float ratio=1.f+.38f*(std::pow(51.f,p1)-1.f);
        const float slope=1.f/ratio-1.f;
        const float kneeA=(std::pow(201.f,p4)-1.f)*.005f;
        const float kneeB=kneeA>1.e-6f?1.f/std::log1p(kneeA):0.f;
        auto toY=[&](float db){return plot.getBottom()-(db+60.f)/60.f*plot.getHeight();};
        juce::Path unity,compressed;
        for(int ix=0;ix<=835;ix+=5) {
            const float inDb=-60.f+60.f*ix/835.f;
            const float over=juce::jmax(0.f,inDb-threshold);
            const float grDb=over<=0.f?0.f:
                kneeA>1.e-5f?
                ((std::log1p((over+1.f)*kneeA)*kneeB-1.f)*(slope+1.f)-over):
                over*slope;
            const float x=plot.getX()+ix;
            if(ix==0) {
                unity.startNewSubPath(x,toY(inDb));
                compressed.startNewSubPath(x,toY(inDb+grDb));
            } else {
                unity.lineTo(x,toY(inDb));
                compressed.lineTo(x,toY(inDb+grDb));
            }
        }
        g.setColour(Theme::muted.withAlpha(.38f));
        g.strokePath(unity,juce::PathStrokeType(1.3f));
        g.setColour(Theme::green);
        g.strokePath(compressed,juce::PathStrokeType(2.6f));
        const float tx=plot.getX()+plot.getWidth()*(threshold+60.f)/60.f;
        g.setColour(Theme::amber.withAlpha(.72f));
        g.drawVerticalLine((int)tx,plot.getY(),plot.getBottom());
        Theme::text(g,"STUDIO RMS COMPRESSION  /  "+juce::String(threshold,1)+" dBFS",
                    {45,163,700,20},12,Theme::green,true);
        Theme::text(g,"STATIC CURVE / REAL GAIN FOLLOWS ATTACK + RELEASE",
                    {43,327,790,17},11,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::Gate){
        const float threshold=-75.f+processor.getSlotParam(slot,0)*70.f;
        const float hysteresis=processor.getSlotParam(slot,3)*8.f;
        const float hold=processor.getSlotParam(slot,2)*120.f;
        const auto toX=[&](float dB){return plot.getX()+plot.getWidth()*juce::jlimit(0.f,1.f,(dB+80.f)/80.f);};
        const float closeX=toX(threshold-hysteresis),openX=toX(threshold);
        g.setColour(Theme::green.withAlpha(.09f));g.fillRect(openX,plot.getY(),plot.getRight()-openX,plot.getHeight());
        g.setColour(Theme::amber.withAlpha(.20f));g.fillRect(closeX,plot.getY(),openX-closeX,plot.getHeight());
        g.setColour(Theme::blue);g.drawVerticalLine((int)closeX,plot.getY(),plot.getBottom());
        g.setColour(Theme::green);g.drawVerticalLine((int)openX,plot.getY(),plot.getBottom());
        Theme::text(g,"OPEN  "+juce::String(threshold,1)+" dB",{45,163,270,20},12,Theme::green,true);
        Theme::text(g,"CLOSE  "+juce::String(threshold-hysteresis,1)+" dB",{45,187,270,20},12,Theme::muted);
        Theme::text(g,"HOLD  "+juce::String(hold,0)+" ms",{45,211,270,20},12,Theme::muted);
        Theme::text(g,"HYSTERESIS THRESHOLDS  /  INPUT LEVEL -80 TO 0 dB",{42,327,700,17},10,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::AdlibThrow){
        // A phrase-end/retrigger timing diagram, deliberately distinct from
        // the ordinary Delay's five vertical tap bars. This illustrates
        // algorithm state; it does not pretend to be a live audio waveform.
        const float seconds=.16f+.48f*processor.getSlotParam(slot,0);
        const float feedback=.2f+.68f*processor.getSlotParam(slot,1);
        const bool manual=processor.getSlotParam(slot,3)>.65f;
        const float boundary=plot.getX()+400.f;
        g.setColour(Theme::blue.withAlpha(.13f));
        g.fillRoundedRectangle(plot.getX()+30.f,plot.getY()+38.f,360.f,123.f,8.f);
        for(int pulse=0;pulse<9;++pulse) {
            const float px=plot.getX()+54.f+pulse*34.f;
            const float height=24.f+18.f*(1.f+std::sin((float)pulse*1.48f));
            g.setColour(Theme::blue.withAlpha(.30f+.04f*pulse));
            g.fillRoundedRectangle(px,plot.getCentreY()-height*.5f,
                                   14.f,height,3.f);
        }
        g.setColour(Theme::amber);
        g.drawVerticalLine((int)boundary,plot.getY()+34.f,plot.getBottom()-18.f);
        const float baseY=plot.getCentreY()+11.f;
        for(int echo=0;echo<4;++echo){
            const float strength=std::pow(feedback,(float)echo);
            const float cx=plot.getX()+470.f+echo*97.f;
            const float height=12.f+32.f*strength;
            const float width=14.f+23.f*strength;
            // Signed echo slices, not circular orbit animation.
            juce::Path slice;
            slice.startNewSubPath(cx-width*.5f,baseY);
            slice.lineTo(cx,baseY-height);
            slice.lineTo(cx+width*.5f,baseY);
            slice.lineTo(cx,baseY+height);
            slice.closeSubPath();
            g.setColour(Theme::green.withAlpha(.17f+.24f*strength));
            g.fillPath(slice);
            g.setColour(Theme::green.withAlpha(.28f+.58f*strength));
            g.strokePath(slice,juce::PathStrokeType(1.8f));
        }
        Theme::text(g,"PHRASE CAPTURE",{70,172,350,18},12,Theme::blue,true);
        Theme::text(g,manual?"MANUAL THROW / LIVE TRIGGER":
                        "AUTO PHRASE-END THROW",
            {477,172,360,18},12,Theme::green,true);
        Theme::text(g,juce::String(seconds*1000.f,0)+" ms / FEEDBACK "+
                       juce::String(feedback*100.f,0)+"%",
            {465,305,360,20},12,Theme::white);
        Theme::text(g,"TIMING DIAGRAM / NOT A LIVE WAVEFORM",
            {60,326,550,17},10,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::Delay){
        const float seconds=.055f+.85f*processor.getSlotParam(slot,0);
        const float feedback=.8f*processor.getSlotParam(slot,1);
        const float signal=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
        const float centre=plot.getCentreY()+12.f;
        g.setColour(Theme::stroke);
        g.drawLine(plot.getX()+28.f,centre,plot.getRight()-28.f,centre,1.f);
        // Echo-envelope illustration derived from the real delay settings
        // and measured rack slot peak. No fabricated live audio waveform.
        const float spacing=132.f+30.f*processor.getSlotParam(slot,0);
        for(int tap=0;tap<5;++tap) {
            const float decay=std::pow(feedback,(float)tap);
            const float x0=plot.getX()+46.f+(float)tap*spacing;
            const float amp=juce::jmax(2.6f,72.f*decay*(.65f+.35f*signal));
            juce::Path shape;
            for(int i=0;i<=54;++i) {
                const float phase=(float)i/54.f;
                const float x=x0+105.f*phase;
                const float envelope=std::sin(phase*juce::MathConstants<float>::pi);
                const float y=centre-std::sin(phase*juce::MathConstants<float>::twoPi*5.f)
                                      *envelope*amp;
                if(i==0)shape.startNewSubPath(x,y);
                else shape.lineTo(x,y);
            }
            g.setColour(Theme::green.withAlpha(.26f+.68f*decay));
            g.strokePath(shape,juce::PathStrokeType(2.f));
            Theme::text(g,juce::String(tap+1),
                {(int)x0+39,(int)plot.getBottom()-24,25,15},9,Theme::muted,
                false,juce::Justification::centred);
        }
        Theme::text(g,"DELAY  "+juce::String(seconds*1000.f,0)+
                      " ms / FEEDBACK "+juce::String(feedback*100.f,0)+"%",
                    {45,163,700,20},12,Theme::green,true);
        Theme::text(g,"ECHO ENVELOPE  /  LIVE PEAK MODULATES BRIGHTNESS",
                    {43,326,690,17},10,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::StudioDelayFX) {
        // Illustrative time-domain echoes only: not a fabricated audio scope.
        const float seconds=.025f*std::pow(80.f,processor.getSlotParam(slot,0));
        const float feedback=.94f*processor.getSlotParam(slot,1);
        const float wet=processor.getSlotParam(slot,10);
        const float dry=processor.getSlotParam(slot,11);
        const bool sync=processor.getSlotParam(slot,12)>=.5f;
        constexpr const char* beatLabels[]{"1/1","1/2","1/4","1/8",
            "1/16","1/32","1/64","1/128"};
        const int division=juce::jlimit(0,7,(int)(processor.getSlotParam(slot,13)*7.999f));
        const int mode=juce::jlimit(0,3,(int)(processor.getSlotParam(slot,2)*3.999f));
        const juce::StringArray modes{"MONO","TRUE STEREO","PING PONG","OFF"};
        const float baseline=plot.getCentreY()+18.f;
        const float left=plot.getX()+52.f,usable=plot.getWidth()-104.f;
        g.setColour(Theme::stroke.withAlpha(.64f));
        g.drawLine(left,baseline,left+usable,baseline,.9f);
        // Compact echo trains use the actual feedback and wet controls.
        // The envelopes describe the settings, not fabricated audio.
        for(int tap=0;tap<6;++tap){
            const float centre=left+(tap+.5f)*usable/6.f;
            const float magnitude=tap==0?dry:mode==3?0.f:
                wet*std::pow(feedback,float(tap-1));
            const float span=usable/6.f-12.f;
            for(int i=0;i<29;++i){
                const float t=float(i)/28.f;
                const float x=centre-span*.5f+t*span;
                const float height=50.f*magnitude*std::sin(t*juce::MathConstants<float>::pi);
                const auto ink=mode==2 && (tap&1)?Theme::blue:Theme::green;
                g.setColour(ink.withAlpha(.20f+.63f*magnitude));
                g.drawLine(x,baseline-height,x,baseline+height,1.6f);
            }
        }
        Theme::text(g,sync?juce::String(beatLabels[division]):juce::String(seconds*1000.f,0)+" ms",
            {320,165,260,40},29,Theme::white,true,juce::Justification::centred);
        Theme::text(g,sync?"HOST TEMPO":"FREE TIME",{320,205,260,16},10,Theme::muted,true,
            juce::Justification::centred);
        Theme::text(g,modes[mode],{47,170,240,20},12,Theme::green,true);
        Theme::text(g,"FEEDBACK  "+juce::String(feedback*100.f,0)+"%",
            {600,170,236,20},12,Theme::green,true,juce::Justification::centredRight);
        Theme::text(g,mode==3?"ECHO DISABLED":sync?"SYNCED ECHO ENVELOPE":"FREE-TIME ECHO ENVELOPE",
            {250,328,405,16},10,Theme::muted,false,juce::Justification::centred);
    } else if(type==SkellPresetAudioProcessor::ExternalVST3) {
        const auto name=processor.hostedVST3Name(slot);
        Theme::text(g,"EXTERNAL VST3 INSERT",{80,185,680,31},25,Theme::green,true);
        Theme::text(g,name.isEmpty()?"NO PLUGIN LOADED":name.toUpperCase(),
            {81,225,720,34},17,Theme::white,true);
        Theme::text(g,name.isEmpty()?"Choose LOAD VST3 below to select a plug-in.":
            (embeddedEditor?"HOSTED EDITOR EMBEDDED / OPEN IN WINDOW TO DETACH":
                            "OPEN IN WINDOW IF THE PLUGIN HAS ITS OWN EDITOR"),
            {83,270,720,23},13,Theme::muted);
        Theme::text(g,"IN-PROCESS HOST  /  THIRD-PARTY CODE RUNS INSIDE YOUR DAW",
            {85,313,735,20},11,Theme::amber);
        const int samples=processor.hostedVST3LatencySamples(slot);
        const double sampleRate=juce::jmax(8000.,processor.preparedAudioRate());
        const double milliseconds=1000.0*(double)samples/sampleRate;
        const bool highLatency=milliseconds>5.0;
        const auto latencyText=name.isEmpty()
            ? juce::String("NO HOSTED PDC / LOAD AN EFFECT TO MEASURE")
            : juce::String("REPORTED PDC  /  ")+juce::String(samples)
                  +" SAMPLES  /  "+juce::String(milliseconds,2)+" MS";
        Theme::text(g,latencyText,{85,343,735,22},12,
                    highLatency?Theme::amber:Theme::green,true);
    } else if(type!=SkellPresetAudioProcessor::Empty){
        // Parameter-driven visualizations. These are not fabricated FFT meters:
        // the user can drag the shared edit point to change P1/P2.
        const float first=processor.getSlotParam(slot,0);
        const float second=processor.getSlotParam(slot,1);
        const float x0=plot.getX()+24.f, x1=plot.getRight()-24.f;
        const float y0=plot.getY()+20.f, y1=plot.getBottom()-27.f;
        const float w=x1-x0, h=y1-y0;
        if(isModulationOrbEffect(type) && modOrbView) {
            // Digital Bones is the only alternate modulation visualization.
            // Each vertebra follows a REAL normalized control value; its
            // illumination follows measured post-FX peak, not fake FFT/LFO
            // phase. No orbit rings, globe ellipses, or generated movement.
            const auto axes=titles(type);
            const float peak=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
            const float left=130.f,top=196.f,spacing=170.f;
            for(int control=0;control<4;++control) {
                const float value=juce::jlimit(0.f,1.f,
                    processor.getSlotParam(slot,control));
                const float centre=left+control*spacing;
                Theme::fittedText(g,axes[(size_t)control].toUpperCase(),
                    {(int)(centre-68.f),167,136,20},10.8f,Theme::muted,
                    true,juce::Justification::centred);
                for(int vertebra=0;vertebra<13;++vertebra) {
                    const float y=top+vertebra*10.9f;
                    const bool filled=(float)(12-vertebra)/12.f<=value;
                    const float span=13.f+4.f*std::sin((float)vertebra*.35f);
                    const auto ink=filled?
                        Theme::green.withAlpha(.35f+.55f*peak):
                        Theme::stroke.withAlpha(.70f);
                    g.setColour(Theme::panelAlt);
                    g.fillRoundedRectangle(centre-span-6.f,y-1.f,
                                           (span+6.f)*2.f,8.7f,3.f);
                    g.setColour(ink);
                    g.fillRoundedRectangle(centre-span,y,span*2.f,5.7f,2.f);
                    g.fillRoundedRectangle(centre-span-4.f,y+1.f,5.f,5.f,2.f);
                    g.fillRoundedRectangle(centre+span-1.f,y+1.f,5.f,5.f,2.f);
                }
                Theme::text(g,juce::String(juce::roundToInt(value*100.f))+"%",
                    {(int)(centre-58.f),326,116,17},10.5f,Theme::white,true,
                    juce::Justification::centred);
            }
            Theme::text(g,"DIGITAL BONES  /  CONTROL VALUES + REAL FX PEAK",
                        {50,342,700,13},10.f,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::StudioEQFX) {
            // Use a separate UI-only instance of the same seven-band plain EQ.
            // Never read the filter states owned by the audio thread.
            skellpreset::studio::ParametricEQ eq;
            const double sr=juce::jmax(8000.,processor.preparedAudioRate());
            eq.prepare(sr);
            std::array<float,16> p{};
            std::array<float,8> q{};
            std::array<int,8> kinds{};
            std::array<bool,8> enabled{};
            for(int i=0;i<16;++i)p[(size_t)i]=processor.getSlotParam(slot,i);
            for(int i=0;i<8;++i) {
                const auto* raw=processor.state.getRawParameterValue(
                    "s"+juce::String(slot+1)+"_eqQ"+juce::String(i+1));
                q[(size_t)i]=raw?raw->load():.5f;
                kinds[(size_t)i]=studioEqBandType(i);
                enabled[(size_t)i]=studioEqBandEnabled(i);
            }
            eq.set(p,q,kinds,enabled);
            juce::Path response;
            for(int px=0;px<=835;px+=5) {
                const double f=20.*std::pow(1000.,(double)px/835.);
                const float db=(float)eq.magnitudeDb(f);
                const float y=juce::jlimit((float)plot.getY(),(float)plot.getBottom(),
                    (float)plot.getCentreY()-db*(plot.getHeight()/72.f));
                if(px==0)response.startNewSubPath((float)plot.getX(),y);
                else response.lineTo((float)plot.getX()+px,y);
            }
            g.setColour(Theme::green.withAlpha(.11f));
            auto area=response;
            area.lineTo((float)plot.getRight(),(float)plot.getBottom());
            area.lineTo((float)plot.getX(),(float)plot.getBottom());
            area.closeSubPath();g.fillPath(area);
            g.setColour(Theme::green.withAlpha(.18f));
            g.strokePath(response,juce::PathStrokeType(6.f));
            g.setColour(Theme::white);g.strokePath(response,juce::PathStrokeType(1.8f));
            for(int band=0;band<8;++band){
                if(!enabled[(size_t)band])continue;
                const float hz=eqVisualFrequency(processor,slot,band,type);
                const int shape=kinds[(size_t)band];
                const float db=(shape==1||shape==2)?0.f:
                    (processor.getSlotParam(slot,band*2+1)-.5f)*36.f;
                const float xx=(float)plot.getX()+
                    (float)(plot.getWidth()*std::log(hz/20.f)/std::log(1000.f));
                const float yy=plot.getCentreY()-db*(plot.getHeight()/72.f);
                const bool active=band==activeEqBand;
                const float size=active?19.f:15.f;
                const auto bandColour=active?Theme::green:Theme::blue.withAlpha(.83f);
                g.setColour(bandColour.withAlpha(.13f));
                g.fillEllipse(xx-size*.7f,yy-size*.7f,size*1.4f,size*1.4f);
                g.setColour(Theme::bg);
                g.fillEllipse(xx-size*.5f,yy-size*.5f,size,size);
                g.setColour(bandColour);
                g.drawEllipse(xx-size*.5f,yy-size*.5f,size,size,1.25f);
                Theme::text(g,juce::String(band+1),
                    {juce::roundToInt(xx)-9,juce::roundToInt(yy)-9,18,18},
                    9.f,bandColour,true,juce::Justification::centred);
                if(shape==1||shape==2)
                    Theme::text(g,shape==1?"HP":"LP",
                        {(int)(xx-15),(int)(yy-21),30,14},9,
                        Theme::amber,true,juce::Justification::centred);
            }
        } else if(type==SkellPresetAudioProcessor::EightBandToneFX) {
            // Exact, summed eight-band DSP response. Every dot edits the
            // actual frequency+gain parameters of one independent bell.
            std::array<skellpreset::dsp::Biquad,8> filter{};
            const double sr=juce::jmax(8000.,processor.preparedAudioRate());
            auto xFor=[&](double freq)->float {
                return plot.getX()+(float)(plot.getWidth()*std::log(freq/20.)/std::log(1000.));
            };
            juce::Path response;
            for(int band=0;band<8;++band) {
                const float low=skellpreset::dsp::EightBandTone::edges[(size_t)band];
                const float high=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
                const float freq=low*std::pow(high/low,processor.getSlotParam(slot,2*band));
                const float gain=(processor.getSlotParam(slot,2*band+1)-.5f)*30.f;
                const auto* qValue=processor.state.getRawParameterValue(
                    "s"+juce::String(slot+1)+"_eqQ"+juce::String(band+1));
                const float q=skellpreset::dsp::EightBandTone::qFromNormalized(
                    qValue?qValue->load():.5f);
                filter[(size_t)band].setPeak(sr,freq,gain,q);
            }
            skellpreset::dsp::Biquad lowCutFilter,highCutFilter;
            const auto* loParam=processor.state.getRawParameterValue(
                "s"+juce::String(slot+1)+"_eqLowCut");
            const auto* hiParam=processor.state.getRawParameterValue(
                "s"+juce::String(slot+1)+"_eqHighCut");
            const float lo=loParam?loParam->load():0.f;
            const float hi=hiParam?hiParam->load():0.f;
            if(lo>.0001f)lowCutFilter.setHighPass(sr,
                skellpreset::dsp::EightBandTone::lowCutFrequency(lo));
            if(hi>.0001f)highCutFilter.setLowPass(sr,
                skellpreset::dsp::EightBandTone::highCutFrequency(hi));
            for(int px=0;px<=835;px+=5) {
                const double freq=20.*std::pow(1000.,(double)px/835.);
                double dB=lowCutFilter.magnitudeDb(freq,sr)
                          +highCutFilter.magnitudeDb(freq,sr);
                for(const auto& band:filter)dB+=band.magnitudeDb(freq,sr);
                const float curveY=juce::jlimit(plot.getY(),plot.getBottom(),
                    plot.getCentreY()-(float)dB*(plot.getHeight()/72.f));
                if(px==0)response.startNewSubPath(plot.getX(),curveY);
                else response.lineTo(plot.getX()+px,curveY);
            }
            g.setColour(Theme::green.withAlpha(.11f));
            auto area=response;area.lineTo(plot.getRight(),plot.getBottom());
            area.lineTo(plot.getX(),plot.getBottom());area.closeSubPath();g.fillPath(area);
            g.setColour(Theme::green);g.strokePath(response,juce::PathStrokeType(2.7f));
            // Readable cut-frequency markers, independent of all bell handles.
            for(int side=0;side<2;++side){
                const float amount=side==0?lo:hi;
                if(amount<=.0001f)continue;
                const float hz=side==0?
                    skellpreset::dsp::EightBandTone::lowCutFrequency(amount):
                    skellpreset::dsp::EightBandTone::highCutFrequency(amount);
                const float x=xFor(hz);
                g.setColour(Theme::amber.withAlpha(.65f));
                g.drawVerticalLine((int)x,plot.getY()+3.f,plot.getBottom()-3.f);
                g.setColour(Theme::amber);
                g.fillEllipse(x-3.f,plot.getY()+2.f,6.f,6.f);
            }
            for(int band=0;band<8;++band) {
                const float low=skellpreset::dsp::EightBandTone::edges[(size_t)band];
                const float high=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
                const float f=low*std::pow(high/low,processor.getSlotParam(slot,band*2));
                const float db=(processor.getSlotParam(slot,band*2+1)-.5f)*30.f;
                const float x=xFor(f);
                const float y=plot.getCentreY()-db*(plot.getHeight()/72.f);
                const bool active=band>=eqBank && band<eqBank+2;
                const float size=active?14.f:10.f;
                g.setColour(active?Theme::green:Theme::blue.withAlpha(.82f));
                g.fillEllipse(x-size*.5f,y-size*.5f,size,size);
                g.setColour(Theme::white.withAlpha(active?.95f:.58f));
                g.drawEllipse(x-size*.5f,y-size*.5f,size,size,1.3f);
            }
        } else if(type==SkellPresetAudioProcessor::StudioPitchFormantFX) {
            const float pitch=std::pow(2.f,
                (processor.getSlotParam(slot,0)-.5f)*2.f*
                (3.f+9.f*processor.getSlotParam(slot,2))/12.f);
            const float formant=std::pow(2.f,
                (processor.getSlotParam(slot,1)-.5f)*2.f*
                (2.f+10.f*processor.getSlotParam(slot,3))/12.f);
            for(int channel=0;channel<2;++channel) {
                juce::Path wave;
                for(int i=0;i<=150;++i){
                    const float t=(float)i/150.f;
                    const float phase=t*36.f*pitch+(channel?2.3f:0.f);
                    const float envelope=(.23f+.06f*std::sin(t*12.f*formant));
                    const float y=plot.getCentreY()+std::sin(phase)*h*envelope+
                                  (channel?20.f:-20.f);
                    const float x=x0+t*w;
                    if(i==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);
                }
                g.setColour(channel?Theme::blue:Theme::green);
                g.strokePath(wave,juce::PathStrokeType(channel==0?2.6f:1.9f));
            }
            Theme::text(g,"STEREO PHASE VOCODER  /  STUDIO QUALITY MODE",
                {83,168,702,19},11,Theme::muted,true);
        } else if(type==SkellPresetAudioProcessor::SerialDynamicsFX) {
            const float threshold=-48.f+42.f*first;
            const float ratio=2.f+10.f*second;
            juce::Path fet,opto;
            for(int i=0;i<=120;++i) {
                const float db=-60.f+60.f*i/120.f;
                const float over=std::max(0.f,db-threshold);
                const float outFet=db-over*(1.f-1.f/ratio)*.70f;
                const float outOpto=outFet-std::max(0.f,outFet-threshold+3.f)*.43f;
                const float x=x0+w*i/120.f;
                const float yyF=y1-(outFet+60.f)/60.f*h;
                const float yyO=y1-(outOpto+60.f)/60.f*h;
                if(i==0){fet.startNewSubPath(x,yyF);opto.startNewSubPath(x,yyO);}
                else {fet.lineTo(x,yyF);opto.lineTo(x,yyO);}
            }
            g.setColour(Theme::muted.withAlpha(.45f));
            g.drawLine(x0,y1,x1,y0,1.1f);
            g.setColour(Theme::blue);g.strokePath(fet,juce::PathStrokeType(2.2f));
            g.setColour(Theme::green);g.strokePath(opto,juce::PathStrokeType(2.8f));
            Theme::text(g,"FET PEAK", {85,169,130,18},10,Theme::blue,true);
            Theme::text(g,"OPTO RMS",{220,169,150,18},10,Theme::green,true);
        } else if(type==SkellPresetAudioProcessor::DynamicHarshnessFX) {
            const float freq=1800.f*std::pow(4.2f,first);
            const float maxCut=10.f*processor.getSlotParam(slot,2);
            skellpreset::dsp::Biquad band;
            const double sr=juce::jmax(8000.,processor.preparedAudioRate());
            band.setPeak(sr,freq,-maxCut,2.3);
            juce::Path curve;
            for(int px=0;px<=835;px+=5){
                const double f=20.*std::pow(1000.,(double)px/835.);
                const float db=(float)band.magnitudeDb(f,sr);
                const float y=juce::jlimit(plot.getY(),plot.getBottom(),
                    plot.getCentreY()-db*(plot.getHeight()/38.f));
                if(px==0)curve.startNewSubPath(plot.getX(),y);
                else curve.lineTo(plot.getX()+px,y);
            }
            g.setColour(Theme::green);g.strokePath(curve,juce::PathStrokeType(2.7f));
            const float nodeX=plot.getX()+plot.getWidth()*std::log(freq/20.f)/std::log(1000.f);
            g.setColour(Theme::amber);g.fillEllipse(nodeX-5.f,plot.getCentreY()-5.f,10.f,10.f);
            Theme::text(g,"DYNAMIC CUT  /  WHEN RESONANCE EXCEEDS THRESHOLD",
                {80,170,710,19},11,Theme::muted,true);
        } else if(type==SkellPresetAudioProcessor::AdaptiveNoiseFX) {
            const float threshold=-75.f+55.f*first;
            const float reduction=36.f*processor.getSlotParam(slot,1);
            juce::Path curve;
            const float thresholdX=x0+w*(threshold+80.f)/80.f;
            for(int i=0;i<=130;++i) {
                const float db=-80.f+i*80.f/130.f;
                const float lift=std::clamp((db-threshold+5.f)/10.f,0.f,1.f);
                const float targetDb=db-reduction*(1.f-lift);
                const float x=x0+w*i/130.f;
                const float y=y1-h*(targetDb+116.f)/116.f;
                if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);
            }
            g.setColour(Theme::amber.withAlpha(.75f));g.drawVerticalLine((int)thresholdX,y0,y1);
            g.setColour(Theme::green);g.strokePath(curve,juce::PathStrokeType(2.8f));
            Theme::text(g,"ADAPTIVE FLOOR  /  QUIET INPUT REDUCTION",
                {80,169,690,20},11,Theme::green,true);
        } else if(type==SkellPresetAudioProcessor::VocalChorusFX) {
            // Animated multivoice geometry, not a fabricated FFT or waveform.
            const float width=processor.getSlotParam(slot,3);
            const float depth=second;
            const int voices=juce::jlimit(2,5,(int)std::round(2.f+3.f*processor.getSlotParam(slot,2)));
            for(int v=0;v<voices;++v) {
                juce::Path voice;
                const float offset=2.f*(float)juce::MathConstants<float>::pi*v/voices;
                for(int i=0;i<=170;++i){
                    const float t=(float)i/170.f;
                    const float envelope=std::sin(t*(float)juce::MathConstants<float>::pi);
                    const float motion=std::sin(t*16.f+offset)*
                                       (h*.18f*depth)*envelope;
                    const float posY=plot.getCentreY()+(v-(voices-1)*.5f)*(6.f+width*11.f)+motion;
                    if(i==0)voice.startNewSubPath(x0,posY);
                    else voice.lineTo(x0+t*w,posY);
                }
                g.setColour(v%2==0?Theme::green.withAlpha(.65f+.08f*(v%3)):
                                      Theme::blue.withAlpha(.7f));
                g.strokePath(voice,juce::PathStrokeType(1.8f+v*.16f));
            }
        } else if(type==SkellPresetAudioProcessor::VocalFlangerFX) {
            const float depth=second;
            const float feedback=2.f*processor.getSlotParam(slot,2)-1.f;
            juce::Path swept,phase;
            for(int i=0;i<=180;++i){
                const float t=(float)i/180.f;
                const float carrier=.5f+.5f*std::sin(t*16.f*(.5f+first));
                const float a=plot.getBottom()-20.f-
                              (h-46.f)*(.1f+depth*.78f*carrier);
                const float b=plot.getCentreY()-
                              feedback*(h*.19f)*std::sin(t*22.f);
                if(i==0){swept.startNewSubPath(x0,a);phase.startNewSubPath(x0,b);}
                else{swept.lineTo(x0+t*w,a);phase.lineTo(x0+t*w,b);}
            }
            g.setColour(Theme::blue.withAlpha(.58f));
            g.strokePath(phase,juce::PathStrokeType(2.f));
            g.setColour(Theme::green);
            g.strokePath(swept,juce::PathStrokeType(3.2f));
        } else if(type==SkellPresetAudioProcessor::RingModFX) {
            const float depth=second,shape=processor.getSlotParam(slot,2);
            const float offset=processor.getSlotParam(slot,3)*2.f;
            for(int ch=0;ch<2;++ch) {
                juce::Path curve;
                for(int i=0;i<=180;++i) {
                    const float t=(float)i/180.f;
                    const float phase=t*(4.f+first*18.f)+(ch?offset:0.f);
                    const float sine=std::sin(phase*(float)juce::MathConstants<float>::twoPi);
                    const float tri=1.f-4.f*std::abs(phase-std::floor(phase)-.5f);
                    const float mod=sine*(1.f-shape)+tri*shape;
                    const float value=(1.f-depth)+depth*mod;
                    const float y=plot.getCentreY()-value*(h*.33f);
                    if(i==0)curve.startNewSubPath(x0,y);
                    else curve.lineTo(x0+t*w,y);
                }
                g.setColour(ch==0?Theme::green:Theme::blue);
                g.strokePath(curve,juce::PathStrokeType(ch==0?2.9f:2.2f));
            }
        } else if(type==SkellPresetAudioProcessor::VoxRackRoom) {
            const float size=first,damp=second;
            const float predelayMs=200.f*processor.getSlotParam(slot,2);
            // Eight comb paths are visualized as decaying reflections, with
            // allpass scatter shown as a smoothed envelope.
            g.setColour(Theme::panelAlt.withAlpha(.8f));
            g.fillRoundedRectangle(x0+24.f,y0+45.f,w-48.f,h-66.f,9.f);
            juce::Path envelope;
            for(int i=0;i<8;++i) {
                const float decay=std::pow(.66f+size*.30f,(float)i);
                const float x=x0+58.f+(float)i*(w-116.f)/7.f;
                const float bar=(.18f+.72f*decay)*(h-65.f);
                g.setColour(i%2?Theme::blue.withAlpha(.65f):Theme::green.withAlpha(.85f));
                g.fillRoundedRectangle(x,y1-9.f-bar,10.f,bar,4.f);
                if(i==0)envelope.startNewSubPath(x+5.f,y1-9.f-bar);
                else envelope.lineTo(x+5.f,y1-9.f-bar);
            }
            g.setColour(Theme::amber.withAlpha(.7f));
            g.strokePath(envelope,juce::PathStrokeType(2.f));
            Theme::text(g,"VOXRACK ROOM / DAMPED COMB + ALLPASS",
                        {60,102,720,23},14,Theme::green,true);
            Theme::text(g,juce::String(predelayMs,0)+" ms PREDELAY / "+
                        juce::String((int)(size*100.f))+"% SIZE / "+
                        juce::String((int)(damp*100.f))+"% DAMP",
                        {60,127,720,21},12,Theme::white);
            Theme::text(g,"8 REFLECTION BANKS / STEREO HALL MODEL (ILLUSTRATIVE)",
                        {60,320,730,18},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::VoxRackSmoothComp) {
            const float threshold=-55.f+53.f*first;
            const float ratio=1.f+11.f*second;
            const float knee=18.f*processor.getSlotParam(slot,2);
            juce::Path unity,curve;
            for(int i=0;i<=140;++i) {
                const float in=-60.f+(float)i/140.f*60.f;
                const float over=in-threshold;
                float gr=0.f;
                if(knee>0.0001f && 2.f*std::abs(over)<=knee) {
                    const float t=over+knee*.5f;
                    gr=(1.f-1.f/ratio)*t*t/(2.f*knee);
                } else if(over>0.f)gr=(1.f-1.f/ratio)*over;
                const float y=y1-h*juce::jlimit(0.f,1.f,(in-gr+60.f)/60.f);
                const float x=x0+w*(float)i/140.f;
                if(i==0){
                    unity.startNewSubPath(x,y1-h*(in+60.f)/60.f);
                    curve.startNewSubPath(x,y);
                } else {
                    unity.lineTo(x,y1-h*(in+60.f)/60.f);
                    curve.lineTo(x,y);
                }
            }
            g.setColour(Theme::muted.withAlpha(.42f));
            g.strokePath(unity,juce::PathStrokeType(1.2f));
            g.setColour(Theme::green);
            g.strokePath(curve,juce::PathStrokeType(3.f));
            const float tx=x0+w*juce::jlimit(0.f,1.f,(threshold+60.f)/60.f);
            g.setColour(Theme::amber.withAlpha(.75f));
            g.drawVerticalLine((int)tx,y0,y1);
            Theme::text(g,"VOXRACK SOFT-KNEE / STEREO LINK",
                        {60,102,670,22},14,Theme::green,true);
            Theme::text(g,juce::String(threshold,1)+" dB  /  "+
                        juce::String(ratio,1)+":1  /  "+
                        juce::String(knee,1)+" dB KNEE",
                        {60,127,690,21},12,Theme::white);
            Theme::text(g,"0 ms PDC / SMOOTH ATTACK + RELEASE",
                        {60,319,700,18},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::Radio) {
            // Illustrated filter response from the Low Cut / High Cut controls.
            const float lowHz=100.f+1400.f*first;
            const float highHz=juce::jmax(lowHz*1.5f,1800.f+10000.f*second);
            const float nasal=processor.getSlotParam(slot,2);
            const auto hzToX=[&](float hz){
                return x0+w*juce::jlimit(0.f,1.f,
                        (float)(std::log(hz/20.f)/std::log(1000.f)));
            };
            g.setColour(Theme::green.withAlpha(.09f));
            g.fillRect(hzToX(lowHz),y0,hzToX(highHz)-hzToX(lowHz),h);
            juce::Path curve;
            for(int i=0;i<=160;++i) {
                const float hz=20.f*std::pow(1000.f,(float)i/160.f);
                const float hp=hz/std::sqrt(hz*hz+lowHz*lowHz);
                const float lp=highHz/std::sqrt(hz*hz+highHz*highHz);
                const float hump=nasal*.18f*std::exp(-std::pow(
                    (float)std::log(hz/std::sqrt(lowHz*highHz))/.75f,2.f));
                const float amplitude=juce::jlimit(0.f,1.f,hp*hp*lp*lp+hump);
                const float x=x0+w*(float)i/160.f;
                const float y=y1-h*.85f*amplitude;
                if(i==0)curve.startNewSubPath(x,y);
                else curve.lineTo(x,y);
            }
            g.setColour(Theme::green);
            g.strokePath(curve,juce::PathStrokeType(3.f));
            g.setColour(Theme::amber.withAlpha(.8f));
            g.drawVerticalLine((int)hzToX(lowHz),y0,y1);
            g.drawVerticalLine((int)hzToX(highHz),y0,y1);
            Theme::text(g,"BROADCAST / RADIO VOICE", {60,102,470,23},
                        16,Theme::green,true);
            Theme::text(g,juce::String(lowHz,0)+" - "+juce::String(highHz,0)+
                        " Hz / VOICE MID BOOST",{60,127,530,20},12,Theme::white);
            Theme::text(g,"APPROXIMATE RESPONSE / LIVE CUTOFF CONTROL",
                        {60,317,680,19},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::RetroCrush) {
            const float bits=3.f+13.f*first;
            const float downsample=juce::jmax(1.f,1.f+16.f*(1.f-second));
            const float levels=std::exp2(bits-1.f);
            juce::Path steps,reference;
            for(int i=0;i<=180;++i) {
                const float t=(float)i/180.f;
                const float phase=std::floor(t*180.f/downsample)*downsample/180.f;
                const float raw=.8f*std::sin(phase*6.f*(float)juce::MathConstants<float>::pi);
                const float crushed=std::round(raw*levels)/levels;
                const float y=y0+h*(.5f-.44f*crushed);
                const float yReference=y0+h*(.5f-.44f*.8f*std::sin(
                    t*6.f*(float)juce::MathConstants<float>::pi));
                if(i==0){steps.startNewSubPath(x0,y);reference.startNewSubPath(x0,yReference);}
                else{steps.lineTo(x0+w*t,y);reference.lineTo(x0+w*t,yReference);}
            }
            g.setColour(Theme::blue.withAlpha(.45f));
            g.strokePath(reference,juce::PathStrokeType(1.3f));
            g.setColour(Theme::green);
            g.strokePath(steps,juce::PathStrokeType(2.7f));
            Theme::text(g,"RETRO CRUSH / BIT DEPTH + SAMPLE HOLD",
                        {61,102,650,23},14,Theme::green,true);
            Theme::text(g,juce::String(bits,1)+" BITS / REDUCED SAMPLE RATE",
                        {61,127,600,20},12,Theme::white);
            Theme::text(g,"STYLIZED RECONSTRUCTION / NOT AN AUDIO WAVEFORM",
                        {61,319,690,17},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::Tremolo) {
            const float depth=second;
            const float shape=processor.getSlotParam(slot,2);
            const float stereo=processor.getSlotParam(slot,3)*.5f;
            for(int ch=0;ch<2;++ch) {
                juce::Path curve;
                for(int i=0;i<=160;++i) {
                    const float t=(float)i/160.f*3.5f+(ch==1?stereo:0.f);
                    const float phase=t-std::floor(t);
                    const float sinShape=.5f-.5f*std::cos(2.f*
                        (float)juce::MathConstants<float>::pi*phase);
                    const float tri=1.f-std::abs(2.f*phase-1.f);
                    const float amp=1.f-depth*(sinShape*(1.f-shape)+tri*shape);
                    const float x=x0+w*(float)i/160.f;
                    const float y=y0+15.f+(1.f-amp)*(h-30.f);
                    if(i==0)curve.startNewSubPath(x,y);
                    else curve.lineTo(x,y);
                }
                g.setColour(ch==0?Theme::green:Theme::blue);
                g.strokePath(curve,juce::PathStrokeType(ch==0?2.9f:2.2f));
            }
            Theme::text(g,"TREMOLO / DUAL-STEREO AMPLITUDE MOTION",
                        {60,102,690,22},14,Theme::green,true);
            Theme::text(g,"GREEN / LEFT     BLUE / RIGHT",
                        {60,127,575,21},12,Theme::white);
            Theme::text(g,"ILLUSTRATIVE MODULATION ENVELOPE / NOT INPUT AUDIO",
                        {60,318,720,18},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::StereoWidth) {
            const float width=first*2.f;
            const float monoHz=60.f+340.f*second;
            // Two stylized voices radiating away from the mid line.
            const float middle=y0+h*.52f;
            g.setColour(Theme::muted.withAlpha(.38f));
            g.drawHorizontalLine((int)middle,x0,x1);
            for(int ch=0;ch<2;++ch){
                juce::Path wave;
                for(int i=0;i<=150;++i){
                    const float t=(float)i/150.f;
                    const float envelope=(.4f+.6f*std::sin(t*(float)juce::MathConstants<float>::pi));
                    const float side=std::sin(t*26.f)*width*h*.26f*envelope;
                    const float x=x0+t*w;
                    const float y=middle+(ch==0?1.f:-1.f)*side;
                    if(i==0)wave.startNewSubPath(x,y);
                    else wave.lineTo(x,y);
                }
                g.setColour(ch==0?Theme::green:Theme::blue);
                g.strokePath(wave,juce::PathStrokeType(2.6f));
            }
            Theme::text(g,"STEREO FIELD / MID + SIDE",
                        {60,102,680,23},15,Theme::green,true);
            Theme::text(g,juce::String(width*100.f,0)+"% WIDTH / BASS MONO "+
                        juce::String(monoHz,0)+" Hz",
                        {60,127,690,21},12,Theme::white);
            Theme::text(g,"LEFT / RIGHT DECORRELATION IS ILLUSTRATIVE",
                        {60,318,650,18},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::ThreeBandDynamics) {
            const juce::StringArray bandLabels{"LOW","MID","HIGH"};
            for(int k=0;k<3;++k){
                const float amount=processor.getSlotParam(slot,k);
                const float x=x0+70.f+k*225.f;
                const float colH=h*.62f;
                g.setColour(Theme::panelAlt);
                g.fillRoundedRectangle(x,y0+55.f,115.f,colH,8.f);
                g.setColour(Theme::green.withAlpha(.23f));
                g.fillRoundedRectangle(x,y0+55.f+(1.f-amount)*colH,
                                       115.f,amount*colH,8.f);
                g.setColour(Theme::green);
                g.drawRoundedRectangle(x,y0+55.f,115.f,colH,8.f,1.5f);
                Theme::text(g,bandLabels[k],{(int)x,(int)y0+16,115,23},14,
                            Theme::white,true,juce::Justification::centred);
                Theme::text(g,juce::String((int)(amount*100.f))+"%",
                            {(int)x,(int)(y0+67.f),115,20},11,
                            Theme::green,true,juce::Justification::centred);
            }
            Theme::text(g,"THREE-BAND DYNAMICS / INDEPENDENT ENVELOPES",
                        {61,102,740,20},12,Theme::green,true);
            Theme::text(g,"240 Hz / 3.4 kHz  /  COMPLEMENTARY CROSSOVERS",
                        {60,320,730,18},10,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::Limiter ||
                  type==SkellPresetAudioProcessor::StudioLimiterFX) {
            // Dedicated native limiter console, parameter-driven like the
            // actual Studio-hybrid ceiling/drive engine. The curve is an
            // idealized steady-state ceiling map, NOT a fake GR/TP meter.
            const bool studio=type==SkellPresetAudioProcessor::StudioLimiterFX;
            const float ceilingGain=.12f+.87f*first;
            const float ceilingDb=20.f*std::log10(ceilingGain);
            const float driveDb=18.f*second;
            juce::Path diagonal,ceilingCurve;
            const float insetL=plot.getX()+25.f;
            const float insetR=plot.getRight()-244.f;
            const float low=-48.f,high=12.f;
            for(int i=0;i<=120;++i){
                const float inputDb=low+(high-low)*(float)i/120.f;
                const float ideal=std::min(ceilingDb,inputDb+driveDb);
                const float px=insetL+(insetR-insetL)*(float)i/120.f;
                const float yin=plot.getBottom()-21.f-
                    (inputDb-low)/(high-low)*(plot.getHeight()-50.f);
                const float yout=plot.getBottom()-21.f-
                    (ideal-low)/(high-low)*(plot.getHeight()-50.f);
                if(i==0){diagonal.startNewSubPath(px,yin);
                         ceilingCurve.startNewSubPath(px,yout);}
                else {diagonal.lineTo(px,yin);ceilingCurve.lineTo(px,yout);}
            }
            g.setColour(Theme::muted.withAlpha(.40f));
            g.strokePath(diagonal,juce::PathStrokeType(1.3f));
            g.setColour(Theme::green);
            g.strokePath(ceilingCurve,juce::PathStrokeType(2.7f));
            Theme::fittedText(g,studio?"STUDIO LIMITER / HYBRID":
                                    "SKELL LOOKAHEAD LIMITER",
                              {60,164,550,22},13.5f,Theme::green,true);
            Theme::text(g,"CEILING "+juce::String(ceilingDb,1)+" dBFS",
                        {58,195,280,22},15.f,Theme::white,true);
            Theme::text(g,"DRIVE +"+juce::String(driveDb,1)+" dB",
                        {58,225,280,19},12.2f,Theme::muted,true);
            const float rackPeak=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
            const auto rail=juce::Rectangle<float>(plot.getRight()-175.f,
                                                   plot.getY()+49.f,101.f,121.f);
            g.setColour(Theme::panelAlt);
            g.fillRoundedRectangle(rail,5.f);
            g.setColour(Theme::stroke);
            g.drawRoundedRectangle(rail,5.f,1.1f);
            const float height=(rail.getHeight()-18.f)*rackPeak;
            g.setColour(Theme::green.withAlpha(.22f+.65f*rackPeak));
            g.fillRoundedRectangle(rail.getX()+17.f,rail.getBottom()-8.f-height,
                                   18.f,height,3.f);
            Theme::fittedText(g,"OUT PEAK",
                        {(int)rail.getX()+39,(int)rail.getY()+19,57,19},
                        10.8f,Theme::muted,true);
            const juce::String peakDb=rackPeak<=.000001f
                ?juce::String("-INF")
                :juce::String(20.f*std::log10(rackPeak),1);
            Theme::fittedText(g,peakDb,
                        {(int)rail.getX()+39,(int)rail.getY()+65,58,23},
                        15.0f,Theme::white,true);
            Theme::text(g,"dBFS",
                        {(int)rail.getX()+40,(int)rail.getY()+88,54,14},
                        10.0f,Theme::muted,true);
            // Explanation belongs in the effect help text, not on top of
            // the green transfer line at the left edge of this plot.
        } else if(type==SkellPresetAudioProcessor::Saturator){
            // Waveshaper response changes as the real Drive and Character do.
            juce::Path shape,unity;
            const float drive=1.f+17.f*first;
            const float character=.6f+1.1f*second;
            for(int i=0;i<=96;++i){
                const float normal=(float)i/96.f, signal=normal*2.f-1.f;
                const float output=std::tanh(signal*drive*character)/
                                   std::max(.5f,drive*.6f);
                const float x=x0+w*normal;
                const float y=plot.getCentreY()-juce::jlimit(-1.f,1.f,output)*h*.45f;
                if(i==0){shape.startNewSubPath(x,y);
                    unity.startNewSubPath(x,y1);}
                else{shape.lineTo(x,y);
                    unity.lineTo(x,y1-h*normal);}
            }
            g.setColour(Theme::muted.withAlpha(.40f));g.strokePath(unity,juce::PathStrokeType(1.4f));
            g.setColour(Theme::green);g.strokePath(shape,juce::PathStrokeType(2.7f));
            Theme::text(g,"SATURATION  /  LIVE SHAPER RESPONSE",
                {62,165,700,20},12,Theme::green,true);
        } else if(type==SkellPresetAudioProcessor::Reverb){
            // Energy decay is visualized as a damped tail, not fake input audio.
            const float decay=1.f+first*7.f;
            const float damping=.15f+second*.80f;
            juce::Path upper,lower;
            for(int i=0;i<=120;++i){
                const float t=(float)i/120.f;
                const float envelope=std::exp(-t*(.9f+6.f/decay));
                const float ripple=std::sin(t*145.f)*envelope*(1.f-.55f*damping);
                const float x=x0+t*w;
                if(i==0){
                    upper.startNewSubPath(x,plot.getCentreY()-envelope*h*.32f);
                    lower.startNewSubPath(x,plot.getCentreY()+ripple*h*.32f);
                } else {
                    upper.lineTo(x,plot.getCentreY()-envelope*h*.32f);
                    lower.lineTo(x,plot.getCentreY()+ripple*h*.32f);
                }
            }
            g.setColour(Theme::green.withAlpha(.75f));
            g.strokePath(upper,juce::PathStrokeType(2.2f));
            g.setColour(Theme::blue);
            g.strokePath(lower,juce::PathStrokeType(1.9f));
            Theme::text(g,"REVERB  /  DECAY ENVELOPE + REFLECTIONS",
                {62,165,700,20},12,Theme::green,true);
        } else if(type==SkellPresetAudioProcessor::Exciter){
            // Harmonic partial ladder reflects Excite and Colour parameters.
            const float focus=.25f+second*.75f;
            for(int i=0;i<11;++i) {
                const float x=x0+50.f+i*(w-80.f)/10.f;
                const float strength=std::pow(1.f/(1.f+(float)i*.36f),1.7f-first);
                const float height=(.08f+first*.78f)*strength*h*focus;
                g.setColour(i%2?Theme::blue:Theme::green);
                g.fillRoundedRectangle(x,plot.getCentreY()+h*.34f-height,
                                       9.f,height+2.f,2.4f);
            }
            Theme::text(g,"EXCITER  /  HARMONIC PARTIALS",
                {62,165,700,20},12,Theme::green,true);
        } else if(type==SkellPresetAudioProcessor::AdlibDoubler){
            // Two phase/micro-delay trails represent the stereo layers.
            const float width=first,offset=.1f+second*.6f;
            for(int side=0;side<2;++side){
                juce::Path voice;
                for(int i=0;i<=140;++i){
                    const float t=(float)i/140.f;
                    const float phase=t*30.f+(side?offset:-offset)*5.f;
                    const float wave=std::sin(phase)*h*(.17f+.18f*width);
                    const float y=plot.getCentreY()+wave+(side?1.f:-1.f)*width*15.f;
                    if(i==0)voice.startNewSubPath(x0,y);
                    else voice.lineTo(x0+t*w,y);
                }
                g.setColour(side?Theme::blue:Theme::green);
                g.strokePath(voice,juce::PathStrokeType(2.2f));
            }
            Theme::text(g,"DOUBLER  /  LEFT + RIGHT MICRO-VOICES",
                {62,165,700,20},12,Theme::green,true);
        } else if(type==SkellPresetAudioProcessor::VocalRiderFX) {
            // Illustrative long-term level tracing, not a claim of live LUFS.
            const float centre=plot.getCentreY();
            const float strength=processor.getSlotParam(slot,0);
            const float target=processor.getSlotParam(slot,1);
            const float targetY=y0+h*(.76f-.42f*target);
            g.setColour(Theme::green.withAlpha(.13f));
            g.fillRoundedRectangle(x0,targetY-13.f,w,26.f,5.f);
            g.setColour(Theme::green.withAlpha(.6f));
            g.drawHorizontalLine((int)targetY,x0,x1);
            for(int lane=0;lane<2;++lane) {
                juce::Path contour;
                for(int i=0;i<=100;++i){
                    const float t=(float)i/100.f;
                    const float source=std::sin(t*26.f)*(.35f+.65f*std::abs(std::sin(t*5.f)));
                    const float wave=lane==0?source:source*(1.f-.53f*strength);
                    const float x=x0+t*w;
                    const float y=centre-wave*h*.25f-(lane==1?16.f:0.f);
                    if(i==0)contour.startNewSubPath(x,y);else contour.lineTo(x,y);
                }
                g.setColour(lane==0?Theme::muted.withAlpha(.48f):Theme::green);
                g.strokePath(contour,juce::PathStrokeType(lane==0?1.5f:2.5f));
            }
        } else if(type==SkellPresetAudioProcessor::DuckingPingPongFX) {
            const float feedback=processor.getSlotParam(slot,1);
            const float duck=processor.getSlotParam(slot,2);
            g.setColour(Theme::muted.withAlpha(.27f));
            g.drawHorizontalLine((int)plot.getCentreY(),x0,x1);
            for(int i=0;i<6;++i) {
                const float decay=std::pow(.55f+.35f*feedback,(float)i);
                const float tx=x0+68.f+(float)i*132.f;
                const float amplitude=decay*(1.f-.60f*duck)*(h*.30f);
                g.setColour((i%2==0?Theme::green:Theme::blue).withAlpha(.9f));
                g.drawLine(tx,plot.getCentreY()-amplitude,tx,plot.getCentreY()+amplitude,4.f);
                g.fillEllipse(tx-5.f,plot.getCentreY()-5.f,10.f,10.f);
            }
            Theme::text(g,"L  /  R  /  L  /  R  /  L  /  R",
                {125,279,675,21},11,Theme::muted,true,juce::Justification::centred);
        } else if(type==SkellPresetAudioProcessor::VocalVibratoFX) {
            const float depth=processor.getSlotParam(slot,1);
            const float stereo=processor.getSlotParam(slot,2);
            for(int channel=0;channel<2;++channel) {
                juce::Path curve;
                for(int i=0;i<=160;++i){
                    const float t=(float)i/160.f;
                    const float phase=t*(10.f+28.f*first)+(channel?stereo*3.14159f:0.f);
                    const float y=plot.getCentreY()+std::sin(phase)*h*(.09f+.22f*depth);
                    const float x=x0+t*w;
                    if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);
                }
                g.setColour(channel==0?Theme::green:Theme::blue);
                g.strokePath(curve,juce::PathStrokeType(channel==0?2.7f:1.9f));
            }
        } else if(type>=SkellPresetAudioProcessor::AdlibChop &&
                  type<=SkellPresetAudioProcessor::AdlibSaturate) {
            // Parameter-driven shapes from the shared DSP's algorithms.
            // This is not a measured spectrum or synthesized live audio trace.
            const float depth=processor.getSlotParam(slot,0);
            const float rate=processor.getSlotParam(slot,1);
            const float tone=processor.getSlotParam(slot,2);
            const float mix=processor.getSlotParam(slot,3);
            const int divisions=1<<juce::jlimit(0,3,(int)(rate*3.999f));
            const float drive=1.f+14.f*depth;
            const float inv=1.f/std::tanh(drive);
            const float colour=.6f+.4f*tone;
            juce::Path primary,secondary;
            for(int k=0;k<=220;++k) {
                const float t=k/220.f;
                float a=0.f,b=0.f;
                if(type==SkellPresetAudioProcessor::AdlibChop) {
                    const float phase=(float)std::fmod((double)t*divisions,1.);
                    const float duty=.82f-.55f*tone;
                    const float attack=juce::jlimit(0.f,1.f,phase*35.f);
                    const float release=juce::jlimit(0.f,1.f,(duty-phase)*35.f);
                    a=1.f-depth*mix*(1.f-juce::jmax(0.f,juce::jmin(attack,release)));
                } else if(type==SkellPresetAudioProcessor::AdlibMotion) {
                    const float pan=std::sin(t*juce::MathConstants<float>::twoPi)*depth;
                    a=.5f+.32f*mix*(-juce::jmax(0.f,pan)+.85f*juce::jmax(0.f,-pan));
                    b=.5f+.32f*mix*(-juce::jmax(0.f,-pan)+.85f*juce::jmax(0.f,pan));
                } else if(type==SkellPresetAudioProcessor::AdlibFilter) {
                    const float hz=20.f*std::pow(1000.f,t);
                    const float cutoff=80.f*std::pow(225.f,tone);
                    const float lp=1.f/std::sqrt(1.f+(hz/cutoff)*(hz/cutoff));
                    const float hp=(hz/cutoff)*lp;
                    a=1.f+depth*mix*((1.f-rate)*lp+rate*hp-1.f);
                } else {
                    const float input=-1.f+2.f*t;
                    const float colourSample=std::tanh(input*drive)*inv*colour+
                                              input*(1.f-colour);
                    a=input+(colourSample-input)*depth*mix;
                    b=input;
                }
                const float px=x0+t*w;
                const bool symmetric=type==SkellPresetAudioProcessor::AdlibSaturate;
                const float scale=symmetric?.36f:.68f;
                const float py=symmetric?plot.getCentreY()-a*h*scale:y1-a*h*scale;
                const float py2=symmetric?plot.getCentreY()-b*h*scale:y1-b*h*scale;
                if(k==0){primary.startNewSubPath(px,py);secondary.startNewSubPath(px,py2);}
                else {primary.lineTo(px,py);secondary.lineTo(px,py2);}
            }
            if(type==SkellPresetAudioProcessor::AdlibMotion ||
               type==SkellPresetAudioProcessor::AdlibSaturate) {
                g.setColour(Theme::blue.withAlpha(.75f));
                g.strokePath(secondary,juce::PathStrokeType(1.7f));
            }
            g.setColour(Theme::green);
            g.strokePath(primary,juce::PathStrokeType(2.8f));
            const juce::String title=type==SkellPresetAudioProcessor::AdlibChop
                ?"BPM GATE  /  "+juce::String(divisions)+"x BEAT DIVISION":
                type==SkellPresetAudioProcessor::AdlibMotion
                ?"STEREO MOTION  /  "+juce::String(.08f+9.92f*rate,2)+" Hz":
                type==SkellPresetAudioProcessor::AdlibFilter
                ?"FILTER PROFILE  /  LOW TO HIGH":
                "TANH SATURATION  /  "+juce::String(drive,1)+"x DRIVE";
            Theme::fittedText(g,title,{55,172,735,22},12.5f,Theme::green,true);
            Theme::text(g,"CONTROL RESPONSE / NOT LIVE AUDIO",
                        {55,327,735,18},10.f,Theme::muted);
        } else if(type==SkellPresetAudioProcessor::StudioFastDistFX) {
            // Real processor function, rendered with an isolated UI instance
            // of the same 20-curve lookup, never the live DSP state.
            skellpreset::studio::FastDist displayDist;
            displayDist.prepare(processor.preparedAudioRate());
            displayDist.set(processor.getSlotParam(slot,0),
                            processor.getSlotParam(slot,1),
                            processor.getSlotParam(slot,2),
                            processor.getSlotParam(slot,3),
                            processor.getSlotParam(slot,4));
            juce::Path neutral,response;
            const float xLeft=plot.getX()+27.f;
            const float xRight=plot.getRight()-27.f;
            const float centre=plot.getCentreY();
            for(int index=0;index<=180;++index){
                const float sample=-1.f+2.f*index/180.f;
                const float shaped=juce::jlimit(-1.f,1.f,
                    displayDist.sample(sample));
                const float px=xLeft+(xRight-xLeft)*index/180.f;
                const float cleanY=centre-sample*plot.getHeight()*.39f;
                const float outputY=centre-shaped*plot.getHeight()*.39f;
                if(index==0){
                    neutral.startNewSubPath(px,cleanY);
                    response.startNewSubPath(px,outputY);
                } else {
                    neutral.lineTo(px,cleanY);
                    response.lineTo(px,outputY);
                }
            }
            g.setColour(Theme::muted.withAlpha(.45f));
            g.strokePath(neutral,juce::PathStrokeType(1.4f));
            g.setColour(Theme::amber.withAlpha(.91f));
            g.strokePath(response,juce::PathStrokeType(2.9f,
                juce::PathStrokeType::curved));
            Theme::text(g,"STUDIO FAST DIST / ACTUAL DSP TRANSFER",
                        {53,167,650,23},12,Theme::amber,true);
            Theme::text(g,"DIM = DRY  /  GOLD = PROCESSED  /  INPUT -1 TO +1",
                        {53,326,720,18},10,Theme::muted);
        } else if(type!=SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
            // Clear, family-specific instrument controls instead of the
            // same old radar and concentric ellipse chart on every fallback.
            // Values are REAL p1..p4, never invented frequency/audio data.
            const auto category=juce::String(skellpreset::registry::at(type).category);
            const bool space=category.equalsIgnoreCase("Space");
            const bool dynamics=category.equalsIgnoreCase("Dynamics");
            const bool motion=category.equalsIgnoreCase("Motion");
            const bool tone=category.equalsIgnoreCase("Tone") ||
                            category.equalsIgnoreCase("Pitch");
            const auto accent=space?Theme::blue:
                dynamics?Theme::amber:motion?Theme::pink:
                tone?Theme::green:Theme::white;
            const auto controls=titles(type);
            const float baseline=plot.getBottom()-38.f;
            const float controlTop=plot.getY()+53.f;
            const float travel=baseline-controlTop;
            for(int index=0;index<4;++index){
                const float x=plot.getX()+131.f+index*186.f;
                const float value=juce::jlimit(0.f,1.f,
                    processor.getSlotParam(slot,index));
                const float height=travel*(.03f+.97f*value);
                g.setColour(Theme::panelAlt);
                g.fillRoundedRectangle(x-23.f,controlTop,46.f,travel,5.f);
                // Four different graphic families, consistent positions:
                // Space = echo steps, Motion = alternating motion planes,
                // Dynamics = threshold ladder, Tone = rising bar.
                if(space) {
                    for(int k=0;k<6;++k){
                        const float y=baseline-(k+1)*travel/7.f;
                        const float width=35.f-k*3.f;
                        g.setColour(accent.withAlpha(y>baseline-height?.87f:.13f));
                        g.fillRoundedRectangle(x-width*.5f,y,width,5.f,1.8f);
                    }
                } else if(motion){
                    for(int k=0;k<8;++k){
                        const float y=controlTop+10.f+k*travel/8.f;
                        g.setColour(accent.withAlpha(y>=baseline-height?.79f:.13f));
                        g.drawLine(x-14.f+(k%2)*8.f,y,
                                   x+14.f-(k%2)*8.f,y,2.9f);
                    }
                } else if(dynamics){
                    for(int k=0;k<7;++k){
                        const float y=baseline-k*travel/7.f;
                        g.setColour(accent.withAlpha(y>=baseline-height?.78f:.12f));
                        g.fillRoundedRectangle(x-18.f,y-4.f,36.f,5.f,1.5f);
                    }
                } else {
                    g.setColour(accent.withAlpha(.16f));
                    g.fillRoundedRectangle(x-13.f,controlTop,26.f,travel,4.f);
                    g.setColour(accent.withAlpha(.83f));
                    g.fillRoundedRectangle(x-13.f,baseline-height,
                                           26.f,height,4.f);
                }
                const float marker=baseline-height;
                juce::Path pointer;
                pointer.startNewSubPath(x-28.f,marker);
                pointer.lineTo(x-35.f,marker+5.f);
                pointer.lineTo(x-35.f,marker-5.f);
                pointer.closeSubPath();
                g.setColour(accent);g.fillPath(pointer);
                Theme::fittedText(g,controls[(size_t)index].toUpperCase(),
                    {(int)x-79,170,158,18},10.f,Theme::muted,true,
                    juce::Justification::centred);
                Theme::text(g,juce::String(juce::roundToInt(value*100.f))+"%",
                    {(int)x-65,327,130,18},11.f,Theme::white,true,
                    juce::Justification::centred);
            }
            Theme::fittedText(g,category.toUpperCase()+
                " / LIVE AUTOMATION PARAMETERS",
                {52,153,730,16},10.f,accent,true);
        }
        // Non-EQ modules share the two-axis universal marker.
        if(!isPagedEQ(type) &&
           type!=SkellPresetAudioProcessor::Limiter &&
           type!=SkellPresetAudioProcessor::StudioLimiterFX &&
           !(type>=SkellPresetAudioProcessor::AdlibChop &&
             type<=SkellPresetAudioProcessor::AdlibSaturate) &&
           !(modOrbView && isModulationOrbEffect(type))) {
        // The limiter has its own output meter and transfer curve.
        // Its generic XY crosshair obscured those readouts without adding
        // meaningful parameter feedback.
        const float markerX=x0+w*first,markerY=y1-h*second;
        g.setColour(Theme::green.withAlpha(.24f));
        g.drawVerticalLine((int)markerX,y0,y1);
        g.drawHorizontalLine((int)markerY,x0,x1);
        juce::Path handle;
        handle.startNewSubPath(markerX,markerY-7.f);
        handle.lineTo(markerX+7.f,markerY);
        handle.lineTo(markerX,markerY+7.f);
        handle.lineTo(markerX-7.f,markerY);
        handle.closeSubPath();
        g.setColour(Theme::plot);g.fillPath(handle);
        g.setColour(Theme::green);
        g.strokePath(handle,juce::PathStrokeType(2.2f));
        }
    } else if(type!=SkellPresetAudioProcessor::Empty) {
        // The empty slot already paints its own call-to-action card above.
        // Painting a second instruction here is what caused the overlapping
        // "EMPTY SLOT" / "SELECT A MODULE" headings.
        Theme::text(g,"SELECT A MODULE FROM THE DROPDOWN",{170,234,500,25},16,Theme::muted,true,juce::Justification::centred);
    }
    g.restoreState();
    // Parameter cards are painted exactly once using the real control bounds below.
    // The graph is deliberately clipped to its plot rectangle. Titles and
    // numeric legends belong ABOVE it, after restoring the Graphics clip,
    // or they become invisible at the default 70% scale.
    juce::String effectTitle, effectDetail;
    if(type==SkellPresetAudioProcessor::Radio) {
        const float low=100.f+1400.f*processor.getSlotParam(slot,0);
        const float high=1800.f+10000.f*processor.getSlotParam(slot,1);
        effectTitle="BROADCAST RADIO  /  BANDPASS";
        effectDetail=juce::String(low,0)+" - "+juce::String(high,0)+" Hz  /  NASAL "+
                     juce::String(processor.getSlotParam(slot,2)*100.f,0)+"%";
    } else if(type==SkellPresetAudioProcessor::RetroCrush) {
        const float bits=3.f+13.f*processor.getSlotParam(slot,0);
        const float sr=(float)juce::jmax(8000.,processor.preparedAudioRate());
        const float hz=100.f*std::pow(sr/100.f,processor.getSlotParam(slot,1));
        effectTitle="RETRO CRUSH  /  SAMPLE-HOLD SHAPER";
        effectDetail=juce::String(bits,1)+" BITS  /  "+juce::String(hz,0)+" Hz";
    } else if(type==SkellPresetAudioProcessor::Tremolo) {
        effectTitle="TREMOLO  /  AMPLITUDE MOTION";
        effectDetail=juce::String(.15f+14.f*processor.getSlotParam(slot,0),1)+
                     " Hz  /  DEPTH "+
                     juce::String(processor.getSlotParam(slot,1)*100.f,0)+"%";
    } else if(type==SkellPresetAudioProcessor::StereoWidth) {
        effectTitle="STEREO FIELD  /  MID + SIDE";
        effectDetail=juce::String(processor.getSlotParam(slot,0)*200.f,0)+
                     "% WIDTH  /  MONO BASS "+
                     juce::String(60.f+340.f*processor.getSlotParam(slot,1),0)+" Hz";
    } else if(type==SkellPresetAudioProcessor::ThreeBandDynamics) {
        effectTitle="THREE-BAND DYNAMICS  /  ZERO LOOKAHEAD";
        effectDetail="LOW "+juce::String(processor.getSlotParam(slot,0)*100.f,0)+
                     "%  /  MID "+juce::String(processor.getSlotParam(slot,1)*100.f,0)+
                     "%  /  HIGH "+juce::String(processor.getSlotParam(slot,2)*100.f,0)+"%";
    } else if(type==SkellPresetAudioProcessor::VocalChorusFX) {
        effectTitle="VOCAL CHORUS  /  STEREO MICRO-VOICES";
        effectDetail=juce::String(.08f+3.9f*processor.getSlotParam(slot,0),2)+
                     " Hz  /  "+juce::String(1.f+10.f*processor.getSlotParam(slot,1),1)+
                     " ms  /  2-5 VOICES";
    } else if(type==SkellPresetAudioProcessor::VocalFlangerFX) {
        effectTitle="VOCAL FLANGER  /  SWEEP + FEEDBACK";
        effectDetail=juce::String(.05f+4.95f*processor.getSlotParam(slot,0),2)+
                     " Hz  /  "+juce::String(.3f+9.f*processor.getSlotParam(slot,1),1)+
                     " ms RANGE";
    } else if(type==SkellPresetAudioProcessor::RingModFX) {
        effectTitle="RING MODULATOR  /  METALLIC VOICE";
        effectDetail=juce::String(15.f*std::pow(70.f,processor.getSlotParam(slot,0)),0)+
                     " Hz  /  DEPTH "+
                     juce::String(processor.getSlotParam(slot,1)*100.f,0)+"%";
    } else if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
        effectTitle="ALCHEMIST MATCH FINISH  /  LIVE ENVELOPE + HARMONICS";
        effectDetail=matchPage==2
            ?"REAL REFERENCE-FITTED LEVEL + AIR / AUTOMATABLE / ZERO LOOKAHEAD"
            :"v0.7 SOURCE MODEL  /  CAUSAL DETECTORS 20 / 120 / 280 ms"
             "  /  ZERO ADDED LATENCY";
    } else if(type==SkellPresetAudioProcessor::StudioPitchFormantFX) {
        effectTitle="STUDIO PITCH + FORMANT  /  SPECTRAL COLOUR";
        effectDetail="768 SAMPLES REPORTED HOST PDC  /  NOT LOW-LATENCY AUTOTUNE";
    } else if(type==SkellPresetAudioProcessor::StudioEQFX) {
        effectTitle="STUDIO PARAMETRIC EQ  /  EIGHT FILTER SLOTS";
        effectDetail="RIGHT-CLICK A NODE FOR HP / LP / SHELF / REMOVE";
    } else if(type==SkellPresetAudioProcessor::EightBandToneFX) {
        effectTitle="8-BAND EQ  /  EIGHT INDEPENDENT FREQUENCY + GAIN NODES";
        effectDetail="SELECT ANY DOT TO EDIT FREQUENCY / GAIN / Q / ZERO ADDED PDC";
    } else if(type==SkellPresetAudioProcessor::SerialDynamicsFX) {
        effectTitle="FET + OPTO  /  TWO-STAGE VOCAL COMPRESSION";
        effectDetail=juce::String(-48.f+42.f*processor.getSlotParam(slot,0),0)+
            " dB  /  RATIO "+juce::String(2.f+10.f*processor.getSlotParam(slot,1),1)+
            ":1  /  OPTIONAL NY PARALLEL";
    } else if(type==SkellPresetAudioProcessor::DynamicHarshnessFX) {
        effectTitle="HARSHNESS TAMER  /  DYNAMIC RESONANT DIP";
        effectDetail=juce::String(1800.f*std::pow(4.2f,processor.getSlotParam(slot,0)),0)+
            " Hz  /  MAX CUT "+juce::String(10.f*processor.getSlotParam(slot,2),1)+" dB";
    } else if(type==SkellPresetAudioProcessor::AdaptiveNoiseFX) {
        effectTitle="ADAPTIVE NOISE CONTROL  /  NATIVE DYNAMIC FLOOR";
        effectDetail="GENTLE NOISE REDUCTION  /  NO NEURAL MODEL  /  ZERO ADDED PDC";
    } else if(type==SkellPresetAudioProcessor::VocalRiderFX) {
        effectTitle="VOCAL RIDER  /  STEREO LINKED LEVELING";
        effectDetail="TARGET "+juce::String(-26.f+14.f*processor.getSlotParam(slot,1),0)+
            " dB  /  MAX RANGE "+juce::String(3.f+12.f*processor.getSlotParam(slot,2),0)+
            " dB  /  ZERO ADDED PDC";
    } else if(type==SkellPresetAudioProcessor::DuckingPingPongFX) {
        effectTitle="DUCKING PING-PONG  /  HOST TEMPO ECHO";
        const float division=processor.getSlotParam(slot,0);
        effectDetail=juce::String("SYNC ")+(division<.25f?"1/16":division<.5f?"1/8":division<.75f?"1/8D":"1/4")+
            "  /  DUCK "+juce::String(processor.getSlotParam(slot,2)*100.f,0)+
            "%  /  HOST BPM FALLBACK 120";
    } else if(type==SkellPresetAudioProcessor::SkelldizerFX) {
        effectTitle="SKELLDIZER / PUNCH / WARM / AIR / GRIT";
        effectDetail="3-BAND HARMONIC CHARACTER / OPTIONAL LEVEL MATCH / ZERO LOOKAHEAD";
    } else if(type==SkellPresetAudioProcessor::MicModelFX) {
        effectTitle="MIC MODELING  /  NATIVE TONAL PROFILE";
        effectDetail="GENERIC COLOR TRANSFER  /  NO PROPRIETARY MIC CAPTURES  /  0ms LATENCY";
    } else if(type==SkellPresetAudioProcessor::VocalVibratoFX) {
        effectTitle="VOCAL VIBRATO  /  STEREO DELAY MODULATION";
        effectDetail=juce::String(.3f+11.7f*processor.getSlotParam(slot,0),1)+
            " Hz  /  DEPTH "+juce::String(3.5f*processor.getSlotParam(slot,1),1)+
            " ms  /  SINE-TRIANGLE";
    } else if(type==SkellPresetAudioProcessor::VoxRackRoom) {
        effectTitle="VOXRACK ROOM  /  COMB + ALLPASS";
        effectDetail=juce::String(processor.getSlotParam(slot,2)*200.f,0)+
                     " ms PREDELAY  /  "+
                     juce::String(processor.getSlotParam(slot,0)*100.f,0)+"% SIZE";
    } else if(type==SkellPresetAudioProcessor::VoxRackSmoothComp) {
        effectTitle="VOXRACK SMOOTH COMP  /  SOFT KNEE";
        effectDetail=juce::String(-55.f+53.f*processor.getSlotParam(slot,0),1)+
                     " dB  /  "+juce::String(1.f+11.f*processor.getSlotParam(slot,1),1)+
                     ":1  /  "+juce::String(18.f*processor.getSlotParam(slot,2),1)+" dB KNEE";
    }
    // The module name is already the main heading at y=29. Repeating an
    // uppercase effect title at y=103 overlapped EQ band tabs, the Q slider,
    // limiter/delay page selectors and the sidechain control (reported by
    // the real Studio EQ screenshot). ALL effects instead share one quiet
    // secondary hint in the dedicated 380–402px metadata lane BELOW the
    // 151–357px live graph and ABOVE the 411px parameter controls.
    // One footer lane, not two overlapping text passes.
    if(type==SkellPresetAudioProcessor::DeEsser){
        const auto frequency=3500.f+8000.f*processor.getSlotParam(slot,0);
        const auto threshold=-53.f+39.f*processor.getSlotParam(slot,1);
        const auto reduction=2.f+20.f*processor.getSlotParam(slot,2);
        Theme::fittedText(g,"SIBILANCE "+juce::String(frequency/1000.f,2)+" kHz",
            {33,103,240,25},14.f,Theme::green,true);
        Theme::fittedText(g,"THRESHOLD "+juce::String(threshold,1)+" dBFS   /   MAX "+juce::String(reduction,1)+" dB",
            {285,103,583,25},12.f,Theme::white,true,juce::Justification::centredRight);
        for(int db=0;db>=-24;db-=6){
            const int y=juce::roundToInt(plot.getY()+25.f-db*(plot.getHeight()-50.f)/24.f);
            Theme::text(g,juce::String(db),{1,y-7,29,14},9.f,Theme::muted,false,juce::Justification::centredRight);
        }
    }
    if(type==SkellPresetAudioProcessor::Compressor ||
       type==SkellPresetAudioProcessor::Gate ||
       type==SkellPresetAudioProcessor::Delay ||
       type==SkellPresetAudioProcessor::AdlibThrow) {
        const float x=plot.getX()+24.f+(plot.getWidth()-48.f)*
                      processor.getSlotParam(slot,0);
        const float y=plot.getBottom()-27.f-(plot.getHeight()-47.f)*
                      processor.getSlotParam(slot,1);
        g.setColour(Theme::plot);
        g.fillEllipse(x-6.f,y-6.f,12.f,12.f);
        g.setColour(Theme::green);
        g.drawEllipse(x-6.f,y-6.f,12.f,12.f,2.0f);
    }
    if(isPagedEQ(type)) {
        Theme::text(g,"BAND "+juce::String(activeEqBand+1)+
                    (type==SkellPresetAudioProcessor::StudioEQFX?" WIDTH":" Q"),
                    {680,96,172,17},10.5f,Theme::green,true);
        if(type==SkellPresetAudioProcessor::EightBandToneFX) {
            Theme::text(g,"LOW CUT",{20,127,55,16},10.5f,Theme::amber,true);
            Theme::text(g,"HIGH CUT",{342,127,80,16},10.5f,Theme::amber,true);
        }
    }
    if(type==SkellPresetAudioProcessor::EQ){
        g.setColour(Theme::green.withAlpha(.13f));g.fillRoundedRectangle(juce::Rectangle<float>(34.f,96.f,145.f,30.f),6.f);
        g.setColour(Theme::green.withAlpha(.7f));g.drawRoundedRectangle(juce::Rectangle<float>(34.f,96.f,145.f,30.f),6.f,1.f);
        Theme::text(g,"BELL + LOW CUT",{44,99,130,23},11,Theme::green,true);
        Theme::text(g,"RESPONSE FROM LIVE DSP COEFFICIENTS",{205,100,390,22},11,Theme::muted);
    }
    if(eqFace) {
        const juce::StringArray ticks{"20","50","100","200","500","1k","2k","5k","10k","20k"};
        constexpr float tickHz[]{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f};
        for(int i=0;i<ticks.size();i++){
            const auto x=plot.getX()+plot.getWidth()*std::log(tickHz[i]/20.f)/std::log(1000.f);
            Theme::text(g,ticks[i],{(int)x-24,360,48,17},10,Theme::muted,false,juce::Justification::centred);
        }
        if(type==SkellPresetAudioProcessor::EQ) {
            Theme::text(g,"+36",{5,142,30,15},9,Theme::muted);
            Theme::text(g,"0",{13,245,22,14},9,Theme::muted);
            Theme::text(g,"-36",{5,341,30,15},9,Theme::muted);
        }
    } else if(type==SkellPresetAudioProcessor::Compressor || type==SkellPresetAudioProcessor::Gate) {
        const bool compressor=type==SkellPresetAudioProcessor::Compressor;
        for(int k=0;k<=4;k++){
            const int db=(compressor?-60:-80)+(compressor?15:20)*k;
            const int x=(int)(plot.getX()+plot.getWidth()*k/4.f);
            Theme::text(g,juce::String(db),{x-22,360,44,17},10,Theme::muted,false,juce::Justification::centred);
        }
        // Old axis title began at x=1 and clipped on the narrow effect
        // deck (especially at 70% UI scale). Place it INSIDE the graph.
        // Gate already draws its OPEN threshold legend in the graph.
        if(compressor)
            Theme::text(g,"OUT dB",
                        {(int)plot.getX()+12,(int)plot.getY()+34,85,17},
                        10.3f,Theme::muted,true);
        Theme::text(g,"INPUT dB",{(int)plot.getRight()-86,376,85,15},9,Theme::muted,false,juce::Justification::centredRight);
    } else if(type==SkellPresetAudioProcessor::Delay || type==SkellPresetAudioProcessor::AdlibThrow) {
        Theme::text(g,"ECHO NUMBER  1 - 5",{38,364,360,17},10,Theme::muted);
    }
    auto labels=titlesForHardware(type);
    if(type==SkellPresetAudioProcessor::StudioLimiterFX && limiterDetailsVisible)
        Theme::text(g,"GATE FLOOR",{430,107,116,20},11.5f,Theme::white,true);
    if(type==SkellPresetAudioProcessor::StudioCompressorFX)
        Theme::text(g,"SOFT KNEE",{430,107,116,20},11.5f,Theme::white,true);
    if(type!=SkellPresetAudioProcessor::ExternalVST3 &&
       type!=SkellPresetAudioProcessor::Empty) {
        // Family-specific parameter cards behind real attached sliders.
        // No extra decorative controls and no hidden limiter labels.
        const bool dynamics=family.equalsIgnoreCase("Dynamics");
        const bool space=family.equalsIgnoreCase("Space");
        const bool motion=family.equalsIgnoreCase("Motion");
        for(int i=0;i<4;++i) {
            if(!knobs[(size_t)i].isVisible())continue;
            const auto card=parameterCardBounds(type,i,!limiterDetailsVisible).toFloat();
            const bool paired=controlFace(type)==ControlFace::eqPairs;
            const auto tint=dynamics?Theme::amber:space?Theme::blue:
                motion?Theme::pink:familyAccent;
            g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(.12f),card.getTopLeft(),
                Theme::plot,card.getBottomRight(),false));
            g.fillRoundedRectangle(card,7.f);
            g.setColour(Theme::stroke.withAlpha(.9f));
            g.drawRoundedRectangle(card.reduced(.6f),7.f,1.f);
            if(!paired){
                g.setColour(tint.withAlpha(.07f));
                g.fillRoundedRectangle(card.getX()+4.f,card.getY()+4.f,card.getWidth()-8.f,23.f,4.f);
                g.setColour(tint.withAlpha(.32f));
                g.drawHorizontalLine((int)card.getY()+28,card.getX()+10.f,card.getRight()-10.f);
            }
            g.setColour(tint.withAlpha(.63f));
            if(dynamics)
                g.fillRoundedRectangle(card.getX()+2.f,card.getY()+6.f,2.f,
                    card.getHeight()-12.f,1.f);
            else if(space)
                g.drawLine(card.getX()+9.f,card.getY()+3.f,card.getRight()-9.f,
                    card.getY()+3.f,.7f);
            else
                g.fillEllipse(card.getX()+6.f,card.getY()+7.f,3.f,3.f);
            const auto caption=!paired?card.toNearestInt().removeFromTop(28).reduced(9,0):
                juce::Rectangle<int>((int)card.getX()+11,(int)card.getY()+8,112,38);
            Theme::fittedText(g,labels[(size_t)i].toUpperCase(),caption,
                13.f,knobs[(size_t)i].isEnabled()?Theme::white:Theme::muted,true,
                paired?juce::Justification::centredLeft:juce::Justification::centred);
        }
    }
    const juce::String gestureHint="DRAG GRAPH  /  X: "+
        labels[0].toUpperCase()+"  /  Y: "+labels[1].toUpperCase();
    Theme::text(g,type==SkellPresetAudioProcessor::Empty
                    ? juce::String("NO PROCESSOR LOADED  /  RACK SLOT KEEPS ITS POSITION AND AUTOMATION")
                    : type==SkellPresetAudioProcessor::AdlibThrow ? "PHRASE-END THROW / REPEATS CONTINUE THROUGH SILENCE" :
                  type==SkellPresetAudioProcessor::AdlibDoubler ? "STEREO MICRO-DOUBLING / MONO INPUT REMAINS COMPATIBLE" :
                  isModulationOrbEffect(type) && modOrbView ? "MODULATION ORB IS VISUAL ONLY / EDIT IN-PANEL CONTROLS" :
                  type==SkellPresetAudioProcessor::EQ ? "DRAG THE EQ NODE TO CHANGE FREQUENCY + GAIN" :
                  type==SkellPresetAudioProcessor::EightBandToneFX ? "CLICK ANY OF EIGHT NODES  /  DRAG TO CHANGE ITS FREQUENCY + GAIN" :
                  type==SkellPresetAudioProcessor::AdaptiveNoiseFX ? "ADAPTIVE GATE  /  NOT AN RNNOISE NEURAL MODEL" :
                  type==SkellPresetAudioProcessor::StudioPitchFormantFX ? "SPECTRAL PITCH / FORMANT / HOST LATENCY 768 SAMPLES" :
                  type==SkellPresetAudioProcessor::AlchemistMatchFinishFX ? "CAUSAL REFERENCE-LEARNED CONTOUR  /  CALIBRATE FOR EACH DIFFERENT VOICE" :
                  type==SkellPresetAudioProcessor::MicModelFX ? "TONAL RESPONSE SKETCH  /  NOT A MEASURED HARDWARE MICROPHONE IMPULSE" :
                  type==SkellPresetAudioProcessor::DeEsser ? "DRAG THE FREQUENCY MARKER  /  USE THRESHOLD, REDUCTION AND RELEASE BELOW" :
                  type==SkellPresetAudioProcessor::ExternalVST3
                    ? juce::String("HOSTED VST3  /  INLINE EDITOR OR OPEN PLUGIN UI")
                  : (type==SkellPresetAudioProcessor::StudioLimiterFX && limiterPage==1) ||
                    (type==SkellPresetAudioProcessor::StudioDelayFX && delayPage!=0) ||
                    (type==SkellPresetAudioProcessor::AlchemistMatchFinishFX && matchPage!=0)
                    ? juce::String("GRAPH READ-ONLY ON THIS PAGE  /  USE IN-PANEL CONTROLS")
                    : effectDetail.isNotEmpty()?effectDetail:effectTitle.isNotEmpty()?effectTitle:gestureHint,
                    {30,379,838,20},11,Theme::muted);
}
void SlotEditor::updateGraphNode(juce::Point<float> point) {
    const int type=processor.slotType(slot);
    // Dynamics is a display of the limiter transfer, while its four knobs
    // edit p5..p8. Do not silently change off-page p1/p2 on graph drag.
    if(type==SkellPresetAudioProcessor::StudioLimiterFX && limiterPage==1)return;
    if(type==SkellPresetAudioProcessor::StudioDelayFX && delayPage!=0)return;
    if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX && matchPage!=0)return;
    // Adlib graphs visualize control responses and do not edit invisible XY axes.
    if(type>=SkellPresetAudioProcessor::AdlibChop &&
       type<=SkellPresetAudioProcessor::AdlibSaturate)return;
    if(type==SkellPresetAudioProcessor::DeEsser){
        const float x=juce::jlimit(0.f,1.f,(point.x-33.f)/835.f);
        const float frequency=20.f*std::pow(1000.f,x);
        processor.setSlotParam(slot,0,juce::jlimit(0.f,1.f,(frequency-3500.f)/8000.f));
    } else if(type==SkellPresetAudioProcessor::EQ){
        const auto x=juce::jlimit(0.f,1.f,(point.x-33.f)/835.f);
        const auto y=juce::jlimit(-1.f,1.f,(254.f-point.y)/103.f);
        const float freq=20.f*std::pow(1000.f,x);
        processor.setSlotParam(slot,0,juce::jlimit(0.f,1.f,
            (float)(std::log(freq/40.f)/std::log(300.))));
        const auto* tone=processor.state.getRawParameterValue("macro1");
        const float macro=tone?tone->load():.5f;
        processor.setSlotParam(slot,1,juce::jlimit(0.f,1.f,
            .5f+y*1.5f-8.f*(macro-.5f)/24.f));
    } else if(isPagedEQ(type)) {
        const float low=type==SkellPresetAudioProcessor::StudioEQFX?20.f:
            skellpreset::dsp::EightBandTone::edges[(size_t)activeEqBand];
        const float high=type==SkellPresetAudioProcessor::StudioEQFX?20000.f:
            skellpreset::dsp::EightBandTone::edges[(size_t)activeEqBand+1];
        const float normalized=juce::jlimit(0.f,1.f,(point.x-33.f)/835.f);
        const float freq=20.f*std::pow(1000.f,normalized);
        const float x=juce::jlimit(0.f,1.f,
            std::log(juce::jlimit(low,high,freq)/low)/std::log(high/low));
        const float gain=eqGainAtY(type,point.y);
        processor.setSlotParam(slot,activeEqBand*2,x);
        const int shape=type==SkellPresetAudioProcessor::StudioEQFX?
            studioEqBandType(activeEqBand):0;
        if(shape!=1 && shape!=2)processor.setSlotParam(slot,activeEqBand*2+1,gain);
    } else if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
        if(gestureSuiteParameter>=10 && gestureSuiteParameter<=14)
            processor.setSlotParam(slot,gestureSuiteParameter,
                juce::jlimit(0.f,1.f,(319.f-point.y)/111.f));
    } else if(type!=SkellPresetAudioProcessor::Empty) {
        // Unified gesture: the same XY marker is used by each bespoke display.
        const float x=juce::jlimit(0.f,1.f,(point.x-57.f)/787.f);
        const float y=juce::jlimit(0.f,1.f,(330.f-point.y)/159.f);
        processor.setSlotParam(slot,0,x);
        processor.setSlotParam(slot,1,y);
    }
    graphSnapshotValid=false;
    repaint();
}
bool SlotEditor::updateSkelldizerCard(juce::Point<float> point) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::SkelldizerFX)
        return false;
    for(int k=0;k<4;++k) {
        if(!juce::Rectangle<float>(69.f+k*188.f,190.f,170.f,48.f).contains(point))
            continue;
        const auto id="s"+juce::String(slot+1)+"_p2";
        if(auto* v=processor.state.getParameter(id))v->beginChangeGesture();
        processor.setSlotParam(slot,1,(float)k/3.f);
        if(auto* v=processor.state.getParameter(id))v->endChangeGesture();
        graphSnapshotValid=false;
        refreshIfChanged();repaint();
        return true;
    }
    return false;
}
bool SlotEditor::updateMicCard(juce::Point<float> point) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::MicModelFX)
        return false;
    int parameter=-1,index=-1;
    for(int n=0;n<4;++n) {
        if(juce::Rectangle<float>(52.f+n*91.f,188.f,85.f,33.f).contains(point)) {
            parameter=0;index=n;break;
        }
        const int column=n%2,row=n/2;
        if(juce::Rectangle<float>(52.f+column*183.f,255.f+row*43.f,174.f,37.f)
               .contains(point)) {
            parameter=1;index=n;break;
        }
    }
    if(parameter<0)return false;
    if(auto* host=processor.state.getParameter(
            "s"+juce::String(slot+1)+"_p"+juce::String(parameter+1)))
        host->beginChangeGesture();
    processor.setSlotParam(slot,parameter,(float)index/3.f);
    if(auto* host=processor.state.getParameter(
            "s"+juce::String(slot+1)+"_p"+juce::String(parameter+1)))
        host->endChangeGesture();
    graphSnapshotValid=false;
    refreshIfChanged();
    repaint();
    return true;
}
bool SlotEditor::testMicModelCards() {
    const int oldType=processor.slotType(slot);
    std::array<float,4> old{};
    for(int i=0;i<4;++i)old[(size_t)i]=processor.getSlotParam(slot,i);
    processor.setSlotType(slot,SkellPresetAudioProcessor::MicModelFX);
    select(slot);
    const bool a=updateMicCard({343.f,205.f}); // input: Ribbon
    const bool b=updateMicCard({258.f,312.f}); // target: Air
    const bool passed=a&&b&&
        skellpreset::dsp::MicModelTone::index(processor.getSlotParam(slot,0))==3 &&
        skellpreset::dsp::MicModelTone::index(processor.getSlotParam(slot,1))==3 &&
        processor.slotType(slot)==SkellPresetAudioProcessor::MicModelFX;
    processor.setSlotType(slot,oldType);
    for(int i=0;i<4;++i)processor.setSlotParam(slot,i,old[(size_t)i]);
    select(slot);
    return passed;
}
void SlotEditor::setModOrbForScreenshot(bool enabled) {
    modOrbView=enabled;
    modOrbButton.setButtonText(modOrbView?"GRAPH":"BONES");
    modOrbButton.setColour(juce::TextButton::buttonColourId,
        modOrbView?Theme::green.withAlpha(.22f):Theme::panelAlt);
    modOrbButton.setColour(juce::TextButton::textColourOffId,
        modOrbView?Theme::green:Theme::white);
    graphSnapshotValid=false;
    repaint();
}
bool SlotEditor::testModOrbToggle() {
    const auto previousType=processor.slotType(slot);
    processor.setSlotType(slot,SkellPresetAudioProcessor::VocalChorusFX);select(slot);
    const bool ok=!modOrbButton.isVisible() && modOrbButton.getBounds().isEmpty() && testIntegratedEffectLayout();
    processor.setSlotType(slot,previousType);select(slot);return ok;
}
bool SlotEditor::addStudioEqNodeAt(juce::Point<float> point){
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::StudioEQFX ||
       !juce::Rectangle<float>(33.f,151.f,835.f,206.f).contains(point))
        return false;
    // Double-click an existing dot to select, not create a duplicate. Check
    // *both* axes, because several filters can share a cutoff frequency.
    for(int band=0;band<8;++band){
        if(!studioEqBandEnabled(band))continue;
        const float hz=eqVisualFrequency(processor,slot,band,
                                          SkellPresetAudioProcessor::StudioEQFX);
        const float px=33.f+835.f*std::log(hz/20.f)/std::log(1000.f);
        const int shape=studioEqBandType(band);
        const float db=(shape==1||shape==2)?0.f:
            (processor.getSlotParam(slot,band*2+1)-.5f)*36.f;
        const float py=254.f-db*(206.f/72.f);
        if(std::hypot(point.x-px,point.y-py)<19.f) {
            activeEqBand=band;
            showEqBank(band);
            return false;
        }
    }
    for(int band=0;band<8;++band){
        if(studioEqBandEnabled(band))continue;
        const float frequency=juce::jlimit(0.f,1.f,(point.x-33.f)/835.f);
        const float gain=eqGainAtY(SkellPresetAudioProcessor::StudioEQFX,point.y);
        // Use existing host parameters. No audio-thread allocations and no
        // hidden DSP instances: the newly enabled node is a real filter.
        processor.setSlotParam(slot,band*2,frequency);
        processor.setSlotParam(slot,band*2+1,gain);
        setStudioEqBandType(band,0); // Peak/Bell for new nodes
        setStudioEqBandEnabled(band,true);
        activeEqBand=band;
        showEqBank(band);
        syncStudioEqNodeButtons();
        graphSnapshotValid=false;
        refreshIfChanged();
        return true;
    }
    return false; // Already at eight real filters
}
bool SlotEditor::keyPressed(const juce::KeyPress& key) {
    const int code=key.getKeyCode();
    if(code==juce::KeyPress::escapeKey) {
        const bool hadOverlay=modulePicker.isVisible() ||
            installedBrowser.isVisible() || vocalFeelStyles.isVisible() ||
            eqNodeMenu.isVisible() || duckPanel.isVisible();
        if(hadOverlay)dismissTransientOverlays();
        return hadOverlay;
    }
    if(key.getModifiers().isCommandDown() && (code=='F'||code=='f')) {
        if(!modulePicker.isVisible())openModulePicker();
        return true;
    }
    if(key.getModifiers().isAnyModifierKeyDown())return false;
    const int type=processor.slotType(slot);
    int currentPage=0,maxPages=0;
    if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
        currentPage=suitePage;maxPages=4;
    } else if(type==SkellPresetAudioProcessor::StudioDelayFX){
        currentPage=delayPage;maxPages=4;
    } else if(type==SkellPresetAudioProcessor::StudioLimiterFX){
        return false;
    } else if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX){
        currentPage=matchPage;maxPages=3;
    } else if(isPagedEQ(type)) {
        // Studio EQ has seven bands and a final overlapping B6+B7 page.
        currentPage=type==SkellPresetAudioProcessor::StudioEQFX &&
                    eqBank>=5?3:eqBank/2;
        maxPages=4;
    }
    if(maxPages==0)return false;
    int destination=-1;
    if(code>='1' && code<='4')destination=code-'1';
    else if(code==juce::KeyPress::leftKey)
        destination=(currentPage+maxPages-1)%maxPages;
    else if(code==juce::KeyPress::rightKey)
        destination=(currentPage+1)%maxPages;
    if(destination<0 || destination>=maxPages)return false;
    if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX)
        showSuitePage(destination);
    else if(type==SkellPresetAudioProcessor::StudioDelayFX)
        showDelayPage(destination);
    else if(type==SkellPresetAudioProcessor::StudioLimiterFX)
        showLimiterPage(destination);
    else if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX)
        showMatchPage(destination);
    else if(isPagedEQ(type))
        showEqBank(2*destination);
    graphSnapshotValid=false;repaint();
    return true;
}
bool SlotEditor::testLiveViewForTest() {
    const int previous=processor.slotType(slot);
    const auto previousPresentation=processor.presentation();
    processor.setSlotType(slot,SkellPresetAudioProcessor::EQ);select(slot);
    const auto* wet=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_wet");
    const float oldWet=wet->load();
    processor.setPresentation(SkellPresetAudioProcessor::Presentation::Clean);syncPresentation();
    const bool cleanIgnored=!liveView.isVisible() && !livePanel.isVisible();
    processor.setPresentation(SkellPresetAudioProcessor::Presentation::Visual);syncPresentation();
    const bool visualIgnored=!liveView.isVisible() && !livePanel.isVisible() && wet->load()==oldWet;
    processor.setPresentation(previousPresentation);
    processor.setSlotType(slot,previous);select(slot);
    return cleanIgnored && visualIgnored;
}
bool SlotEditor::testEffectPageShortcutsForTest() {
    const int previousType=processor.slotType(slot);
    const int previousSlot=slot;
    bool okay=true;
    const juce::ModifierKeys plain;
    const juce::KeyPress right(juce::KeyPress::rightKey,plain,0);
    const juce::KeyPress left(juce::KeyPress::leftKey,plain,0);
    constexpr int types[]{
        SkellPresetAudioProcessor::VocalFeelSuiteFX,
        SkellPresetAudioProcessor::StudioDelayFX,
        SkellPresetAudioProcessor::AlchemistMatchFinishFX,
        SkellPresetAudioProcessor::EightBandToneFX,
        SkellPresetAudioProcessor::StudioEQFX
    };
    for(const int type:types) {
        processor.setSlotType(previousSlot,type);
        select(previousSlot);
        const int maxPages=type==SkellPresetAudioProcessor::StudioLimiterFX?2:
            type==SkellPresetAudioProcessor::AlchemistMatchFinishFX?3:4;
        okay=okay && keyPressed(right);
        const int afterRight=type==SkellPresetAudioProcessor::VocalFeelSuiteFX?suitePage:
            type==SkellPresetAudioProcessor::StudioDelayFX?delayPage:
            type==SkellPresetAudioProcessor::StudioLimiterFX?limiterPage:
            type==SkellPresetAudioProcessor::AlchemistMatchFinishFX?matchPage:
            type==SkellPresetAudioProcessor::StudioEQFX && eqBank>=5?3:eqBank/2;
        okay=okay && afterRight==1 && keyPressed(left);
        const int afterLeft=type==SkellPresetAudioProcessor::VocalFeelSuiteFX?suitePage:
            type==SkellPresetAudioProcessor::StudioDelayFX?delayPage:
            type==SkellPresetAudioProcessor::StudioLimiterFX?limiterPage:
            type==SkellPresetAudioProcessor::AlchemistMatchFinishFX?matchPage:
            type==SkellPresetAudioProcessor::StudioEQFX && eqBank>=5?3:eqBank/2;
        okay=okay && afterLeft==0;
        okay=okay && keyPressed(juce::KeyPress('1'+maxPages-1,plain,0));
        const int afterLast=type==SkellPresetAudioProcessor::VocalFeelSuiteFX?suitePage:
            type==SkellPresetAudioProcessor::StudioDelayFX?delayPage:
            type==SkellPresetAudioProcessor::StudioLimiterFX?limiterPage:
            type==SkellPresetAudioProcessor::AlchemistMatchFinishFX?matchPage:
            type==SkellPresetAudioProcessor::StudioEQFX && eqBank>=5?3:eqBank/2;
        okay=okay && afterLast==maxPages-1;
        const bool fourthHandled=keyPressed(juce::KeyPress('4',plain,0));
        okay=okay && fourthHandled==(maxPages==4);
    }
    processor.setSlotType(previousSlot,previousType);
    select(previousSlot);
    const juce::ModifierKeys command(juce::ModifierKeys::commandModifier);
    okay=okay && keyPressed(juce::KeyPress('F',command,0));
    okay=okay && modulePicker.isVisible();
    okay=okay && keyPressed(juce::KeyPress(juce::KeyPress::escapeKey,plain,0));
    okay=okay && !modulePicker.isVisible();
    return okay && processor.slotType(previousSlot)==previousType;
}
void SlotEditor::mouseDoubleClick(const juce::MouseEvent& e){
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::StudioEQFX)return;
    // A second mouse-down can precede this callback. Close the pending
    // old-band host gesture before inserting another filter.
    if(draggingGraph)mouseUp(e);
    addStudioEqNodeAt(e.position);
}
void SlotEditor::mouseMove(const juce::MouseEvent&) {setMouseCursor(juce::MouseCursor::NormalCursor);}
void SlotEditor::mouseExit(const juce::MouseEvent&) {setMouseCursor(juce::MouseCursor::NormalCursor);}
void SlotEditor::mouseDown(const juce::MouseEvent& e){
    const int type=processor.slotType(slot);
    if(e.mods.isPopupMenu() && type!=SkellPresetAudioProcessor::StudioEQFX)return;
    if(eqNodeMenu.isVisible()){eqNodeMenu.close();return;}
    if(vocalFeelStyles.isVisible()){vocalFeelStyles.close();return;}
    if(type==SkellPresetAudioProcessor::Empty) {
        if(e.mods.isLeftButtonDown() && e.getNumberOfClicks()==1 &&
           juce::Rectangle<int>(333,204,240,90).contains(e.getPosition()))
            openModulePicker();
        return;
    }
    // A read-only analyzer must not silently edit the return parameters.
    if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX &&
       suiteSpectrumView && juce::Rectangle<int>(33,151,835,206).contains(e.getPosition()))
        return;
    if(type>=SkellPresetAudioProcessor::AdlibChop &&
       type<=SkellPresetAudioProcessor::AdlibSaturate &&
       juce::Rectangle<int>(33,151,835,206).contains(e.getPosition()))
        return; // Adlib response is a visual diagnostic, never a fake XY editor
    if(isModulationOrbEffect(type) && modOrbView)
        return; // display-only modulation orb must not alter DSP automation
    if(type==SkellPresetAudioProcessor::SkelldizerFX) {
        updateSkelldizerCard(e.position);
        return; // no invisible XY changes: only pads and rotary knobs edit DSP
    }
    if(type==SkellPresetAudioProcessor::MicModelFX) {
        // Card clicks are distinct from the generic XY graph gesture.
        updateMicCard(e.position);
        return;
    }
    if(skellpreset::face::hardware(type) || type==SkellPresetAudioProcessor::Empty
        || type==SkellPresetAudioProcessor::ExternalVST3
        || (type==SkellPresetAudioProcessor::StudioLimiterFX && limiterPage==1)
        || (type==SkellPresetAudioProcessor::StudioDelayFX && delayPage!=0)
        || (type==SkellPresetAudioProcessor::AlchemistMatchFinishFX && matchPage!=0)
        || !juce::Rectangle<int>(33,151,835,206).contains(e.getPosition()))return;
    if(type==SkellPresetAudioProcessor::StudioEQFX && e.mods.isPopupMenu()){
        int best=-1;
        float distance=std::numeric_limits<float>::max();
        for(int band=0;band<8;++band){
            if(!studioEqBandEnabled(band))continue;
            const float hz=eqVisualFrequency(processor,slot,band,type);
            const float bx=33.f+835.f*std::log(hz/20.f)/std::log(1000.f);
            const int shape=studioEqBandType(band);
            const float db=(shape==1||shape==2)?0.f:
                (processor.getSlotParam(slot,band*2+1)-.5f)*36.f;
            const float by=254.f-db*206.f/72.f;
            const float d=std::hypot(bx-(float)e.x,by-(float)e.y);
            if(d<distance){distance=d;best=band;}
        }
        if(best<0 || distance>25.f)return;
        contextEqBand=activeEqBand=best;
        showEqBank(best);
        eqNodeMenu.setTopLeftPosition(juce::jlimit(12,688,e.x-60),
                                      juce::jlimit(150,286,e.y-18));
        eqNodeMenu.open(studioEqBandType(best));
        return;
    }
    if(e.mods.isPopupMenu() || !e.mods.isLeftButtonDown())return;
    if(isPagedEQ(type) && type==SkellPresetAudioProcessor::StudioEQFX){
        bool enabled=false;
        for(int b=0;b<8;++b)enabled=enabled||studioEqBandEnabled(b);
        if(!enabled)return;
    }
    endGraphGesture();
    draggingGraph=true;
    gestureSlot=slot;gestureType=type;
    grabKeyboardFocus(); // only the graph, never steal slider text-entry focus
    if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX) {
        gestureSuiteParameter=juce::jlimit(10,14,10+(int)((e.x-49.f)/163.f));
        if(auto* parameter=processor.state.getParameter(
                "s"+juce::String(slot+1)+"_p"+juce::String(gestureSuiteParameter+1)))
            parameter->beginChangeGesture();
        updateGraphNode(e.position);
        return;
    }
    if(isPagedEQ(type)){
        float nearest=std::numeric_limits<float>::max();
        const int bands=8;
        for(int b=0;b<bands;++b){
            if(type==SkellPresetAudioProcessor::StudioEQFX &&
               !studioEqBandEnabled(b))continue;
            const float freq=eqVisualFrequency(processor,slot,b,type);
            const float bx=33.f+835.f*std::log(freq/20.f)/std::log(1000.f);
            const int shape=type==SkellPresetAudioProcessor::StudioEQFX?studioEqBandType(b):0;
            const float db=(shape==1||shape==2)?0.f:
                (processor.getSlotParam(slot,b*2+1)-.5f)*
                    (type==SkellPresetAudioProcessor::StudioEQFX?36.f:30.f);
            const float by=254.f-db*206.f/72.f;
            const float dist=std::hypot(bx-(float)e.x,by-(float)e.y);
            if(dist<nearest){nearest=dist;activeEqBand=b;}
        }
        showEqBank(activeEqBand);
        gestureEqBand=activeEqBand;
        for(int k=0;k<2;++k)
            if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+
                          "_p"+juce::String(gestureEqBand*2+k+1)))
                p->beginChangeGesture();
    } else {
        gestureEqBand=-1;
        for(int param:{1,2})
            if(type!=SkellPresetAudioProcessor::DeEsser || param==1)
            if(auto*p=processor.state.getParameter("s"+juce::String(slot+1)+
                                                  "_p"+juce::String(param)))
                p->beginChangeGesture();
    }
    // Select on mouse-down, mutate only while dragging. This keeps the
    // first click of an empty-area double-click from moving a nearby dot
    // into that position and preventing a new node from being added.
    if(!isPagedEQ(type))updateGraphNode(e.position);
}
void SlotEditor::mouseDrag(const juce::MouseEvent& e){
    if(!draggingGraph)return;
    if(slot!=gestureSlot || processor.slotType(slot)!=gestureType){
        endGraphGesture();return;
    }
    updateGraphNode(e.position);
}
void SlotEditor::mouseUp(const juce::MouseEvent&){endGraphGesture();}
void SlotEditor::endGraphGesture(){
    if(!draggingGraph)return;
    const auto prefix="s"+juce::String(gestureSlot+1)+"_p";
    if(gestureSuiteParameter>=0){
        if(auto* p=processor.state.getParameter(prefix+juce::String(gestureSuiteParameter+1)))
            p->endChangeGesture();
    } else {
        for(int k=0;k<(gestureType==SkellPresetAudioProcessor::DeEsser?1:2);++k){
            const int param=gestureEqBand>=0?gestureEqBand*2+k+1:k+1;
            if(auto* p=processor.state.getParameter(prefix+juce::String(param)))
                p->endChangeGesture();
        }
    }
    draggingGraph=false;gestureSlot=-1;gestureType=-1;
    gestureEqBand=-1;gestureSuiteParameter=-1;
}

bool SlotEditor::testPowerDotSync() {
    const int original=slot;
    const int type=processor.slotType(slot);
    const auto* raw=processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_on");
    if(!raw)return false;
    const bool initial=raw->load()>.5f;
    if(type==SkellPresetAudioProcessor::Empty) {
        // Scratch only the test slot, then restore the same empty identity.
        processor.setSlotType(slot,SkellPresetAudioProcessor::Compressor);
        processor.setSlotOn(slot,true);
        refreshIfChanged();
    }
    const bool before=processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_on")->load()>.5f;
    const bool clickable=slotPower.isEnabled();
    slotPower.onClick();
    const bool toggled=(processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_on")->load()>.5f)!=before;
    slotPower.onClick();
    const bool restored=(processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_on")->load()>.5f)==before;
    if(type==SkellPresetAudioProcessor::Empty) {
        processor.setSlotOn(slot,initial);
        processor.setSlotType(slot,SkellPresetAudioProcessor::Empty);
    } else processor.setSlotOn(slot,initial);
    select(original);
    return clickable && toggled && restored;
}
bool SlotEditor::testAllNativeEffectLayoutsForTest() {
    // Enumerate every native effect through the ACTUAL APVTS-backed JUCE
    // editor. We use the spare 16th physical slot on the test processor;
    // published factory IDs are untouched and the caller reloads its fixture.
    const int originalSelected=slot;
    constexpr int probeSlot=SkellPresetAudioProcessor::slotCount-1;
    bool clean=true;
    for(int type=0;type<SkellPresetAudioProcessor::NumberOfModules;++type) {
        // External VST3 belongs to its independent native plugin browser;
        // MicModelFX is a legacy slot compatibility type with its own MIC UI.
        if(type==SkellPresetAudioProcessor::ExternalVST3 ||
           type==SkellPresetAudioProcessor::MicModelFX)continue;
        processor.setSlotType(probeSlot,type);
        processor.setSlotOn(probeSlot,type!=SkellPresetAudioProcessor::Empty);
        select(probeSlot);
        if(type==SkellPresetAudioProcessor::DeEsser){
            const float threshold=processor.getSlotParam(probeSlot,1);
            const float x=33.f+835.f*std::log(6000.f/20.f)/std::log(1000.f);
            updateGraphNode({x,220.f});
            clean=clean && std::abs(processor.getSlotParam(probeSlot,0)-.3125f)<.002f &&
                std::abs(processor.getSlotParam(probeSlot,1)-threshold)<.0001f;
        }
        if(type==SkellPresetAudioProcessor::StudioCompressorFX) {
            const bool kneeOK=studioCompKnee.textFromValueFunction &&
                studioCompKnee.valueFromTextFunction &&
                studioCompKnee.textFromValueFunction(.5)=="50%" &&
                std::abs(studioCompKnee.valueFromTextFunction("75%")-.75)<1.e-6 &&
                studioCompKnee.textFromValueFunction(0.)=="0%" &&
                studioCompKnee.textFromValueFunction(1.)=="100%";
            if(!kneeOK){std::fprintf(stderr,"Studio knee physical readout failed after attachment\n");clean=false;}
        }
        const bool geometryOK=testIntegratedEffectLayout();
        const bool popupsOK=testPopupLayoutsForTest();
        if(!geometryOK || !popupsOK){
            std::fprintf(stderr,"Native UI geometry failure: type=%d geometry=%d popup=%d\n",
                         type,(int)geometryOK,(int)popupsOK);
            clean=false;
        }
        for(size_t index=0;index<knobs.size();++index) {
            const bool expected=skellpreset::face::visible(type,(int)index);
            if(knobs[index].isVisible()!=expected) {
                std::fprintf(stderr,"Native UI knob visibility: type=%d knob=%zu expected=%d actual=%d\n",
                             type,index,(int)expected,(int)knobs[index].isVisible());clean=false;
            }
        }
        if(!clean)break;
    }
    // This method is only used from the headless UI smoke, immediately
    // followed by an explicit factory reload. Never leave a probe type
    // active in a user session.
    processor.setSlotType(probeSlot,SkellPresetAudioProcessor::Empty);
    processor.setSlotOn(probeSlot,false);
    select(originalSelected);
    return clean;
}
bool SlotEditor::testIntegratedEffectLayout() const {
    if(slotPower.getBounds()!=juce::Rectangle<int>(843,22,40,40) ||
       slotPower.getBounds().intersects(editExternal.getBounds()) ||
       !getLocalBounds().contains(slotPower.getBounds()))return false;
    const int faceType=processor.slotType(slot);
    const juce::Rectangle<int> categoryBounds(528,10,294,18);
    for(const auto& button:matchPageButtons)
        if(button.isVisible() && categoryBounds.intersects(button.getBounds()))return false;
    if(slotPower.isVisible()!=(faceType!=SkellPresetAudioProcessor::Empty))return false;
    if(skellpreset::face::hardware(faceType)) {
        for(int i=0;i<4;++i) {
            const auto& knob=knobs[(size_t)i];
            if(!skellpreset::face::visible(faceType,i))continue;
            const auto r=knob.getBounds();
            if(r!=skellpreset::face::control(faceType,i) || !getLocalBounds().contains(r) ||
               knob.getTextBoxHeight()<26 || r.intersects(slotPower.getBounds()))return false;
            for(int j=i+1;j<4;++j)if(skellpreset::face::visible(faceType,j) && r.intersects(knobs[(size_t)j].getBounds()))return false;
        }
        return true;
    }
    for(int i=0;i<4;++i) {
        const auto r=knobs[(size_t)i].getBounds();
        const int type=processor.slotType(slot);
        const bool compact=type==SkellPresetAudioProcessor::StudioLimiterFX && !limiterDetailsVisible;
        const auto card=parameterCardBounds(type,i,compact);
        if(!getLocalBounds().contains(r) || !getLocalBounds().contains(card) ||
           !card.contains(r) || r.getY()<405 || r.getBottom()>521 ||
           r.intersects(slotPower.getBounds()) ||
           knobs[(size_t)i].getTextBoxHeight()<26 ||
           knobs[(size_t)i].getSliderStyle()!=(rotaryControl(type,i)?
               juce::Slider::RotaryHorizontalVerticalDrag:juce::Slider::LinearHorizontal))
            return false;
        if(compact && i<2 && card.getWidth()<400)return false;
        for(int j=i+1;j<4;++j){
            // Hidden limiter parameters retain attachments but occupy no visible space.
            if(compact && j>=2)continue;
            if(r.intersects(knobs[(size_t)j].getBounds()) ||
               card.intersects(parameterCardBounds(type,j,compact)))return false;
        }
    }
    return true;
}
bool SlotEditor::testClassicDynamicsUnits() {
    // Headless test only: save and restore all existing slot parameters.
    const int physical=slot, originalType=processor.slotType(physical);
    std::array<float,9> original{};
    for(int i=0;i<9;++i)original[(size_t)i]=processor.getSlotParam(physical,i);
    processor.setSlotType(physical,SkellPresetAudioProcessor::Compressor);
    select(physical);
    const bool compressor=knobs[0].getTextFromValue(.5).contains("dBFS") &&
        knobs[1].getTextFromValue(.5)=="6.5:1" &&
        knobs[2].getTextFromValue(.5).contains("ms") &&
        knobs[1].valueFromTextFunction &&
        std::abs(knobs[1].valueFromTextFunction("6.5:1")-.5)<.002;
    processor.setSlotType(physical,SkellPresetAudioProcessor::Gate);
    select(physical);
    const bool gate=knobs[0].getTextFromValue(.5)=="-40.0 dBFS" &&
        knobs[2].getTextFromValue(.5)=="60 ms" &&
        knobs[0].valueFromTextFunction &&
        std::abs(knobs[0].valueFromTextFunction("-40 dBFS")-.5)<.002;
    processor.setSlotType(physical,SkellPresetAudioProcessor::DeEsser);
    select(physical);
    const bool deesser=knobs[0].getTextFromValue(.5)=="7.50 kHz" &&
        knobs[1].getTextFromValue(.5).contains("dBFS") &&
        knobs[0].valueFromTextFunction &&
        std::abs(knobs[0].valueFromTextFunction("7.5 kHz")-.5)<.002;
    processor.setSlotType(physical,originalType);
    for(int i=0;i<9;++i)processor.setSlotParam(physical,i,original[(size_t)i]);
    select(physical);
    return compressor&&gate&&deesser;
}
bool SlotEditor::testKnobReadouts() {
    const auto frequency=knobs[0].getTextFromValue(.5);
    const auto gain=knobs[1].getTextFromValue(.5);
    const auto bandwidth=eqQSlider.getTextFromValue(.5);
    return processor.slotType(slot)==SkellPresetAudioProcessor::EightBandToneFX
        && frequency.containsIgnoreCase("Hz")
        && gain.containsIgnoreCase("dB")
        && bandwidth.isNotEmpty() && bandwidth.length()<=5 &&
           eqQSlider.valueFromTextFunction &&
           std::abs(eqQSlider.valueFromTextFunction(bandwidth)-.5)<.015;
}

bool SlotEditor::testStudioEqGraphDoubleClick() {
    const int physical=slot;
    processor.setSlotType(physical,SkellPresetAudioProcessor::StudioEQFX);
    processor.setSlotOn(physical,true);
    select(physical);
    setStudioEqBandEnabled(7,false);
    const auto click=juce::Point<float>(700.f,271.f);
    const bool inserted=addStudioEqNodeAt(click);
    const float wantHz=(700.f-33.f)/835.f;
    const float wantGain=.5f+(254.f-271.f)*2.f/206.f;
    const bool mapped=inserted && studioEqBandEnabled(7)
        && activeEqBand==7 && eqBank==6
        && std::abs(processor.getSlotParam(physical,14)-wantHz)<.015f
        && std::abs(processor.getSlotParam(physical,15)-wantGain)<.015f
        && studioEqBandType(7)==0 && !addEqNode.isEnabled();
    // A double-click directly on B8 must select it, not create a ninth node.
    const bool noDuplicate=!addStudioEqNodeAt(click)
        && studioEqBandEnabled(7) && activeEqBand==7;
    updateGraphNode({700.f,235.f});
    const bool dragMapped=std::abs(processor.getSlotParam(physical,15)-
        eqGainAtY(SkellPresetAudioProcessor::StudioEQFX,235.f))<.002f;
    setStudioEqBandType(7,1);
    const float before=processor.getSlotParam(physical,15);
    updateGraphNode({650.f,211.f});
    const bool cutGainPreserved=std::abs(processor.getSlotParam(physical,15)-before)<.0001f;
    setStudioEqBandType(7,0);
    setStudioEqBandEnabled(7,false);
    return mapped && noDuplicate && dragMapped && cutGainPreserved;
}

bool SlotEditor::testStudioEqAddAndFilterTypes() {
    const int physical=slot;
    processor.setSlotType(physical,SkellPresetAudioProcessor::StudioEQFX);
    processor.setSlotOn(physical,true);
    select(physical);
    const bool seven=addEqNode.isVisible() && addEqNode.isEnabled()
                     && !studioEqBandEnabled(7);
    addEqNode.onClick();
    const bool eighth=studioEqBandEnabled(7) && eqBank==6 &&
        activeEqBand==7 && eqBandButtons[7].isVisible()
        && !addEqNode.isEnabled();
    knobs[2].setValue(.78,juce::sendNotificationSync); // B8 frequency p15
    const bool editable=std::abs(processor.getSlotParam(physical,14)-.78f)<.015f;
    contextEqBand=7;
    eqNodeMenu.open(0);eqNodeMenu.chooseTypeForTest(1);
    const bool hp=studioEqBandType(7)==1;
    eqNodeMenu.open(1);eqNodeMenu.chooseTypeForTest(2);
    const bool lp=studioEqBandType(7)==2;
    eqNodeMenu.open(2);eqNodeMenu.removeForTest();
    const bool removed=!studioEqBandEnabled(7) && !eqBandButtons[7].isVisible()
                    && addEqNode.isEnabled();
    return seven && eighth && editable && hp && lp && removed;
}
bool SlotEditor::testStudioEqSeventhBand() {
    const int physical=slot;
    processor.setSlotType(physical,SkellPresetAudioProcessor::StudioEQFX);
    processor.setSlotOn(physical,true);
    select(physical);
    // The seventh visible node must have real, accessible frequency/gain
    // rotaries, not a disguised duplicate page 5/6 or unused B8 controls.
    eqBandButtons[6].onClick();
    const bool page=eqBank==5 && activeEqBand==6 &&
        eqBandButtons[6].isVisible() &&
        !eqBandButtons[7].isVisible() && moduleSelectorMatchesAudio();
    const float previousSix=processor.getSlotParam(physical,10);
    const float previousSeven=processor.getSlotParam(physical,12);
    knobs[2].setValue(.77,juce::sendNotificationSync); // B7 frequency p13
    knobs[3].setValue(.72,juce::sendNotificationSync); // B7 gain p14
    eqQSlider.setValue(.63,juce::sendNotificationSync); // B7 width eqQ7
    const auto* width=processor.state.getRawParameterValue(
        "s"+juce::String(physical+1)+"_eqQ7");
    const bool controls=std::abs(processor.getSlotParam(physical,12)-.77f)<.015f
        && std::abs(processor.getSlotParam(physical,13)-.72f)<.015f
        && std::abs(processor.getSlotParam(physical,10)-previousSix)<1.e-5f
        && std::abs(processor.getSlotParam(physical,12)-previousSeven)>.05f
        && width && std::abs(width->load()-.63f)<.015f
        && eqQSlider.getTextFromValue(.63)=="0.63" &&
           eqQSlider.valueFromTextFunction &&
           std::abs(eqQSlider.valueFromTextFunction("0.63")-.63)<.01;
    return page && controls;
}

bool SlotEditor::testEightBandGraphAndQ() {
    const auto physical=slot;
    processor.setSlotType(physical,SkellPresetAudioProcessor::EightBandToneFX);
    select(physical);
    // User-facing band buttons must select the right automatable slot band.
    eqBandButtons[7].onClick();
    const float oldFreq=processor.getSlotParam(physical,14);
    const float oldGain=processor.getSlotParam(physical,15);
    updateGraphNode({790.f,225.f});
    const auto freq=processor.getSlotParam(physical,14);
    const auto gain=processor.getSlotParam(physical,15);
    eqQSlider.setValue(.86,juce::sendNotificationSync);
    eqCutSliders[0].setValue(.58,juce::sendNotificationSync);
    eqCutSliders[1].setValue(.45,juce::sendNotificationSync);
    const auto* cutLo=processor.state.getRawParameterValue(
        "s"+juce::String(physical+1)+"_eqLowCut");
    const auto* cutHi=processor.state.getRawParameterValue(
        "s"+juce::String(physical+1)+"_eqHighCut");
    const auto* q=processor.state.getRawParameterValue(
        "s"+juce::String(physical+1)+"_eqQ8");
    const bool good=eqBank==6 && activeEqBand==7 &&
        std::abs(freq-oldFreq)>.05f && std::abs(gain-oldGain)>.05f
        && q && std::abs(q->load()-.86f)<.015f
        && cutLo && std::abs(cutLo->load()-.58f)<.015f
        && cutHi && std::abs(cutHi->load()-.45f)<.015f
        && moduleSelectorMatchesAudio();
    // Restore neutral filters so screenshot/gesture probes do not alter the
    // sound of the selected rack slot after testing.
    eqCutSliders[0].setValue(0.,juce::sendNotificationSync);
    eqCutSliders[1].setValue(0.,juce::sendNotificationSync);
    return good;
}

bool SlotEditor::testExternalBrowseCancel() {
    const int oldType=processor.slotType(slot);
    const auto* power=processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_on");
    const bool onBefore=power && power->load()>.5f;
    if(!modulePicker.onExternal)return false;
    modulePicker.onExternal();
    const bool opened=installedBrowser.isVisible()
        && processor.slotType(slot)==oldType
        && power && (power->load()>.5f)==onBefore;
    installedBrowser.hide();
    const bool canceled=!installedBrowser.isVisible()
        && !externalBrowsePending
        && processor.slotType(slot)==oldType
        && power && (power->load()>.5f)==onBefore;
    return opened && canceled;
}

bool SlotEditor::testModulePickerInteraction() {
    if(!modulePicker.filterMatchesForTest("alien",SkellPresetAudioProcessor::RingModFX))
        return false;
    if(!modulePicker.filterMatchesForTest("reverb",SkellPresetAudioProcessor::VoxRackRoom))
        return false;
    if(!modulePicker.filterMatchesForTest("choruz",SkellPresetAudioProcessor::VocalChorusFX))
        return false;
    if(modulePicker.filterMatchesForTest("compressor",SkellPresetAudioProcessor::VocalChorusFX))
        return false;
    openModulePicker();
    if(!modulePicker.isVisible() || !modulePicker.testVisibleKeyboardRow()
       || !modulePicker.testResizeKeepsSizeForTest())
        return false;
    openModulePicker();
    if(modulePicker.isVisible())return false;
    openModulePicker();
    modulePicker.chooseForTest(SkellPresetAudioProcessor::RingModFX);
    const auto* type=processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_type");
    return !modulePicker.isVisible() && type
        && (int)type->load()==SkellPresetAudioProcessor::RingModFX
        && moduleSelectorMatchesAudio();
}
