#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>

// Default Advanced view when no effect is selected. The only moving visual
// is a real 128-sample waveform published by the audio DSP. No fake FFT,
// no animation timer, no audio allocations, no effect or state modification.
class SkellIdleDeck final : public juce::Component {
public:
    explicit SkellIdleDeck(SkellPresetAudioProcessor& p):processor(p){
        setInterceptsMouseClicks(false,false);
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
