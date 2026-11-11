#include "SlotEditor.h"
#include "Theme.h"
#include <cmath>
#include <limits>
namespace {
const juce::StringArray names{"Empty","EQ","Compressor","Limiter","Gate","Saturator","Reverb","Delay","Exciter","Adlib Throw","Adlib Doubler","De-Esser","External VST3","Broadcast Radio","Retro Crush","Tremolo","Stereo Width","Three-Band Dynamics","VoxRack Room","VoxRack Smooth Comp","Vocal Chorus","Vocal Flanger","Ring Modulator","Vocal Rider","Ducking Ping-Pong","Vocal Vibrato","8-Band EQ","FET + Opto","Harshness Tamer","Adaptive Noise Control"};
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
        default:return {"Control 1","Control 2","Control 3","Control 4"};
    }
}
}
SlotEditor::SlotEditor(SkellPresetAudioProcessor& p):
    processor(p),modulePicker(p) {
    for(int i=0;i<names.size();i++)moduleBox.addItem(names[i],i+1);
    moduleBox.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
    moduleBox.setColour(juce::ComboBox::textColourId,Theme::white);
    moduleBox.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    addChildComponent(moduleBox); // retain selected IDs for host/UI assertions
    moduleBox.setVisible(false);
    moduleTrigger.setTooltip("Browse native audio effects by name and category");
    moduleTrigger.onClick=[this]{
        if(modulePicker.isVisible())modulePicker.hide();
        else modulePicker.open(processor.slotType(slot));
    };
    modulePicker.onSelect=[this](int type){
        processor.setSlotType(slot,type);
        moduleBox.setSelectedId(type+1,juce::dontSendNotification);
        moduleTrigger.setButtonText(processor.moduleName(type));
        graphSnapshotValid=false;
        refreshIfChanged();
        repaint();
    };
    addAndMakeVisible(moduleTrigger);
    addAndMakeVisible(modulePicker);
    modulePicker.setVisible(false);
    for(auto* button:{&loadExternal,&editExternal}){
        button->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        button->setColour(juce::TextButton::textColourOffId,Theme::green);
        addAndMakeVisible(*button);
        button->setVisible(false);
    }
    loadExternal.setTooltip("Load an installed third-party VST3 effect into this rack slot");
    editExternal.setTooltip("Open the hosted VST3 editor in a separate window");
    loadExternal.onClick=[this] {
        // JUCE FileChooser must outlive its asynchronous callback.
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
                const auto result=safe->processor.loadHostedVST3(safe->slot,path);
                if(result.failed())juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,"VST3 could not load",
                    result.getErrorMessage());
                safe->graphSnapshotValid=false;
                safe->refreshIfChanged();
            });
    };
    editExternal.onClick=[this] {
        processor.openHostedVST3Editor(slot);
    };
    activeButton.setButtonText("ON");activeButton.setColour(juce::ToggleButton::tickColourId,Theme::green);
    activeButton.setColour(juce::ToggleButton::textColourId,Theme::white);addAndMakeVisible(activeButton);
    wetKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    wetKnob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,21);
    wetKnob.textFromValueFunction=[](double v) {
        return juce::String(juce::roundToInt(juce::jlimit(0.,1.,v)*100.))+"%";
    };
    wetKnob.valueFromTextFunction=[](const juce::String& text) {
        const double value=text.getDoubleValue();
        return juce::jlimit(0.,1.,(text.contains("%")||value>1.)?value/100.:value);
    };
    addAndMakeVisible(wetKnob);
    for(int i=0;i<4;++i){
        auto& k=knobs[(size_t)i];
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,76,21);
        // Human-readable display only; hosts still automate the exact stable
        // normalized parameter range. Never make display conversions in DSP.
        k.textFromValueFunction=[this,i](double v)->juce::String {
            v=juce::jlimit(0.,1.,v);
            const int selectedType=processor.slotType(slot);
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
            if(module==SkellPresetAudioProcessor::EightBandToneFX) {
                const int band=eqBank+i/2;
                if(i%2==0){
                    const double lo=skellpreset::dsp::EightBandTone::edges[(size_t)band];
                    const double hi=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
                    const double hz=lo*std::pow(hi/lo,v);
                    return hz>=1000.?juce::String(hz/1000.,1)+"kHz":
                           juce::String(juce::roundToInt(hz))+"Hz";
                }
                return juce::String((v-.5)*30.,1)+"dB";
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
            if(module==SkellPresetAudioProcessor::EightBandToneFX) {
                if(i%2==1)return juce::jlimit(0.,1.,value/30.+.5);
                const int band=eqBank+i/2;
                const double lo=skellpreset::dsp::EightBandTone::edges[(size_t)band];
                const double hi=skellpreset::dsp::EightBandTone::edges[(size_t)band+1];
                const double hz=juce::jmax(1.,value*(text.contains("khz")?1000.:1.));
                return juce::jlimit(0.,1.,std::log(hz/lo)/std::log(hi/lo));
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
void SlotEditor::applyTheme() {
    Theme::selectFor(processor.state);
    moduleBox.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
    moduleBox.setColour(juce::ComboBox::textColourId,Theme::white);
    moduleBox.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    moduleTrigger.setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
    moduleTrigger.setColour(juce::TextButton::textColourOffId,Theme::white);
    modulePicker.applyTheme();
    activeButton.setColour(juce::ToggleButton::tickColourId,Theme::green);
    activeButton.setColour(juce::ToggleButton::textColourId,Theme::white);
    for(auto* button:{&loadExternal,&editExternal}) {
        button->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
        button->setColour(juce::TextButton::textColourOffId,Theme::green);
    }
    for(auto* s:{&wetKnob,&knobs[0],&knobs[1],&knobs[2],&knobs[3]}) {
        s->setColour(juce::Slider::rotarySliderFillColourId,Theme::green);
        s->setColour(juce::Slider::rotarySliderOutlineColourId,Theme::stroke);
        s->setColour(juce::Slider::thumbColourId,Theme::white);
        s->setColour(juce::Slider::textBoxTextColourId,Theme::white);
        s->setColour(juce::Slider::textBoxBackgroundColourId,Theme::panelAlt);
        s->setColour(juce::Slider::textBoxOutlineColourId,Theme::stroke);
    }
    repaint();
}
void SlotEditor::showEqBank(int firstBand) {
    if(processor.slotType(slot)!=SkellPresetAudioProcessor::EightBandToneFX)return;
    eqBank=juce::jlimit(0,3,firstBand/2)*2;
    const auto prefix="s"+juce::String(slot+1)+"_";
    for(auto& a:knobAttachment)a.reset();
    for(int k=0;k<4;++k){
        const int param=eqBank*2+k+1;
        knobAttachment[(size_t)k]=
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.state,prefix+"p"+juce::String(param),knobs[(size_t)k]);
    }
    graphSnapshotValid=false;
    repaint();
}
void SlotEditor::select(int newSlot) {
    slot=juce::jlimit(0,SkellPresetAudioProcessor::slotCount-1,newSlot);
    eqBank=0;activeEqBand=0;
    if(modulePicker.isVisible())modulePicker.hide();
    graphSnapshotValid=false;
    enabledAttachment.reset();wetAttachment.reset();
    externalChooser.reset();
    for(auto& a:knobAttachment)a.reset();
    const auto prefix="s"+juce::String(slot+1)+"_";
    moduleBox.setSelectedId(processor.slotType(slot)+1,juce::dontSendNotification);
    enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,prefix+"on",activeButton);
    wetAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,prefix+"wet",wetKnob);
    for(int k=0;k<4;k++)knobAttachment[(size_t)k]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,prefix+"p"+juce::String(k+1),knobs[(size_t)k]);
    refreshIfChanged();
    repaint();
}
void SlotEditor::refreshIfChanged() {
    const auto prefix="s"+juce::String(slot+1)+"_";
    const int type=processor.slotType(slot);
    std::array<float,6> current{};
    for(int i=0;i<4;++i)current[(size_t)i]=processor.getSlotParam(slot,i);
    if(const auto* macro=processor.state.getRawParameterValue("macro1"))current[4]=macro->load();
    if(const auto* macro=processor.state.getRawParameterValue("macro2"))current[5]=macro->load();
    bool changed=!graphSnapshotValid || type!=lastType;
    for(int i=0;i<6;++i)changed=changed || std::abs(current[(size_t)i]-lastControls[(size_t)i])>1.e-5f;
    if(!changed)return;
    if(type!=lastType){
        // A new effect resets the EQ bank, preserving all saved parameter
        // values while reattaching only the controls visible on this page.
        eqBank=0;activeEqBand=0;
        for(auto& attachment:knobAttachment)attachment.reset();
        for(int k=0;k<4;++k)
            knobAttachment[(size_t)k]=
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor.state,prefix+"p"+juce::String(k+1),knobs[(size_t)k]);
        moduleBox.setSelectedId(type+1,juce::dontSendNotification);
        moduleTrigger.setButtonText(processor.moduleName(type));
    }
    lastType=type;lastControls=current;graphSnapshotValid=true;
    const bool external=type==SkellPresetAudioProcessor::ExternalVST3;
    loadExternal.setVisible(external);
    editExternal.setVisible(external && processor.hasHostedVST3(slot));
    for(auto& k:knobs){k.setEnabled(!external);k.setVisible(!external);}
    repaint();
}
void SlotEditor::resized() {
    // 210px avoids "VoxRack Smooth Co..." truncation while preserving ON/WET.
    moduleTrigger.setBounds(489,25,211,29);
    moduleBox.setBounds(489,25,211,29);
    modulePicker.setBounds(405,66,470,400);
    activeButton.setBounds(711,26,70,28);
    wetKnob.setBounds(805,22,86,66);
    loadExternal.setBounds(52,420,177,62);
    editExternal.setBounds(246,420,188,62);
    for(int k=0;k<4;k++)knobs[(size_t)k].setBounds(48+211*k,416,97,78);
}
void SlotEditor::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    Theme::panelBox(g,{0,0,906.f,406.f},9.f);
    Theme::panelBox(g,{0,410.f,906.f,86.f},9.f);
    // Subtle rack mounting holes make this feel like a physical processor,
    // without crossing the interactive plot or module controls.
    for(float sx:{12.f,894.f})for(float sy:{13.f,395.f,422.f,483.f}){
        g.setColour(Theme::bg);g.fillEllipse(sx-2.5f,sy-2.5f,5.f,5.f);
        g.setColour(Theme::stroke);g.drawEllipse(sx-2.5f,sy-2.5f,5.f,5.f,1.f);
    }
    const int type=processor.slotType(slot);
    Theme::text(g,"SLOT "+juce::String(slot+1).paddedLeft('0',2),{21,10,200,18},10,Theme::muted,true);
    Theme::text(g,processor.moduleName(type),{21,29,420,38},25,Theme::white,true);
    Theme::text(g,"MODULE",{491,9,140,15},9,Theme::muted,true);
    Theme::text(g,"WET",{821,5,65,15},11,Theme::muted,true,juce::Justification::centred);
    const auto plot=juce::Rectangle<float>(33,151,835,206);
    g.setColour(Theme::plot);g.fillRoundedRectangle(plot,6);
    g.saveState();g.reduceClipRegion(plot.toNearestInt());
    constexpr float frequencies[]{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f};
    if(type==SkellPresetAudioProcessor::EQ || type==SkellPresetAudioProcessor::DeEsser || type==SkellPresetAudioProcessor::EightBandToneFX) {
        for(float freq:frequencies){
            const auto x=plot.getX()+plot.getWidth()*std::log(freq/20.f)/std::log(1000.f);
            g.setColour(Theme::stroke.withAlpha(.75f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());
        }
    } else {
        for(int i=0;i<=8;i++) {
            const auto x=plot.getX()+plot.getWidth()*i/8.f;
            g.setColour(Theme::stroke.withAlpha(.75f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());
        }
    }
    for(int i=0;i<=6;i++){
        const auto y=plot.getY()+plot.getHeight()*i/6.f;
        g.setColour(i==3 && type==SkellPresetAudioProcessor::EQ ?Theme::blue.withAlpha(.7f):Theme::stroke);
        g.drawHorizontalLine((int)y,plot.getX(),plot.getRight());
    }
    if(type==SkellPresetAudioProcessor::EQ) {
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
        const float threshold=-53.f+processor.getSlotParam(slot,1)*39.f;
        const float maxReduction=2.f+processor.getSlotParam(slot,2)*20.f;
        auto xFor=[&](float frequency){
            return plot.getX()+plot.getWidth()*std::log(frequency/20.f)/std::log(1000.f);
        };
        juce::Path response;
        for(int ix=0;ix<=835;ix+=3) {
            const float frequency=20.f*std::pow(1000.f,(float)ix/835.f);
            const float highWeight=frequency/std::sqrt(frequency*frequency+cutoff*cutoff);
            const float attenuation=20.f*std::log10(juce::jmax(.0001f,
                1.f-(1.f-skellpreset::dsp::dbToGain(-maxReduction))*highWeight));
            const float y=plot.getY()+35.f-attenuation*6.f;
            if(ix==0)response.startNewSubPath(plot.getX(),y);
            else response.lineTo(plot.getX()+ix,y);
        }
        g.setColour(Theme::green.withAlpha(.13f));
        g.fillRect(xFor(cutoff),plot.getY(),plot.getRight()-xFor(cutoff),plot.getHeight());
        g.setColour(Theme::amber.withAlpha(.8f));
        g.drawVerticalLine((int)xFor(cutoff),plot.getY(),plot.getBottom());
        g.setColour(Theme::green);g.strokePath(response,juce::PathStrokeType(2.8f));
        Theme::text(g,"SIBILANCE REGION  /  "+juce::String(cutoff,0)+" Hz",
            {47,163,500,20},12,Theme::green,true);
        Theme::text(g,juce::String(threshold,1)+" dB THRESHOLD  /  UP TO "
            +juce::String(maxReduction,1)+" dB REDUCTION",
            {47,186,650,21},13,Theme::white);
        Theme::text(g,"ILLUSTRATIVE MAX REDUCTION  /  ACTUAL GAIN IS LEVEL-DEPENDENT",
            {43,327,790,17},11,Theme::muted);
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
        for(int echo=0;echo<4;++echo) {
            const float amplitude=std::pow(feedback,(float)echo);
            const float cx=plot.getX()+470.f+echo*97.f;
            const float radius=10.f+22.f*amplitude;
            g.setColour(Theme::green.withAlpha(.25f+.65f*amplitude));
            g.drawEllipse(cx-radius,baseY-radius,2.f*radius,2.f*radius,2.f);
            g.fillEllipse(cx-4.f,baseY-4.f,8.f,8.f);
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
        const float unit=juce::jmin(175.f,plot.getWidth()/5.f);
        for(int tap=0;tap<5;tap++){
            const float amplitude=std::pow(feedback,(float)tap);
            const float x=plot.getX()+43.f+tap*unit;
            g.setColour(Theme::green.withAlpha(.3f+.7f*amplitude));
            g.fillRoundedRectangle(x,plot.getBottom()-amplitude*133.f,
                                   10.f,amplitude*133.f,3.f);
            Theme::text(g,juce::String(tap+1),
                {(int)x-7,(int)plot.getBottom()-20,26,16},9,Theme::muted,
                false,juce::Justification::centred);
        }
        Theme::text(g,"DELAY  "+juce::String(seconds*1000.f,0)+
                      " ms / FEEDBACK "+juce::String(feedback*100.f,0)+"%",
                    {45,163,700,20},12,Theme::green,true);
        Theme::text(g,"RELATIVE TAP DECAY / NOT AN AUDIO WAVEFORM",
                    {43,326,650,17},10,Theme::muted);
    } else if(type==SkellPresetAudioProcessor::ExternalVST3) {
        const auto name=processor.hostedVST3Name(slot);
        Theme::text(g,"EXTERNAL VST3 INSERT",{80,185,680,31},25,Theme::green,true);
        Theme::text(g,name.isEmpty()?"NO PLUGIN LOADED":name.toUpperCase(),
            {81,225,720,34},17,Theme::white,true);
        Theme::text(g,name.isEmpty()?"Choose LOAD VST3 below to select a plug-in.":
            "OPEN PLUGIN UI to adjust the hosted effect.",
            {83,270,720,23},13,Theme::muted);
        Theme::text(g,"IN-PROCESS HOST  /  THIRD-PARTY CODE RUNS INSIDE YOUR DAW",
            {85,313,735,20},11,Theme::amber);
    } else if(type!=SkellPresetAudioProcessor::Empty){
        // Parameter-driven visualizations. These are not fabricated FFT meters:
        // the user can drag the shared edit point to change P1/P2.
        const float first=processor.getSlotParam(slot,0);
        const float second=processor.getSlotParam(slot,1);
        const float x0=plot.getX()+24.f, x1=plot.getRight()-24.f;
        const float y0=plot.getY()+20.f, y1=plot.getBottom()-27.f;
        const float w=x1-x0, h=y1-y0;
        if(type==SkellPresetAudioProcessor::EightBandToneFX) {
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
                filter[(size_t)band].setPeak(sr,freq,gain,1.1);
            }
            for(int px=0;px<=835;px+=5) {
                const double freq=20.*std::pow(1000.,(double)px/835.);
                double dB=0.;
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
        } else if(type==SkellPresetAudioProcessor::Limiter){
            // Limiter transfer: unprocessed diagonal with adjustable ceiling.
            const float ceiling=y0+18.f+(1.f-first)*h*.58f;
            g.setColour(Theme::muted.withAlpha(.37f));
            g.drawLine(x0,y1,x1,y0,1.3f);
            juce::Path limited;
            limited.startNewSubPath(x0,y1);
            const float knee=juce::jlimit(x0+25.f,x1-15.f,
                                         x0+w*(.50f+.40f*first-.12f*second));
            limited.lineTo(knee,ceiling);limited.lineTo(x1,ceiling);
            g.setColour(Theme::green);
            g.strokePath(limited,juce::PathStrokeType(3.2f));
            g.setColour(Theme::amber.withAlpha(.65f));
            g.drawHorizontalLine((int)ceiling,x0,x1);
            Theme::text(g,"LOOKAHEAD LIMITER  /  CEILING + DRIVE",
                {62,165,700,20},12,Theme::green,true);
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
        } else {
            Theme::text(g,processor.moduleName(type).toUpperCase(),
                {65,198,740,40},25,Theme::green,true,juce::Justification::centred);
            Theme::text(g,"DRAG THE GRAPH TO EDIT THE FIRST TWO CONTROLS",
                {120,249,620,22},12,Theme::muted,false,juce::Justification::centred);
        }
        // Non-EQ modules share the two-axis universal marker.
        if(type!=SkellPresetAudioProcessor::EightBandToneFX) {
        const float markerX=x0+w*first,markerY=y1-h*second;
        g.setColour(Theme::green.withAlpha(.24f));
        g.drawVerticalLine((int)markerX,y0,y1);
        g.drawHorizontalLine((int)markerY,x0,x1);
        g.setColour(Theme::plot);
        g.fillEllipse(markerX-6.f,markerY-6.f,12.f,12.f);
        g.setColour(Theme::green);
        g.drawEllipse(markerX-6.f,markerY-6.f,12.f,12.f,2.4f);
        }
    } else {
        Theme::text(g,"SELECT A MODULE FROM THE DROPDOWN",{170,234,500,25},16,Theme::muted,true,juce::Justification::centred);
    }
    g.restoreState();
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
    } else if(type==SkellPresetAudioProcessor::EightBandToneFX) {
        effectTitle="8-BAND EQ  /  EIGHT INDEPENDENT FREQUENCY + GAIN NODES";
        effectDetail="SELECT A DOT TO SHOW ITS BAND PAIR  /  FIXED Q 1.1  /  ZERO ADDED PDC";
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
    if(effectTitle.isNotEmpty()){
        Theme::text(g,effectTitle,{47,103,730,22},14,Theme::green,true);
        Theme::text(g,effectDetail,{47,128,755,18},12,Theme::muted);
    }
    if(type==SkellPresetAudioProcessor::Compressor ||
       type==SkellPresetAudioProcessor::Gate ||
       type==SkellPresetAudioProcessor::DeEsser ||
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
    if(type==SkellPresetAudioProcessor::EightBandToneFX) {
        Theme::text(g,"8 BANDS  /  SELECT DOT TO EDIT",
            {36,101,420,20},12,Theme::green,true);
        Theme::text(g,"B"+juce::String(eqBank+1)+" + B"+
            juce::String(eqBank+2)+" SELECTED",{655,101,212,20},11,
            Theme::muted,true,juce::Justification::centredRight);
    }
    if(type==SkellPresetAudioProcessor::EQ){
        g.setColour(Theme::green.withAlpha(.13f));g.fillRoundedRectangle(juce::Rectangle<float>(34.f,96.f,145.f,30.f),6.f);
        g.setColour(Theme::green.withAlpha(.7f));g.drawRoundedRectangle(juce::Rectangle<float>(34.f,96.f,145.f,30.f),6.f,1.f);
        Theme::text(g,"BELL + LOW CUT",{44,99,130,23},11,Theme::green,true);
        Theme::text(g,"RESPONSE FROM LIVE DSP COEFFICIENTS",{205,100,390,22},11,Theme::muted);
    }
    if(type==SkellPresetAudioProcessor::EQ || type==SkellPresetAudioProcessor::DeEsser || type==SkellPresetAudioProcessor::EightBandToneFX) {
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
        Theme::text(g,compressor?"OUTPUT dB":"OPEN",{1,142,36,15},9,Theme::muted);
        Theme::text(g,"INPUT dB",{(int)plot.getRight()-86,376,85,15},9,Theme::muted,false,juce::Justification::centredRight);
    } else if(type==SkellPresetAudioProcessor::Delay || type==SkellPresetAudioProcessor::AdlibThrow) {
        Theme::text(g,"ECHO NUMBER  1 - 5",{38,364,360,17},10,Theme::muted);
    }
    auto labels=titles(type);
    if(type==SkellPresetAudioProcessor::EightBandToneFX) {
        labels={ "B"+juce::String(eqBank+1)+" FREQ", "B"+juce::String(eqBank+1)+" GAIN",
                 "B"+juce::String(eqBank+2)+" FREQ", "B"+juce::String(eqBank+2)+" GAIN" };
    }
    if(type==SkellPresetAudioProcessor::ExternalVST3) {
        Theme::text(g,"HOSTED VST3  /  USE ITS OWN UI TO EDIT PARAMETERS",
            {447,445,424,49},12,Theme::muted,true);
    } else {
        for(int i=0;i<4;i++)
            Theme::text(g,labels[(size_t)i].toUpperCase(),{148+211*i,438,105,24},
                        12,Theme::muted,true);
    }
    const juce::String gestureHint="DRAG GREEN HANDLE  /  X: "+
        labels[0].toUpperCase()+"  /  Y: "+labels[1].toUpperCase();
    Theme::text(g,type==SkellPresetAudioProcessor::AdlibThrow ? "PHRASE-END THROW / REPEATS CONTINUE THROUGH SILENCE" :
                  type==SkellPresetAudioProcessor::AdlibDoubler ? "STEREO MICRO-DOUBLING / MONO INPUT REMAINS COMPATIBLE" :
                  type==SkellPresetAudioProcessor::EQ ? "DRAG THE EQ NODE TO CHANGE FREQUENCY + GAIN" :
                  type==SkellPresetAudioProcessor::EightBandToneFX ? "CLICK ANY OF EIGHT NODES  /  DRAG TO CHANGE ITS FREQUENCY + GAIN" :
                  type==SkellPresetAudioProcessor::AdaptiveNoiseFX ? "ADAPTIVE GATE  /  NOT AN RNNOISE NEURAL MODEL" :
                  type==SkellPresetAudioProcessor::DeEsser ? "ZERO ADDED LATENCY  /  STEREO-LINKED CONSONANT CONTROL" :
                  type==SkellPresetAudioProcessor::ExternalVST3
                    ? juce::String("EXTERNAL VST3  /  THIRD-PARTY EDITOR OPENS SEPARATELY")
                    : gestureHint,{30,395,700,17},10,Theme::muted);
}
void SlotEditor::updateGraphNode(juce::Point<float> point) {
    const int type=processor.slotType(slot);
    constexpr float plotCentreY=254.f;
    if(type==SkellPresetAudioProcessor::EQ){
        const auto x=juce::jlimit(0.f,1.f,(point.x-33.f)/835.f);
        const auto y=juce::jlimit(-1.f,1.f,(254.f-point.y)/103.f);
        const float freq=20.f*std::pow(1000.f,x);
        processor.setSlotParam(slot,0,juce::jlimit(0.f,1.f,
            (float)(std::log(freq/40.f)/std::log(300.))));
        processor.setSlotParam(slot,1,juce::jlimit(0.f,1.f,.5f+y*1.5f));
    } else if(type==SkellPresetAudioProcessor::EightBandToneFX) {
        const float low=skellpreset::dsp::EightBandTone::edges[(size_t)activeEqBand];
        const float high=skellpreset::dsp::EightBandTone::edges[(size_t)activeEqBand+1];
        const float normalized=juce::jlimit(0.f,1.f,(point.x-33.f)/835.f);
        const float freq=20.f*std::pow(1000.f,normalized);
        const float x=juce::jlimit(0.f,1.f,
            std::log(juce::jlimit(low,high,freq)/low)/std::log(high/low));
        const float gain=juce::jlimit(0.f,1.f,
            .5f+(plotCentreY-point.y)/206.f*2.4f);
        processor.setSlotParam(slot,activeEqBand*2,x);
        processor.setSlotParam(slot,activeEqBand*2+1,gain);
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
void SlotEditor::mouseDown(const juce::MouseEvent& e){
    const int type=processor.slotType(slot);
    if(type==SkellPresetAudioProcessor::Empty
        || type==SkellPresetAudioProcessor::ExternalVST3
        || !juce::Rectangle<int>(33,151,835,206).contains(e.getPosition()))return;
    draggingGraph=true;
    if(type==SkellPresetAudioProcessor::EightBandToneFX){
        // Choose the nearest of eight actual DSP nodes, then latch this band
        // for the whole drag and show its two-band knob page.
        float nearest=std::numeric_limits<float>::max();
        for(int b=0;b<8;++b){
            const float low=skellpreset::dsp::EightBandTone::edges[(size_t)b];
            const float high=skellpreset::dsp::EightBandTone::edges[(size_t)b+1];
            const float freq=low*std::pow(high/low,processor.getSlotParam(slot,b*2));
            const float bx=33.f+835.f*std::log(freq/20.f)/std::log(1000.f);
            const float dist=std::abs(bx-(float)e.x);
            if(dist<nearest){nearest=dist;activeEqBand=b;}
        }
        if(activeEqBand/2 !=eqBank/2)showEqBank(activeEqBand);
        for(int k=0;k<2;++k)
            if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+
                          "_p"+juce::String(activeEqBand*2+k+1)))
                p->beginChangeGesture();
    } else {
        for(int param:{1,2})
            if(auto*p=processor.state.getParameter("s"+juce::String(slot+1)+
                                                  "_p"+juce::String(param)))
                p->beginChangeGesture();
    }
    updateGraphNode(e.position);
}
void SlotEditor::mouseDrag(const juce::MouseEvent& e){if(draggingGraph)updateGraphNode(e.position);}
void SlotEditor::mouseUp(const juce::MouseEvent&){
    if(!draggingGraph)return;
    const bool eightBand=processor.slotType(slot)==SkellPresetAudioProcessor::EightBandToneFX;
    for(int k=0;k<2;++k) {
        const int param=eightBand?activeEqBand*2+k+1:k+1;
        if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+
                                               "_p"+juce::String(param)))
            p->endChangeGesture();
    }
    draggingGraph=false;
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
    moduleTrigger.onClick();
    if(!modulePicker.isVisible())return false;
    moduleTrigger.onClick();
    if(modulePicker.isVisible())return false;
    moduleTrigger.onClick();
    modulePicker.chooseForTest(SkellPresetAudioProcessor::RingModFX);
    const auto* type=processor.state.getRawParameterValue(
        "s"+juce::String(slot+1)+"_type");
    return !modulePicker.isVisible() && type
        && (int)type->load()==SkellPresetAudioProcessor::RingModFX
        && moduleSelectorMatchesAudio();
}
