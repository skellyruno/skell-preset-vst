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
        setInterceptsMouseClicks(true,true);
        addAndMakeVisible(micMatch);
        micMatch.setTitle("Match from the stage microphone");
        micMatch.setTooltip("Analyze your dry vocal and build a matching preset");
        micMatch.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        micMatch.setWantsKeyboardFocus(true);
        micMatch.onClick=[this]{if(onMicrophoneMatch)onMicrophoneMatch();};
    }
    std::function<void()> onMicrophoneMatch;
    void resized() override {updateMicMatchBounds();}
    bool testMicrophoneMatchTarget(){
        updateMicMatchBounds();
        const auto b=micMatch.getBounds();
        if(b.isEmpty()||!getLocalBounds().contains(b)||getComponentAt(b.getCentre())!=&micMatch||getComponentAt(2,2)==&micMatch)return false;
        if(micMatch.onClick)micMatch.onClick();
        return true;
    }
    // The waveform is a post-FX snapshot. Its last frame must not remain
    // marked LIVE after the host stops sending audio. Called only by the UI
    // timer; no locks, allocations or changes to audio processing.
    void setAudioFresh(bool fresh) {
        if(audioFresh==fresh)return;
        audioFresh=fresh;
        if(!fresh){inputMeter={};outputMeter={};activityLevel=0.f;vocalActivity=0.f;cachedSignal.fill(0.f);stageBands.fill(0.f);lastAnalyzedScope=0;}
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
        vocalActivity=followActivity(vocalActivity,juce::jlimit(0.f,1.f,inputMeter.level*3.f));

    }
    void tickAnimation(double nowMs,bool localCapture=false) {
        if(!localCapture && !isShowing()){stageClock.pause();return;}
        crowdPhase=static_cast<float>(stageClock.tick(nowMs,hasAudibleOutput()?activityLevel:0.f));
        updateSpectrum();
        updateMicMatchBounds();
        repaint();
    }
    void updateSpectrum() {
        const auto count=processor.liveScopeSampleCount();
        if(audioFresh && count!=lastAnalyzedScope && (++spectrumTick%3u)==1u){
            lastAnalyzedScope=count;
            std::array<float,skellpreset::visual::LiveSpectrumCapture::capacity> samples{};
            processor.getLiveSpectrumOutput(samples);
            const auto values=stageAnalyzer.analyze(samples,processor.preparedAudioRate());
            for(size_t i=0;i<values.size();++i){
                const float db=20.f*std::log10(std::max(.00025119f,values[i]));
                const float v=juce::jlimit(0.f,1.f,(db+72.f)/72.f);
                stageBands[i]+=(v-stageBands[i])*(v>stageBands[i]?.78f:.33f);
            }
        }
    }
    const auto& spectrumBands() const noexcept {return stageBands;}
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
        if(w<=160.f || h<=160.f)return out;
        const float margin=juce::jmax(17.f,juce::jmin(46.f,w*.075f));
        const float bottom=12.f;
        // Only reserve a lower safety margin; the audio-reactive stage owns the canvas.
        const float scopeHeight=1.f; // virtual lower safety margin, not a visible rail
        out.scope={margin,h-bottom-scopeHeight,
                   juce::jmax(1.f,w-2.f*margin),scopeHeight};
        // The stage above the trace is split into the performer's boards and a
        // deep crowd pit. Reserving the pit keeps the audience cinematic
        // instead of a thin strip pressed under the figure's feet.
        const float stageBottom=out.scope.getY()-6.f;
        const float headroom=h>=350.f?juce::jmin(16.f,h*.025f):0.f;
        const float stageTop=12.f+headroom;
        const float available=juce::jmax(0.f,stageBottom-stageTop);
        // Keep enough room for the smallest supported figure before the pit
        // takes its share, so tiny frames still produce a valid layout.
        const float pitMax=juce::jmax(0.f,available-372.f*.30f);
        const float pitHeight=juce::jmin(pitMax,juce::jlimit(28.f,162.f,stageBottom*(w>=580.f && h>=440.f?.16f:w>=580.f?.17f:.21f)));
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
    static juce::Rectangle<float> sceneryBounds(const StageLayout& stage,float width) noexcept {
        return {10.f,10.f,width-20.f,stage.scope.getY()-6.f};
    }
    static float stageHorizon(const StageLayout& stage) noexcept {
        const float feet=stage.originY+372.f*stage.scale;
        return juce::jmax(106.f,feet-juce::jlimit(70.f,190.f,feet*.34f));
    }
    static bool testStageGeometryForTest() noexcept {
        constexpr int sizes[8][2]{{320,240},{580,390},{906,349},{906,535},
                                  {906,561},{1140,680},{2000,1000},{160,160}};
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
            const auto scenery=sceneryBounds(stage,(float)dimensions[0]);
            const auto moon=skellpreset::stage::Backdrop::moonBounds(scenery,stageHorizon(stage));
            if(!scenery.contains(moon) || moon.getY()<scenery.getY()+8.f ||
               std::abs(moon.getWidth()-moon.getHeight())>.001f)return false;
        }
        return !layoutFor(0,0).valid;
    }
    juce::Rectangle<int> moonBounds() const noexcept {
        const auto stage=layoutFor(getWidth(),getHeight());
        if(!stage.valid)return {};
        return skellpreset::stage::Backdrop::moonBounds(
            sceneryBounds(stage,(float)getWidth()),stageHorizon(stage)).toNearestInt();
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
        auto bandEnergy=[&](int first,int last){float sum=0.f;for(int i=first;i<last;++i)sum+=stageBands[(size_t)i];return sum/(last-first);};
        const std::array<float,3> rigEnergy{{bandEnergy(0,12),bandEnergy(12,24),bandEnergy(24,36)}};
        const float crowdEnergy=juce::jlimit(0.f,1.f,.52f*activity+.30f*rigEnergy[1]+.18f*rigEnergy[2]);

        // Minimal midnight performance stage. All lighting is drawn in the
        // native editor; reactive intensity is measured from actual audio.
        const auto stageBounds=sceneryBounds(stage,width);
        const float centreX=width*.5f;
        // Two separate ground lines. `feetY` is where the performer stands;
        // `horizon` is the cemetery skyline, lifted well above the feet so the
        // graves, fence and chapel get a real band of depth instead of being
        // crushed into a strip behind the crowd pit.
        const float feetY=stage.originY+372.f*stage.scale;
        const float horizon=stageHorizon(stage);
        const auto* mode=processor.state.getRawParameterValue("mode");
        const bool bypassed=mode && (int)mode->load()==3;
        backdrop.draw(g,stageBounds,horizon);
        backdrop.atmosphere(g,stageBounds,horizon,crowdPhase,activity);
        // A layered eclipse behind the performer, distinct from the signal rail.
        // Its radiance follows real output bands; no simulated frequency data.
        if(stageBounds.getHeight()>220.f){
            juce::Graphics::ScopedSaveState eclipse(g);g.reduceClipRegion(stageBounds.toNearestInt());
            const float cx=centreX,cy=stage.originY+120.f*stage.scale,r=110.f*stage.scale;
            g.setGradientFill(juce::ColourGradient(accent.withAlpha(.055f+.09f*rigEnergy[1]),cx,cy,
                Theme::bg.withAlpha(0.f),cx+r*1.8f,cy,true));
            g.fillEllipse(cx-r*1.8f,cy-r*1.8f,r*3.6f,r*3.6f);
            for(int ring=0;ring<2;++ring){
                const float radius=r*(.80f+.15f*ring);
                juce::Path arc;arc.addCentredArc(cx,cy,radius,radius,0.f,.22f+ring*.3f,5.9f-ring*.32f,true);
                g.setColour((ring==1?Theme::blue:accent).withAlpha(.055f+.12f*rigEnergy[(size_t)ring]));
                g.strokePath(arc,juce::PathStrokeType(juce::jmax(.7f,1.3f*stage.scale)));
            }
        }
        // Restrained moving key light leaves the static environment cached.
        g.setGradientFill(juce::ColourGradient(Theme::white.withAlpha(.28f+.18f*activity),
            centreX-65.f,feetY-240.f*stage.scale,Theme::bg.withAlpha(0.f),centreX+150.f,feetY-70.f,true));
        g.fillEllipse(centreX-175.f*stage.scale,feetY-365.f*stage.scale,350.f*stage.scale,365.f*stage.scale);
        // Concert architecture: low-profile truss, line arrays and a segmented
        // rear light wall. Keep the moon and chapel visible outside the rig.
        // All measured intensity comes from the existing GUI spectrum snapshot.
        if(stageBounds.getWidth()>=480.f && stageBounds.getHeight()>235.f) {
            juce::Graphics::ScopedSaveState venue(g);g.reduceClipRegion(stageBounds.toNearestInt());
            const float u=stage.scale;
            const float half=juce::jmin(stageBounds.getWidth()*.32f,340.f*u);
            const float top=stageBounds.getY()+12.f*u;
            const float lower=top+13.f*u;
            const auto metal=juce::Colour(0xff334044);
            g.setColour(juce::Colour(0xff080d10));
            g.fillRoundedRectangle(centreX-half,top-3.f*u,2.f*half,20.f*u,2.f*u);
            g.setColour(metal.withAlpha(.85f));
            g.drawLine(centreX-half,top,centreX+half,top,1.9f*u);
            g.drawLine(centreX-half,lower,centreX+half,lower,1.9f*u);
            for(int cell=0;cell<16;++cell) {
                const float x0=centreX-half+2.f*half*cell/16.f,x1=centreX-half+2.f*half*(cell+1)/16.f;
                g.drawLine(x0,cell%2?top:lower,x1,cell%2?lower:top,1.0f*u);
            }
            // Rear light wall uses separate vertical panels, not a solid UI card.
            for(int side:{-1,1}) {
                const auto tint=side<0?Theme::blue:accent;
                for(int column=0;column<5;++column) {
                    const float x=centreX+side*(91.f+column*24.f)*u;
                    const float y=horizon-(103.f-column*8.f)*u;
                    const float h=juce::jmax(8.f,horizon-y+8.f*u);
                    const float v=live?stageBands[(size_t)(side<0?column*3:35-column*3)]:0.f;
                    const juce::Rectangle<float> panel(x-8.f*u,y,16.f*u,h);
                    g.setColour(juce::Colour(0xff0b1316).withAlpha(.86f));g.fillRoundedRectangle(panel,2.f*u);
                    g.setColour(tint.withAlpha(.12f+.27f*v));g.drawRoundedRectangle(panel,2.f*u,.9f*u);
                    for(int k=0;k<9;++k) {
                        const float f=(float)(9-k)/9.f;
                        const float brightness=.09f+(v>=f?.56f*v:0.f);
                        g.setColour(tint.withAlpha(brightness));
                        g.fillRoundedRectangle(x-5.f*u,y+h*(k+.28f)/9.f,10.f*u,juce::jmax(.8f,h*.053f),.65f*u);
                    }
                }
                // Suspended line arrays with a slight curvature and grille detail.
                const float arrayX=centreX+side*(half-17.f*u);
                for(int cab=0;cab<5;++cab) {
                    const float x=arrayX+side*(float)(cab*cab)*.32f*u;
                    const juce::Rectangle<float> r(x-15.f*u,lower+5.f*u+cab*17.f*u,30.f*u,16.f*u);
                    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff293236),r.getX(),r.getY(),juce::Colour(0xff090e10),r.getRight(),r.getBottom(),false));
                    g.fillRoundedRectangle(r,2.2f*u);
                    g.setColour(metal);g.drawRoundedRectangle(r,2.2f*u,.7f*u);
                    g.setColour(juce::Colour(0xff080d10));g.fillRoundedRectangle(r.reduced(3.f*u),1.4f*u);
                    for(int line=0;line<4;++line){g.setColour(metal.withAlpha(.65f));g.drawLine(r.getX()+5.f*u,r.getY()+(4.f+line*2.3f)*u,r.getRight()-5.f*u,r.getY()+(4.f+line*2.3f)*u,.5f*u);}
                    g.setColour(tint.withAlpha(.24f+.52f*rigEnergy[0]));g.fillEllipse(r.getRight()-4.f*u,r.getY()+2.f*u,1.8f*u,1.8f*u);
                }
            }
        }
        // Volumetric stage beams from the rig above the performer. These are
        // soft screen-blended wedges, not textured sprites, and breathe with
        // measured output so the rig feels alive without ever lying about audio.
        {
            juce::Graphics::ScopedSaveState beams(g);
            g.reduceClipRegion(stageBounds.toNearestInt());
            const float rigY=stageBounds.getY()+(stageBounds.getWidth()>=480.f && stageBounds.getHeight()>235.f?31.f*stage.scale:7.f);
            const float spread=.55f+.10f*activity;
            for(int beam=0;beam<3;++beam) {
                const float rigX=centreX+(beam-1)*92.f*stage.scale;
                const float targetX=centreX+(beam-1)*205.f*spread+std::sin(crowdPhase*.39f+beam*1.8f)*32.f*stage.scale*activity;
                const float targetY=feetY+6.f;
                juce::Path wedge;
                wedge.startNewSubPath(rigX-3.f,rigY);
                wedge.lineTo(rigX+3.f,rigY);
                wedge.lineTo(targetX+70.f*stage.scale,targetY);
                wedge.lineTo(targetX-70.f*stage.scale,targetY);
                wedge.closeSubPath();
                const auto tint=(beam==1?accent:Theme::blue);
                g.setGradientFill(juce::ColourGradient(
                    tint.withAlpha(.25f+.32f*rigEnergy[(size_t)beam]),rigX,rigY,
                    tint.withAlpha(0.f),targetX,targetY,false));
                g.fillPath(wedge);
                juce::Path core;
                core.startNewSubPath(rigX-1.4f,rigY);core.lineTo(rigX+1.4f,rigY);
                core.lineTo(targetX+24.f*stage.scale,targetY);core.lineTo(targetX-24.f*stage.scale,targetY);core.closeSubPath();
                g.setGradientFill(juce::ColourGradient(tint.withAlpha(.045f+.055f*rigEnergy[(size_t)beam]),rigX,rigY,
                    tint.withAlpha(0.f),targetX,targetY,false));g.fillPath(core);
            }
            // Rig housing and three lens lamps.
            g.setColour(juce::Colour(0xff121719));
            g.fillRoundedRectangle(centreX-104.f*stage.scale,rigY-5.f*stage.scale,
                                   208.f*stage.scale,7.f*stage.scale,3.f*stage.scale);
            for(int lamp=0;lamp<3;++lamp) {
                const float lx=centreX+(lamp-1)*92.f*stage.scale;
                const int lit=juce::roundToInt(65+190*rigEnergy[(size_t)lamp]);
                g.setColour(Theme::green.withAlpha(.10f));
                g.fillEllipse(lx-9.f*stage.scale,rigY-3.f*stage.scale,
                              18.f*stage.scale,12.f*stage.scale);
                g.setColour((lamp==1?accent:Theme::blue).withAlpha(
                    juce::jlimit(.18f,.98f,(float)lit/255.f)));
                g.fillEllipse(lx-3.2f*stage.scale,rigY-1.f*stage.scale,
                              6.4f*stage.scale,4.2f*stage.scale);
            }
        }
        // A broad raised deck with a faceted apron grounds the headliner.
        {
            juce::Graphics::ScopedSaveState boards(g);g.reduceClipRegion(stageBounds.toNearestInt());
            const float u=stage.scale,half=juce::jmin(stageBounds.getWidth()*.39f,350.f*u);
            const float back=feetY-39.f*u,front=feetY+5.f*u;
            juce::Path deck;deck.startNewSubPath(centreX-half*.74f,back);deck.lineTo(centreX+half*.74f,back);
            deck.lineTo(centreX+half,front);deck.lineTo(centreX-half,front);deck.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff283339),centreX,back,juce::Colour(0xff0b1114),centreX,front,false));g.fillPath(deck);
            g.setColour(juce::Colour(0xff526361).withAlpha(.6f));g.strokePath(deck,juce::PathStrokeType(.8f*u));
            for(int k=-4;k<=4;++k){g.setColour(Theme::white.withAlpha(.055f));g.drawLine(centreX+k*half*.16f,back,centreX+k*half*.22f,front,.8f*u);}
            for(int i=0;i<18;++i){
                const float x=centreX-half+(i+.5f)*2.f*half/18.f;
                const float v=live?stageBands[(size_t)(i*2)]:0.f;
                g.setColour(accent.withAlpha(.18f+.72f*v));g.fillRoundedRectangle(x-4.f*u,front-2.f*u,8.f*u,2.f*u,1.f*u);
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
        // Side-wing floor lights provide separation from the graveyard and
        // make the venue read as a real stage. Drive only from existing bands.
        if(stageBounds.getHeight()>235.f){
            juce::Graphics::ScopedSaveState wings(g);g.reduceClipRegion(stageBounds.toNearestInt());
            for(int side:{-1,1}){
                const float x=centreX+side*stageBounds.getWidth()*.31f,y=feetY-8.f*stage.scale;
                const float r=15.f*stage.scale;const auto tint=side<0?Theme::blue:accent;
                const float power=.09f+.20f*rigEnergy[(size_t)(side<0?0:2)];
                g.setGradientFill(juce::ColourGradient(tint.withAlpha(power),x,y,tint.withAlpha(0.f),x,y-104.f*stage.scale,true));
                juce::Path light;light.startNewSubPath(x-r,y);light.lineTo(x+r,y);light.lineTo(x+49.f*stage.scale,y-118.f*stage.scale);light.lineTo(x-49.f*stage.scale,y-118.f*stage.scale);light.closeSubPath();g.fillPath(light);
                g.setColour(juce::Colour(0xff182124));g.fillRoundedRectangle(x-r,y-4.f*stage.scale,r*2.f,7.f*stage.scale,2.f*stage.scale);
                g.setColour(tint.withAlpha(.52f+power));g.drawLine(x-r*.6f,y-2.f*stage.scale,x+r*.6f,y-2.f*stage.scale,1.2f);
            }
        }
        // Soft spotlight pool on the boards beneath the performer. It is the
        // brightest floor element and gives the figure real stage presence.
        {
            const float poolW=195.f*stage.scale,poolH=(.28f+.10f*activity)*poolW;
            for(int ring=3;ring>=1;--ring) {
                const float f=(float)ring/3.f;
                g.setGradientFill(juce::ColourGradient(
                    Theme::blue.interpolatedWith(accent,.3f).withAlpha(.075f*activity+.048f),centreX,feetY,
                    Theme::bg.withAlpha(0.f),centreX+poolW*f*.55f,feetY,true));
                g.fillEllipse(centreX-poolW*f,feetY-poolH*f*.42f,
                              poolW*2.f*f,poolH*2.f*f);
            }
            // Mirrored figure glint, kept subtle and tied to measured peak.
            g.setGradientFill(juce::ColourGradient(
                Theme::white.withAlpha(.09f+.075f*activity),centreX,feetY,
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
                           activity,crowdPhase,vocalActivity);
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
            const float pitTop=feetY+juce::jmax(4.f,8.f*stage.scale);
            const float pitBottom=stageBounds.getBottom();
            const float depth=juce::jmax(0.f,pitBottom-pitTop);
            if(depth>10.f){
                // The stage floor is what the crowd is silhouetted against:
                // a lit, slightly green-grey ramp that keeps the near rows
                // readable without any figure being bright itself.
                g.setGradientFill(juce::ColourGradient(
                    juce::Colour(0xff222c29).withAlpha(.82f),centreX,pitTop,
                    juce::Colour(0xff070b0e),centreX,pitBottom,false));
                g.fillRect(stageBounds.getX(),pitTop,stageBounds.getWidth(),depth);
                // Warm key spill from the performer's pool plus a cold balcony
                // glow at the edges, both audio-tinted.
                g.setGradientFill(juce::ColourGradient(
                    Theme::green.withAlpha(.07f+.06f*activity),centreX,pitTop,
                    Theme::bg.withAlpha(0.f),centreX,pitTop+depth*.55f,true));
                g.fillRect(stageBounds.getX(),pitTop,stageBounds.getWidth(),depth*.6f);
                ensureCrowd();
                if(crowdCacheTheme!=Theme::current || std::abs(crowdCacheDepth-depth)>.05f){
                    for(auto& sprite:crowdSprites)sprite=juce::Image{};
                    crowdCacheTheme=Theme::current;crowdCacheDepth=depth;
                }
                const float idleSway=.16f+.84f*activity;
                for(int row=0;row<crowdLayers;++row) {
                    const float t=crowdLayers>1?(float)row/(float)(crowdLayers-1):0.f;
                    // Far rows are small and high, near rows large and cropped
                    // by the bottom of the stage. The pit starts narrow so the
                    // cemetery keeps its own band of picture above the crowd.
                    const float heightPix=depth*(.48f+1.05f*t*t);
                    if(heightPix<6.f)continue;
                    // Near figures are only slightly lighter than far ones, so
                    // depth comes from size and overlap rather than brightness.
                    const auto bodyColour=juce::Colour(0xff04070a).withAlpha(.78f+.16f*t);
                    const auto rowRim=porcelain.interpolatedWith(Theme::blue,.22f).withAlpha(.20f+.48f*t);
                    const auto edgeGlow=accent.withAlpha(.025f+.07f*activity);
                    for(int i=0;i<crowdCount;++i) {
                        const auto& figure=crowd[(size_t)i];
                        if(figure.layer!=row)continue;
                        // Individual timing, irregular emphasis and smaller idle gestures.
                        const float motion=crowdPhase*figure.rate+figure.phase;
                        const float wave=(.50f*std::sin(motion*.62f)+.32f*std::sin(motion*1.17f+figure.phase)
                            +.18f*std::sin(motion*1.91f))*idleSway;
                        const float bob=std::sin(motion*.93f+figure.phase*1.7f)*idleSway;
                        const float hgt=heightPix*figure.sizeJitter;
                        const float unit=hgt/figure.height;
                        const float x=stageBounds.getX()+stageBounds.getWidth()*figure.x
                            +wave*(3.2f+2.4f*crowdEnergy)*unit;
                        // Vertical jitter inside the row prevents ruled lines.
                        const float headY=pitTop+depth*(.09f+.56f*t)+figure.yJitter*depth*.19f
                            -std::abs(bob)*(2.2f+2.4f*crowdEnergy)*unit;
                        // Feet stay at the ground depth while the knees absorb the body bob.
                        const float base=pitTop+depth*(.09f+.56f*t)+figure.yJitter*depth*.19f+hgt;
                        const float ry=hgt*figure.headRatio;
                        const float rx=ry*(.67f+.19f*figure.turn);
                        const auto rimColour=rowRim.withMultipliedAlpha(.72f+.38f*figure.turn);
                        const float armW=juce::jmax(1.1f,hgt*.045f);
                        const float shoulderY=headY+ry*1.35f;
                        const float shoulderW=rx*(1.75f+.35f*figure.turn);
                        if(x+ry*6.f<stageBounds.getX() || x-ry*6.f>stageBounds.getRight() || headY-ry*5.f>pitBottom)continue;
                        // Distant silhouettes do not need sub-pixel anatomy,
                        // multiple stroked paths or a rotated graphics context.
                        // Near rows retain the full articulated skeleton art.
                        if(ry<2.6f) {
                            auto& sprite=crowdSprites[(size_t)i];
                            const int sw=juce::jmax(8,(int)std::ceil(ry*10.f+8.f));
                            const int sh=juce::jmax(8,(int)std::ceil(hgt+ry*5.f+8.f));
                            if(!sprite.isValid()){
                                sprite=juce::Image(juce::Image::ARGB,sw,sh,true);
                                juce::Graphics art(sprite);
                                const float localX=sw*.5f,localHeadY=ry*3.f+3.f,localBase=localHeadY+hgt;
                                const float localShoulderY=localHeadY+ry*1.35f;
                                
                            art.setColour(bodyColour);
                            art.fillEllipse(localX-rx,localHeadY-ry,2.f*rx,2.f*ry);
                            // Even distant figures have tapered shoulders,
                            // a cervical stem and separate angled arms.
                            juce::Path distant;
                            distant.startNewSubPath(localX-rx*.45f,localHeadY+ry*.6f);
                            distant.quadraticTo(localX-shoulderW,localShoulderY,localX-rx*.30f,localShoulderY+ry*2.8f);
                            distant.lineTo(localX+rx*.30f,localShoulderY+ry*2.8f);
                            distant.quadraticTo(localX+shoulderW,localShoulderY,localX+rx*.45f,localHeadY+ry*.6f);
                            distant.closeSubPath();art.fillPath(distant);
                            art.setColour(rimColour.withMultipliedAlpha(.82f));
                            art.drawLine(localX,localHeadY+ry,localX,localShoulderY+ry*3.7f,.65f);
                            for(int rib=0;rib<3;++rib){const float y=localShoulderY+ry*(.6f+rib*.6f);art.drawLine(localX-rx*(1.25f-rib*.22f),y,localX+rx*(1.25f-rib*.22f),y,.55f);}
                            art.setColour(bodyColour);art.fillEllipse(localX-rx*.8f,localShoulderY+ry*3.f,rx*1.6f,ry*.7f);
                            art.drawLine(localX-rx*.4f,localShoulderY+ry*3.5f,localX-rx*.7f,localBase,1.f);
                            art.drawLine(localX+rx*.4f,localShoulderY+ry*3.5f,localX+rx*.7f,localBase,1.f);
                            for(int side:{-1,1}){
                                const float sign=(float)side;
                                const bool up=figure.pose==0 || (figure.pose==1 && ((side>0)==figure.phoneSide));
                                const float elbowX=localX+sign*shoulderW*1.2f,elbowY=localShoulderY+ry*.4f;
                                art.drawLine(localX+sign*rx*.65f,localShoulderY,elbowX,elbowY,.8f);
                                art.drawLine(elbowX,elbowY,elbowX+sign*rx*.6f,elbowY+(up?-ry*2.6f:ry*1.7f),.7f);
                            }
                            }
                            g.setOpacity(1.f);g.drawImageAt(sprite,juce::roundToInt(x-sw*.5f),juce::roundToInt(headY-ry*3.f-3.f));
                            continue;
                        }
                        // Raised arms behind the skull. Any visible phone must
                        // be physically attached to the corresponding hand.
                        float phoneX=0.f,phoneY=0.f;bool phoneHeld=false;
                        for(int side:{-1,1}) {
                            const float sign=(float)side;
                            const float sx=x+sign*rx*.80f;
                            const float wristMotion=std::sin(motion*(1.08f+.11f*sign)+sign*1.31f)*idleSway;
                            const float elbowMotion=std::sin(motion*.72f+sign*1.6f)*idleSway;
                            float ex,ey,hx,hy;
                            if(figure.pose==0) {
                                ex=x+sign*rx*1.8f;ey=headY+ry*.35f;
                                hx=x+sign*(rx*2.1f+wave*rx*.5f);hy=headY-hgt*.24f;
                            } else if(figure.pose==1) {
                                const bool up=(side>0)==figure.phoneSide;
                                ex=x+sign*rx*(up?1.5f:2.0f);ey=headY+ry*(up?.20f:1.20f);
                                hx=ex+sign*rx*.35f;hy=ey+(up?-hgt*.26f:hgt*.09f);
                            } else if(figure.pose==2) {
                                ex=x+sign*rx*1.5f;ey=shoulderY+ry*.55f;
                                hx=x+sign*rx*(.55f+std::abs(wave)*.55f);hy=ey-ry*.32f;
                            } else if(figure.pose==3) {
                                ex=x+sign*rx*1.6f;ey=headY+ry*.45f;
                                hx=ex+sign*rx*.20f;hy=ey-hgt*.18f;
                            } else {
                                ex=x+sign*rx*1.35f;ey=shoulderY+ry*1.8f;
                                hx=ex-sign*rx*.20f;hy=ey+ry*1.75f;
                            }
                            ex+=sign*rx*.24f*elbowMotion;
                            ey+=ry*.32f*elbowMotion;
                            hx+=rx*.44f*wristMotion;hy+=ry*.52f*wristMotion;
                            // Maintain stable upper/forearm lengths rather than stretching the bones.
                            const float upper=ry*2.05f,lower=ry*2.15f;
                            auto extend=[](float ax,float ay,float& bx,float& by,float len){
                                const float dx=bx-ax,dy=by-ay,d=std::sqrt(dx*dx+dy*dy);
                                if(d>.001f){bx=ax+dx*len/d;by=ay+dy*len/d;}
                            };
                            extend(sx,shoulderY,ex,ey,upper);extend(ex,ey,hx,hy,lower);
                            g.setColour(bodyColour);
                            g.drawLine(sx,shoulderY,ex,ey,armW+2.2f);
                            g.drawLine(ex,ey,hx,hy,armW+2.2f);
                            g.setColour(rimColour.withMultipliedAlpha(.62f));
                            g.drawLine(sx,shoulderY,ex,ey,armW*.54f);
                            g.drawLine(ex,ey,hx,hy,armW*.58f);
                            g.setColour(rimColour.withMultipliedAlpha(.48f));
                            g.drawLine(ex+sign*armW*.38f,ey,hx+sign*armW*.25f,hy,juce::jmax(.45f,armW*.19f));
                            g.fillEllipse(ex-armW*.42f,ey-armW*.42f,armW*.84f,armW*.84f);
                            if(figure.phone>0.f && ((side>0)==figure.phoneSide) && hy<headY+ry){phoneX=hx;phoneY=hy;phoneHeld=true;}
                            g.fillEllipse(hx-armW*.6f,hy-armW*.7f,armW*1.2f,armW*1.4f);
                            // Fingers follow the forearm direction rather than all pointing skyward.
                            const float dx=(hx-ex)/lower,dy=(hy-ey)/lower;
                            if(ry>4.f)for(int finger=0;finger<4;++finger){
                                const float f=(finger-1.5f)*armW*.36f;
                                g.drawLine(hx-dy*f,hy+dx*f,hx+dx*armW*1.45f-dy*f*1.5f,
                                    hy+dy*armW*1.45f+dx*f*1.5f,juce::jmax(.45f,armW*.19f));
                            }
                        }
                        const float pelvisY=shoulderY+ry*3.7f;
                        for(int side:{-1,1}){
                            const float sign=(float)side,hip=x+sign*rx*.45f;
                            const float kneeX=hip+sign*rx*(.33f+.14f*wave),kneeY=pelvisY+(base-pelvisY)*.55f;
                            const float footX=x+sign*rx*(.85f+.21f*figure.turn);
                            g.setColour(bodyColour);g.drawLine(hip,pelvisY+ry*.5f,kneeX,kneeY,juce::jmax(1.f,rx*.42f));g.drawLine(kneeX,kneeY,footX,base,juce::jmax(1.f,rx*.30f));
                            g.setColour(rimColour.withMultipliedAlpha(.38f));g.drawLine(hip,pelvisY+ry*.5f,kneeX,kneeY,juce::jmax(.5f,rx*.15f));g.drawLine(kneeX,kneeY,footX,base,juce::jmax(.5f,rx*.12f));
                        }
                        // The fixed rear anatomy is individually rasterized at logical resolution.
                        // Limbs, phone positions, head/torso movement and stage lights stay live.
                        auto& sprite=crowdSprites[(size_t)i];
                        const int sw=juce::jmax(12,(int)std::ceil(ry*6.f+8.f));
                        const int sh=juce::jmax(12,(int)std::ceil(ry*8.f+8.f));
                        if(!sprite.isValid()){
                            sprite=juce::Image(juce::Image::ARGB,sw,sh,true);
                            juce::Graphics art(sprite);
                            const float localX=sw*.5f,localHeadY=ry*1.5f+3.f,localShoulderY=localHeadY+ry*1.35f;
                        // Back, shoulders and neck as one silhouette; only the
                        // shoulder line catches light, which is what sells a
                        // crowd seen from behind.
                        juce::Path body;
                        body.startNewSubPath(localX-rx*.54f,localShoulderY+ry*3.6f);
                        body.lineTo(localX-shoulderW*.96f,localShoulderY+ry*.30f);
                        body.quadraticTo(localX-shoulderW*.58f,localShoulderY-ry*.42f,localX-rx*.62f,localShoulderY);
                        body.lineTo(localX-rx*.66f,localHeadY+ry*.60f);
                        body.lineTo(localX+rx*.66f,localHeadY+ry*.60f);
                        body.lineTo(localX+rx*.62f,localShoulderY);
                        body.quadraticTo(localX+shoulderW*.58f,localShoulderY-ry*.42f,localX+shoulderW*.96f,localShoulderY+ry*.30f);
                        body.lineTo(localX+rx*.54f,localShoulderY+ry*3.6f);
                        body.closeSubPath();
                        art.setGradientFill(juce::ColourGradient(bodyColour.interpolatedWith(rimColour,.10f),localX-rx*1.6f,localHeadY+ry,bodyColour,localX+rx*1.8f,localHeadY+ry*4.f,false));
                        art.fillPath(body);
                        const float localPelvisY=localShoulderY+ry*3.7f;
                        art.fillEllipse(localX-rx*.85f,localPelvisY,rx*1.7f,ry*.85f);
                        // Near bodies show the rear skeleton rather than
                        // featureless black coats. No detail on subpixel heads.
                        if(ry>2.6f) {
                            const float spineY=localShoulderY+ry*.25f;
                            for(int rib=0;rib<4;++rib){
                                const float y=spineY+ry*(.55f+rib*.49f),w=shoulderW*(.86f-.10f*rib);
                                juce::Path bones;
                                // Two rear rib arcs with a gap at the spine and varied depth.
                                for(int side:{-1,1}){const float d=(float)side;
                                    bones.startNewSubPath(localX+d*rx*.18f,y);bones.cubicTo(localX+d*w*.5f,y-ry*.20f,localX+d*w,y+ry*.18f,localX+d*w*.89f,y+ry*.53f);}
                                
                                art.setColour(rimColour.withMultipliedAlpha(.55f));art.strokePath(bones,juce::PathStrokeType(juce::jmax(.55f,ry*.10f)));
                            }
                            for(int v=0;v<4;++v) {
                                const float vy=spineY+v*ry*.48f;
                                art.setColour(rimColour.withMultipliedAlpha(.62f-.08f*v));
                                art.fillRoundedRectangle(localX-rx*.20f,vy,rx*.40f,ry*.29f,ry*.10f);
                            }
                            for(int side:{-1,1}) {
                                const float sign=(float)side;
                                juce::Path blade;
                                blade.startNewSubPath(localX+sign*rx*.48f,spineY+ry*.12f);
                                blade.quadraticTo(localX+sign*shoulderW*.90f,spineY+ry*.15f,
                                                  localX+sign*rx*.87f,spineY+ry*1.36f);
                                blade.quadraticTo(localX+sign*rx*.40f,spineY+ry*.96f,
                                                  localX+sign*rx*.48f,spineY+ry*.12f);
                                blade.closeSubPath();
                                art.setColour(rimColour.withMultipliedAlpha(.32f));
                                art.fillPath(blade);
                            }
                        }
                        // Rim: shoulder crest then the skull dome.
                        art.setColour(rimColour);
                        juce::Path shoulders;
                        shoulders.startNewSubPath(localX-shoulderW*.94f,localShoulderY+ry*.34f);
                        shoulders.quadraticTo(localX-shoulderW*.56f,localShoulderY-ry*.44f,localX-rx*.60f,localShoulderY);
                        art.strokePath(shoulders,juce::PathStrokeType(juce::jmax(.7f,hgt*.014f)));
                        juce::Path shouldersR;
                        shouldersR.startNewSubPath(localX+rx*.60f,localShoulderY);
                        shouldersR.quadraticTo(localX+shoulderW*.56f,localShoulderY-ry*.44f,localX+shoulderW*.94f,localShoulderY+ry*.34f);
                        art.strokePath(shouldersR,juce::PathStrokeType(juce::jmax(.7f,hgt*.014f)));
                        // Near skulls use shaped craniums; distant ones remain cheap silhouettes.
                        juce::Path backSkull;
                        backSkull.startNewSubPath(localX,localHeadY-ry);
                        const float asym=(figure.turn-.5f)*rx*.30f;
                        backSkull.cubicTo(localX-rx*1.13f+asym,localHeadY-ry*1.08f,localX-rx*1.16f,localHeadY+ry*.12f,localX-rx*.70f,localHeadY+ry*.61f);
                        backSkull.quadraticTo(localX+asym,localHeadY+ry*.98f,localX+rx*.62f,localHeadY+ry*.66f);
                        backSkull.cubicTo(localX+rx*1.18f,localHeadY+ry*.15f,localX+rx*1.12f+asym,localHeadY-ry*.94f,localX,localHeadY-ry);
                        backSkull.closeSubPath();
                        art.setGradientFill(juce::ColourGradient(rimColour.withMultipliedAlpha(.62f),localX-rx,localHeadY-ry,bodyColour,localX+rx,localHeadY+ry,false));
                        art.fillPath(backSkull);
                        // Asymmetric occipital planes and mastoid ridge give each head volume.
                        {juce::Graphics::ScopedSaveState shade(art);art.reduceClipRegion(backSkull);
                         art.setGradientFill(juce::ColourGradient(rimColour.withMultipliedAlpha(.32f),localX-rx*.45f,localHeadY-ry*.65f,bodyColour.withAlpha(0.f),localX+rx*.7f,localHeadY+ry*.55f,true));
                         art.fillEllipse(localX-rx,localHeadY-ry,2.f*rx,2.f*ry);}

                        if(ry>4.f){
                            juce::Graphics::ScopedSaveState skullClip(art);art.reduceClipRegion(backSkull);
                            const float sutureX=localX+(figure.turn-.5f)*rx*.60f;
                            juce::Path seam;seam.startNewSubPath(sutureX,localHeadY-ry);seam.lineTo(sutureX-rx*.12f,localHeadY-ry*.56f);seam.lineTo(sutureX+rx*.08f,localHeadY-ry*.28f);seam.lineTo(sutureX,localHeadY+ry*.08f);
                            art.setColour(bodyColour.withMultipliedAlpha(.58f));art.strokePath(seam,juce::PathStrokeType(.65f));
                            art.setColour(rimColour.withMultipliedAlpha(.21f));art.fillEllipse(localX-rx*.72f,localHeadY-ry*.65f,rx*.68f,ry*.48f);
                        }
                        juce::Path crescent;
                        crescent.addCentredArc(localX,localHeadY,rx,ry,0.f,
                            juce::MathConstants<float>::pi*1.06f,
                            juce::MathConstants<float>::pi*1.94f,true);
                        art.setColour(rimColour);
                        art.strokePath(crescent,juce::PathStrokeType(juce::jmax(.7f,hgt*.016f)));
                        // Rear-facing skull: occipital shadow and cervical joint, not a face.
                        if(ry>2.6f) {
                            art.setColour(juce::Colour(0xff0b1012));
                            art.fillEllipse(localX-rx*.45f,localHeadY+ry*.38f,rx*.90f,ry*.37f);
                            art.setColour(rimColour.withMultipliedAlpha(.58f));
                            art.drawLine(localX,localHeadY+ry*.64f,localX,localShoulderY+ry*.35f,juce::jmax(.65f,rx*.21f));
                            if(ry>4.5f) {
                                art.setColour(rimColour.withMultipliedAlpha(.55f));
                                art.drawLine(localX-rx*.92f,localHeadY-ry*.10f,localX-rx*.92f,localHeadY+ry*.55f,
                                           juce::jmax(.6f,hgt*.016f));
                            }
                        }
                        }
                        g.setOpacity(1.f);g.drawImageAt(sprite,juce::roundToInt(x-sw*.5f),juce::roundToInt(headY-ry*1.5f-3.f));
                        // Concert phone lights: the brightest thing in the pit.
                        if(phoneHeld) {
                            const float glow=juce::jlimit(.18f,1.f,.28f+.72f*activity);
                            const float pw=juce::jmax(1.8f,rx*.48f),ph=pw*1.75f;
                            const auto phone=juce::Rectangle<float>(phoneX-pw*.5f,phoneY-ph*.85f,pw,ph);
                            g.setGradientFill(juce::ColourGradient(Theme::blue.withAlpha(.18f*glow),phone.getCentre(),Theme::blue.withAlpha(0.f),phone.getCentre().translated(ph*1.4f,0.f),true));
                            g.fillEllipse(phone.expanded(ph));
                            g.setColour(bodyColour);g.fillRoundedRectangle(phone,.5f);
                            g.setColour(Theme::blue.withAlpha(.55f+.30f*glow));g.fillRoundedRectangle(phone.reduced(.3f),.35f);
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
                // A diffuse stage edge replaces the artificial hard green seam.
                g.setGradientFill(juce::ColourGradient(accent.withAlpha(.08f+.08f*activity),centreX,pitTop,
                    accent.withAlpha(0.f),centreX,pitTop+12.f,false));
                g.fillRect(stageBounds.getX(),pitTop,stageBounds.getWidth(),12.f);
            }
        }
        // The spectrum is part of the cemetery set: two banks of illuminated
        // fence spires, with ground reflections instead of floating meter cards.
        if(width>=580.f && stageBounds.getHeight()>270.f){
            juce::Graphics::ScopedSaveState scene(g);g.reduceClipRegion(stageBounds.toNearestInt());
            const float bankW=juce::jlimit(108.f,205.f,width*.235f);
            const float base=horizon+juce::jmin(20.f,stage.scale*22.f);
            const float maxH=juce::jlimit(62.f,128.f,(base-stageBounds.getY())*.66f);
            for(int side=0;side<2;++side){
                const float start=side==0?stageBounds.getX()+22.f:stageBounds.getRight()-bankW-22.f;
                const auto tint=side==0?accent:Theme::blue;
                const float spacing=bankW/18.f;
                for(int band=0;band<18;++band){
                    const int index=side==0?band:17-band;
                    const float depth=(float)index/17.f;
                    const float x=start+spacing*(band+.5f);
                    const float floor=base+12.f*(1.f-depth);
                    const float height=maxH*(.70f+.30f*(1.f-depth));
                    const float value=juce::jlimit(0.f,1.f,stageBands[(size_t)(side*18+band)]);
                    const float w=juce::jmax(1.2f,spacing*.38f);
                    // Short stone-like footlights when idle, spectrum rises only
                    // with measured audio. No duplicate fence of dead bars.
                    g.setColour(Theme::bg.withAlpha(.62f));g.fillRoundedRectangle(x-w,floor-6.f,2.f*w,6.f,1.5f);
                    g.setColour(tint.withAlpha(.12f));g.drawLine(x-w*.65f,floor-5.f,x+w*.65f,floor-5.f,1.f);
                    if(value>.002f){
                        const float top=floor-height*value;
                        g.setGradientFill(juce::ColourGradient(tint.withAlpha(.10f),x,top,tint.withAlpha(.02f),x,floor,false));
                        g.fillRect(x-w*2.f,top,w*4.f,floor-top);
                        g.setGradientFill(juce::ColourGradient(tint.interpolatedWith(Theme::white,.30f).withAlpha(.90f),x,top,
                            tint.withAlpha(.12f),x,floor,false));
                        g.fillRoundedRectangle(x-w*.5f,top,w,floor-top,1.f);
                        g.setColour(tint.withAlpha(.13f*value));
                        g.drawLine(x,floor+2.f,x+(x-centreX)*.055f,floor+height*value*.20f,w);
                    }
                    if(value>.002f){
                        const float top=floor-height*value;
                        g.setColour(tint.withAlpha(.38f*value));
                        g.fillEllipse(x-w*.65f,top-w*.65f,1.3f*w,1.3f*w);
                    }
                }
                g.setColour(tint.withAlpha(.09f));g.drawLine(start,base+12.f,start+bankW,base,1.f);
            }
        }
        // The trace and bone illumination are directly audio-reactive;
        // nothing is a fake spectrum or prerecorded animation.
        // The stage is the display. Live waveform light belongs to the boards.
        if(live){
            const juce::Rectangle<float> plot(stageBounds.getX()+28.f,feetY-8.f,stageBounds.getWidth()-56.f,14.f);
            juce::Path trace;
            for(int index=0;index<128;++index){
                const float raw=cachedSignal[(size_t)index];
                const float value=std::isfinite(raw)?juce::jlimit(-1.f,1.f,raw):0.f;
                const float x=plot.getX()+plot.getWidth()*index/127.f;
                const float y=plot.getCentreY()-value*plot.getHeight()*.43f;
                if(index==0)trace.startNewSubPath(x,y);else trace.lineTo(x,y);
            }
            g.setColour(accent.withAlpha(.16f));g.strokePath(trace,juce::PathStrokeType(3.f));
            g.setColour(accent.withAlpha(.62f));g.strokePath(trace,juce::PathStrokeType(1.f));
        }
        const auto status=juce::Rectangle<float>(stageBounds.getX()+15.f,
            stageBounds.getY()+13.f,118.f,24.f);
        g.setColour(Theme::bg.withAlpha(.84f));
        g.fillRoundedRectangle(status,6.f);
        const auto lamp=bypassed?Theme::amber:live?accent:Theme::muted;
        if(live && !bypassed){
            g.setColour(lamp.withAlpha(.13f+.18f*activity));
            g.fillEllipse(status.getX()+6.f,status.getCentreY()-7.f,14.f,14.f);
        }
        g.setColour(lamp);
        g.fillEllipse(status.getX()+10.f,status.getCentreY()-3.f,6.f,6.f);
        Theme::text(g,bypassed?"BYPASS":live?"LIVE SET":"STANDBY",
            {juce::roundToInt(status.getX()+23.f),juce::roundToInt(status.getY()),
             92,24},10.7f,lamp,true);

    }
private:
    // Transparent, keyboard-accessible hit target. The mic artwork remains
    // the actual performer; hover/focus adds only a subtle local halo.
    class MicMatchTarget final:public juce::Button {
    public:
        MicMatchTarget():juce::Button("Stage microphone — Match"){}
        void paintButton(juce::Graphics& g,bool over,bool down)override{
            if(!over&&!down&&!hasKeyboardFocus(true))return;
            const auto r=getLocalBounds().toFloat().reduced(1.f);
            g.setColour(Theme::green.withAlpha(down?.20f:.09f));g.fillRoundedRectangle(r,6.f);
            g.setColour(Theme::green.withAlpha(.75f));g.drawRoundedRectangle(r,6.f,1.f);
        }
    } micMatch;
    void updateMicMatchBounds(){
        const auto stage=layoutFor(getWidth(),getHeight());
        micMatch.setVisible(stage.valid);
        if(stage.valid)micMatch.setBounds(skellpreset::stage::Performer::microphoneHitBounds(stage.originX,stage.originY,stage.scale,hasAudibleOutput()?activityLevel:0.f,crowdPhase).getIntersection(getLocalBounds()));
    }
    skellpreset::stage::Backdrop backdrop;
    skellpreset::stage::Performer performer;
    SkellPresetAudioProcessor& processor;
    bool audioFresh=false;
    MeterEnvelope inputMeter,outputMeter;
    float activityLevel=0.f,crowdPhase=0.f,vocalActivity=0.f;
    skellpreset::stage::StageClock stageClock;
    std::array<float,128> cachedSignal{};
    skellpreset::visual::LiveSpectrumAnalyzer stageAnalyzer;
    std::array<float,skellpreset::visual::LiveSpectrumAnalyzer::bands> stageBands{};
    unsigned spectrumTick=0,lastAnalyzedScope=0;

    // Crowd figures are generated once and reused every frame. Keeping the
    // geometry data here avoids per-frame hashing; artwork is cached per size/theme and the
    // same figure keeps the same pose, depth and phone light across repaints.
    struct CrowdFigure {
        float x=0.f,height=26.f,headRatio=.115f,phase=0.f,rate=1.f,sizeJitter=1.f;
        float yJitter=0.f,tilt=0.f,turn=.5f;
        int layer=0,pose=0;
        float phone=0.f;
        bool phoneSide=false;
    };
    static constexpr int crowdLayers=4;
    static constexpr int crowdCount=136;
    std::array<CrowdFigure,crowdCount> crowd{};
    bool crowdBuilt=false;
    std::array<juce::Image,crowdCount> crowdSprites{};
    float crowdCacheDepth=-1.f;
    int crowdCacheTheme=-1;
    void ensureCrowd() {
        if(crowdBuilt)return;
        crowdBuilt=true;
        int index=0;
        // Slots per row, farthest (top) to nearest (bottom). Distant rows hold
        // more people at a tighter spacing; near rows are wider and sparser,
        // which is what sells the perspective.
        constexpr int perRow[crowdLayers]{44,38,31,23};
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
                const float even=(i+.5f+(rnd(0)-.5f)*1.40f)/people;
                f.x=.5f+(even-.5f)*spread[layer];
                // Height is a modest jitter around the row size; the skull is
                // always a small fraction of the figure so nobody reads as a
                // giant balloon head.
                f.height=24.f+rnd(7)*12.f;
                f.headRatio=.075f+rnd(17)*.042f;
                f.sizeJitter=.69f+rnd(27)*.60f;
                f.phase=rnd(37)*6.2831f;
                f.rate=.86f+rnd(47)*.34f;
                f.pose=(int)(rnd(57)*6.f)%6;
                f.turn=rnd(97);
                f.yJitter=(rnd(91)-.5f)*1.24f;
                f.tilt=(rnd(63)-.5f)*1.8f;
                // Phone lights thin out with distance so the pit glitters
                // toward the front rather than uniformly.
                const float chance=layer==3?.44f:layer==2?.32f:layer==1?.21f:.12f;
                if(rnd(67)<chance)f.phone=.5f+rnd(77)*.5f;
                f.phoneSide=rnd(87)>.5f;
            }
        }
    }
};
