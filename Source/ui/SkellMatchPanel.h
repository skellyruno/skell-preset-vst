#pragma once
#include "SkellActionButton.h"
#include "SkellHubRadar.h"
#include "SkellMatchOrb.h"
#include "../chain/LiveVocalMatch.h"
#include <functional>

class SkellMatchPanel final:public juce::Component,private juce::Timer {
public:
    enum Phase { Listening, More, Reading, Refining, Done, Problem };
    SkellMatchPanel(){
        for(auto* b:{&close,&retry,&listen,&reference,&upload}){addAndMakeVisible(*b);b->setComponentID("header-control");}
        close.setButtonText(juce::String::charToString(0x00d7));close.setTooltip("Close Match");
        retry.setButtonText("RETRY");retry.setTooltip("Analyze a new dry vocal");
        listen.setButtonText("LISTEN");listen.setTooltip("Return to the effect rack and audition in your DAW");
        reference.setButtonText("REFERENCE");reference.setTooltip("Choose an isolated reference vocal file");
        upload.setButtonText("DRY FILE");upload.setTooltip("Choose an isolated dry vocal file");
        close.onClick=[this]{if(onClose)onClose();};retry.onClick=[this]{if(onRetry)onRetry();};listen.onClick=[this]{if(onListen)onListen();};reference.onClick=[this]{if(onReference)onReference();};upload.onClick=[this]{if(onUpload)onUpload();};
        setTitle("Skell Match - vocal preset assistant");
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    ~SkellMatchPanel()override{stopTimer();}
    std::function<void()> onClose,onRetry,onListen,onReference,onUpload;
    bool hasOriginalArtwork()const noexcept{return radar.usesOriginalArt();}
    void setThemeSource(const juce::AudioProcessorValueTreeState& s) noexcept {themeSource=&s;}
    void resetPosition() noexcept {moved=false;dragging=false;}
    void placeWithin(juce::Rectangle<int> available,bool /*left*/){
        limits=available.reduced(12);
        if(limits.isEmpty())return;
        // 25% larger in both dimensions, preserving aspect ratio when
        // the stage is compact. Center on opening, retain manual drag
        // across subsequent refreshes.
        const float s=std::min({1.25f,limits.getWidth()/340.f,limits.getHeight()/332.f});
        const int w=juce::roundToInt(340.f*s),h=juce::roundToInt(332.f*s);
        if(w<=0||h<=0)return;
        if(!moved)setBounds(limits.withSizeKeepingCentre(w,h));
        else setSize(w,h);
        clampPosition(getPosition());
    }
    void advanceAnimationForScreenshot(float seconds) {advanceAnimation(seconds);}
    bool testPlacement(){
        if(limits.isEmpty()||!limits.contains(getBounds()))return false;
        if(!moved&&getBounds().getCentre().getDistanceFrom(limits.getCentre())>1)return false;
        if(limits.getWidth()>=425&&limits.getHeight()>=415&&(getWidth()!=425||getHeight()!=415))return false;
        const auto previous=getPosition();const bool wasMoved=moved;
        clampPosition({limits.getRight()+100,limits.getBottom()+100});
        const bool bottom=limits.contains(getBounds())&&getRight()==limits.getRight()&&getBottom()==limits.getBottom();
        clampPosition({limits.getX()-100,limits.getY()-100});
        const bool top=limits.contains(getBounds())&&getX()==limits.getX()&&getY()==limits.getY();
        moved=true;const auto atCorner=getPosition();
        placeWithin(limits.expanded(12),false);const bool preserved=getPosition()==atCorner;
        clampPosition(previous);moved=wasMoved;return bottom&&top&&preserved;
    }
    void mouseMove(const juce::MouseEvent& ev)override{setMouseCursor(ev.y<38*layoutScale()?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::NormalCursor);}
    void mouseDown(const juce::MouseEvent& ev)override{
        dragging=ev.y<38*layoutScale()&&getParentComponent()!=nullptr;
        if(dragging){dragOrigin=getPosition();dragStart=getParentComponent()->getLocalPoint(nullptr,ev.getScreenPosition());toFront(false);}
    }
    void mouseDrag(const juce::MouseEvent& ev)override{
        if(!dragging||!getParentComponent())return;
        // Parent-space pointer positions are stable even as this component
        // moves underneath the mouse; avoid a drag delta relative to self.
        const auto delta=getParentComponent()->getLocalPoint(nullptr,ev.getScreenPosition())-dragStart;
        moved=true;clampPosition(dragOrigin+delta);
    }
    void mouseUp(const juce::MouseEvent&)override{dragging=false;}
    void visibilityChanged()override{syncAnimation();}
    void update(Phase next,const juce::String& detail,float progress,float level,bool haveDry){
        const bool phaseChanged=phase!=next||dryAvailable!=haveDry;
        phase=next;dryAvailable=haveDry;message=detail;fraction=juce::jlimit(0.f,1.f,progress);energy=juce::jlimit(0.f,1.f,level);
        if(phaseChanged||!initialised){
            initialised=true;
            retry.setVisible(next==Problem||next==Done);
            listen.setVisible(next==Done);reference.setVisible(haveDry&&(next==Done||next==Problem));upload.setVisible(next!=Reading&&next!=Refining);
            listen.setToggleState(next==Done,juce::dontSendNotification);
            resized();syncAnimation();
        }
        repaint();
    }
    void resized()override{
        close.setBounds(scaled({304,8,26,26}));
        std::array<SkellActionButton*,4> buttons{{&retry,&listen,&reference,&upload}};
        int count=0;for(auto* b:buttons)if(b->isVisible())++count;
        int i=0;
        for(auto* b:buttons)if(b->isVisible()){
            const int col=i%2,row=i/2,cell=148;
            const bool single=count==1||(count==3&&i==2);
            const int x=single?(340-cell)/2:(340-2*cell-8)/2+col*(cell+8);
            const int y=count<=2?332-43:332-70+row*32;
            b->setBounds(scaled({x,y,cell,28}));++i;
        }
    }
    void paint(juce::Graphics& g)override{
        if(themeSource)Theme::selectFor(*themeSource);
        juce::Graphics::ScopedSaveState local(g);
        g.addTransform(juce::AffineTransform::scale(getWidth()/340.f,getHeight()/332.f));
        const auto r=juce::Rectangle<float>(0,0,340,332).reduced(1.f);const float cx=r.getCentreX();
        const auto tint=phase==Problem?Theme::amber:Theme::green;
        g.setColour(Theme::bg);g.fillRoundedRectangle(r,10.f);
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(.07f),r.getTopLeft(),Theme::bg,r.getBottomLeft(),false));g.fillRoundedRectangle(r,10.f);
        g.setColour(Theme::stroke);g.drawRoundedRectangle(r,10.f,1.f);
        g.setColour(tint.withAlpha(.62f));g.drawLine(13.f,2.f,r.getRight()-13.f,2.f,1.f);
        radar.drawBrand(g,{12,9,20,22});
        Theme::fittedText(g,"SKELL MATCH",{38,9,106,24},12.f,tint,true);
        // Grip is decoration only; entire title strip is draggable.
        g.setColour(Theme::muted.withAlpha(.7f));
        for(int i=0;i<3;++i)for(int j=0;j<2;++j)g.fillEllipse(cx-5+i*5,17.f+j*5,2.f,2.f);
        g.setColour(Theme::stroke.withAlpha(.65f));g.drawLine(12.f,39.f,r.getRight()-12.f,39.f,.8f);
        const char* title=phase==Done?"Your preset is ready":phase==Problem?"More information needed":phase==Refining?"Refining your preset":phase==Reading?"Analyzing vocal file":phase==More?"Keep playing your vocals":"Listening to your dry vocals";
        Theme::fittedText(g,title,{13,48,340-26,27},17.f,Theme::white,true,juce::Justification::centred);
        const bool active=phase==Listening||phase==More||phase==Refining||phase==Reading;
        if(active)orb.draw(g,{cx-60.f,74.f,120.f,120.f},rotation,smoothedEnergy,fraction,phase==Refining);
        else radar.draw(g,{cx-55.f,78.f,110.f,110.f},rotation,false);
        if(phase==Done||phase==Problem){
            const float x=cx+37.f,y=168.f;g.setColour(Theme::bg);g.fillEllipse(x-10,y-10,20,20);g.setColour(tint);g.drawEllipse(x-10,y-10,20,20,1.2f);
            if(phase==Done){juce::Path check;check.startNewSubPath(x-4,y);check.lineTo(x-1,y+3);check.lineTo(x+5,y-4);g.strokePath(check,juce::PathStrokeType(1.7f));}
            else{g.drawLine(x,y-5,x,y+1,1.5f);g.fillEllipse(x-1,y+4,2,2);}
        }
        g.setColour(Theme::white);g.setFont(Theme::font(11.9f));g.drawFittedText(message,{15,190,340-30,39},juce::Justification::centred,3,.82f);
        const auto bar=juce::Rectangle<float>(18.f,237.f,340-36.f,4.f);
        g.setColour(Theme::stroke);g.fillRoundedRectangle(bar,2.f);g.setColour(tint);if(fraction>0.f)g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*fraction),2.f);
        if(phase==Listening||phase==More){
            // Actual measured input amplitude; do not draw a fake waveform.
            g.setColour(Theme::muted);g.setFont(Theme::font(9.f,true));g.drawText("DRY INPUT",18,247,66,14,juce::Justification::centredLeft);
            const juce::Rectangle<float> meter(87.f,253.f,46.f,3.f);g.setColour(Theme::stroke);g.fillRect(meter);
            g.setColour(tint);g.fillRect(meter.withWidth(meter.getWidth()*energy));
        }else Theme::fittedText(g,phase==Done?"RACK APPLIED  /  UNDO AVAILABLE":"LOCAL ANALYSIS  /  VOCALS ONLY",{16,245,340-32,15},9.5f,Theme::muted,true,juce::Justification::centred);
        if(phase==Listening||phase==More)Theme::fittedText(g,"LOCAL / VOCALS ONLY",{143,245,340-160,15},9.5f,Theme::muted,true,juce::Justification::centredRight);
    }
