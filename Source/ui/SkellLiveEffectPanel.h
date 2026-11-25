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
        deessHz=3500.f+p.getSlotParam(slot,0)*8000.f;
        gateDb=-75.f+p.getSlotParam(slot,0)*70.f;
        const auto now=juce::Time::getMillisecondCounter();
        skellpreset::visual::SlotVisualCapture::Frame frame;
        const bool available=p.getSlotVisual(slot,frame);
        if(available && frame.serial!=lastSerial){
            lastSerial=frame.serial;lastFrame=now;fresh=true;
            inputRms=frame.inputRms;outputRms=frame.outputRms;sideRms=frame.sideRms;
            gr=std::max(p.getSlotGainReductionDb(slot),p.getSlotDeessReductionDb(slot));
            duck=p.getSlotDuckReductionDb(slot);
            correlation=frame.correlation;leftRms=frame.leftRms;rightRms=frame.rightRms;
            mid=frame.mid;side=frame.side;
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
            for(size_t i=0;i<wave.size();++i){
                wave[i]=frame.output[frame.output.size()-wave.size()+i];
                inputWave[i]=frame.input[frame.input.size()-inputWave.size()+i];
            }
            shift(leftHistory,level(leftRms));shift(rightHistory,level(rightRms));
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
        const bool stereo=isStereo(lastType),drive=isDrive(lastType);
        const bool deess=lastType==P::DeEsser,gate=lastType==P::Gate;
        const bool dynamics=isDynamics(lastType) && !deess && !gate;
        const bool delay=lastType==P::Delay || lastType==P::StudioDelayFX ||
            lastType==P::DuckingPingPongFX || lastType==P::AdlibThrow;
        const bool space=P::supportsWetSidechain(lastType) && !stereo;
        const auto accent=dynamics || gate || deess?Theme::amber:space || stereo?Theme::blue:Theme::green;
        auto inner=bounds.reduced(14.f,9.f);
        auto top=inner.removeFromTop(24.f);
        const auto heading=stereo?"STEREO / VECTOR FIELD":drive?"DRIVE / WAVE SHAPE":deess?"DE-ESS / SIBILANCE":gate?"GATE / LEVEL + THRESHOLD":dynamics?"DYNAMICS / REDUCTION":delay?"DELAY / LEFT + RIGHT HISTORY":space?"SPACE / ENERGY HISTORY":"SPECTRUM / BEFORE + AFTER";
        Theme::fittedText(g,heading,
            top.withTrimmedRight(110.f).toNearestInt(),13.f,Theme::white,true);
        Theme::fittedText(g,fresh?"LIVE":"WAITING FOR AUDIO",top.removeFromRight(110.f).toNearestInt(),10.f,
            fresh?Theme::green:Theme::muted,true,juce::Justification::centredRight);
        auto footer=inner.removeFromBottom(23.f);
        auto right=inner.removeFromRight(146.f).reduced(4.f,0.f);
        auto graph=inner.reduced(1.f,5.f);
        g.setGradientFill(juce::ColourGradient(Theme::bg.darker(.35f),graph.getTopLeft(),
            Theme::bg.interpolatedWith(accent,.065f),graph.getBottomRight(),false));
        g.fillRoundedRectangle(graph,6.f);
        g.setColour(accent.withAlpha(.28f));g.drawRoundedRectangle(graph,6.f,1.f);
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
            if(stereo){
                const float radius=graph.getHeight()*.44f;
                const auto centre=graph.getCentre();
                g.setColour(Theme::bg);g.fillEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius);
                g.setColour(accent.withAlpha(.35f));g.drawEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius,1.f);
                g.drawLine(centre.x-radius,centre.y,centre.x+radius,centre.y,.8f);
                g.drawLine(centre.x,centre.y-radius,centre.x,centre.y+radius,.8f);
                // Actual M/S points from the output block, not a simulated stereo image.
                for(size_t i=0;i<mid.size();++i){
                    const float x=centre.x+std::clamp(side[i],-1.f,1.f)*radius;
                    const float y=centre.y-std::clamp(mid[i],-1.f,1.f)*radius;
                    if(fresh){g.setColour(accent.withAlpha(.16f));g.fillEllipse(x-2.f,y-2.f,4.f,4.f);
                        g.setColour(accent.withAlpha(.82f));g.fillEllipse(x-.8f,y-.8f,1.6f,1.6f);}
                }
                Theme::text(g,"MID",juce::Rectangle<int>((int)centre.x+8,(int)graph.getY()+4,40,15),9.f,Theme::muted);
                Theme::text(g,"SIDE",juce::Rectangle<int>((int)centre.x+(int)radius+7,(int)centre.y-8,45,16),9.f,Theme::muted);
                const auto meter=juce::Rectangle<float>(graph.getX()+12.f,graph.getBottom()-22.f,graph.getWidth()*.23f,6.f);
                g.setColour(Theme::stroke);g.fillRoundedRectangle(meter,3.f);
                const float marker=meter.getX()+meter.getWidth()*(correlation+1.f)*.5f;
                if(fresh && outputRms>1.e-6f){g.setColour(accent);g.fillEllipse(marker-3.f,meter.getCentreY()-3.f,6.f,6.f);}
                Theme::text(g,"-1     CORRELATION     +1",meter.expanded(0.f,10.f).translated(0.f,-10.f).toNearestInt(),9.f,Theme::muted);
            }else if(drive){
                auto drawWave=[&](const auto& samples,juce::Colour colour){
                    juce::Path path;
                    for(size_t n=0;n<samples.size();++n){
                        const float x=graph.getX()+graph.getWidth()*(float)n/(samples.size()-1);
                        const float y=graph.getCentreY()-std::clamp(samples[n],-1.f,1.f)*graph.getHeight()*.46f;
                        if(n==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
                    }
                    g.setColour(colour.withAlpha(.15f));g.strokePath(path,juce::PathStrokeType(4.f));
                    g.setColour(colour);g.strokePath(path,juce::PathStrokeType(1.5f));
                };
                drawWave(inputWave,Theme::muted);drawWave(wave,accent);
            }else if(gate){
                plot(inputHistory,Theme::muted,false);plot(outputHistory,accent,true);
                const float y=graph.getBottom()-std::clamp((gateDb+72.f)/72.f,0.f,1.f)*graph.getHeight();
                g.setColour(Theme::white.withAlpha(.8f));g.drawLine(graph.getX(),y,graph.getRight(),y,1.f);
                Theme::fittedText(g,"THRESHOLD "+juce::String(gateDb,1)+" dB",juce::Rectangle<int>((int)graph.getX()+4,(int)std::clamp(y-16.f,graph.getY(),graph.getBottom()-16.f),170,16),10.f,Theme::white,true);
            }else if(dynamics){plot(grHistory,accent,true,true);plot(duckHistory,Theme::blue,false,true);}
            else if(delay){
                plot(leftHistory,accent,true);plot(rightHistory,Theme::green,false);
            }else if(space){plot(inputHistory,Theme::muted,false);plot(outputHistory,accent,true);}
            else{
                if(deess){
                    const float x=graph.getX()+graph.getWidth()*std::clamp(std::log(deessHz/80.f)/std::log(9000.f/80.f),0.f,1.f);
                    g.setColour(accent.withAlpha(.10f));g.fillRect(graph.withLeft(x));
                    if(deessHz<=9000.f){g.setColour(accent.withAlpha(.8f));g.drawLine(x,graph.getY(),x,graph.getBottom(),1.f);}
                }
                plot(inBands,Theme::muted,false);plot(outBands,accent,true);
            }
        }
        // Explicit units distinguish sampled meter histories from frequency plots.
        if(dynamics){
            Theme::text(g,"0",graph.getSmallestIntegerContainer().withHeight(13).translated(3,0),9,Theme::muted);
            Theme::text(g,"-24 dB",graph.getSmallestIntegerContainer().withTop((int)graph.getBottom()-14).translated(3,0),9,Theme::muted);
        }else if(!space && !stereo && !drive && !gate){
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
        metric(stereo?"CORRELATION":deess || gate || dynamics?"REDUCTION":space?"DUCK":"SIDE RMS",
               stereo?(fresh && outputRms>1.e-6f?juce::String(correlation,2):juce::String("--")):
               deess || gate || dynamics?juce::String(gr,1)+" dB":space?juce::String(duck,1)+" dB":db(sideRms),accent);
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
        const juce::String legend=stereo?juce::String("ACTUAL OUTPUT / MID-SIDE"):
            drive?juce::String("INPUT / OUTPUT WAVEFORM / 128 SAMPLES"):
            deess?"DE-ESS CENTER "+juce::String(deessHz/1000.f,2)+" kHz / BEFORE + AFTER":
            gate?juce::String("INPUT / OUTPUT LEVEL / -72 TO 0 dBFS"):
            dynamics?juce::String("GAIN REDUCTION / DUCKING / 24 dB RANGE"):
            delay?juce::String("LEFT (BLUE) / RIGHT (GREEN) / OUTPUT ENERGY"):
            space?juce::String("INPUT / OUTPUT / -72 TO 0 dBFS"):juce::String("INPUT / OUTPUT / -72 TO 0 dBFS");
        Theme::fittedText(g,legend,
            footer.toNearestInt(),10.f,Theme::muted,false,juce::Justification::centred);
    }
