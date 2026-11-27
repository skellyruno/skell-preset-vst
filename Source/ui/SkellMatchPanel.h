#pragma once
#include "SkellActionButton.h"
#include "../chain/LiveVocalMatch.h"
#include <functional>

class SkellMatchPanel final:public juce::Component {
public:
    enum Phase { Listening, More, Reading, Done, Problem };
    SkellMatchPanel(){
        for(auto* b:{&close,&retry,&listen,&reference,&upload})addAndMakeVisible(*b);
        close.setButtonText("X");close.setTooltip("Close Match");retry.setButtonText("RETRY");listen.setButtonText("LISTEN");reference.setButtonText("REFERENCE");upload.setButtonText("DRY FILE");
        close.onClick=[this]{if(onClose)onClose();};retry.onClick=[this]{if(onRetry)onRetry();};listen.onClick=[this]{if(onListen)onListen();};reference.onClick=[this]{if(onReference)onReference();};upload.onClick=[this]{if(onUpload)onUpload();};
        setTitle("Skell Match — vocal preset assistant");
    }
    std::function<void()> onClose,onRetry,onListen,onReference,onUpload;
    void update(Phase next,const juce::String& detail,float progress,float level,bool haveDry){
        phase=next;message=detail;fraction=juce::jlimit(0.f,1.f,progress);energy=juce::jlimit(0.f,1.f,level);
        retry.setVisible(next==Problem||next==Done);
        listen.setVisible(next==Done);reference.setVisible(haveDry&&(next==Done||next==Problem));upload.setVisible(next!=Reading);
        resized();repaint();
    }
    void resized()override{
        close.setBounds(getWidth()-35,10,25,25);
        auto row=getLocalBounds().reduced(16,0).removeFromBottom(42).withTrimmedBottom(8);
        int count=0;for(auto* b:{&retry,&listen,&reference,&upload})if(b->isVisible())++count;
        if(count)for(auto* b:{&retry,&listen,&reference,&upload})if(b->isVisible())b->setBounds(row.removeFromLeft(row.getWidth()/count--).reduced(3,0));
    }
    void paint(juce::Graphics& g)override{
        const auto r=getLocalBounds().toFloat().reduced(1.f);const float cx=r.getCentreX(),cy=108.f;
        g.setColour(Theme::bg.withAlpha(.97f));g.fillRoundedRectangle(r,13.f);
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.withAlpha(.85f),r.getTopLeft(),Theme::bg.withAlpha(.6f),r.getBottomLeft(),false));g.fillRoundedRectangle(r,13.f);
        const auto tint=phase==Problem?Theme::amber:Theme::green;
        g.setColour(Theme::stroke);g.drawRoundedRectangle(r,13.f,1.f);
        g.setColour(tint.withAlpha(.65f));g.drawLine(16.f,2.f,r.getRight()-16.f,2.f,1.f);
        Theme::fittedText(g,"SKELL MATCH",{18,12,getWidth()-68,18},11.f,tint,true);
        const char* title=phase==Done?"Your preset is ready":phase==Problem?"Match needs your attention":phase==Reading?"Analyzing vocal file":phase==More?"Keep playing your vocals":"Listening to your dry vocals";
        Theme::fittedText(g,title,{18,36,getWidth()-36,30},20.f,Theme::white,true,juce::Justification::centred);
        const float radius=31.f+energy*3.f;
        g.setGradientFill(juce::ColourGradient(tint.withAlpha(.22f+energy*.18f),cx,cy,tint.withAlpha(0.f),cx+radius*1.65f,cy,true));g.fillEllipse(cx-radius*1.7f,cy-radius*1.7f,radius*3.4f,radius*3.4f);
        g.setColour(Theme::bg);g.fillEllipse(cx-radius,cy-radius,radius*2,radius*2);
        g.setColour(tint.withAlpha(.8f));g.drawEllipse(cx-radius,cy-radius,radius*2,radius*2,1.3f);
        juce::Path trace;for(int i=0;i<=48;++i){const float x=(float)i/48.f;const float amp=(phase==Done?3.f:energy*13.f)*std::sin(juce::MathConstants<float>::pi*x);const float y=cy+amp*std::sin(x*20.f);if(i==0)trace.startNewSubPath(cx-radius+5,y);else trace.lineTo(cx-radius+5+x*(2*radius-10),y);}
        g.setColour(tint);g.strokePath(trace,juce::PathStrokeType(2.f));
        g.setColour(Theme::white);g.setFont(Theme::font(12.5f));g.drawFittedText(message,{22,156,getWidth()-44,43},juce::Justification::centred,2,.85f);
        const auto bar=juce::Rectangle<float>(24.f,204.f,getWidth()-48.f,4.f);
        g.setColour(Theme::stroke);g.fillRoundedRectangle(bar,2.f);g.setColour(tint);g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*fraction),2.f);
        Theme::fittedText(g,phase==Done?"Native rack applied  •  Undo available":"Isolated vocals only  •  Local analysis",{20,214,getWidth()-40,18},10.5f,Theme::muted,true,juce::Justification::centred);
    }
private:
    Phase phase=Listening;juce::String message;float fraction=0,energy=0;
    SkellActionButton close,retry,listen,reference,upload;
};
