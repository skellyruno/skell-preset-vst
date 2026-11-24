#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Native, resolution-independent mic performer: a skeletal vocalist holding
// a studio microphone. This is the *theme* toggle; the rack MIC control still
// opens the input modeling panel. No external images or DSP-thread artwork.
class SkullThemeButton final : public juce::Button, private juce::Timer {
public:
    SkullThemeButton():juce::Button("Cycle Skell colour theme") {
        setTooltip("Skeleton vocalist / switch Obsidian and Crimson themes");
        setWantsKeyboardFocus(true);
        startTimerHz(16);
    }
    ~SkullThemeButton() override {stopTimer();}
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool hovered,bool pressed) override {
        Theme::select(variant);
        const auto r=getLocalBounds().toFloat().reduced(.75f);
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(hovered?.23f:.12f),r.getTopLeft(),
            Theme::bg,r.getBottomRight(),false));
        g.fillRoundedRectangle(r,7.f);
        g.setColour(Theme::stroke.withAlpha(hovered?.92f:.6f));
        g.drawRoundedRectangle(r,7.f,1.f);
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.addTransform(juce::AffineTransform::scale(
                (float)getWidth()/64.f,(float)getHeight()/64.f));
            // A brief native glitch offset on hover. No full editor repaint,
            // no new DSP work, and no animation when hidden or untouched.
            if(hovered){
                const float shift=std::sin(phase)*.8f;
                g.addTransform(juce::AffineTransform::translation(shift,0.f));
                g.setColour(Theme::green.withAlpha(.30f));
                for(int i=0;i<3;++i){
                    const float y=13.f+17.f*i+std::sin(phase+float(i))*3.f;
                    g.drawLine(5.f,y,12.f+2.f*i,y,.9f);
                }
            }

            // Off-centre silhouette: light falls from upper left while the
            // opposite side stays in the dark, not a flat cartoon icon.
            juce::Path skull;
            skull.startNewSubPath(28.f,8.f);
            skull.cubicTo(16.f,8.f,14.f,17.f,15.f,24.f);
            skull.cubicTo(16.f,29.f,19.f,32.f,22.f,33.f);
            skull.lineTo(23.f,37.f);
            skull.quadraticTo(29.f,40.f,37.f,37.f);
            skull.lineTo(39.f,32.f);
            skull.cubicTo(43.f,28.f,44.f,23.f,42.f,17.f);
            skull.cubicTo(40.f,11.f,35.f,8.f,28.f,8.f);
            skull.closeSubPath();
            g.setColour(Theme::bg.darker(.3f));
            g.fillPath(skull);
            g.setGradientFill(juce::ColourGradient(
                Theme::white.withAlpha(.95f),18.f,11.f,
                Theme::muted.withAlpha(.64f),42.f,33.f,false));
            g.fillPath(skull);
            g.setColour(Theme::white.withAlpha(.30f));
            g.strokePath(skull,juce::PathStrokeType(.75f));

            // Anatomical orbital sockets and nose are angular paths; no
            // cartoon black dots or concentric ellipses.
            g.setColour(Theme::bg);
            juce::Path eyes;
            eyes.startNewSubPath(19.f,23.f);
            eyes.lineTo(28.f,23.f);eyes.lineTo(27.f,28.f);
            eyes.quadraticTo(22.f,30.f,19.f,26.f);eyes.closeSubPath();
            eyes.startNewSubPath(32.f,23.f);
            eyes.lineTo(41.f,22.f);eyes.lineTo(39.f,27.f);
            eyes.quadraticTo(35.f,30.f,32.f,27.f);eyes.closeSubPath();
            eyes.startNewSubPath(30.f,28.f);
            eyes.lineTo(27.f,34.f);eyes.lineTo(34.f,34.f);eyes.closeSubPath();
            g.fillPath(eyes);
            g.fillRoundedRectangle(24.f,36.f,13.f,1.5f,.6f);
            for(int k=0;k<4;++k)
                g.drawLine(25.f+3.5f*k,36.f,25.f+3.5f*k,39.f,.8f);

            // Clavicles, sternum, paired ribs and a tilted performer stance.
            auto line=[&](float a,float b,float c,float d,float thickness,
                          juce::Colour ink) {
                g.setColour(ink);g.drawLine(a,b,c,d,thickness);
            };
            const auto ivory=Theme::white.withAlpha(.8f);
            line(30.f,40.f,30.f,59.f,2.5f,ivory);
            line(30.f,43.f,18.f,46.f,2.f,ivory);
            line(30.f,43.f,42.f,46.f,2.f,ivory);
            for(int i=0;i<3;++i){
                const float y=46.f+4.f*i;
                juce::Path rib;
                rib.startNewSubPath(30.f,y);
                rib.quadraticTo(19.f,y-5.f,20.f,y+3.f);
                rib.startNewSubPath(30.f,y);
                rib.quadraticTo(43.f,y-5.f,40.f,y+3.f);
                g.setColour(ivory.withAlpha(.9f-i*.12f));
                g.strokePath(rib,juce::PathStrokeType(1.35f));
            }
            line(20.f,46.f,16.f,58.f,2.0f,ivory);
            // The raised right arm visibly GRIPS the microphone handle.
            line(42.f,46.f,47.f,40.f,2.3f,ivory);
            line(47.f,40.f,49.f,31.f,2.1f,ivory);
            g.setColour(Theme::green.withAlpha(.9f));
            g.fillRoundedRectangle(46.6f,15.5f,7.5f,15.3f,3.f);
            g.setColour(Theme::bg);
            for(int k=0;k<4;++k)
                g.drawLine(48.f,18.f+k*2.f,53.f,18.f+k*2.f,.75f);
            g.setColour(Theme::white.withAlpha(.86f));
            g.drawRoundedRectangle(46.6f,15.5f,7.5f,15.3f,3.f,1.f);
            line(50.5f,30.f,50.5f,39.f,2.1f,Theme::muted);
            g.setColour(ivory);
            g.fillRoundedRectangle(47.0f,31.1f,5.4f,3.6f,1.2f);
            line(49.6f,35.f,47.f,37.f,1.7f,ivory);
            // Diagonal sound strokes, restrained stage glint.
            g.setColour(Theme::green.withAlpha(hovered?.85f:.46f));
            g.drawLine(56.f,17.f,61.f,14.f,1.3f);
            g.drawLine(56.f,22.f,62.f,22.f,1.f);
            g.drawLine(56.f,27.f,60.f,30.f,1.f);
        }
        if(hasKeyboardFocus(true)) {
            g.setColour(Theme::green.withAlpha(.9f));
            g.drawRoundedRectangle(r.reduced(2.f),5.f,1.5f);
        }
        if(pressed){
            g.setColour(Theme::bg.withAlpha(.13f));g.fillRoundedRectangle(r,7.f);
        }
    }
private:
    void timerCallback() override {
        if(isShowing() && isMouseOver(true)){
            phase+=.45f;
            if(phase>juce::MathConstants<float>::twoPi)
                phase-=juce::MathConstants<float>::twoPi;
            repaint();
        }
    }
    int variant=Theme::Obsidian;
    float phase=0.f;
};
