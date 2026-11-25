#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Two-state workspace selector for the header. Renders as one machined plate
// split into two equal segments with the active segment lit, so the current
// workspace is readable at a glance instead of being hidden in a single
// generic toggle caption. Clicks on either half select that mode directly.
//
// This is presentation only: the editor still writes the existing "mode"
// APVTS parameter, so automation, drag and host state recall are unchanged.
class SkellModeSwitch final : public juce::Component,
                              public juce::SettableTooltipClient {
public:
    SkellModeSwitch() {
        setInterceptsMouseClicks(true,false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    // 0 = Simple workspace, 1 = Advanced workspace. Host automation can move
    // this at any time, so it is a plain setter, never an internal state.
    void setAdvanced(bool advanced, bool notify) {
        if(advancedState==advanced && !notify) return;
        const bool changed=advancedState!=advanced;
        advancedState=advanced;
        if(changed && notify && onModeChange) onModeChange(advanced?1:0);
        if(changed) repaint();
    }
    bool isAdvanced() const noexcept {return advancedState;}
    std::function<void(int)> onModeChange;

    void paint(juce::Graphics& g) override {
        auto bounds=getLocalBounds().toFloat().reduced(.65f);
        if(bounds.isEmpty())return;
        const auto face=Theme::panelAlt;
        // Shared plate so the two segments read as one machined part.
        g.setGradientFill(juce::ColourGradient(
            face.brighter(.13f),bounds.getTopLeft(),
            face.darker(.20f),bounds.getBottomLeft(),false));
        g.fillRoundedRectangle(bounds,5.6f);
        g.setColour(Theme::stroke.withAlpha(.78f));
        g.drawRoundedRectangle(bounds.reduced(.5f),5.6f,1.05f);
        g.setColour(Theme::white.withAlpha(.10f));
        g.drawLine(bounds.getX()+7.f,bounds.getY()+2.f,
                   bounds.getRight()-7.f,bounds.getY()+2.f,1.f);
        g.setColour(Theme::bg.withAlpha(.55f));
        g.drawLine(bounds.getX()+8.f,bounds.getBottom()-2.f,
                   bounds.getRight()-8.f,bounds.getBottom()-2.f,1.f);

        const float mid=bounds.getCentreX();
        const auto simpleRect=bounds.withRight(mid-1.f).reduced(1.6f);
        const auto advancedRect=bounds.withLeft(mid+1.f).reduced(1.6f);
        auto segment=[&](juce::Rectangle<float> r,bool lit,bool hovered) {
            if(lit) {
                g.setGradientFill(juce::ColourGradient(
                    Theme::green.withAlpha(.16f),r.getTopLeft(),
                    Theme::green.withAlpha(.05f),r.getBottomLeft(),false));
                g.fillRoundedRectangle(r,4.4f);
                g.setColour(Theme::green.withAlpha(.72f));
                g.drawRoundedRectangle(r.reduced(.5f),4.4f,1.05f);
            } else if(hovered) {
                g.setColour(Theme::white.withAlpha(.055f));
                g.fillRoundedRectangle(r,4.4f);
            }
        };
        segment(simpleRect,!advancedState,hoverSegment==0);
        segment(advancedRect,advancedState,hoverSegment==1);
        // Divider between the halves.
        g.setColour(Theme::bg.withAlpha(.72f));
        g.drawVerticalLine((int)mid,bounds.getY()+5.f,bounds.getBottom()-5.f);

        auto caption=[&](juce::Rectangle<float> r,const juce::String& label,bool lit) {
            g.setColour(lit?Theme::green:Theme::muted.withAlpha(.86f));
            g.setFont(Theme::font(11.4f,true));
            g.drawFittedText(label,r.toNearestInt(),juce::Justification::centred,1,.80f);
        };
        caption(simpleRect,"SIMPLE",!advancedState);
        caption(advancedRect,"ADVANCED",advancedState);

        // Mode lamp at the far left, matching the header status dots.
        const float lampX=bounds.getX()+9.f,lampY=bounds.getCentreY();
        g.setColour((advancedState?Theme::green:Theme::blue).withAlpha(.20f));
        g.fillEllipse(lampX-4.4f,lampY-4.4f,8.8f,8.8f);
        g.setColour(advancedState?Theme::green:Theme::blue);
        g.fillEllipse(lampX-1.9f,lampY-1.9f,3.8f,3.8f);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const int segmentIndex=e.position.x>getWidth()*.5f?1:0;
        if(segmentIndex!=hoverSegment){hoverSegment=segmentIndex;repaint();}
    }
    void mouseExit(const juce::MouseEvent&) override {
        if(hoverSegment>=0){hoverSegment=-1;repaint();}
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(!isEnabled())return;
        const bool wantAdvanced=e.position.x>getWidth()*.5f;
        if(wantAdvanced!=advancedState) setAdvanced(wantAdvanced,true);
    }
private:
    bool advancedState=false;
    int hoverSegment=-1;
};
