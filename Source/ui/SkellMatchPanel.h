#pragma once
#include "SkellActionButton.h"
#include "SkellHubRadar.h"
#include "SkellMatchOrb.h"
#include "../chain/AutoVocalMix.h"
#include <functional>
#include <cstdio>

class SkellMatchPanel final:public juce::Component,private juce::Timer {
public:
    enum Phase { Listening, More, Reading, Refining, Done, Problem, ChooseStyle, RefineText };
    SkellMatchPanel(){
        for(auto* b:{&close,&retry,&listen,&reference,&upload}){addAndMakeVisible(*b);b->setComponentID("header-control");}
        close.setButtonText(juce::String::charToString(0x00d7));close.setTooltip("Close Match");
        retry.setButtonText("RETRY");retry.setTooltip("Analyze a new dry vocal");
        listen.setButtonText("LISTEN");listen.setTooltip("Return to the effect rack and audition in your DAW");
        reference.setButtonText("REFERENCE");reference.setTooltip("Choose an isolated reference vocal file");
        upload.setButtonText("DRY FILE");upload.setTooltip("Choose an isolated dry vocal file");
        close.onClick=[this]{if(onClose)onClose();};retry.onClick=[this]{if(onRetry)onRetry();};listen.onClick=[this]{if(onListen)onListen();};reference.onClick=[this]{if(onReference)onReference();};upload.onClick=[this]{if(onUpload)onUpload();};
        for(size_t i=0;i<styleButtons.size();++i){auto& b=styleButtons[i];addAndMakeVisible(b);b.setComponentID("header-control");b.setButtonText(skellpreset::liveMatch::styleGuides[i].name);b.onClick=[this,i]{chooseStyle((int)i);};}
        addAndMakeVisible(start);start.setComponentID("header-control");start.setButtonText("START LISTENING");start.onClick=[this]{if(onStart)onStart();};
        for(auto* b:{&beat,&refine,&submit,&back}){addAndMakeVisible(*b);b->setComponentID("header-control");}
        beat.setButtonText("BEAT FILE");beat.setTooltip("Optional instrumental context, never processed or uploaded");beat.onClick=[this]{if(onBeat)onBeat();};
        refine.setButtonText("REFINE");refine.onClick=[this]{if(onRefine)onRefine();};
        submit.setButtonText("APPLY REQUEST");submit.onClick=[this]{if(onSubmit)onSubmit(prompt.getText());};
        back.setButtonText("BACK");back.onClick=[this]{if(onBack)onBack();};
        addAndMakeVisible(prompt);prompt.setMultiLine(false);prompt.setReturnKeyStartsNewLine(false);prompt.setInputRestrictions(240);prompt.setTextToShowWhenEmpty("More aggressive, but keep it clear",juce::Colours::grey);prompt.onReturnKey=[this]{if(submit.isVisible()&&submit.onClick)submit.onClick();};
        chooseStyle(0);
        setTitle("Skell Match - vocal preset assistant");
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    ~SkellMatchPanel()override{stopTimer();}
    std::function<void()> onClose,onRetry,onListen,onReference,onUpload,onStart,onBeat,onRefine,onBack;
    std::function<void(juce::String)> onSubmit;
    void setBeatReady(bool ready){beatReady=ready;beat.setButtonText(ready?"BEAT LOADED":"BEAT FILE");repaint();}
    void setRefinementAvailable(bool ready){refinementAvailable=ready;initialised=false;}
    bool editingPrompt() const noexcept {return phase==RefineText&&prompt.hasKeyboardFocus(true);}
    bool testRefine(){if(!refine.isVisible()||!refine.onClick)return false;refine.onClick();return phase==RefineText&&prompt.isVisible()&&submit.isVisible();}
    skellpreset::liveMatch::MixStyle selectedStyle() const noexcept {return static_cast<skellpreset::liveMatch::MixStyle>(styleIndex);}
    bool testChooseStyle(){if(phase!=ChooseStyle||!start.isVisible())return false;chooseStyle(1);const bool ok=styleButtons[1].getToggleState()&&selectedStyle()==skellpreset::liveMatch::MixStyle::Underground;chooseStyle(0);return ok;}
    void testStart(){if(start.isVisible()&&start.onClick)start.onClick();}
    bool hasOriginalArtwork()const noexcept{return radar.usesOriginalArt();}
    void setThemeSource(const juce::AudioProcessorValueTreeState& s) noexcept {themeSource=&s;}
    void resetPosition() noexcept {moved=false;dragging=false;}
    void placeWithin(juce::Rectangle<int> available,bool /*left*/){
        limits=available.reduced(12);
        if(limits.isEmpty())return;
        // Wider, shallower console; biased right and slightly upward.
        // Retain manual drag and fit compact bounds.
        const float s=std::min({1.125f,limits.getWidth()/600.f,limits.getHeight()/304.f});
        // Explicit half-up rounding: JUCE roundToInt rounds 412.5 to 412 on some targets.
        const int w=juce::jmin(limits.getWidth(),(int)std::round(600.f*s)),h=juce::jmin(limits.getHeight(),(int)std::round(304.f*s));
        if(w<=0||h<=0)return;
        if(!moved){setBounds(juce::Rectangle<int>(openingPosition(available,w,h).x,openingPosition(available,w,h).y,w,h));}
        else setSize(w,h);
        clampPosition(getPosition());
    }
    void advanceAnimationForScreenshot(float seconds) {advanceAnimation(seconds);}
    double benchmarkOrbForScreenshot(){
        if(themeSource)Theme::selectFor(*themeSource);
        juce::Image image(juce::Image::ARGB,675,342,true);juce::Graphics g(image);
        g.addTransform(juce::AffineTransform::scale(675.f/600.f,342.f/304.f));
        std::array<double,120> times{};
        for(size_t i=0;i<times.size();++i){image.clear(image.getBounds());const auto start=juce::Time::getMillisecondCounterHiRes();orb.draw(g,{20,50,210,210},(float)i*.035f,.25f,.5f,false);times[i]=juce::Time::getMillisecondCounterHiRes()-start;}
        std::sort(times.begin(),times.end());
        const double p95=times[113];std::fprintf(stderr,"Match orb software paint: median %.3f ms, p95 %.3f ms (120 frames)\n",times[60],p95);return p95;
    }
    bool testPlacement(){
        if(limits.isEmpty()||!limits.contains(getBounds()))return false;
        if(!moved&&(!getParentComponent()||getPosition()!=openingPosition(getParentComponent()->getLocalBounds(),getWidth(),getHeight())))return false;
        if(limits.getWidth()>=675&&limits.getHeight()>=342&&(getWidth()!=675||getHeight()!=342))return false;
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
            beat.setVisible(next==ChooseStyle);refine.setVisible(next==Done&&refinementAvailable);
            submit.setVisible(next==RefineText);back.setVisible(next==RefineText);prompt.setVisible(next==RefineText);
            if(next==RefineText){prompt.clear();prompt.setColour(juce::TextEditor::backgroundColourId,Theme::bg);prompt.setColour(juce::TextEditor::textColourId,Theme::white);prompt.setColour(juce::TextEditor::outlineColourId,Theme::stroke);prompt.setColour(juce::TextEditor::focusedOutlineColourId,Theme::green);}
            for(auto& b:styleButtons)b.setVisible(next==ChooseStyle);
            start.setVisible(next==ChooseStyle);
            listen.setVisible(next==Done);reference.setVisible(haveDry&&(next==Done||next==Problem));upload.setVisible(next!=Reading&&next!=Refining&&next!=RefineText);
            listen.setToggleState(next==Done,juce::dontSendNotification);
            resized();syncAnimation();
        }
        repaint();
    }
    void resized()override{
        close.setBounds(scaled({562,8,28,28}));
        if(phase==ChooseStyle){for(size_t i=0;i<styleButtons.size();++i)styleButtons[i].setBounds(scaled({263+(int)(i%2)*160,113+(int)(i/2)*27,153,24}));start.setBounds(scaled({263,230,101,30}));upload.setBounds(scaled({369,230,101,30}));beat.setBounds(scaled({475,230,101,30}));return;}
        if(phase==RefineText){prompt.setBounds(scaled({263,148,313,32}));submit.setBounds(scaled({263,230,153,30}));back.setBounds(scaled({423,230,153,30}));return;}
        if(phase==Done&&refinementAvailable){refine.setBounds(scaled({263,222,101,29}));listen.setBounds(scaled({369,222,101,29}));retry.setBounds(scaled({475,222,101,29}));reference.setBounds(scaled({263,256,153,29}));upload.setBounds(scaled({423,256,153,29}));return;}
        std::array<SkellActionButton*,4> buttons{{&retry,&listen,&reference,&upload}};
        int count=0;for(auto* b:buttons)if(b->isVisible())++count;
        int i=0;
        for(auto* b:buttons)if(b->isVisible()){
            const int cell=153,gap=10,col=i%2,row=i/2;
            const bool single=count==1||(count==3&&i==2);
            const int x=single?343:263+col*(cell+gap);
            const int y=count<=2?236:222+row*34;
            b->setBounds(scaled({x,y,cell,29}));++i;
        }
    }
    void paint(juce::Graphics& g)override{
        if(themeSource)Theme::selectFor(*themeSource);
        juce::Graphics::ScopedSaveState local(g);
        g.addTransform(juce::AffineTransform::scale(getWidth()/600.f,getHeight()/304.f));
        const auto r=juce::Rectangle<float>(0,0,600,304).reduced(1.f);
        const auto tint=phase==Problem?Theme::amber:Theme::green;
        // Translucent smoked glass: retain the real venue, not a fake
        // background screenshot. The text side has extra dark backing.
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.withAlpha(.90f),r.getTopLeft(),Theme::bg.withAlpha(.78f),r.getBottomRight(),false));
        g.fillRoundedRectangle(r,12.f);
        g.setGradientFill(juce::ColourGradient(Theme::bg.withAlpha(.26f),250.f,43.f,Theme::bg.withAlpha(.78f),588.f,288.f,false));
        g.fillRoundedRectangle(juce::Rectangle<float>(248,43,340,248),8.f);
        g.setColour(Theme::stroke.withAlpha(.46f));g.drawVerticalLine(244,57.f,283.f);
        g.setColour(Theme::stroke.brighter(.28f));g.drawRoundedRectangle(r,12.f,1.15f);
        g.setColour(tint.withAlpha(.52f));g.drawLine(16.f,2.f,r.getRight()-16.f,2.f,1.f);
        radar.drawBrand(g,{13,9,23,24});
        Theme::fittedText(g,"SKELL MATCH",{44,9,138,25},13.f,tint,true);
        g.setColour(Theme::muted.withAlpha(.72f));
        for(int i=0;i<3;++i)for(int j=0;j<2;++j)g.fillEllipse(294.f+i*5,18.f+j*5,2.f,2.f);
        g.setColour(Theme::stroke.withAlpha(.72f));g.drawLine(13.f,40.f,587.f,40.f,.8f);
        const bool active=phase==Listening||phase==More||phase==Refining||phase==Reading;
        if(active)orb.draw(g,{20.f,50.f,210.f,210.f},rotation,smoothedEnergy,fraction,phase==Refining);
        else radar.draw(g,{29.f,59.f,192.f,192.f},rotation,false);
        if(phase==Done||phase==Problem){
            const float x=201.f,y=232.f;g.setColour(Theme::bg);g.fillEllipse(x-12,y-12,24,24);g.setColour(tint);g.drawEllipse(x-12,y-12,24,24,1.3f);
            if(phase==Done){juce::Path check;check.startNewSubPath(x-5,y);check.lineTo(x-1,y+4);check.lineTo(x+6,y-5);g.strokePath(check,juce::PathStrokeType(1.9f));}
            else{g.drawLine(x,y-6,x,y+1,1.7f);g.fillEllipse(x-1,y+5,2,2);}
        }
        if(phase==ChooseStyle){
            Theme::fittedText(g,"AUTO MIX / LOCAL",{14,267,222,22},11.f,tint,true,juce::Justification::centred);
            Theme::fittedText(g,"Choose your vocal style",{263,53,313,29},19.f,Theme::white,true);
            Theme::fittedText(g,message.startsWith("Beat not usable")?message:beatReady?"Instrumental context loaded. Choose your style.":"No reference needed. Match listens to your voice.",{263,84,313,23},12.f,Theme::muted,false);
            Theme::fittedText(g,beatReady?"BEAT CONTEXT LOADED  /  LOCAL ONLY":"REFERENCE OPTIONAL  /  NOTHING UPLOADED",{263,270,313,17},9.5f,Theme::muted,true,juce::Justification::centred);return;
        }
        if(phase==RefineText){
            Theme::fittedText(g,"REFINE / LOCAL",{14,267,222,22},11.f,tint,true,juce::Justification::centred);
            Theme::fittedText(g,"What should change?",{263,53,313,29},19.f,Theme::white,true);
            g.setColour(Theme::white);g.setFont(Theme::font(13.f));g.drawFittedText(message,{263,85,313,52},juce::Justification::topLeft,3,.84f);
            Theme::fittedText(g,"Brighter / drier / wider / louder / softer",{263,189,313,24},10.5f,Theme::muted,false);
            Theme::fittedText(g,"YOUR CURRENT RACK STAYS UNTIL VALIDATION",{263,270,313,17},9.5f,Theme::muted,true,juce::Justification::centred);return;
        }
        Theme::fittedText(g,active?"ANALYZING SIGNAL":phase==Done?"MATCH COMPLETE":"INPUT REQUIRED",{14,267,222,22},11.f,tint,true,juce::Justification::centred);
        const char* title=phase==Done?"Your preset is ready":phase==Problem?"More information needed":phase==Refining?"Refining your preset":phase==Reading?"Analyzing vocal file":phase==More?"Keep playing your vocals":"Listening to your dry vocals";
        g.setColour(Theme::white);g.setFont(Theme::font(20.f,true));g.drawFittedText(title,{263,50,313,46},juce::Justification::centredLeft,2,.84f);
        g.setColour(Theme::white);g.setFont(Theme::font(13.5f));g.drawFittedText(message,{263,102,313,54},juce::Justification::topLeft,4,.84f);
        Theme::fittedText(g,phase==Done?"COMPLETE":phase==Problem?"CHECK INPUT":phase==Refining?"VALIDATING":phase==Reading?"READING FILE":"CAPTURING",{263,162,158,18},10.5f,tint,true);
        if(phase!=Problem&&phase!=Reading)Theme::fittedText(g,juce::String(juce::roundToInt(fraction*100.f))+"%",{493,162,83,18},11.f,tint,true,juce::Justification::centredRight);
        const auto bar=juce::Rectangle<float>(263.f,184.f,313.f,5.f);
        g.setColour(Theme::stroke);g.fillRoundedRectangle(bar,2.5f);g.setColour(tint);if(fraction>0.f)g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*fraction),2.5f);
        if(phase==Listening||phase==More){
            Theme::fittedText(g,"DRY INPUT",{263,195,77,18},10.5f,Theme::muted,true);
            const juce::Rectangle<float> meter(342.f,202.f,89.f,4.f);g.setColour(Theme::stroke);g.fillRect(meter);
            g.setColour(tint);g.fillRect(meter.withWidth(meter.getWidth()*energy));
            Theme::fittedText(g,"VOCALS ONLY",{464,195,112,18},10.f,Theme::muted,true,juce::Justification::centredRight);
        }else Theme::fittedText(g,phase==Done?"RACK APPLIED / UNDO AVAILABLE":"LOCAL ANALYSIS / VOCALS ONLY",{263,195,313,18},10.5f,Theme::muted,true);
        if(active&&phase!=Refining&&phase!=Reading)Theme::fittedText(g,"LOCAL ANALYSIS  /  NOTHING UPLOADED",{263,270,313,17},9.5f,Theme::muted,true,juce::Justification::centred);
    }
