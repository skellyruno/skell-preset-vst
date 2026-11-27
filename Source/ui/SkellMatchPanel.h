#pragma once
#include "SkellActionButton.h"
#include "SkellHubRadar.h"
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
    void placeWithin(juce::Rectangle<int> available,bool left){
        limits=available.reduced(12);
        const int w=juce::jmin(340,limits.getWidth()),h=juce::jmin(332,limits.getHeight());
        if(w<=0||h<=0)return;
        if(!moved)setBounds(left?limits.getX()+6:limits.getRight()-w-6,limits.getCentreY()-h/2+14,w,h);
        else setSize(w,h);
        clampPosition(getPosition());
    }
    bool testPlacement(){
        if(limits.isEmpty()||!limits.contains(getBounds()))return false;
        const auto previous=getPosition();const bool wasMoved=moved;
        clampPosition({limits.getRight()+100,limits.getBottom()+100});
        const bool bottom=limits.contains(getBounds())&&getRight()==limits.getRight()&&getBottom()==limits.getBottom();
        clampPosition({limits.getX()-100,limits.getY()-100});
        const bool top=limits.contains(getBounds())&&getX()==limits.getX()&&getY()==limits.getY();
        clampPosition(previous);moved=wasMoved;return bottom&&top;
    }
    void mouseMove(const juce::MouseEvent& ev)override{setMouseCursor(ev.y<38?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::NormalCursor);}
    void mouseDown(const juce::MouseEvent& ev)override{
        dragging=ev.y<38;if(dragging){dragOrigin=getPosition();toFront(false);}
    }
    void mouseDrag(const juce::MouseEvent& ev)override{
        if(!dragging)return;
        // Both positions are in parent coordinates; JUCE handles host scale.
        const auto delta=ev.getEventRelativeTo(getParentComponent()).getOffsetFromDragStart();
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
        close.setBounds(getWidth()-36,8,26,26);
        std::array<SkellActionButton*,4> buttons{{&retry,&listen,&reference,&upload}};
        int count=0;for(auto* b:buttons)if(b->isVisible())++count;
        int i=0;
        for(auto* b:buttons)if(b->isVisible()){
            const int col=i%2,row=i/2,cell=juce::jmin(148,(getWidth()-44)/2);
            const bool single=count==1||(count==3&&i==2);
            const int x=single?(getWidth()-cell)/2:(getWidth()-2*cell-8)/2+col*(cell+8);
            const int y=count<=2?getHeight()-43:getHeight()-70+row*32;
            b->setBounds(x,y,cell,28);++i;
        }
    }
    void paint(juce::Graphics& g)override{
        const auto r=getLocalBounds().toFloat().reduced(1.f);const float cx=r.getCentreX();
        const auto tint=phase==Problem?Theme::amber:juce::Colour(0xffaaff1a);
        g.setColour(Theme::bg);g.fillRoundedRectangle(r,10.f);
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(.07f),r.getTopLeft(),Theme::bg,r.getBottomLeft(),false));g.fillRoundedRectangle(r,10.f);
        g.setColour(Theme::stroke);g.drawRoundedRectangle(r,10.f,1.f);
        g.setColour(tint.withAlpha(.62f));g.drawLine(13.f,2.f,r.getRight()-13.f,2.f,1.f);
        Theme::fittedText(g,"SKELL MATCH",{16,9,106,24},12.f,tint,true);
        // Grip is decoration only; entire title strip is draggable.
        g.setColour(Theme::muted.withAlpha(.7f));
        for(int i=0;i<3;++i)for(int j=0;j<2;++j)g.fillEllipse(cx-5+i*5,17.f+j*5,2.f,2.f);
        g.setColour(Theme::stroke.withAlpha(.65f));g.drawLine(12.f,39.f,r.getRight()-12.f,39.f,.8f);
        const char* title=phase==Done?"Your preset is ready":phase==Problem?"More information needed":phase==Refining?"Refining your preset":phase==Reading?"Analyzing vocal file":phase==More?"Keep playing your vocals":"Listening to your dry vocals";
        Theme::fittedText(g,title,{13,48,getWidth()-26,27},17.f,Theme::white,true,juce::Justification::centred);
        radar.draw(g,{cx-55.f,78.f,110.f,110.f},rotation,phase==Listening||phase==More||phase==Refining||phase==Reading);
        if(phase==Done||phase==Problem){
            const float x=cx+37.f,y=168.f;g.setColour(Theme::bg);g.fillEllipse(x-10,y-10,20,20);g.setColour(tint);g.drawEllipse(x-10,y-10,20,20,1.2f);
            if(phase==Done){juce::Path check;check.startNewSubPath(x-4,y);check.lineTo(x-1,y+3);check.lineTo(x+5,y-4);g.strokePath(check,juce::PathStrokeType(1.7f));}
            else{g.drawLine(x,y-5,x,y+1,1.5f);g.fillEllipse(x-1,y+4,2,2);}
        }
        g.setColour(Theme::white);g.setFont(Theme::font(11.9f));g.drawFittedText(message,{15,190,getWidth()-30,39},juce::Justification::centred,3,.82f);
        const auto bar=juce::Rectangle<float>(18.f,237.f,getWidth()-36.f,4.f);
        g.setColour(Theme::stroke);g.fillRoundedRectangle(bar,2.f);g.setColour(tint);if(fraction>0.f)g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*fraction),2.f);
        if(phase==Listening||phase==More){
            // Actual measured input amplitude; do not draw a fake waveform.
            g.setColour(Theme::muted);g.setFont(Theme::font(9.f,true));g.drawText("DRY INPUT",18,247,66,14,juce::Justification::centredLeft);
            const juce::Rectangle<float> meter(87.f,253.f,46.f,3.f);g.setColour(Theme::stroke);g.fillRect(meter);
            g.setColour(tint);g.fillRect(meter.withWidth(meter.getWidth()*energy));
        }else Theme::fittedText(g,phase==Done?"RACK APPLIED  /  UNDO AVAILABLE":"LOCAL ANALYSIS  /  VOCALS ONLY",{16,245,getWidth()-32,15},9.5f,Theme::muted,true,juce::Justification::centred);
        if(phase==Listening||phase==More)Theme::fittedText(g,"LOCAL / VOCALS ONLY",{143,245,getWidth()-160,15},9.5f,Theme::muted,true,juce::Justification::centredRight);
    }
private:
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
        rotation=std::fmod(rotation+(float)juce::jlimit(0.,.1,(now-lastTime)*.001)*juce::MathConstants<float>::twoPi/18.f,juce::MathConstants<float>::twoPi);
        lastTime=now;repaint(0,76,getWidth(),114);
    }
    Phase phase=Listening;juce::String message;float fraction=0,energy=0,rotation=0;
    bool initialised=false,dryAvailable=false,moved=false,dragging=false;
    double lastTime=0.;juce::Point<int> dragOrigin;juce::Rectangle<int> limits;
    SkellHubRadar radar;
    SkellActionButton close,retry,listen,reference,upload;
};
