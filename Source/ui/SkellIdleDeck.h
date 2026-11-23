#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include <array>
#include <cmath>

// A full skeletal vocalist on an angular, cinematic stage. Every contour is
// native JUCE geometry on the UI thread: no imported mockups, random spectrum,
// prerecorded animation or new audio-thread allocations.
class SkellIdleDeck final : public juce::Component {
public:
    explicit SkellIdleDeck(SkellPresetAudioProcessor& p):processor(p) {
        setInterceptsMouseClicks(true,false);
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
        const float rms=(float)std::sqrt(energy/128.);
        const bool live=actualPeak>.0005f;
        const float activity=live?juce::jlimit(0.f,1.f,
            .48f*actualPeak+2.1f*rms):0.f;
        const float metered=processor.getOutputPeak();
        const float outputPeak=std::isfinite(metered)?
            juce::jmax(0.f,metered):0.f;
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
                porcelain.withAlpha(.93f),110.f,25.f,
                Theme::muted.withAlpha(.67f),195.f,132.f,false));
            g.fillPath(skull);
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

        // Restrained waveform monitor remains the only moving diagnostic.
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
        // Quiet waveform grid is rectangular, never an orbit/ellipse.
        for(int index=1;index<8;++index){
            const float x=scope.getX()+scope.getWidth()*index/8.f;
            g.setColour(Theme::stroke.withAlpha(.17f));
            g.drawLine(x,scope.getY()+8.f,x,scope.getBottom()-8.f,.7f);
        }
        juce::Path trace;
        for(int index=0;index<128;++index){
            const float raw=signal[(size_t)index];
            const float value=std::isfinite(raw)?
                juce::jlimit(-1.f,1.f,raw):0.f;
            const float x=scope.getX()+11.f+(scope.getWidth()-22.f)*index/127.f;
            const float y=scope.getCentreY()-value*scope.getHeight()*.43f;
            if(index==0)trace.startNewSubPath(x,y);
            else trace.lineTo(x,y);
        }
        g.setColour(accent.withAlpha(live?.95f:.43f));
        g.strokePath(trace,juce::PathStrokeType(live?1.85f:1.2f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        Theme::text(g,live?"LIVE":"IDLE",
                    {juce::roundToInt(stage.scope.getX()+12.f),34,84,22},
                    10.7f,live?accent:Theme::muted,true);
        const juce::String db=outputPeak<1.e-6f?juce::String("-INF"):
            juce::String(20.f*std::log10(outputPeak),1);
        const bool clipping=outputPeak>=.999f;
        Theme::text(g,clipping?"CLIP":db+" dBFS",
                    {getWidth()-177,34,131,22},10.7f,
                    clipping?Theme::red:Theme::muted,true,
                    juce::Justification::centredRight);
    }
private:
    SkellPresetAudioProcessor& processor;
};
