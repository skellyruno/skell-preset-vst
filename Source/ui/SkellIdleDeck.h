#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>

// Native, theme-aware skull sculpture and measured output waveform.
// Audio capture remains in the processor; artwork is GUI-thread-only.
class SkellIdleDeck final : public juce::Component {
public:
    explicit SkellIdleDeck(SkellPresetAudioProcessor& p):processor(p) {
        // The inactive editor beneath this deck must not receive graph clicks.
        setInterceptsMouseClicks(true,false);
    }
    struct StageLayout {
        juce::Rectangle<float> scope;
        float artScale=1.f,artX=0.f,artY=39.f;
        bool valid=false;
    };
    static StageLayout layoutFor(int width,int height) noexcept {
        const float w=(float)width,h=(float)height;
        StageLayout out;
        if(w<160.f || h<160.f)return out;
        const float x=juce::jmax(16.f,juce::jmin(50.f,w*.08f));
        const float bottom=juce::jmax(16.f,juce::jmin(42.f,h*.08f));
        const float scopeHeight=juce::jmax(30.f,juce::jmin(71.f,h*.18f));
        out.scope={x,h-bottom-scopeHeight,juce::jmax(1.f,w-2.f*x),scopeHeight};
        out.artScale=juce::jlimit(.24f,1.f,
            juce::jmin((w-100.f)/260.f,
                       (out.scope.getY()-39.f)/340.f));
        out.artX=w*.5f-120.f*out.artScale;
        out.artY=juce::jmax(12.f,39.f-(1.f-out.artScale)*14.f);
        out.valid=out.scope.getY()>out.artY+285.f*out.artScale;
        return out;
    }
    static bool testStageGeometryForTest() noexcept {
        constexpr int sizes[5][2]{{320,240},{580,390},{906,535},
                                  {1140,680},{2000,1000}};
        for(const auto& dims:sizes) {
            const auto v=layoutFor(dims[0],dims[1]);
            if(!v.valid || v.artScale<=0.f ||
               v.scope.getX()<0.f || v.scope.getY()<0.f ||
               v.scope.getRight()>(float)dims.first ||
               v.scope.getBottom()>(float)dims.second ||
               v.artX<0.f || v.artY<0.f)return false;
        }
        return !layoutFor(0,0).valid;
    }
    void paint(juce::Graphics&g) override {
        Theme::selectFor(processor.state);
        const float w=(float)getWidth(),h=(float)getHeight(),cx=w*.5f;
        const auto layout=layoutFor(getWidth(),getHeight());
        if(!layout.valid)return;
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        std::array<float,128> wave{};
        processor.getLiveWaveform(wave);
        float waveformPeak=0.f;
        double waveformPower=0.;
        for(const auto v:wave)if(std::isfinite(v)){
            const float safe=juce::jlimit(-1.f,1.f,v);
            waveformPeak=juce::jmax(waveformPeak,std::abs(safe));
            waveformPower+=(double)safe*(double)safe;
        }
        const float waveRms=(float)std::sqrt(waveformPower/128.);
        const bool live=waveformPeak>.0005f;
        const float reported=processor.getOutputPeak();
        const float peak=std::isfinite(reported)?juce::jmax(0.f,reported):0.f;
        // Glow is driven by captured audio energy, not time/random motion.
        const float activity=live?juce::jlimit(0.f,1.f,
             .55f*waveformPeak+2.2f*waveRms):0.f;
        const auto accent=Theme::green;
        const auto bone=Theme::white;
        g.setColour(Theme::plot.withAlpha(.70f));
        g.fillRoundedRectangle({26.f,24.f,juce::jmax(1.f,w-52.f),
                                juce::jmax(1.f,h-48.f)},12.f);
        g.setColour(accent.withAlpha(.13f));
        for(int i=0;i<4;++i) {
            const float radius=(120.f+38.f*i)*layout.artScale;
            const float ringY=layout.artY+176.f*layout.artScale;
            g.drawEllipse(cx-radius,ringY-radius*.74f,
                          radius*2.f,radius*1.8f,.85f);
        }
        g.setColour(accent.withAlpha(.12f));
        g.drawVerticalLine((int)cx,layout.artY+5.f,
                           layout.scope.getY()-9.f);

        // Anatomical skull, mandible, shoulder bones and ribs are drawn as
        // scalable JUCE paths: no text-heavy tutorial or random fake spectrum.
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.addTransform(juce::AffineTransform::scale(layout.artScale)
                .translated(layout.artX,layout.artY));
            juce::Path head;
            head.startNewSubPath(120.f,29.f);
            head.cubicTo(45.f,29.f,31.f,82.f,40.f,137.f);
            head.cubicTo(44.f,158.f,58.f,173.f,76.f,183.f);
            head.lineTo(78.f,211.f);
            head.quadraticTo(120.f,240.f,162.f,211.f);
            head.lineTo(164.f,183.f);
            head.cubicTo(183.f,173.f,197.f,158.f,200.f,137.f);
            head.cubicTo(209.f,82.f,195.f,29.f,120.f,29.f);
            head.closeSubPath();
            g.setColour(accent.withAlpha(.09f+.32f*activity));
            g.fillEllipse(29.f,22.f,182.f,225.f);
            g.setColour(bone.withAlpha(.84f));
            g.fillPath(head);
            g.setColour(Theme::bg);
            g.fillEllipse(56.f,132.f,55.f,48.f);
            g.fillEllipse(129.f,132.f,55.f,48.f);
            g.setColour(accent.withAlpha(.06f+.24f*activity));
            g.fillEllipse(68.f,143.f,31.f,26.f);
            g.fillEllipse(141.f,143.f,31.f,26.f);
            g.setColour(Theme::bg);
            juce::Path nose;
            nose.startNewSubPath(120.f,177.f);
            nose.lineTo(107.f,199.f);
            nose.lineTo(133.f,199.f);nose.closeSubPath();
            g.fillPath(nose);
            g.fillRoundedRectangle(85.f,208.f,70.f,5.f,2.f);
            for(int k=0;k<7;++k)
                g.drawLine(89.f+10.f*k,212.f,89.f+10.f*k,230.f,1.5f);
            g.setColour(accent.withAlpha(.50f+.38f*activity));
            g.drawLine(45.f,110.f,64.f,73.f,1.5f);
            g.drawLine(178.f,84.f,196.f,106.f,1.f);
            g.setColour(bone.withAlpha(.70f));
            g.drawLine(120.f,246.f,120.f,337.f,4.f);
            juce::Path clavicle;
            clavicle.startNewSubPath(120.f,269.f);
            clavicle.cubicTo(87.f,261.f,75.f,282.f,33.f,294.f);
            clavicle.startNewSubPath(120.f,269.f);
            clavicle.cubicTo(153.f,261.f,165.f,282.f,207.f,294.f);
            g.strokePath(clavicle,juce::PathStrokeType(4.f,
                juce::PathStrokeType::curved));
            for(int k=0;k<4;++k){
                const float y=289.f+16.f*k;
                juce::Path ribs;
                ribs.startNewSubPath(120.f,y);
                ribs.cubicTo(78.f,y-13.f,52.f,y-5.f,58.f,y+18.f);
                ribs.startNewSubPath(120.f,y);
                ribs.cubicTo(162.f,y-13.f,188.f,y-5.f,182.f,y+18.f);
                g.setColour(bone.withAlpha(.48f+.055f*k+.12f*activity));
                g.strokePath(ribs,juce::PathStrokeType(2.7f,
                    juce::PathStrokeType::curved));
            }
        }

