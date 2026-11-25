#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "GraveyardScene.h"
#include "SkellPerformer.h"
#include "StageClock.h"
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
        if(isShowing())repaint();
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
    void tickAnimation(double nowMs) {
        if(!isShowing()){stageClock.pause();return;}
        crowdPhase=static_cast<float>(stageClock.tick(nowMs,hasAudibleOutput()?activityLevel:0.f));
        repaint();
    }
    void visibilityChanged() override {stageClock.pause();}
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
        // Keep the trace compact so the performance stage owns the composition.
        // Advanced mode gets a slim audio rail rather than a second empty panel.
        const float scopeHeight=juce::jmax(26.f,juce::jmin(32.f,h*.095f));
        out.scope={margin,h-bottom-scopeHeight,
                   juce::jmax(1.f,w-2.f*margin),scopeHeight};
        // The stage above the trace is split into the performer's boards and a
        // deep crowd pit. Reserving the pit keeps the audience cinematic
        // instead of a thin strip pressed under the figure's feet.
        const float stageBottom=out.scope.getY()-18.f;
        const float headroom=h>=350.f?juce::jmin(44.f,h*.085f):0.f;
        const float stageTop=12.f+headroom;
        const float available=juce::jmax(0.f,stageBottom-stageTop);
        // Keep enough room for the smallest supported figure before the pit
        // takes its share, so tiny frames still produce a valid layout.
        const float pitMax=juce::jmax(0.f,available-372.f*.30f);
        const float pitHeight=juce::jmin(pitMax,juce::jlimit(28.f,176.f,stageBottom*.30f));
        const float feet=stageBottom-pitHeight;
        out.scale=juce::jlimit(.30f,1.35f,juce::jmin(
            // Let the performer read as the headliner, using the space recovered
            // from the compact trace and preserving a safe edge at every size.
            (w-2.f*margin)/345.f,(feet-stageTop)/372.f));
        out.originX=w*.5f-151.f*out.scale;
        out.originY=feet-372.f*out.scale;
        out.valid=out.originX>0.f && out.originX+290.f*out.scale<=w &&
            out.originY>=10.f &&
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
        // Two separate ground lines. `feetY` is where the performer stands;
        // `horizon` is the cemetery skyline, lifted well above the feet so the
        // graves, fence and chapel get a real band of depth instead of being
        // crushed into a strip behind the crowd pit.
        const float feetY=stage.originY+372.f*stage.scale;
        const float horizon=juce::jmax(stageBounds.getY()+96.f,
            feetY-juce::jlimit(70.f,190.f,feetY*.34f));
        const auto* mode=processor.state.getRawParameterValue("mode");
        const bool bypassed=mode && (int)mode->load()==3;
        backdrop.draw(g,stageBounds,horizon);
        backdrop.atmosphere(g,stageBounds,horizon,crowdPhase,activity);
        // Restrained moving key light leaves the static environment cached.
        g.setGradientFill(juce::ColourGradient(Theme::white.withAlpha(.055f+.05f*activity),
            centreX-65.f,feetY-240.f*stage.scale,Theme::bg.withAlpha(0.f),centreX+150.f,feetY-70.f,true));
        g.fillEllipse(centreX-175.f*stage.scale,feetY-365.f*stage.scale,350.f*stage.scale,365.f*stage.scale);
        // Volumetric stage beams from the rig above the performer. These are
        // soft screen-blended wedges, not textured sprites, and breathe with
        // measured output so the rig feels alive without ever lying about audio.
        {
            juce::Graphics::ScopedSaveState beams(g);
            g.reduceClipRegion(stageBounds.toNearestInt());
            const float rigY=stageBounds.getY()+7.f;
            const float spread=.55f+.10f*activity;
            for(int beam=0;beam<3;++beam) {
                const float rigX=centreX+(beam-1)*92.f*stage.scale;
                const float targetX=centreX+(beam-1)*205.f*spread;
                const float targetY=feetY+6.f;
                juce::Path wedge;
                wedge.startNewSubPath(rigX-3.f,rigY);
                wedge.lineTo(rigX+3.f,rigY);
                wedge.lineTo(targetX+44.f*stage.scale,targetY);
                wedge.lineTo(targetX-44.f*stage.scale,targetY);
                wedge.closeSubPath();
                const auto tint=(beam==1?accent:Theme::blue);
                g.setGradientFill(juce::ColourGradient(
                    tint.withAlpha(.028f+.055f*activity),rigX,rigY,
                    tint.withAlpha(0.f),targetX,targetY,false));
                g.fillPath(wedge);
            }
            // Rig housing and three lens lamps.
            g.setColour(juce::Colour(0xff121719));
            g.fillRoundedRectangle(centreX-104.f*stage.scale,stageBounds.getY()+2.f,
                                   208.f*stage.scale,7.f*stage.scale,3.f*stage.scale);
            for(int lamp=0;lamp<3;++lamp) {
                const float lx=centreX+(lamp-1)*92.f*stage.scale;
                const int lit=lamp==1?juce::roundToInt(160+95*activity):juce::roundToInt(70+120*activity);
                g.setColour(Theme::green.withAlpha(.10f));
                g.fillEllipse(lx-9.f*stage.scale,rigY-3.f*stage.scale,
                              18.f*stage.scale,12.f*stage.scale);
                g.setColour((lamp==1?accent:Theme::blue).withAlpha(
                    juce::jlimit(.18f,.98f,(float)lit/255.f)));
                g.fillEllipse(lx-3.2f*stage.scale,rigY-1.f*stage.scale,
                              6.4f*stage.scale,4.2f*stage.scale);
            }
        }
        // Low mist is a soft gradient, not a large animated texture.
        g.setGradientFill(juce::ColourGradient(
            Theme::muted.withAlpha(.045f+.035f*activity),centreX,feetY-18.f*stage.scale,
            Theme::bg.withAlpha(0.f),centreX,feetY-58.f*stage.scale,false));
        g.fillRect(stageBounds.getX(),feetY-58.f*stage.scale,stageBounds.getWidth(),58.f*stage.scale);
        // Restrained floor reflections and soft contact shadow.
        // Static set dressing, with brightness driven by real audio.
        for(int i=0;i<5;++i){
            const float spread=(45.f+i*24.f)*stage.scale;
            const float y=feetY+(3.f+2.6f*i)*stage.scale;
            g.setColour(accent.withAlpha((.020f+.025f*activity)/(1.f+.30f*i)));
            g.drawLine(centreX-spread,y,centreX+spread,y,1.1f*stage.scale);
        }
        g.setColour(Theme::bg.withAlpha(.90f));
        g.fillEllipse(centreX-105.f*stage.scale,feetY-4.f*stage.scale,
                      210.f*stage.scale,10.f*stage.scale);
        // Soft spotlight pool on the boards beneath the performer. It is the
        // brightest floor element and gives the figure real stage presence.
        {
            const float poolW=168.f*stage.scale,poolH=(.34f+.10f*activity)*poolW;
            for(int ring=3;ring>=1;--ring) {
                const float f=(float)ring/3.f;
                g.setGradientFill(juce::ColourGradient(
                    Theme::green.withAlpha(.050f*activity+.012f),centreX,feetY,
                    Theme::bg.withAlpha(0.f),centreX+poolW*f*.55f,feetY,true));
                g.fillEllipse(centreX-poolW*f,feetY-poolH*f*.42f,
                              poolW*2.f*f,poolH*2.f*f);
            }
            // Mirrored figure glint, kept subtle and tied to measured peak.
            g.setGradientFill(juce::ColourGradient(
                Theme::white.withAlpha(.035f+.055f*activity),centreX,feetY,
                Theme::bg.withAlpha(0.f),centreX,feetY+26.f*stage.scale,false));
            g.fillEllipse(centreX-26.f*stage.scale,feetY-2.f*stage.scale,
                          52.f*stage.scale,30.f*stage.scale);
        }

        // Main character: a full-body skeletal vocalist built in its own
        // procedural module. Model space is 345x372 with the floor at y=368,
        // exactly the box layoutFor() reserves, so the figure stays grounded
        // from 320x240 up to 2000x1000 without per-size tuning.
        {
            juce::Graphics::ScopedSaveState performerLayer(g);
            g.reduceClipRegion(stageBounds.toNearestInt());
            performer.draw(g,stage.originX,stage.originY,stage.scale,
                           activity,crowdPhase);
        }

        // A deep, cheering skeleton pit between the camera and the boards: we
        // look over their shoulders at the performer. Every figure is a very
        // dark silhouette rimmed by stage spill, so the pit reads as a mass of
        // bone and raised arms instead of a tiled pattern. Motion follows the
        // measured output; a slow ambient sway keeps the crowd alive between
        // phrases and never claims audio that is not there.
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(stageBounds.toNearestInt());
            // Lit boards under the feet before the pit begins.
            const float pitTop=feetY+juce::jmax(5.f,16.f*stage.scale);
            const float pitBottom=stageBounds.getBottom();
            const float depth=juce::jmax(0.f,pitBottom-pitTop);
            if(depth>10.f){
                // The stage floor is what the crowd is silhouetted against:
                // a lit, slightly green-grey ramp that keeps the near rows
                // readable without any figure being bright itself.
                g.setGradientFill(juce::ColourGradient(
                    juce::Colour(0xff1a2320),centreX,pitTop,
                    juce::Colour(0xff05080a),centreX,pitBottom,false));
                g.fillRect(stageBounds.getX(),pitTop,stageBounds.getWidth(),depth);
                // Warm key spill from the performer's pool plus a cold balcony
                // glow at the edges, both audio-tinted.
                g.setGradientFill(juce::ColourGradient(
                    Theme::green.withAlpha(.07f+.06f*activity),centreX,pitTop,
                    Theme::bg.withAlpha(0.f),centreX,pitTop+depth*.55f,true));
                g.fillRect(stageBounds.getX(),pitTop,stageBounds.getWidth(),depth*.6f);
                ensureCrowd();
                const float idleSway=.22f+.78f*activity;
                for(int row=0;row<crowdLayers;++row) {
                    const float t=crowdLayers>1?(float)row/(float)(crowdLayers-1):0.f;
                    // Far rows are small and high, near rows large and cropped
                    // by the bottom of the stage. The pit starts narrow so the
                    // cemetery keeps its own band of picture above the crowd.
                    const float heightPix=depth*(.22f+.42f*t);
                    if(heightPix<6.f)continue;
                    // Near figures are only slightly lighter than far ones, so
                    // depth comes from size and overlap rather than brightness.
                    const auto bodyColour=juce::Colour(0xff04070a).withAlpha(.72f+.20f*t);
                    const auto rimColour=porcelain.withAlpha(.06f+.16f*t);
                    const auto edgeGlow=accent.withAlpha(.05f+.10f*activity);
                    for(int i=0;i<crowdCount;++i) {
                        const auto& figure=crowd[(size_t)i];
                        if(figure.layer!=row)continue;
                        const float wave=(.35f*std::sin(crowdPhase*.62f+figure.phase)
                            +.65f*std::sin(crowdPhase*(.78f+.05f*figure.rate)+figure.phase))
                            *idleSway;
                        const float bob=std::sin(crowdPhase*1.28f+figure.phase*1.7f)*idleSway;
                        const float hgt=heightPix*figure.sizeJitter;
                        const float unit=hgt/figure.height;
                        const float x=stageBounds.getX()+stageBounds.getWidth()*figure.x
                            +wave*2.6f*unit;
                        // Vertical jitter inside the row prevents ruled lines.
                        const float headY=pitTop+depth*(.03f+.62f*t)+figure.yJitter*depth*.10f
                            -std::abs(bob)*2.0f*unit;
                        const float base=headY+hgt;
                        const float ry=hgt*figure.headRatio;
                        const float rx=ry*.86f;
                        const float armW=juce::jmax(1.1f,hgt*.045f);
                        const float shoulderY=headY+ry*1.35f;
                        const float shoulderW=rx*1.9f;
                        // Distant silhouettes do not need sub-pixel anatomy,
                        // multiple stroked paths or a rotated graphics context.
                        // Near rows retain the full articulated skeleton art.
                        if(ry<2.6f) {
                            g.setColour(bodyColour);
                            g.fillEllipse(x-rx,headY-ry,2.f*rx,2.f*ry);
                            g.fillRoundedRectangle(x-shoulderW,shoulderY,
                                2.f*shoulderW,juce::jmax(1.f,base-shoulderY),rx);
                            g.setColour(rimColour);
                            g.drawLine(x-shoulderW*.8f,shoulderY,x-rx,headY+ry,1.f);
                            g.drawLine(x+rx,headY+ry,x+shoulderW*.8f,shoulderY,1.f);
                            continue;
                        }
                        const juce::Graphics::ScopedSaveState tilt(g);
                        g.addTransform(juce::AffineTransform::rotation(
                            figure.tilt*.06f*idleSway,x,headY+ry));
                        // Raised arms behind the skull.
                        for(int side:{-1,1}) {
                            const float sign=(float)side;
                            const float sx=x+sign*rx*.80f;
                            float ex,ey,hx,hy;
                            if(figure.pose==0) {
                                ex=x+sign*rx*1.8f;ey=headY+ry*.35f;
                                hx=x+sign*(rx*2.1f+wave*rx*.5f);hy=headY-hgt*.24f;
                            } else if(figure.pose==1) {
                                const bool up=sign>0.f;
                                ex=x+sign*rx*(up?1.5f:2.0f);ey=headY+ry*(up?.20f:1.20f);
                                hx=ex+sign*rx*.35f;hy=ey+(up?-hgt*.26f:hgt*.09f);
                            } else if(figure.pose==2) {
                                ex=x+sign*rx*1.5f;ey=shoulderY+ry*.55f;
                                hx=x+sign*rx*(.55f+std::abs(wave)*.55f);hy=ey-ry*.32f;
                            } else {
                                ex=x+sign*rx*1.6f;ey=headY+ry*.45f;
                                hx=ex+sign*rx*.20f;hy=ey-hgt*.18f;
                            }
                            g.setColour(bodyColour);
                            g.drawLine(sx,shoulderY,ex,ey,armW+2.2f);
                            g.drawLine(ex,ey,hx,hy,armW+2.2f);
                            g.setColour(rimColour.withMultipliedAlpha(.75f));
                            g.drawLine(sx,shoulderY,ex,ey,armW*.7f);
                            g.drawLine(ex,ey,hx,hy,armW*.7f);
                            g.fillEllipse(hx-armW*1.1f,hy-armW*1.1f,armW*2.2f,armW*2.2f);
                        }
                        // Back, shoulders and neck as one silhouette; only the
                        // shoulder line catches light, which is what sells a
                        // crowd seen from behind.
                        juce::Path body;
                        body.startNewSubPath(x-shoulderW,base+hgt*.45f);
                        body.lineTo(x-shoulderW*.96f,shoulderY+ry*.30f);
                        body.quadraticTo(x-shoulderW*.58f,shoulderY-ry*.42f,x-rx*.62f,shoulderY);
                        body.lineTo(x-rx*.66f,headY+ry*.60f);
                        body.lineTo(x+rx*.66f,headY+ry*.60f);
                        body.lineTo(x+rx*.62f,shoulderY);
                        body.quadraticTo(x+shoulderW*.58f,shoulderY-ry*.42f,x+shoulderW*.96f,shoulderY+ry*.30f);
                        body.lineTo(x+shoulderW,base+hgt*.45f);
                        body.closeSubPath();
                        g.setColour(bodyColour);
                        g.fillPath(body);
                        // Rim: shoulder crest then the skull dome.
                        g.setColour(rimColour);
                        juce::Path shoulders;
                        shoulders.startNewSubPath(x-shoulderW*.94f,shoulderY+ry*.34f);
                        shoulders.quadraticTo(x-shoulderW*.56f,shoulderY-ry*.44f,x-rx*.60f,shoulderY);
                        g.strokePath(shoulders,juce::PathStrokeType(juce::jmax(.7f,hgt*.014f)));
                        juce::Path shouldersR;
                        shouldersR.startNewSubPath(x+rx*.60f,shoulderY);
                        shouldersR.quadraticTo(x+shoulderW*.56f,shoulderY-ry*.44f,x+shoulderW*.94f,shoulderY+ry*.34f);
                        g.strokePath(shouldersR,juce::PathStrokeType(juce::jmax(.7f,hgt*.014f)));
                        // Skull dome: dark fill, then a thin lit crescent.
                        g.setColour(bodyColour);
                        g.fillEllipse(x-rx,headY-ry,2.f*rx,2.f*ry);
                        juce::Path crescent;
                        crescent.addCentredArc(x,headY,rx,ry,0.f,
                            juce::MathConstants<float>::pi*1.06f,
                            juce::MathConstants<float>::pi*1.94f,true);
                        g.setColour(rimColour);
                        g.strokePath(crescent,juce::PathStrokeType(juce::jmax(.7f,hgt*.016f)));
                        // Jaw hinge and open maw read as a notch, not a face.
                        if(ry>2.6f) {
                            g.setColour(juce::Colour(0xff0b1012));
                            g.fillEllipse(x-rx*.34f,headY+ry*.30f,rx*.68f,
                                          ry*(.42f+.26f*std::abs(wave)));
                            if(ry>4.5f) {
                                g.setColour(rimColour.withMultipliedAlpha(.55f));
                                g.drawLine(x-rx*.92f,headY-ry*.10f,x-rx*.92f,headY+ry*.55f,
                                           juce::jmax(.6f,hgt*.016f));
                            }
                        }
                        // Concert phone lights: the brightest thing in the pit.
                        if(figure.phone>0) {
                            const float px=x+(figure.phoneSide?1.f:-1.f)*shoulderW*1.02f
                                +wave*rx*.3f;
                            const float py=headY-hgt*.20f-std::abs(bob)*hgt*.10f;
                            const float glow=juce::jlimit(.18f,1.f,.30f+.70f*activity);
                            g.setColour(accent.withAlpha(.05f*glow));
                            g.fillEllipse(px-5.2f*unit,py-5.2f*unit,10.4f*unit,10.4f*unit);
                            g.setColour(juce::Colours::white.withAlpha(.16f*glow));
                            g.fillEllipse(px-3.f*unit,py-3.f*unit,6.f*unit,6.f*unit);
                            g.setColour(juce::Colours::white.withAlpha(.48f+.50f*glow));
                            g.fillEllipse(px-unit,py-unit,2.f*unit,2.f*unit);
                        }
                        if(row>=crowdLayers-2) {
                            g.setColour(edgeGlow);
                            g.drawLine(x-shoulderW*.9f,shoulderY+ry*.4f,
                                       x-shoulderW*.72f,base+hgt*.3f,
                                       juce::jmax(.6f,hgt*.014f));
                        }
                    }
                }
                // Cold haze rolling off the pit and a warm lip line.
                g.setGradientFill(juce::ColourGradient(
                    Theme::blue.withAlpha(.05f+.04f*activity),centreX,pitBottom,
                    Theme::bg.withAlpha(0.f),centreX,pitTop+depth*.30f,false));
                g.fillRect(stageBounds.getX(),pitTop+depth*.30f,
                           stageBounds.getWidth(),pitBottom-pitTop-depth*.30f);
                g.setColour(accent.withAlpha(.14f+.10f*activity));
                g.drawHorizontalLine(juce::roundToInt(pitTop),stageBounds.getX(),stageBounds.getRight());
            }
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
    skellpreset::stage::Backdrop backdrop;
    skellpreset::stage::Performer performer;
    SkellPresetAudioProcessor& processor;
    bool audioFresh=false;
    MeterEnvelope inputMeter,outputMeter;
    float activityLevel=0.f,crowdPhase=0.f;
    skellpreset::stage::StageClock stageClock;
    std::array<float,128> cachedSignal{};

    // Crowd figures are generated once and reused every frame. Keeping the
    // geometry data here means paint() does no hashing or allocation, and the
    // same figure keeps the same pose, depth and phone light across repaints.
    struct CrowdFigure {
        float x=0.f,height=26.f,headRatio=.115f,phase=0.f,rate=1.f,sizeJitter=1.f;
        float yJitter=0.f,tilt=0.f;
        int layer=0,pose=0;
        float phone=0.f;
        bool phoneSide=false;
    };
    static constexpr int crowdLayers=4;
    static constexpr int crowdCount=150;
    std::array<CrowdFigure,crowdCount> crowd{};
    bool crowdBuilt=false;
    void ensureCrowd() {
        if(crowdBuilt)return;
        crowdBuilt=true;
        int index=0;
        // Slots per row, farthest (top) to nearest (bottom). Distant rows hold
        // more people at a tighter spacing; near rows are wider and sparser,
        // which is what sells the perspective.
        constexpr int perRow[crowdLayers]{46,38,33,31};
        constexpr float spread[crowdLayers]{1.02f,1.08f,1.16f,1.28f};
        for(int layer=0;layer<crowdLayers && index<crowdCount;++layer) {
            const int people=perRow[layer];
            for(int i=0;i<people && index<crowdCount;++i,++index) {
                auto& f=crowd[(size_t)index];
                const int seed=layer*997+i*131;
                auto rnd=[seed](int offset){ return skellpreset::stage::hash01(seed+offset); };
                f.layer=layer;
                // Staggered spacing with deterministic jitter avoids a grid,
                // and the wider near rows bleed past the stage edges.
                const float even=(i+.5f+(rnd(0)-.5f)*.66f)/people;
                f.x=.5f+(even-.5f)*spread[layer];
                // Height is a modest jitter around the row size; the skull is
                // always a small fraction of the figure so nobody reads as a
                // giant balloon head.
                f.height=24.f+rnd(7)*12.f;
                f.headRatio=.098f+rnd(17)*.026f;
                f.sizeJitter=.94f+rnd(27)*.12f;
                f.phase=rnd(37)*6.2831f;
                f.rate=.86f+rnd(47)*.34f;
                f.pose=(int)(rnd(57)*4.f)%4;
                f.yJitter=rnd(91)-.5f;
                f.tilt=rnd(63)-.5f;
                // Phone lights thin out with distance so the pit glitters
                // toward the front rather than uniformly.
                const float chance=layer==3?.30f:layer==2?.21f:layer==1?.13f:.08f;
                if(rnd(67)<chance)f.phone=.5f+rnd(77)*.5f;
                f.phoneSide=rnd(87)>.5f;
            }
        }
    }
};
