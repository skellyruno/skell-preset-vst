#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>
#include <limits>

// A full skeletal vocalist on an angular, cinematic stage. Every contour is
// native JUCE geometry on the UI thread: no imported mockups, random spectrum,
// prerecorded animation or new audio-thread allocations.
class SkellIdleDeck final : public juce::Component {
public:
    explicit SkellIdleDeck(SkellPresetAudioProcessor& p):processor(p) {
        setInterceptsMouseClicks(true,false);
    }
    // The waveform is a post-FX snapshot. Its last frame must not remain
    // marked LIVE after the host stops sending audio. Called only by the UI
    // timer; no locks, allocations or changes to audio processing.
    void setAudioFresh(bool fresh) {
        if(audioFresh==fresh)return;
        audioFresh=fresh;
        if(!fresh){inputMeter={};outputMeter={};}
        repaint();
    }
    struct MeterEnvelope {
        float level=0.f;
        float held=0.f;
        int holdFrames=0;
    };
    static void followPeak(MeterEnvelope& meter,float peak) noexcept {
        peak=std::isfinite(peak)?juce::jmax(0.f,peak):0.f;
        // Immediate attack, 30 Hz UI-only release; the actual audio buffer
        // and every host automation parameter remain untouched.
        meter.level=peak>=meter.level?peak:
            meter.level*.80f+peak*.20f;
        if(peak>=meter.held && peak>0.f){
            meter.held=peak;
            meter.holdFrames=24; // ~800ms peak-hold at 30Hz
        }else if(meter.holdFrames>0){
            --meter.holdFrames;
        }else{
            meter.held=juce::jmax(meter.level,meter.held*.956f);
        }
    }
    void setMeasuredPeaks(float input,float output) noexcept {
        followPeak(inputMeter,input);
        followPeak(outputMeter,output);
    }
    static bool testMeterEnvelopeForTest() noexcept {
        MeterEnvelope m;
        followPeak(m,.8f);
        if(std::abs(m.level-.8f)>.0001f || m.held<.8f ||
           m.holdFrames!=24)return false;
        followPeak(m,0.f);
        if(!(m.level<.8f && m.level>.60f) ||
           m.held<.8f || m.holdFrames!=23)return false;
        for(int i=0;i<150;++i)followPeak(m,0.f);
        if(!(m.level<.001f && m.held<.005f && m.holdFrames==0))
            return false;
        MeterEnvelope silent;
        followPeak(silent,0.f);
        followPeak(silent,std::numeric_limits<float>::quiet_NaN());
        if(silent.level!=0.f || silent.held!=0.f || silent.holdFrames!=0)
            return false;
        MeterEnvelope clipped;
        followPeak(clipped,1.2f);
        followPeak(clipped,.2f);
        return clipped.level<1.2f && clipped.held>=1.f &&
            clipped.holdFrames==23;
    }
    bool isAudioFreshForTest() const noexcept {return audioFresh;}