        const auto scope=layout.scope;
        g.setColour(Theme::plot.withAlpha(.85f));
        g.fillRoundedRectangle(scope,10.f);
        g.setColour(accent.withAlpha(.24f));
        g.drawRoundedRectangle(scope,10.f,1.f);
        g.setColour(Theme::stroke.withAlpha(.55f));
        g.drawLine(scope.getX()+8.f,scope.getCentreY(),
                   scope.getRight()-8.f,scope.getCentreY(),.7f);
        juce::Path waveform;
        for(int i=0;i<128;++i) {
            const float v=std::isfinite(wave[(size_t)i])
                ?juce::jlimit(-1.f,1.f,wave[(size_t)i]):0.f;
            const float x=scope.getX()+11.f+(scope.getWidth()-22.f)*i/127.f;
            const float y=scope.getCentreY()-v*scope.getHeight()*.42f;
            if(i==0)waveform.startNewSubPath(x,y);
            else waveform.lineTo(x,y);
        }
        g.setColour(accent.withAlpha(live?.96f:.50f));
        g.strokePath(waveform,juce::PathStrokeType(live?1.9f:1.2f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        Theme::text(g,live?"LIVE":"IDLE",
                    {juce::roundToInt(layout.scope.getX()+14.f),
                     juce::roundToInt(layout.artY+6.f),91,22},
                    11.f,live?accent:Theme::muted,true);
        const auto db=peak<1.e-6f?juce::String("-INF")
            :juce::String(20.f*std::log10(peak),1);
        const bool clipping=peak>=.999f;
        Theme::text(g,clipping?"CLIP":db+" dBFS",
                    {getWidth()-181,
                     juce::roundToInt(layout.artY+6.f),130,22},
                    11.f,clipping?Theme::red:Theme::muted,true,
                    juce::Justification::centredRight);
    }
private:
    SkellPresetAudioProcessor& processor;
};
