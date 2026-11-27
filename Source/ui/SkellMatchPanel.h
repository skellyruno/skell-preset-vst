#pragma once
#include "SkellActionButton.h"
#include "SkellHubRadar.h"
#include "SkellMatchOrb.h"
#include "../chain/LiveVocalMatch.h"
#include <functional>
#include <cstdio>

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
        // 25% larger in both dimensions; center on the entire plugin canvas.
        // Retain manual drag and fit compact bounds.
        const float s=std::min({1.25f,limits.getWidth()/536.f,limits.getHeight()/330.f});
        // Explicit half-up rounding: JUCE roundToInt rounds 412.5 to 412 on some targets.
        const int w=juce::jmin(limits.getWidth(),(int)std::round(536.f*s)),h=juce::jmin(limits.getHeight(),(int)std::round(330.f*s));
        if(w<=0||h<=0)return;
        if(!moved)setBounds(limits.withSizeKeepingCentre(w,h));
        else setSize(w,h);
        clampPosition(getPosition());
    }
    void advanceAnimationForScreenshot(float seconds) {advanceAnimation(seconds);}
    double benchmarkOrbForScreenshot(){
        if(themeSource)Theme::selectFor(*themeSource);
        juce::Image image(juce::Image::ARGB,670,413,true);juce::Graphics g(image);
        g.addTransform(juce::AffineTransform::scale(1.25f));
        std::array<double,120> times{};
        for(size_t i=0;i<times.size();++i){image.clear(image.getBounds());const auto start=juce::Time::getMillisecondCounterHiRes();orb.draw(g,{14,57,222,222},(float)i*.035f,.25f,.5f,false);times[i]=juce::Time::getMillisecondCounterHiRes()-start;}
        std::sort(times.begin(),times.end());
        const double p95=times[113];std::fprintf(stderr,"Match orb software paint: median %.3f ms, p95 %.3f ms (120 frames)\n",times[60],p95);return p95;
    }
    bool testPlacement(){
        if(limits.isEmpty()||!limits.contains(getBounds()))return false;
        if(!moved&&(getBounds().getCentre().getDistanceFrom(limits.getCentre())>1||!getParentComponent()||getBounds().getCentre().getDistanceFrom(getParentComponent()->getLocalBounds().getCentre())>1))return false;
        if(limits.getWidth()>=670&&limits.getHeight()>=413&&(getWidth()!=670||getHeight()!=413))return false;
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
        close.setBounds(scaled({498,8,28,28}));
        std::array<SkellActionButton*,4> buttons{{&retry,&listen,&reference,&upload}};
        int count=0;for(auto* b:buttons)if(b->isVisible())++count;
        int i=0;
        for(auto* b:buttons)if(b->isVisible()){
            const int cell=122,gap=8,col=i%2,row=i/2;
            const bool single=count==1||(count==3&&i==2);
            const int x=single?328:263+col*(cell+gap);
            const int y=count<=2?278:246+row*34;
            b->setBounds(scaled({x,y,cell,29}));++i;
        }
    }
    void paint(juce::Graphics& g)override{
        if(themeSource)Theme::selectFor(*themeSource);
        juce::Graphics::ScopedSaveState local(g);
        g.addTransform(juce::AffineTransform::scale(getWidth()/536.f,getHeight()/330.f));
        const auto r=juce::Rectangle<float>(0,0,536,330).reduced(1.f);
        const auto tint=phase==Problem?Theme::amber:Theme::green;
        // Translucent smoked glass: retain the real venue, not a fake
        // background screenshot. The text side has extra dark backing.
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.withAlpha(.90f),r.getTopLeft(),Theme::bg.withAlpha(.78f),r.getBottomRight(),false));
        g.fillRoundedRectangle(r,12.f);
        g.setGradientFill(juce::ColourGradient(Theme::bg.withAlpha(.26f),250.f,43.f,Theme::bg.withAlpha(.78f),524.f,310.f,false));
        g.fillRoundedRectangle(juce::Rectangle<float>(248,43,276,274),8.f);
        g.setColour(Theme::stroke.brighter(.28f));g.drawRoundedRectangle(r,12.f,1.15f);
        g.setColour(tint.withAlpha(.52f));g.drawLine(16.f,2.f,r.getRight()-16.f,2.f,1.f);
        radar.drawBrand(g,{13,9,23,24});
        Theme::fittedText(g,"SKELL MATCH",{44,9,138,25},13.f,tint,true);
        g.setColour(Theme::muted.withAlpha(.72f));
        for(int i=0;i<3;++i)for(int j=0;j<2;++j)g.fillEllipse(263.f+i*5,17.f+j*5,2.f,2.f);
        g.setColour(Theme::stroke.withAlpha(.72f));g.drawLine(13.f,40.f,523.f,40.f,.8f);
        const bool active=phase==Listening||phase==More||phase==Refining||phase==Reading;
        if(active)orb.draw(g,{14.f,57.f,222.f,222.f},rotation,smoothedEnergy,fraction,phase==Refining);
        else radar.draw(g,{29.f,72.f,192.f,192.f},rotation,false);
        if(phase==Done||phase==Problem){
            const float x=201.f,y=245.f;g.setColour(Theme::bg);g.fillEllipse(x-12,y-12,24,24);g.setColour(tint);g.drawEllipse(x-12,y-12,24,24,1.3f);
            if(phase==Done){juce::Path check;check.startNewSubPath(x-5,y);check.lineTo(x-1,y+4);check.lineTo(x+6,y-5);g.strokePath(check,juce::PathStrokeType(1.9f));}
            else{g.drawLine(x,y-6,x,y+1,1.7f);g.fillEllipse(x-1,y+5,2,2);}
        }
        Theme::fittedText(g,active?"ANALYZING SIGNAL":phase==Done?"MATCH COMPLETE":"INPUT REQUIRED",{14,287,222,22},11.f,tint,true,juce::Justification::centred);
        const char* title=phase==Done?"Your preset is ready":phase==Problem?"More information needed":phase==Refining?"Refining your preset":phase==Reading?"Analyzing vocal file":phase==More?"Keep playing your vocals":"Listening to your dry vocals";
        g.setColour(Theme::white);g.setFont(Theme::font(20.f,true));g.drawFittedText(title,{263,54,249,55},juce::Justification::centredLeft,2,.84f);
        g.setColour(Theme::white);g.setFont(Theme::font(13.5f));g.drawFittedText(message,{263,116,249,60},juce::Justification::topLeft,4,.84f);
        Theme::fittedText(g,phase==Done?"COMPLETE":phase==Problem?"CHECK INPUT":phase==Refining?"VALIDATING":phase==Reading?"READING FILE":"CAPTURING",{263,180,158,18},10.5f,tint,true);
        if(phase!=Problem&&phase!=Reading)Theme::fittedText(g,juce::String(juce::roundToInt(fraction*100.f))+"%",{429,180,83,18},11.f,tint,true,juce::Justification::centredRight);
        const auto bar=juce::Rectangle<float>(263.f,204.f,249.f,5.f);
        g.setColour(Theme::stroke);g.fillRoundedRectangle(bar,2.5f);g.setColour(tint);if(fraction>0.f)g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*fraction),2.5f);
        if(phase==Listening||phase==More){
            Theme::fittedText(g,"DRY INPUT",{263,215,77,18},10.5f,Theme::muted,true);
            const juce::Rectangle<float> meter(342.f,222.f,53.f,4.f);g.setColour(Theme::stroke);g.fillRect(meter);
            g.setColour(tint);g.fillRect(meter.withWidth(meter.getWidth()*energy));
            Theme::fittedText(g,"VOCALS ONLY",{400,215,112,18},10.f,Theme::muted,true,juce::Justification::centredRight);
        }else Theme::fittedText(g,phase==Done?"RACK APPLIED / UNDO AVAILABLE":"LOCAL ANALYSIS / VOCALS ONLY",{263,215,249,18},10.5f,Theme::muted,true);
    }
private:
    float layoutScale() const noexcept {return getHeight()/330.f;}
    juce::Rectangle<int> scaled(juce::Rectangle<int> b) const noexcept {return b.toFloat().transformedBy(juce::AffineTransform::scale(getWidth()/536.f,getHeight()/330.f)).toNearestInt();}
    void advanceAnimation(float seconds){
        const float dt=juce::jmax(0.f,seconds);
        rotation=std::fmod(rotation+dt*juce::MathConstants<float>::twoPi/18.f,juce::MathConstants<float>::twoPi);
        smoothedEnergy+=(energy-smoothedEnergy)*(1.f-std::exp(-dt*9.f));
        repaint(scaled({12,55,226,226}));
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