    static float meterFraction(float peak) noexcept {
        if(!std::isfinite(peak) || peak<=0.f)return 0.f;
        const float db=20.f*std::log10(juce::jmax(.001f,peak));
        return juce::jlimit(0.f,1.f,(db+60.f)/60.f);
    }
    static bool testTelemetryScaleForTest() noexcept {
        return testMeterEnvelopeForTest() && meterFraction(0.f)==0.f &&
            meterFraction(-1.f)==0.f &&
            meterFraction(std::numeric_limits<float>::quiet_NaN())==0.f &&
            std::abs(meterFraction(.01f)-1.f/3.f)<.002f &&
            std::abs(meterFraction(.1f)-2.f/3.f)<.002f &&
            meterFraction(1.f)==1.f && meterFraction(2.f)==1.f;
    }
    struct StageLayout {
        juce::Rectangle<float> scope;
        float scale=1.f,originX=0.f,originY=20.f;
        bool valid=false;
    };
    static StageLayout layoutFor(int width,int height) noexcept {
        const float w=(float)width,h=(float)height;
        StageLayout out;
        if(w<160.f || h<160.f)return out;
        const float margin=juce::jmax(17.f,juce::jmin(46.f,w*.075f));
        const float bottom=juce::jmax(14.f,juce::jmin(40.f,h*.078f));
        const float scopeHeight=juce::jmax(30.f,juce::jmin(70.f,h*.18f));
        out.scope={margin,h-bottom-scopeHeight,
                   juce::jmax(1.f,w-2.f*margin),scopeHeight};
        out.scale=juce::jlimit(.30f,1.15f,juce::jmin(
            (w-2.f*margin)/345.f,(out.scope.getY()-22.f)/390.f));
        out.originX=w*.5f-151.f*out.scale;
        out.originY=juce::jmax(10.f,22.f+(1.f-out.scale)*3.f);
        out.valid=out.originX>0.f &&
            out.scope.getY()>out.originY+377.f*out.scale &&
            out.scope.getRight()<=w && out.scope.getBottom()<=h;
        return out;
    }
    static bool testStageGeometryForTest() noexcept {
        constexpr int sizes[6][2]{{320,240},{580,390},{906,535},
                                  {1140,680},{2000,1000},{160,160}};
        for(const auto& dimensions:sizes) {
            const auto stage=layoutFor(dimensions[0],dimensions[1]);
            if(dimensions[0]==160) {
                if(stage.valid) return false; // unsupported tiny frame
                continue;
            }
            if(!stage.valid || !(stage.scale>=.30f) ||
               stage.scope.getX()<0.f || stage.scope.getY()<0.f ||
               stage.scope.getRight()>(float)dimensions[0] ||
               stage.scope.getBottom()>(float)dimensions[1] ||
               stage.originX<0.f || stage.originY<0.f)
                return false;
        }
        return !layoutFor(0,0).valid;
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        const float width=(float)getWidth();
        const auto stage=layoutFor(getWidth(),getHeight());
        if(!stage.valid)return;
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        std::array<float,128> signal{};
        processor.getLiveWaveform(signal);
        float actualPeak=0.f;
        double energy=0.;
        for(const auto sample:signal)if(std::isfinite(sample)){
            const float v=juce::jlimit(-1.f,1.f,sample);
            actualPeak=juce::jmax(actualPeak,std::abs(v));
            energy+=(double)v*(double)v;
        }
        const float rms=audioFresh?(float)std::sqrt(energy/128.):0.f;
        // A decimated scope can miss a short transient. The actual peak
        // envelope also drives the activity indicator, never a fake signal.
        const bool live=audioFresh &&
            (actualPeak>.0005f || outputMeter.level>.0005f);
        const float activity=live?juce::jlimit(0.f,1.f,
            .48f*actualPeak+2.1f*rms):0.f;
        const float outputPeak=audioFresh?outputMeter.level:0.f;
        const auto accent=Theme::green;
        const auto porcelain=Theme::white;
        const auto shadow=Theme::bg;

        // A composed stage: angular light shafts and hand-etched details,
        // intentionally no circular or elliptical neon decorations.
        const juce::Rectangle<float> stageBounds{22.f,22.f,width-44.f,
                                                  stage.scope.getY()-31.f};
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.withAlpha(.56f),stageBounds.getTopLeft(),
            Theme::plot.withAlpha(.92f),stageBounds.getBottomRight(),false));
        g.fillRoundedRectangle(stageBounds,10.f);
        const float centreX=width*.5f;
        // Honest two-stage meters: INPUT is the processor's pre-rack peak,
        // OUTPUT is post-FX; bars use the -60 to 0 dBFS log scale. Drawing
        // these is bounded GUI work and never fabricates audio data.
        const float meterWidth=juce::jmin(119.f,(width-70.f)*.28f);
        // Leave an optical gap below the LIVE/dB header even at 70% scale.
        const float meterY=stageBounds.getY()+55.f;
        const auto levelBar=[&](float x,const MeterEnvelope& meter,
                                const juce::String& label){
            const auto bar=juce::Rectangle<float>(x,meterY,meterWidth,4.f);
            g.setColour(Theme::stroke.withAlpha(.34f));
            g.fillRoundedRectangle(bar,2.f);
            const float fraction=meterFraction(audioFresh?meter.level:0.f);
            if(fraction>0.f) {
                g.setColour(meter.level>=1.f?Theme::red:accent.withAlpha(.78f));
                g.fillRoundedRectangle(x,meterY,meterWidth*fraction,4.f,2.f);
            }
            // True peak-hold marker and quiet calibrated scale notches;
            // 0 dB is at right edge, -60 dB at left edge.
            for(const float position:{.25f,.5f,.75f}){
                g.setColour(Theme::bg.withAlpha(.43f));
                const float tick=x+meterWidth*position;
                g.drawLine(tick,meterY,tick,meterY+4.f,.7f);
            }
            const float marker=meterFraction(audioFresh?meter.held:0.f);
            if(marker>0.f){
                const float mark=x+juce::jmin(meterWidth-1.f,meterWidth*marker);
                g.setColour(meter.held>=1.f?Theme::red:Theme::white.withAlpha(.83f));
                g.drawLine(mark,meterY-1.5f,mark,meterY+5.5f,1.15f);
            }
            juce::String title=label;
            if(meterWidth>=95.f){
                const float peak=audioFresh?meter.level:0.f;
                const juce::String db=peak<1.e-5f?"-INF":
                    juce::String(20.f*std::log10(peak),1);
                title+="  "+db;
            }
            Theme::fittedText(g,title,
                {juce::roundToInt(x),juce::roundToInt(meterY-15.f),
                 juce::roundToInt(meterWidth),13},
                 8.4f,Theme::muted,true);
        };
        levelBar(stageBounds.getX()+14.f,inputMeter,"IN / PRE");
        levelBar(stageBounds.getRight()-meterWidth-14.f,outputMeter,"OUT / POST");
        juce::Path backplate;
        backplate.startNewSubPath(centreX-130.f*stage.scale,stage.originY+30.f);
        backplate.lineTo(centreX-200.f*stage.scale,stage.scope.getY()-18.f);
        backplate.lineTo(centreX-50.f*stage.scale,stage.scope.getY()-26.f);
        backplate.lineTo(centreX,stage.originY+32.f);
        backplate.closeSubPath();
        g.setColour(accent.withAlpha(.025f+.055f*activity));
        g.fillPath(backplate);
        juce::Path rightBlade;
        rightBlade.startNewSubPath(centreX+69.f*stage.scale,stage.originY+25.f);
        rightBlade.lineTo(centreX+188.f*stage.scale,stage.scope.getY()-30.f);
        rightBlade.lineTo(centreX+103.f*stage.scale,stage.scope.getY()-30.f);
        rightBlade.closeSubPath();
        g.setColour(accent.withAlpha(.035f+.064f*activity));
        g.fillPath(rightBlade);
        for(int i=0;i<7;++i) {
            const float x=stageBounds.getX()+stageBounds.getWidth()*(i+1)/8.f;
            const float top=stageBounds.getY()+8.f;
            g.setColour(Theme::stroke.withAlpha(.10f+.02f*(i%2)));
            g.drawLine(x,top,x,stage.scope.getY()-26.f,.65f);
        }
        const float beamAlpha=.10f+.14f*activity;
        g.setColour(accent.withAlpha(beamAlpha));
        g.drawLine(centreX-195.f*stage.scale,stage.originY+51.f,
                   centreX-108.f*stage.scale,stage.scope.getY()-31.f,1.25f);
        g.drawLine(centreX+200.f*stage.scale,stage.originY+49.f,
                   centreX+95.f*stage.scale,stage.scope.getY()-31.f,1.25f);

        // Main character. Outline is defined as a full-bodied, standing
        // performer: anatomical pelvis/femurs/tibia/ribs, one arm gripping
        // a real microphone. Slight asymmetry avoids a flat stick-figure.
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.addTransform(juce::AffineTransform::scale(stage.scale)
                .translated(stage.originX,stage.originY));
            auto bone=[&](float x0,float y0,float x1,float y1,
                          float thickness,float highlight){
                g.setColour(shadow.withAlpha(.94f));
                g.drawLine(x0,y0,x1,y1,thickness+3.9f);
                g.setColour(Theme::muted.withAlpha(.78f));
                g.drawLine(x0,y0,x1,y1,thickness+1.f);
                g.setColour(porcelain.withAlpha(highlight));
                g.drawLine(x0-.6f,y0-.8f,x1-.6f,y1-.8f,thickness*.59f);
                g.setColour(accent.withAlpha(.06f+.16f*activity));
                g.drawLine(x0+thickness*.28f,y0,
                           x1+thickness*.28f,y1,1.f);
            };
            // Draped stage coat panels create depth and a distinctive
            // performer silhouette without covering the skeletal chest.
            // Their opening frames the actual ribs instead of turning the
            // character into a simple white stick figure.
            juce::Path leftCoat,rightCoat;
            leftCoat.startNewSubPath(97.f,143.f);
            leftCoat.cubicTo(78.f,156.f,71.f,193.f,73.f,234.f);
            leftCoat.cubicTo(73.f,284.f,62.f,322.f,60.f,352.f);
            leftCoat.lineTo(95.f,347.f);
            leftCoat.lineTo(112.f,255.f);
            leftCoat.cubicTo(106.f,214.f,111.f,183.f,120.f,161.f);
            leftCoat.closeSubPath();
            rightCoat.startNewSubPath(203.f,142.f);
            rightCoat.cubicTo(229.f,164.f,231.f,193.f,226.f,226.f);
            rightCoat.cubicTo(229.f,277.f,239.f,312.f,247.f,350.f);
            rightCoat.lineTo(211.f,351.f);
            rightCoat.lineTo(191.f,254.f);
            rightCoat.cubicTo(196.f,215.f,190.f,174.f,183.f,159.f);
            rightCoat.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                Theme::panelAlt.darker(.11f).withAlpha(.93f),69.f,145.f,
                Theme::bg.withAlpha(.91f),134.f,355.f,false));
            g.fillPath(leftCoat);
            g.setGradientFill(juce::ColourGradient(
                Theme::panelAlt.withAlpha(.91f),229.f,154.f,
                Theme::bg.withAlpha(.93f),186.f,343.f,false));
            g.fillPath(rightCoat);
            g.setColour(Theme::muted.withAlpha(.24f));
            g.strokePath(leftCoat,juce::PathStrokeType(1.6f));
            g.strokePath(rightCoat,juce::PathStrokeType(1.6f));
            g.setColour(porcelain.withAlpha(.14f));
            g.drawLine(98.f,158.f,79.f,289.f,1.1f);
            g.drawLine(205.f,159.f,224.f,294.f,1.1f);
            // Subtle live edge light is tied to measured output amplitude.
            g.setColour(accent.withAlpha(.07f+.20f*activity));
            g.drawLine(96.f,148.f,81.f,258.f,1.f);
            g.drawLine(204.f,147.f,224.f,246.f,1.f);
            // Lower body first, to keep the ribcage and arms layered above.
            bone(126.f,263.f,113.f,318.f,9.f,.74f);
            bone(178.f,263.f,191.f,316.f,9.f,.72f);
            bone(113.f,318.f,105.f,364.f,6.5f,.76f);
            bone(191.f,316.f,205.f,360.f,6.5f,.75f);
            bone(113.f,318.f,118.f,326.f,11.f,.58f);
            bone(191.f,316.f,185.f,325.f,11.f,.56f);
            bone(105.f,364.f,86.f,368.f,5.f,.72f);
            bone(205.f,360.f,226.f,367.f,5.f,.72f);
            // The pelvis is an angular anatomical saddle, not a circle.
            juce::Path hips;
            hips.startNewSubPath(113.f,239.f);
            hips.cubicTo(100.f,240.f,96.f,254.f,106.f,270.f);
            hips.quadraticTo(121.f,283.f,144.f,268.f);
            hips.lineTo(151.f,277.f);
            hips.lineTo(164.f,267.f);
            hips.cubicTo(187.f,285.f,206.f,263.f,197.f,245.f);
            hips.quadraticTo(185.f,234.f,174.f,243.f);
            hips.quadraticTo(152.f,251.f,133.f,240.f);
            hips.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                porcelain.withAlpha(.87f),114.f,241.f,
                Theme::muted.withAlpha(.69f),199.f,277.f,false));
            g.fillPath(hips);
            g.setColour(shadow.withAlpha(.9f));
            g.fillRoundedRectangle(141.f,253.f,18.f,11.f,4.f);
            // Spine = segmented, tapered vertebrae with real depth.
            bone(151.f,123.f,151.f,252.f,7.5f,.57f);
            for(int i=0;i<11;++i){
                const float y=137.f+i*10.2f;
                const float size=juce::jmax(7.f,10.f-i*.19f);
                g.setColour(shadow.withAlpha(.9f));
                g.fillRoundedRectangle(151.f-size*.52f,y-2.f,size+1.2f,
                                       8.f,2.f);
                g.setGradientFill(juce::ColourGradient(
                    porcelain.withAlpha(.77f),145.f,y,
                    Theme::muted.withAlpha(.57f),160.f,y+7.f,false));
                g.fillRoundedRectangle(151.f-size*.44f,y-1.f,size,6.2f,1.4f);
            }
            // Broad scapulae/real collarbones.
            bone(151.f,140.f,100.f,153.f,6.f,.83f);
            bone(151.f,140.f,206.f,150.f,6.f,.83f);
            juce::Path scapula;
            scapula.startNewSubPath(97.f,147.f);
            scapula.lineTo(76.f,161.f);
            scapula.lineTo(106.f,194.f);
            scapula.closeSubPath();
            scapula.startNewSubPath(206.f,146.f);
            scapula.lineTo(228.f,159.f);
            scapula.lineTo(202.f,192.f);
            scapula.closeSubPath();
            g.setColour(Theme::muted.withAlpha(.55f));
            g.fillPath(scapula);
            // Six swept, separated rib pairs, gradually narrowing. Bone
            // thickness, direction and inner highlights suggest volume.
            for(int i=0;i<6;++i){
                const float y=163.f+i*14.3f;
                const float reach=49.f-2.25f*i;
                juce::Path left,right;
                left.startNewSubPath(151.f,y);
                left.cubicTo(133.f,y-9.f,101.f,y-9.f,
                             151.f-reach,y+10.f);
                left.cubicTo(105.f,y+14.f,113.f,y+22.f,
                             133.f,y+22.f);
                right.startNewSubPath(151.f,y);
                right.cubicTo(169.f,y-9.f,199.f,y-8.f,
                              151.f+reach,y+11.f);
                right.cubicTo(200.f,y+15.f,193.f,y+22.f,
                              169.f,y+22.f);
                g.setColour(shadow.withAlpha(.90f));
                g.strokePath(left,juce::PathStrokeType(6.f-i*.32f));
                g.strokePath(right,juce::PathStrokeType(6.f-i*.32f));
                g.setColour(porcelain.withAlpha(.73f-i*.048f+.12f*activity));
                g.strokePath(left,juce::PathStrokeType(3.5f-i*.23f));
                g.strokePath(right,juce::PathStrokeType(3.5f-i*.23f));
                g.setColour(accent.withAlpha(.12f+.19f*activity));
                g.drawLine(151.f,y+3.f,151.f,y+6.f,1.f);
            }
            // Left arm hangs in a natural stage pose; right is raised to
            // the microphone. Hand bones remain visible.
            bone(99.f,154.f,78.f,208.f,8.f,.78f);
            bone(78.f,208.f,59.f,256.f,6.4f,.72f);
            bone(206.f,151.f,245.f,187.f,8.3f,.85f);
            bone(245.f,187.f,269.f,117.f,6.5f,.82f);
            for(int finger=0;finger<4;++finger) {
                bone(58.f+finger*2.f,256.f,51.f+finger*3.f,
                     275.f+finger*1.5f,2.0f,.56f);
            }
            // A real stage microphone with mesh/capsule/handle, drawn over
            // the right hand so the character is visibly performing.
            g.setColour(shadow);
            g.fillRoundedRectangle(260.f,67.f,28.f,50.f,10.f);
            g.setGradientFill(juce::ColourGradient(
                porcelain.withAlpha(.91f),260.f,70.f,
                Theme::muted.withAlpha(.71f),287.f,113.f,false));
            g.fillRoundedRectangle(263.f,69.f,22.f,43.f,9.f);
            g.setColour(Theme::stroke.withAlpha(.82f));
            for(int grille=0;grille<8;++grille){
                const float y=77.f+3.8f*grille;
                const float margin=grille<2 || grille>6?3.5f:1.5f;
                g.drawLine(263.f+margin,y,285.f-margin,y,1.f);
            }
            g.setColour(Theme::green.withAlpha(.82f));
            g.fillRoundedRectangle(268.f,111.f,12.f,4.2f,1.f);
            bone(274.f,115.f,274.f,154.f,4.8f,.66f);
            // Bony knuckles close around the microphone stem.
            bone(265.f,121.f,274.f,124.f,3.8f,.93f);
            bone(264.f,128.f,277.f,131.f,3.5f,.86f);
            for(int i=0;i<3;++i) {
                const float y=133.f+i*5.f;
                g.setColour(porcelain.withAlpha(.77f));
                g.drawLine(271.f,y,279.f,y+2.f,2.f);
            }

            // The head is sculpted from multiple unequal planes, not a
            // smiley face: brow ridge, deep polygonal sockets, cheekbones,
            // mandible, teeth, cracks and asymmetric stage highlights.
            bone(146.f,131.f,146.f,140.f,5.5f,.69f);
            bone(158.f,129.f,160.f,140.f,5.f,.62f);
            juce::Path skull;
            skull.startNewSubPath(151.f,13.f);
            skull.cubicTo(111.f,10.f,101.f,37.f,106.f,69.f);
            skull.cubicTo(106.f,87.f,113.f,102.f,124.f,107.f);
            skull.lineTo(125.f,119.f);
            skull.cubicTo(133.f,138.f,170.f,139.f,181.f,119.f);
            skull.lineTo(181.f,107.f);
            skull.cubicTo(193.f,100.f,198.f,84.f,197.f,64.f);
            skull.cubicTo(200.f,32.f,180.f,12.f,151.f,13.f);
            skull.closeSubPath();
            g.setColour(shadow);
            g.strokePath(skull,juce::PathStrokeType(5.0f));
            g.setGradientFill(juce::ColourGradient(
                porcelain.withAlpha(.88f),110.f,23.f,
                Theme::muted.withAlpha(.54f),195.f,136.f,false));
            g.fillPath(skull);
            // Angular skull facets catch directional stage light, giving
            // depth and cheekbone planes instead of a single flat mask.
            juce::Path foreheadPlane,cheekPlane;
            foreheadPlane.startNewSubPath(120.f,40.f);
            foreheadPlane.quadraticTo(146.f,19.f,167.f,32.f);
            foreheadPlane.lineTo(156.f,57.f);
            foreheadPlane.lineTo(132.f,60.f);
            foreheadPlane.closeSubPath();
            cheekPlane.startNewSubPath(175.f,98.f);
            cheekPlane.lineTo(189.f,94.f);
            cheekPlane.lineTo(177.f,118.f);
            cheekPlane.lineTo(168.f,122.f);
            cheekPlane.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                porcelain.withAlpha(.15f),125.f,33.f,
                Theme::muted.withAlpha(.055f),171.f,65.f,false));
            g.fillPath(foreheadPlane);
            g.setColour(Theme::bg.withAlpha(.23f));
            g.fillPath(cheekPlane);
            g.setColour(porcelain.withAlpha(.20f));
            g.drawLine(121.f,42.f,129.f,36.f,.9f);
            g.drawLine(178.f,103.f,170.f,119.f,.9f);
            // Temporal shadows and irregular planar cheekbone ridges.
            juce::Path temple;
            temple.startNewSubPath(107.f,67.f);
            temple.lineTo(116.f,89.f);temple.lineTo(125.f,97.f);
            temple.lineTo(117.f,103.f);temple.closeSubPath();
            temple.startNewSubPath(196.f,69.f);
            temple.lineTo(188.f,90.f);temple.lineTo(179.f,100.f);
            temple.lineTo(185.f,104.f);temple.closeSubPath();
            g.setColour(Theme::muted.withAlpha(.49f));
            g.fillPath(temple);
            juce::Path sockets;
            sockets.startNewSubPath(116.f,76.f);
            sockets.lineTo(142.f,73.f);
            sockets.lineTo(143.f,86.f);
            sockets.quadraticTo(131.f,95.f,119.f,89.f);
            sockets.closeSubPath();
            sockets.startNewSubPath(159.f,74.f);
            sockets.lineTo(186.f,77.f);
            sockets.lineTo(184.f,91.f);
            sockets.quadraticTo(168.f,96.f,158.f,85.f);
            sockets.closeSubPath();
            sockets.startNewSubPath(151.f,88.f);
            sockets.lineTo(142.f,106.f);
            sockets.lineTo(159.f,105.f);
            sockets.closeSubPath();
            g.setColour(shadow);
            g.fillPath(sockets);
            g.setColour(accent.withAlpha(.07f+.25f*activity));
            g.drawLine(121.f,88.f,140.f,85.f,1.7f);
            g.drawLine(163.f,85.f,180.f,89.f,1.7f);
            g.setColour(porcelain.withAlpha(.56f));
            g.drawLine(114.f,72.f,142.f,69.f,2.f);
            g.drawLine(160.f,69.f,189.f,72.f,2.f);
            g.drawLine(119.f,100.f,137.f,105.f,2.f);
            g.drawLine(165.f,105.f,184.f,100.f,2.f);
            // Split maxillary teeth and mandible.
            g.setColour(shadow);
            g.fillRoundedRectangle(130.f,115.f,44.f,4.f,1.f);
            for(int tooth=0;tooth<8;++tooth)
                g.drawLine(132.f+5.f*tooth,115.f,132.f+5.f*tooth,
                           tooth==3||tooth==4?128.f:125.f,1.2f);
            juce::Path jaw;
            jaw.startNewSubPath(123.f,109.f);
            jaw.cubicTo(122.f,135.f,136.f,143.f,150.f,144.f);
            jaw.cubicTo(171.f,145.f,185.f,131.f,183.f,107.f);
            g.setColour(porcelain.withAlpha(.53f));
            g.strokePath(jaw,juce::PathStrokeType(3.f));
            // Fine forehead fissure and spectral one-sided edge light.
            g.setColour(Theme::muted.withAlpha(.47f));
            g.drawLine(160.f,30.f,153.f,49.f,1.f);
            g.drawLine(153.f,49.f,158.f,62.f,1.f);
            g.setColour(accent.withAlpha(.12f+.31f*activity));
            g.drawLine(104.f,64.f,107.f,88.f,2.0f);
            g.drawLine(187.f,45.f,193.f,69.f,1.f);
        }

        // The trace and bone illumination are directly audio-reactive;
        // nothing is a fake spectrum or prerecorded animation.
        const auto scope=stage.scope;
        g.setGradientFill(juce::ColourGradient(
            Theme::plot.withAlpha(.97f),scope.getTopLeft(),
            Theme::panelAlt.withAlpha(.86f),scope.getBottomRight(),false));
        g.fillRoundedRectangle(scope,8.f);
        g.setColour(accent.withAlpha(.29f));
        g.drawRoundedRectangle(scope.reduced(.5f),8.f,1.f);
        g.setColour(Theme::stroke.withAlpha(.43f));
        g.drawLine(scope.getX()+9.f,scope.getCentreY(),
                   scope.getRight()-9.f,scope.getCentreY(),.85f);
        for(const float offset:{.25f,.75f}) {
            g.setColour(Theme::stroke.withAlpha(.17f));
            g.drawLine(scope.getX()+9.f,scope.getY()+scope.getHeight()*offset,
                       scope.getRight()-9.f,scope.getY()+scope.getHeight()*offset,.65f);
        }
        // Quiet waveform grid is rectangular, never an orbit/ellipse.
        for(int index=1;index<8;++index){
            const float x=scope.getX()+scope.getWidth()*index/8.f;
            g.setColour(Theme::stroke.withAlpha(.17f));
            g.drawLine(x,scope.getY()+8.f,x,scope.getBottom()-8.f,.7f);
        }
        juce::Path trace;
        for(int index=0;index<128;++index){
            const float raw=signal[(size_t)index];
            // Clear the visual trace when the DAW has stopped processing
            // instead of displaying a stale recorded vocal as LIVE.
            const float value=audioFresh && std::isfinite(raw)?
                juce::jlimit(-1.f,1.f,raw):0.f;
            const float x=scope.getX()+11.f+(scope.getWidth()-22.f)*index/127.f;
            const float y=scope.getCentreY()-value*scope.getHeight()*.43f;
            if(index==0)trace.startNewSubPath(x,y);
            else trace.lineTo(x,y);
        }
        if(live) {
            auto envelope=trace;
            envelope.lineTo(scope.getRight()-11.f,scope.getCentreY());
            envelope.lineTo(scope.getX()+11.f,scope.getCentreY());
            envelope.closeSubPath();
            g.setColour(accent.withAlpha(.045f+.07f*activity));
            g.fillPath(envelope);
        }
        g.setColour(accent.withAlpha(live?.95f:.43f));
        g.strokePath(trace,juce::PathStrokeType(live?1.85f:1.2f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // Measured waveform RMS, not a synthetic envelope or new processor.
        if(scope.getHeight()>=52.f){
            const float areaX=scope.getX()+11.f,areaY=scope.getBottom()-11.f;
            const float areaW=juce::jmin(126.f,scope.getWidth()*.28f);
            g.setColour(Theme::stroke.withAlpha(.55f));
            g.fillRoundedRectangle(areaX,areaY,areaW,2.5f,1.f);
            if(rms>.0005f) {
                g.setColour(accent.withAlpha(.65f));
                g.fillRoundedRectangle(areaX,areaY,areaW*meterFraction(rms),2.5f,1.f);
            }
            Theme::text(g,"WAVEFORM RMS",
                {juce::roundToInt(areaX),juce::roundToInt(areaY-13.f),
                 juce::roundToInt(areaW+18.f),12},8.1f,Theme::muted,true);
        }
        Theme::text(g,live?"LIVE":"IDLE",
                    {juce::roundToInt(stage.scope.getX()+12.f),34,84,22},
                    10.7f,live?accent:Theme::muted,true);
        const juce::String db=outputPeak<1.e-6f?juce::String("-INF"):
            juce::String(20.f*std::log10(outputPeak),1);
        const bool clipping=audioFresh && outputMeter.held>=1.f;
        Theme::text(g,clipping?"CLIP HOLD":db+" dBFS",
                    {getWidth()-177,34,131,22},10.7f,
                    clipping?Theme::red:Theme::muted,true,
                    juce::Justification::centredRight);
    }
private:
    SkellPresetAudioProcessor& processor;
    bool audioFresh=false;
    MeterEnvelope inputMeter,outputMeter;
};