private:
    float layoutScale() const noexcept {return getHeight()/332.f;}
    juce::Rectangle<int> scaled(juce::Rectangle<int> b) const noexcept {return b.toFloat().transformedBy(juce::AffineTransform::scale(getWidth()/340.f,getHeight()/332.f)).toNearestInt();}
    void advanceAnimation(float seconds){
        const float dt=juce::jmax(0.f,seconds);
        rotation=std::fmod(rotation+dt*juce::MathConstants<float>::twoPi/18.f,juce::MathConstants<float>::twoPi);
        smoothedEnergy+=(energy-smoothedEnergy)*(1.f-std::exp(-dt*9.f));
        repaint(scaled({105,73,130,122}));
    }
    void clampPosition(juce::Point<int> p){
        if(limits.isEmpty())return;
        setTopLeftPosition(juce::jlimit(limits.getX(),juce::jmax(limits.getX(),limits.getRight()-getWidth()),p.x),
            juce::jlimit(limits.getY(),juce::jmax(limits.getY(),limits.getBottom()-getHeight()),p.y));
    }
    void syncAnimation(){
        if(isShowing()&&(phase==Listening||phase==More||phase==Reading||phase==Refining)){lastTime=juce::Time::getMillisecondCounterHiRes();startTimerHz(30);}
        else stopTimer();
    }
    void timerCallback()override{
        if(!isShowing()){stopTimer();return;}
        const auto now=juce::Time::getMillisecondCounterHiRes();
        advanceAnimation((float)juce::jlimit(0.,.1,(now-lastTime)*.001));
        lastTime=now;
    }
    Phase phase=Listening;juce::String message;float fraction=0,energy=0,rotation=0,smoothedEnergy=0;
    bool initialised=false,dryAvailable=false,moved=false,dragging=false;
    double lastTime=0.;juce::Point<int> dragOrigin,dragStart;juce::Rectangle<int> limits;
    const juce::AudioProcessorValueTreeState* themeSource=nullptr;
    SkellHubRadar radar;
    SkellMatchOrb orb;
    SkellActionButton close,retry,listen,reference,upload;
};
