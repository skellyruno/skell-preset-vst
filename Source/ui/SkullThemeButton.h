#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

// Clean Skell skull + miniature broadcast microphone. This local icon does
// not modify the shared master SkellCore logo or any other plugin assets.
// Existing mouse/keyboard theme cycling and original 56px hitbox remain.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton():juce::Button("Cycle Skell colour theme"){
        setTooltip("SKELL skull / Change Obsidian, Bone or Crimson theme");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id){variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool hovered,bool pressed)override {
        Theme::select(variant);
        const auto bounds=getLocalBounds().toFloat().reduced(.7f);
        g.setColour(Theme::bg.brighter(.14f));g.fillRoundedRectangle(bounds,8.f);
        g.setColour(Theme::green.withAlpha(hovered?.8f:.30f));
        g.drawRoundedRectangle(bounds,8.f,hovered?1.55f:1.05f);
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(
            (float)getWidth()/56.f,(float)getHeight()/56.f));
        const auto bone=Theme::white,shade=Theme::muted,ink=Theme::bg;
        const auto signal=Theme::green;

        // Unified, balanced cranial silhouette: symmetric sockets, short
        // maxilla and ONE connected jaw. No second mouth or elongated neck.
        juce::Path head;
        head.startNewSubPath(26.5f,8.f);
        head.cubicTo(15.f,8.f,10.5f,16.f,11.3f,26.f);
        head.cubicTo(11.5f,31.6f,14.2f,34.7f,18.6f,37.5f);
        head.lineTo(20.f,41.6f);head.lineTo(33.8f,41.6f);
        head.lineTo(35.2f,37.5f);
        head.cubicTo(39.7f,34.8f,42.f,31.f,42.1f,25.5f);
        head.cubicTo(42.f,16.f,37.f,8.f,26.5f,8.f);
        head.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            bone,15.f,12.f,shade.darker(.35f),38.f,43.f,false));
        g.fillPath(head);
        g.setColour(bone.withAlpha(.5f));
        g.strokePath(head,juce::PathStrokeType(.85f));

        // Real cavities, not angry angular green eyes.
        g.setColour(ink);
        juce::Path left,right;
        left.addEllipse(15.8f,22.5f,9.4f,10.8f);
        right.addEllipse(28.2f,22.5f,9.4f,10.8f);
        g.fillPath(left);g.fillPath(right);
        g.setColour(shade.darker(.35f).withAlpha(.55f));
        g.drawEllipse(16.4f,23.f,8.3f,9.4f,.85f);
        g.drawEllipse(28.9f,23.f,8.3f,9.4f,.85f);

        juce::Path nose;
        nose.startNewSubPath(26.7f,31.5f);
        nose.lineTo(23.7f,37.f);nose.quadraticTo(26.7f,38.6f,29.7f,37.f);
        nose.closeSubPath();
        g.setColour(ink);g.fillPath(nose);

        juce::Path jaw;
        jaw.startNewSubPath(19.3f,39.8f);
        jaw.lineTo(34.f,39.8f);
        jaw.lineTo(33.6f,47.2f);
        jaw.quadraticTo(26.7f,50.9f,19.8f,47.2f);
        jaw.closeSubPath();
        g.setColour(bone.darker(.10f));g.fillPath(jaw);
        g.setColour(ink.withAlpha(.90f));
        g.drawLine(20.f,41.4f,33.2f,41.4f,1.f);
        for(int i=1;i<=5;++i){
            const float x=19.8f+13.6f*i/6.f;
            g.drawLine(x,41.4f,x,48.f-1.2f*std::abs(x-26.7f)/7.f,.86f);
        }
        g.setColour(shade.withAlpha(.35f));
        g.drawLine(16.f,34.f,19.8f,38.5f,.8f);
        g.drawLine(37.5f,34.f,33.6f,38.5f,.8f);

        // Compact microphone accessory, ~32% smaller. The SKELL skull
        // itself is unchanged; only this local decoration is adjusted.
        g.setColour(ink);
        g.fillRoundedRectangle(43.9f,30.2f,6.2f,10.2f,3.f);
        g.setColour(signal);
        g.drawRoundedRectangle(44.4f,30.7f,5.2f,9.2f,2.6f,.95f);
        for(int k=0;k<3;++k){
            g.setColour(signal.withAlpha(.65f));
            g.drawLine(45.5f,33.f+1.65f*k,48.5f,33.f+1.65f*k,.65f);
        }
        g.setColour(signal);
        juce::Path mount;
        mount.startNewSubPath(42.8f,37.6f);
        mount.quadraticTo(42.8f,43.2f,47.f,43.2f);
        mount.quadraticTo(51.2f,43.2f,51.2f,37.6f);
        g.strokePath(mount,juce::PathStrokeType(.8f));
        g.drawLine(47.f,43.3f,47.f,47.2f,.9f);
        g.drawLine(44.2f,47.3f,49.8f,47.3f,.9f);

        if(pressed){
            g.setColour(ink.withAlpha(.2f));
            g.fillRoundedRectangle(4.5f,4.5f,47.f,47.f,6.f);
        }
    }
private:
    int variant=Theme::Obsidian;
};
