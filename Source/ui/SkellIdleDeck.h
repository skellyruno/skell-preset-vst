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
        if(!fresh){inputMeter={};outputMeter={};activityLevel=0.f;cachedSignal.fill(0.f);}
        if(isVisible())repaint();
    }
    struct MeterEnvelope {
        float level=0.f;
        float held=0.f;
        int holdFrames=0;
    };
    static void followPeak(MeterEnvelope& meter,float peak) noexcept {
        peak=std::isfinite(peak)?juce::jlimit(0.f,4.f,peak):0.f;
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
        processor.getLiveWaveform(cachedSignal);
        float maximum=0.f;
        double energy=0.;
        for(auto& sample:cachedSignal){
            sample=std::isfinite(sample)?juce::jlimit(-1.f,1.f,sample):0.f;
            maximum=juce::jmax(maximum,std::abs(sample));
            energy+=static_cast<double>(sample)*sample;
        }
        const float rms=static_cast<float>(std::sqrt(energy/static_cast<double>(cachedSignal.size())));
        const float target=juce::jlimit(0.f,1.f,.38f*maximum+1.75f*rms+.30f*outputMeter.level);
        // Smooth the measured animation on the UI timer, not during paint.
        // Fast attack and gentler release avoid flickering between frames.
        activityLevel=followActivity(activityLevel,target);
    }
    static float followActivity(float previous,float next) noexcept {
        previous=std::isfinite(previous)?juce::jlimit(0.f,1.f,previous):0.f;
        next=std::isfinite(next)?juce::jlimit(0.f,1.f,next):0.f;
        return previous+(next-previous)*(next>previous?.68f:.22f);
    }
    bool hasFreshAudio() const noexcept {return audioFresh;}
    bool hasAudibleOutput() const noexcept {
        return audioFresh && activityLevel>.0005f;
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
        return testMeterEnvelopeForTest() &&
            followActivity(0.f,1.f)>.6f &&
            followActivity(1.f,0.f)>.7f &&
            followActivity(0.f,std::numeric_limits<float>::quiet_NaN())==0.f &&
            meterFraction(0.f)==0.f &&
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
        out.scale=juce::jlimit(.30f,1.35f,juce::jmin(
            (w-2.f*margin)/345.f,(out.scope.getY()-22.f)/390.f));
        out.originX=w*.5f-151.f*out.scale;
        const float spare=juce::jmax(0.f,out.scope.getY()-22.f-390.f*out.scale);
        out.originY=juce::jmax(10.f,22.f+(1.f-out.scale)*3.f+spare*.44f);
        out.valid=out.originX>0.f && out.originX+290.f*out.scale<=w &&
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
        Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
        if(!stage.valid)return;
        const bool live=hasAudibleOutput();
        const float activity=live?activityLevel:0.f;
        const auto accent=Theme::green;
        const auto porcelain=Theme::white;
        const auto shadow=Theme::bg;

        // Minimal midnight performance stage. All lighting is drawn in the
        // native editor; reactive intensity is measured from actual audio.
        const auto stageBounds=juce::Rectangle<float>(10.f,10.f,width-20.f,
            stage.scope.getY()-18.f);
        const float centreX=width*.5f;
        // The feet are drawn around y=368 in model space. Ground the
        // floor at the character's feet at every supported aspect ratio.
        const float horizon=stage.originY+372.f*stage.scale;
        // Stage reflections must never spill into the waveform lane.
        const float floorBottom=stage.scope.getY()-5.f;
        const auto* mode=processor.state.getRawParameterValue("mode");
        const bool bypassed=mode && (int)mode->load()==3;
        g.setColour(Theme::bg);
        g.fillRoundedRectangle(stageBounds,8.f);
        g.setGradientFill(juce::ColourGradient(
            accent.withAlpha(.08f+.12f*activity),centreX,horizon-170.f*stage.scale,
            Theme::bg,centreX+width*.43f,horizon-170.f*stage.scale,true));
        g.fillRoundedRectangle(stageBounds,8.f);
        // Wide, shallow stage floor with a restrained fog band.
        const auto floor=juce::Rectangle<float>(stageBounds.getX(),horizon,
            stageBounds.getWidth(),juce::jmax(1.f,floorBottom-horizon));
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.withAlpha(.80f),centreX,horizon,
            Theme::bg,centreX,floor.getBottom(),false));
        g.fillRect(floor);
        g.setColour(accent.withAlpha(.12f+.20f*activity));
        g.drawLine(stageBounds.getX()+14.f,horizon,stageBounds.getRight()-14.f,horizon,.8f);
        // Directional beams and a restrained central spotlight.
        const float halo=120.f*stage.scale;
        g.setGradientFill(juce::ColourGradient(
            accent.withAlpha(.045f+.055f*activity),centreX,horizon-225.f*stage.scale,
            accent.withAlpha(0.f),centreX+halo,horizon-225.f*stage.scale,true));
        g.fillEllipse(centreX-halo,horizon-345.f*stage.scale,halo*2.f,halo*2.f);
        // Two muted directional lights rather than a grid of meters.
        for(int side:{-1,1}){
            const float x=centreX+(float)side*stageBounds.getWidth()*.38f;
            juce::Path beam;
            beam.startNewSubPath(x,stageBounds.getY()+8.f);
            beam.lineTo(centreX+(float)side*85.f*stage.scale,horizon);
            beam.lineTo(centreX+(float)side*175.f*stage.scale,horizon);
            beam.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                accent.withAlpha(.045f+.065f*activity),x,stageBounds.getY()+8.f,
                accent.withAlpha(.008f),centreX,horizon,false));
            g.fillPath(beam);
        }
        // Distant tombstones frame the performer without competing for focus.
        for(int side:{-1,1})for(int k=0;k<3;++k){
            const float x=centreX+(float)side*(stageBounds.getWidth()*.23f+k*stageBounds.getWidth()*.075f);
            const float h=(24.f+9.f*(float)(k%2)+4.f*(float)(2-k))*stage.scale;
            const float w=(17.f+3.f*(float)k)*stage.scale;
            const float y=horizon-h;
            g.setColour(Theme::panelAlt.withAlpha(.58f));
            g.fillRoundedRectangle(x-w*.5f,y,w,h,3.f*stage.scale);
            g.setColour(Theme::bg.withAlpha(.48f));
            g.fillRoundedRectangle(x-w*.72f,y+h-2.f*stage.scale,
                w*1.44f,4.f*stage.scale,1.f*stage.scale);
            g.setColour(Theme::muted.withAlpha(.14f));
            g.drawRoundedRectangle(x-w*.5f,y,w,h,3.f*stage.scale,.8f);
            g.setColour(Theme::muted.withAlpha(.10f));
            g.drawLine(x-w*.28f,y+8.f*stage.scale,x+w*.28f,y+8.f*stage.scale,.75f);
            g.drawLine(x,y+4.f*stage.scale,x,y+13.f*stage.scale,.75f);
        }
        // Low mist is a soft gradient, not a large animated texture.
        g.setGradientFill(juce::ColourGradient(
            Theme::muted.withAlpha(.045f+.035f*activity),centreX,horizon-18.f*stage.scale,
            Theme::bg.withAlpha(0.f),centreX,horizon-58.f*stage.scale,false));
        g.fillRect(stageBounds.getX(),horizon-58.f*stage.scale,stageBounds.getWidth(),58.f*stage.scale);
        // Restrained floor reflections and soft contact shadow.
        // Static set dressing, with brightness driven by real audio.
        for(int i=0;i<5;++i){
            const float spread=(45.f+i*24.f)*stage.scale;
            const float y=horizon+(3.f+2.6f*i)*stage.scale;
            if(y>=floorBottom)break;
            g.setColour(accent.withAlpha((.020f+.025f*activity)/(1.f+.30f*i)));
            g.drawLine(centreX-spread,y,centreX+spread,y,1.1f*stage.scale);
        }
        g.setColour(Theme::bg.withAlpha(.90f));
        g.fillEllipse(centreX-105.f*stage.scale,horizon-4.f*stage.scale,
                      210.f*stage.scale,juce::jmax(1.f,juce::jmin(10.f*stage.scale,
                          floorBottom-horizon+4.f*stage.scale)));

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
                g.drawLine(x0-.6f,y0-.8f,x1-.6f,y1-.8f,thickness*.72f);
                g.setColour(accent.withAlpha(.06f+.16f*activity));
                g.drawLine(x0+thickness*.28f,y0,
                           x1+thickness*.28f,y1,1.f);
            };
            // Bare anatomical silhouette, no cape, coat or draped cloth.
            // A slightly stronger directional bone highlight reads clearly
            // against the dark graveyard without concealing the limbs.
            // Lower body first, to keep the ribcage and arms layered above.
            bone(126.f,263.f,113.f,318.f,9.f,.74f);
            bone(178.f,263.f,191.f,316.f,9.f,.72f);
            bone(113.f,318.f,105.f,364.f,6.5f,.76f);
            bone(191.f,316.f,205.f,360.f,6.5f,.75f);
            // Paired fibulae and articulated ankle joints.
            bone(119.f,323.f,111.f,361.f,2.7f,.56f);
            bone(185.f,322.f,199.f,357.f,2.7f,.56f);
            bone(105.f,360.f,106.f,366.f,8.f,.82f);
            bone(205.f,357.f,205.f,364.f,8.f,.82f);
            bone(113.f,318.f,118.f,326.f,11.f,.58f);
            bone(191.f,316.f,185.f,325.f,11.f,.56f);
            bone(105.f,364.f,86.f,368.f,5.f,.72f);
            bone(205.f,360.f,226.f,367.f,5.f,.72f);
            for(int toe=0;toe<4;++toe){
                bone(102.f,366.f,84.f+toe*3.4f,369.f+toe*.6f,2.2f,.66f);
                bone(207.f,364.f,227.f-toe*3.4f,368.f+toe*.6f,2.2f,.66f);
            }
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
            bone(83.f,211.f,65.f,255.f,2.8f,.57f);
            bone(239.f,183.f,263.f,118.f,2.8f,.59f);
            bone(78.f,205.f,77.f,211.f,9.f,.69f);
            bone(245.f,183.f,245.f,190.f,9.f,.74f);
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
            const float jawDrop=live?4.5f*activity:0.f;
            juce::Path jaw;
            jaw.startNewSubPath(123.f,109.f);
            jaw.cubicTo(122.f,135.f+jawDrop,136.f,143.f+jawDrop,150.f,144.f+jawDrop);
            jaw.cubicTo(171.f,145.f+jawDrop,185.f,131.f+jawDrop,183.f,107.f);
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
        // One uninterrupted audio trace floating over the black stage.
        g.setColour(accent.withAlpha(.10f));
        g.drawLine(scope.getX()+9.f,scope.getCentreY(),
                   scope.getRight()-9.f,scope.getCentreY(),.7f);
        juce::Path trace;
        for(int index=0;index<128;++index){
            const float raw=cachedSignal[(size_t)index];
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
        if(live){
            g.setColour(accent.withAlpha(.13f));
            g.strokePath(trace,juce::PathStrokeType(5.f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        g.setColour(accent.withAlpha(live?.95f:.29f));
        g.strokePath(trace,juce::PathStrokeType(live?1.85f:1.0f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const auto status=juce::Rectangle<float>(stageBounds.getX()+15.f,
            stageBounds.getY()+13.f,90.f,24.f);
        g.setColour(Theme::bg.withAlpha(.84f));
        g.fillRoundedRectangle(status,6.f);
        const auto lamp=bypassed?Theme::amber:live?accent:Theme::muted;
        if(live && !bypassed){
            g.setColour(lamp.withAlpha(.13f+.18f*activity));
            g.fillEllipse(status.getX()+6.f,status.getCentreY()-7.f,14.f,14.f);
        }
        g.setColour(lamp);
        g.fillEllipse(status.getX()+10.f,status.getCentreY()-3.f,6.f,6.f);
        Theme::text(g,bypassed?"BYPASS":live?"LIVE":"IDLE",
            {juce::roundToInt(status.getX()+23.f),juce::roundToInt(status.getY()),
             66,24},10.7f,lamp,true);

    }
private:
    SkellPresetAudioProcessor& processor;
    bool audioFresh=false;
    MeterEnvelope inputMeter,outputMeter;
    float activityLevel=0.f;
    std::array<float,128> cachedSignal{};
};