private:
    static bool isStereo(int t) noexcept {
        return t==P::AdlibDoubler || t==P::StereoWidth || t==P::VocalChorusFX || t==P::VocalFlangerFX || t==P::VocalVibratoFX || t==P::AdlibMotion;
    }
    static bool isDrive(int t) noexcept {
        return t==P::Saturator || t==P::StudioFastDistFX || t==P::AdlibSaturate || t==P::RetroCrush || t==P::Exciter;
    }
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
        grHistory.fill(0);duckHistory.fill(0);wave.fill(0);inputWave.fill(0);
        leftHistory.fill(0);rightHistory.fill(0);mid.fill(0);side.fill(0);
        correlation=leftRms=rightRms=0.f;
        inputRms=outputRms=sideRms=gr=duck=0.f;lastSerial=0;lastFrame=0;fresh=false;ticks=0;
    }
    Analyzer analyzer;
    std::array<float,Analyzer::bands> inBands{},outBands{};
    std::array<float,historySize> inputHistory{},outputHistory{},grHistory{},duckHistory{};
    std::array<float,128> wave{},inputWave{},mid{},side{};
    std::array<float,historySize> leftHistory{},rightHistory{};
    float correlation=0.f,leftRms=0.f,rightRms=0.f,deessHz=7500.f,gateDb=-40.f;
    int lastSlot=-1,lastType=-1;
    std::uint64_t lastSerial=0;
    unsigned lastFrame=0,ticks=0;
    bool fresh=false;
    float inputRms=0,outputRms=0,sideRms=0,gr=0,duck=0;
};