private:
    // Shift the full-canvas opening point toward the visual workspace.
    // At 1168 x 688, opening moves +93px right and -28px up from canvas centre.
    static juce::Point<int> openingPosition(juce::Rectangle<int> available,int w,int h) noexcept {
        const auto safe=available.reduced(12);
        const auto c=available.getCentre();
        return {juce::jlimit(safe.getX(),juce::jmax(safe.getX(),safe.getRight()-w),c.x-w/2+juce::roundToInt(available.getWidth()*.08f)),
                juce::jlimit(safe.getY(),juce::jmax(safe.getY(),safe.getBottom()-h),c.y-h/2-juce::roundToInt(available.getHeight()*.04f))};
    }
    float layoutScale() const noexcept {return getHeight()/304.f;}
    juce::Rectangle<int> scaled(juce::Rectangle<int> b) const noexcept {return b.toFloat().transformedBy(juce::AffineTransform::scale(getWidth()/600.f,getHeight()/304.f)).toNearestInt();}
    void advanceAnimation(float seconds){
        const float dt=juce::jmax(0.f,seconds);
        rotation=std::fmod(rotation+dt*juce::MathConstants<float>::twoPi/18.f,juce::MathConstants<float>::twoPi);
        smoothedEnergy+=(energy-smoothedEnergy)*(1.f-std::exp(-dt*9.f));
        repaint(scaled({18,48,214,214}));
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
    void chooseStyle(int index){styleIndex=index;for(size_t i=0;i<styleButtons.size();++i)styleButtons[i].setToggleState((int)i==styleIndex,juce::dontSendNotification);repaint();}
    bool beatReady=false,refinementAvailable=false;
    juce::TextEditor prompt;
    SkellActionButton beat,refine,submit,back;
    int styleIndex=0;
    std::array<SkellActionButton,8> styleButtons;
    SkellActionButton start;
    Phase phase=Listening;juce::String message;float fraction=0,energy=0,rotation=0,smoothedEnergy=0;
    bool initialised=false,dryAvailable=false,moved=false,dragging=false;
    double lastTime=0.;juce::Point<int> dragOrigin,dragStart;juce::Rectangle<int> limits;
    const juce::AudioProcessorValueTreeState* themeSource=nullptr;
    SkellHubRadar radar;
    SkellMatchOrb orb;
    SkellActionButton close,retry,listen,reference,upload;
};
