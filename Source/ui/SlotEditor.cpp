#include "SlotEditor.h"
#include "Theme.h"
#include <cmath>
namespace {
const juce::StringArray names{"Empty","EQ","Compressor","Limiter","Gate","Saturator","Reverb","Delay","Exciter","Adlib Throw","Adlib Doubler"};
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
        default:return {"Control 1","Control 2","Control 3","Control 4"};
    }
}
}
SlotEditor::SlotEditor(SkellPresetAudioProcessor& p):processor(p) {
    for(int i=0;i<names.size();i++)moduleBox.addItem(names[i],i+1);
    moduleBox.setColour(juce::ComboBox::backgroundColourId,Theme::panelAlt);
    moduleBox.setColour(juce::ComboBox::textColourId,Theme::white);
    moduleBox.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    addAndMakeVisible(moduleBox);
    activeButton.setButtonText("ON");activeButton.setColour(juce::ToggleButton::tickColourId,Theme::green);
    activeButton.setColour(juce::ToggleButton::textColourId,Theme::white);addAndMakeVisible(activeButton);
    wetKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    wetKnob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,60,18);
    addAndMakeVisible(wetKnob);
    for(auto& k:knobs){
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,20);
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
    activeButton.setColour(juce::ToggleButton::tickColourId,Theme::green);
    activeButton.setColour(juce::ToggleButton::textColourId,Theme::white);
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
void SlotEditor::select(int newSlot) {
    slot=juce::jlimit(0,SkellPresetAudioProcessor::slotCount-1,newSlot);
    enabledAttachment.reset();wetAttachment.reset();
    for(auto& a:knobAttachment)a.reset();
    const auto prefix="s"+juce::String(slot+1)+"_";
    if(auto* type=processor.state.getRawParameterValue(prefix+"type"))
        moduleBox.setSelectedId((int)type->load()+1,juce::dontSendNotification);
    enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,prefix+"on",activeButton);
    wetAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,prefix+"wet",wetKnob);
    for(int k=0;k<4;k++)knobAttachment[(size_t)k]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,prefix+"p"+juce::String(k+1),knobs[(size_t)k]);
    repaint();
}
void SlotEditor::resized() {
    moduleBox.setBounds(550,25,144,29);activeButton.setBounds(711,26,70,28);
    wetKnob.setBounds(802,6,88,78);
    for(int k=0;k<4;k++)knobs[(size_t)k].setBounds(42+211*k,437,83,70);
}
void SlotEditor::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    Theme::panelBox(g,{0,0,906.f,425.f});
    Theme::panelBox(g,{0,421.f,906.f,98.f});
    const int type=(int)processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_type")->load();
    Theme::text(g,"SLOT "+juce::String(slot+1).paddedLeft('0',2),{21,10,200,18},10,Theme::muted,true);
    Theme::text(g,processor.moduleName(type),{21,29,420,38},25,Theme::white,true);
    Theme::text(g,"MODULE",{552,9,140,15},9,Theme::muted,true);
    Theme::text(g,"WET",{832,3,50,15},9,Theme::muted,true);
    const auto plot=juce::Rectangle<float>(33,151,835,206);
    g.setColour(Theme::plot);g.fillRoundedRectangle(plot,6);
    g.saveState();g.reduceClipRegion(plot.toNearestInt());
    constexpr float frequencies[]{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f};
    for(float freq:frequencies){
        const auto x=plot.getX()+plot.getWidth()*std::log(freq/20.f)/std::log(1000.f);
        g.setColour(Theme::stroke.withAlpha(.75f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());
    }
    for(int i=0;i<=6;i++){
        const auto y=plot.getY()+plot.getHeight()*i/6.f;
        g.setColour(i==3?Theme::blue.withAlpha(.7f):Theme::stroke);
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
    } else if(type!=SkellPresetAudioProcessor::Empty){
        juce::Path env;
        const float amount=processor.getSlotParam(slot,0);
        for(int x=0;x<830;x+=5){
            const float u=x/830.f;
            const float value=(1.f-amount*.4f)*std::sin(u*12.f)*std::exp(-u*1.3f);
            const float y=plot.getCentreY()-value*75.f;
            if(x==0)env.startNewSubPath(plot.getX()+x,y);else env.lineTo(plot.getX()+x,y);
        }
        g.setColour(Theme::green.withAlpha(.8f));g.strokePath(env,juce::PathStrokeType(2.4f));
        Theme::text(g,"ILLUSTRATIVE PARAMETER SHAPE",{45,160,400,18},10,Theme::muted,true);
    } else {
        Theme::text(g,"SELECT A MODULE FROM THE DROPDOWN",{170,234,500,25},16,Theme::muted,true,juce::Justification::centred);
    }
    g.restoreState();
    if(type==SkellPresetAudioProcessor::EQ){
        g.setColour(Theme::green.withAlpha(.13f));g.fillRoundedRectangle(juce::Rectangle<float>(34.f,96.f,145.f,30.f),6.f);
        g.setColour(Theme::green.withAlpha(.7f));g.drawRoundedRectangle(juce::Rectangle<float>(34.f,96.f,145.f,30.f),6.f,1.f);
        Theme::text(g,"BELL + LOW CUT",{44,99,130,23},11,Theme::green,true);
        Theme::text(g,"RESPONSE FROM LIVE DSP COEFFICIENTS",{205,100,390,22},11,Theme::muted);
    }
    const juce::StringArray ticks{"20","50","100","200","500","1k","2k","5k","10k","20k"};
    constexpr float tickHz[]{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f};
    for(int i=0;i<ticks.size();i++){
        const auto x=plot.getX()+plot.getWidth()*std::log(tickHz[i]/20.f)/std::log(1000.f);
        Theme::text(g,ticks[i],{(int)x-24,360,48,17},10,Theme::muted,false,juce::Justification::centred);
    }
    Theme::text(g,"+36",{5,142,30,15},9,Theme::muted);Theme::text(g,"0",{13,245,22,14},9,Theme::muted);Theme::text(g,"-36",{5,341,30,15},9,Theme::muted);
    const auto labels=titles(type);
    for(int i=0;i<4;i++)Theme::text(g,labels[(size_t)i].toUpperCase(),{133+211*i,459,98,19},11,Theme::muted,true);
    Theme::text(g,type==SkellPresetAudioProcessor::AdlibThrow ? "PHRASE-END THROW • REPEATS CONTINUE THROUGH SILENCE" :
                  type==SkellPresetAudioProcessor::AdlibDoubler ? "STEREO MICRO-DOUBLING • MONO INPUT REMAINS COMPATIBLE" :
                  type==SkellPresetAudioProcessor::EQ ? "DRAG THE EQ NODE TO CHANGE FREQUENCY + GAIN" :
                  "SLOT CONTROL • 20 MS WET/BYPASS FADES",{30,395,700,17},10,Theme::muted);
}
void SlotEditor::updateGraphNode(const juce::MouseEvent& e){
    const auto x=juce::jlimit(0.f,1.f,(e.position.x-33.f)/835.f);
    const auto y=juce::jlimit(-1.f,1.f,(254.f-e.position.y)/103.f);
    const float freq=20.f*std::pow(1000.f,x);
    const float normalized=juce::jlimit(0.f,1.f,(float)(std::log(freq/40.f)/std::log(300.)));
    processor.setSlotParam(slot,0,normalized);
    processor.setSlotParam(slot,1,juce::jlimit(0.f,1.f,.5f+y*1.5f));
    repaint();
}
void SlotEditor::mouseDown(const juce::MouseEvent& e){
    auto* t=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_type");
    if(!t||(int)t->load()!=1||!juce::Rectangle<int>(33,151,835,206).contains(e.getPosition()))return;
    draggingGraph=true;
    for(int param:{1,2})if(auto*p=processor.state.getParameter("s"+juce::String(slot+1)+"_p"+juce::String(param)))p->beginChangeGesture();
    updateGraphNode(e);
}
void SlotEditor::mouseDrag(const juce::MouseEvent& e){if(draggingGraph)updateGraphNode(e);}
void SlotEditor::mouseUp(const juce::MouseEvent&){
    if(!draggingGraph)return;
    for(int param:{1,2})if(auto*p=processor.state.getParameter("s"+juce::String(slot+1)+"_p"+juce::String(param)))p->endChangeGesture();
    draggingGraph=false;
}
