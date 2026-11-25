#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"

class SkellLiveEffectPanel final:public juce::Component {
    using P=SkellPresetAudioProcessor;
    using Analyzer=skellpreset::visual::LiveSpectrumAnalyzer;
    static constexpr int historySize=90;
public:
    void update(P& p,int slot) {
        const int type=p.slotType(slot);
        if(slot!=lastSlot || type!=lastType){
            lastSlot=slot;lastType=type;reset();
        }
        const auto now=juce::Time::getMillisecondCounter();
        skellpreset::visual::SlotVisualCapture::Frame frame;
        const bool available=p.getSlotVisual(slot,frame);
        if(available && frame.serial!=lastSerial){
            lastSerial=frame.serial;lastFrame=now;fresh=true;
            inputRms=frame.inputRms;outputRms=frame.outputRms;sideRms=frame.sideRms;
            gr=std::max(p.getSlotGainReductionDb(slot),p.getSlotDeessReductionDb(slot));
            duck=p.getSlotDuckReductionDb(slot);
            shift(inputHistory,level(inputRms));shift(outputHistory,level(outputRms));
            shift(grHistory,std::clamp(gr/24.f,0.f,1.f));
            shift(duckHistory,std::clamp(duck/24.f,0.f,1.f));
            // Spectrum analysis is UI-only and throttled to ~15 Hz.
            if((++ticks&1u)!=0 || ticks==1){
                const auto input=analyzer.analyze(frame.input,p.preparedAudioRate());
                const auto output=analyzer.analyze(frame.output,p.preparedAudioRate());
                for(size_t i=0;i<input.size();++i){
                    inBands[i]=follow(inBands[i],level(input[i]));
                    outBands[i]=follow(outBands[i],level(output[i]));
                }
            }
            for(size_t i=0;i<wave.size();++i)wave[i]=frame.output[i*8];
        }else if((unsigned)(now-lastFrame)>=400u){
            if(fresh){reset();repaint();}
            return;
        }else return;
        repaint();
    }
    bool testLayoutForTest() const noexcept {return getWidth()>=600 && getHeight()>=190;}
    void paint(juce::Graphics& g) override {
        const auto bounds=getLocalBounds().toFloat();
        Theme::panelBox(g,bounds,8.f,true);
        if(bounds.getWidth()<300 || bounds.getHeight()<140)return;
        const bool dynamics=isDynamics(lastType);
        const bool space=P::supportsWetSidechain(lastType);
        const auto accent=dynamics?Theme::amber:space?Theme::blue:Theme::green;
        auto inner=bounds.reduced(14.f,9.f);
        auto top=inner.removeFromTop(24.f);
        Theme::fittedText(g,dynamics?"DYNAMICS / REDUCTION":space?"TIME / RETURN ENERGY":"SPECTRUM / BEFORE + AFTER",
            top.withTrimmedRight(110.f).toNearestInt(),13.f,Theme::white,true);
        Theme::fittedText(g,fresh?"LIVE":"WAITING FOR AUDIO",top.removeFromRight(110.f).toNearestInt(),10.f,
            fresh?Theme::green:Theme::muted,true,juce::Justification::centredRight);
        auto footer=inner.removeFromBottom(23.f);
        auto right=inner.removeFromRight(146.f).reduced(4.f,0.f);
        auto graph=inner.reduced(1.f,5.f);
        g.setColour(Theme::bg.withAlpha(.75f));g.fillRoundedRectangle(graph,5.f);
        for(int i=0;i<=4;++i){
            const float y=graph.getY()+graph.getHeight()*i/4.f;
            g.setColour(Theme::stroke.withAlpha(.42f));g.drawLine(graph.getX(),y,graph.getRight(),y,.7f);
        }
        for(int i=1;i<8;++i){
            const float x=graph.getX()+graph.getWidth()*i/8.f;
            g.drawLine(x,graph.getY(),x,graph.getBottom(),.65f);
        }
        auto plot=[&](const auto& values,juce::Colour colour,bool fill,bool inverted=false){
            juce::Path line;
            for(size_t i=0;i<values.size();++i){
                const float x=graph.getX()+graph.getWidth()*(float)i/(float)(values.size()-1);
                const float v=std::clamp(values[i],0.f,1.f);
                const float y=graph.getY()+graph.getHeight()*(inverted?v:1.f-v);
                if(i==0)line.startNewSubPath(x,y);else line.lineTo(x,y);
            }
            if(fill){
                auto area=line;
                const float baseline=inverted?graph.getY():graph.getBottom();
                area.lineTo(graph.getRight(),baseline);area.lineTo(graph.getX(),baseline);area.closeSubPath();
                g.setGradientFill(juce::ColourGradient(colour.withAlpha(.21f),graph.getCentreX(),graph.getY(),
                    colour.withAlpha(.035f),graph.getCentreX(),graph.getBottom(),false));g.fillPath(area);
            }
            g.setColour(colour.withAlpha(.13f));g.strokePath(line,juce::PathStrokeType(4.f));
            g.setColour(colour.withAlpha(.92f));g.strokePath(line,juce::PathStrokeType(1.6f));
        };
        {
            juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(graph.toNearestInt());
            if(dynamics){plot(grHistory,accent,true,true);plot(duckHistory,Theme::blue,false,true);}
            else if(space){plot(inputHistory,Theme::muted,false);plot(outputHistory,accent,true);}
            else{plot(inBands,Theme::muted,false);plot(outBands,accent,true);}
        }
        // Explicit units distinguish sampled meter histories from frequency plots.
        if(dynamics){
            Theme::text(g,"0",graph.getSmallestIntegerContainer().withHeight(13).translated(3,0),9,Theme::muted);
            Theme::text(g,"-24 dB",graph.getSmallestIntegerContainer().withTop((int)graph.getBottom()-14).translated(3,0),9,Theme::muted);
        }else if(!space){
            Theme::text(g,"80 Hz",footer.removeFromLeft(68.f).toNearestInt(),9,Theme::muted);
            Theme::fittedText(g,"9 kHz",footer.removeFromRight(68.f).toNearestInt(),9,Theme::muted,false,juce::Justification::centredRight);
        }
        auto metric=[&](const juce::String& label,const juce::String& value,juce::Colour colour){
            auto row=right.removeFromTop(30.f);
            Theme::text(g,label,row.removeFromTop(12.f).toNearestInt(),9.f,Theme::muted,true);
            Theme::text(g,value,row.toNearestInt(),13.f,colour,true);
        };
        metric("INPUT RMS",db(inputRms),Theme::white);
        metric("OUTPUT RMS",db(outputRms),accent);
        metric(dynamics?"REDUCTION":space?"DUCK":"SIDE RMS",
               dynamics?juce::String(gr,1)+" dB":space?juce::String(duck,1)+" dB":db(sideRms),accent);
        // The small footer trace is actual full-rate output, not a generated wave.
        if(right.getHeight()>12.f){
            juce::Path trace;const auto a=right.reduced(1.f,2.f);
            for(size_t i=0;i<wave.size();++i){
                const float x=a.getX()+a.getWidth()*(float)i/(wave.size()-1);
                const float y=a.getCentreY()-std::clamp(wave[i],-1.f,1.f)*a.getHeight()*.48f;
                if(i==0)trace.startNewSubPath(x,y);else trace.lineTo(x,y);
            }
            g.setColour(accent.withAlpha(.8f));g.strokePath(trace,juce::PathStrokeType(1.2f));
        }
        Theme::fittedText(g,dynamics?"GAIN REDUCTION / DUCKING     /     24 dB RANGE":
            space?"INPUT / OUTPUT     /     -72 TO 0 dBFS":"INPUT (GREY) / OUTPUT (GREEN)     /     -72 TO 0 dBFS",
            footer.toNearestInt(),10.f,Theme::muted,false,juce::Justification::centred);
    }
private:
    static bool isDynamics(int t) noexcept {
        return t==P::Compressor || t==P::Limiter || t==P::Gate || t==P::DeEsser ||
            t==P::ThreeBandDynamics || t==P::VoxRackSmoothComp || t==P::VocalRiderFX ||
            t==P::SerialDynamicsFX || t==P::DynamicHarshnessFX || t==P::AdaptiveNoiseFX ||
            t==P::StudioLimiterFX || t==P::StudioCompressorFX || t==P::VocalFeelOptoFX;
    }
    static float level(float x) noexcept {
        return std::clamp((20.f*std::log10(std::max(0.00025119f,x))+72.f)/72.f,0.f,1.f);
    }
    static juce::String db(float x) {return x<.00025119f?juce::String("-INF"):juce::String(20.f*std::log10(x),1)+" dBFS";}
    static float follow(float old,float next) noexcept {return old+(next-old)*(next>old?.82f:.32f);}
    template<size_t N>static void shift(std::array<float,N>& values,float x) noexcept {
        std::move(values.begin()+1,values.end(),values.begin());values.back()=x;
    }
    void reset() noexcept {
        inBands.fill(0);outBands.fill(0);inputHistory.fill(0);outputHistory.fill(0);
        grHistory.fill(0);duckHistory.fill(0);wave.fill(0);
        inputRms=outputRms=sideRms=gr=duck=0.f;lastSerial=0;lastFrame=0;fresh=false;ticks=0;
    }
    Analyzer analyzer;
    std::array<float,Analyzer::bands> inBands{},outBands{};
    std::array<float,historySize> inputHistory{},outputHistory{},grHistory{},duckHistory{};
    std::array<float,128> wave{};
    int lastSlot=-1,lastType=-1;
    std::uint64_t lastSerial=0;
    unsigned lastFrame=0,ticks=0;
    bool fresh=false;
    float inputRms=0,outputRms=0,sideRms=0,gr=0,duck=0;
};
