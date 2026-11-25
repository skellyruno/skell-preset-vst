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
        setWantsKeyboardFocus(true);
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

        const auto label=advancedState?"ADVANCED":"SIMPLE";
        const auto next=advancedState?"GO SIMPLE":"GO ADVANCED";
        const auto track=bounds.removeFromRight(50.f).reduced(6.f,8.f);
        g.setColour(Theme::bg);g.fillRoundedRectangle(track,track.getHeight()*.5f);
        g.setColour(Theme::green.withAlpha(advancedState?.85f:.55f));
        const float d=track.getHeight()-4.f;
        g.fillEllipse((advancedState?track.getRight()-d-2.f:track.getX()+2.f),track.getY()+2.f,d,d);
        g.setColour(Theme::green);
        g.setFont(Theme::font(12.8f,true));
        g.drawFittedText(label,bounds.removeFromTop(21.f).reduced(9.f,0.f).toNearestInt(),
            juce::Justification::centredLeft,1);
        g.setColour(Theme::muted);g.setFont(Theme::font(9.f,true));
        g.drawFittedText(next,bounds.reduced(9.f,0.f).toNearestInt(),juce::Justification::centredLeft,1);
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
        (void)e;
        setAdvanced(!advancedState,true);
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(isEnabled() && (key==juce::KeyPress::returnKey || key==juce::KeyPress::spaceKey)) {
            setAdvanced(!advancedState,true);return true;
        }
        return false;
    }
private:
    bool advancedState=false;
    int hoverSegment=-1;
};
