#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>

// Default Advanced view when no effect is selected. The only moving visual
// is a real 128-sample waveform published by the audio DSP. No fake FFT,
// no animation timer, no audio allocations, no effect or state modification.
class SkellIdleDeck final : public juce::Component {
public:
    explicit SkellIdleDeck(SkellPresetAudioProcessor& p):processor(p){
        // Swallow empty-deck mouse clicks. Passing them through would mutate
        // the hidden SlotEditor's XY graph even while NO effect is selected.
        setInterceptsMouseClicks(true,false);
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        const auto top=getLocalBounds().toFloat().reduced(24.f);
        g.setColour(Theme::green.withAlpha(.14f));
        g.drawRoundedRectangle(top.reduced(2.f),10.f,1.f);
        Theme::text(g,"SKELL  /  PRESET",{58,58,getWidth()-115,34},
                    23,Theme::white,true);
        Theme::text(g,"YOUR SOUND, YOUR CHAIN",{58,106,getWidth()-115,27},
                    13,Theme::green,true);
        Theme::text(g,"Select an effect on the rack to open its controls.",
                    {58,146,getWidth()-115,24},13,Theme::muted);
        // Two tiny bone dancers react to the REAL processed output level.
        // Animation is UI-thread only and repaints with the existing live
        // waveform timer; silence leaves them still. No fake FFT/DSP work.
        const float peak=juce::jlimit(0.f,1.f,processor.getOutputPeak());
        const float activity=juce::jlimit(0.f,1.f,peak*2.4f);
        const float seconds=(float)(juce::Time::getMillisecondCounterHiRes()*.001);
        auto dancer=[&](float x,float phase){
            const float swing=std::sin(seconds*6.2f+phase)*activity;
            const float bounce=std::abs(std::sin(seconds*5.3f+phase))*activity*6.f;
            const float y=137.f-bounce;
            g.setColour(Theme::green.withAlpha(.11f+.12f*activity));
            g.fillEllipse(x-20.f,y-13.f,40.f,102.f);
            g.setColour(Theme::white.withAlpha(.80f));
            g.fillEllipse(x-10.f,y,20.f,22.f);
            g.setColour(Theme::bg);
            g.fillEllipse(x-6.2f,y+8.f,4.5f,6.f);
            g.fillEllipse(x+1.7f,y+8.f,4.5f,6.f);
            g.fillEllipse(x-1.3f,y+13.5f,2.6f,3.5f);
            g.setColour(Theme::white.withAlpha(.87f));
            g.drawLine(x,y+22.f,x,y+53.f,2.7f);
            for(int k=0;k<3;++k)
                g.drawLine(x-7.f,y+31.f+k*6.f,x+7.f,y+31.f+k*6.f,1.3f);
            g.drawLine(x-7.f,y+32.f,x-17.f-8.f*swing,y+49.f-5.f*swing,2.2f);
            g.drawLine(x+7.f,y+32.f,x+17.f+8.f*swing,y+49.f+5.f*swing,2.2f);
            g.drawLine(x,y+53.f,x-12.f-8.f*swing,y+76.f,2.4f);
            g.drawLine(x,y+53.f,x+12.f+8.f*swing,y+76.f,2.4f);
            g.setColour(Theme::green.withAlpha(.8f));
            g.fillEllipse(x-16.f-8.f*swing,y+46.f-5.f*swing,4.f,4.f);
            g.fillEllipse(x+14.f+8.f*swing,y+46.f+5.f*swing,4.f,4.f);
        };
        dancer((float)getWidth()-178.f,0.f);
        dancer((float)getWidth()-97.f,1.5f);
        Theme::text(g,"BONE MOTION / LIVE AUDIO",
                    {getWidth()-295,109,265,17},9.8f,Theme::green.withAlpha(.85f),
                    true,juce::Justification::centred);
        const auto plot=juce::Rectangle<float>(62.f,217.f,getWidth()-124.f,152.f);
        g.setColour(Theme::plot);
        g.fillRoundedRectangle(plot,9.f);
        g.setColour(Theme::stroke.withAlpha(.70f));
        g.drawRoundedRectangle(plot,9.f,1.f);
        g.drawLine(plot.getX()+8.f,plot.getCentreY(),
                   plot.getRight()-8.f,plot.getCentreY(),.6f);
        std::array<float,128> wave{};
        processor.getLiveWaveform(wave);
        juce::Path signal;
        for(int i=0;i<128;++i) {
            const float v=juce::jlimit(-1.f,1.f,wave[(size_t)i]);
            const float x=plot.getX()+12.f+(plot.getWidth()-24.f)*i/127.f;
            const float y=plot.getCentreY()-v*(plot.getHeight()*.42f);
            if(i==0)signal.startNewSubPath(x,y);
            else signal.lineTo(x,y);
        }
        g.setColour(Theme::green.withAlpha(.96f));
        g.strokePath(signal,juce::PathStrokeType(1.8f,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));
        Theme::text(g,"LIVE AUDIO  /  NO EFFECT SELECTED",
                    {64,382,getWidth()-128,22},10.8f,Theme::muted,true);
        Theme::text(g,"DOUBLE-CLICK A RACK SLOT TO REPLACE AN EFFECT",
                    {64,getHeight()-83,getWidth()-128,20},11.4f,Theme::white,true);
        Theme::text(g,"CLICK + TO ADD A PLUGIN",
                    {64,getHeight()-59,getWidth()-128,18},10.5f,Theme::green);
    }
private:
    SkellPresetAudioProcessor& processor;
};
