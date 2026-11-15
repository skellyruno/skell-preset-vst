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

        // A shorter, cleaner Skell skull: narrower jaw, pronounced
        // cheekbones, aligned teeth, and properly recessed eye sockets.
        // Keep the approved miniature microphone accessory unchanged.
        juce::Path cranium;
        cranium.startNewSubPath(26.8f,8.1f);
        cranium.cubicTo(15.9f,7.5f,11.7f,13.7f,11.7f,23.8f);
        cranium.cubicTo(11.5f,28.3f,12.9f,31.7f,16.8f,35.3f);
        cranium.lineTo(19.9f,37.9f);
        cranium.lineTo(33.8f,37.9f);
        cranium.lineTo(36.9f,35.3f);
        cranium.cubicTo(41.1f,31.3f,42.1f,28.1f,41.8f,23.4f);
        cranium.cubicTo(41.6f,14.2f,37.2f,8.1f,26.8f,8.1f);
        cranium.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            bone.brighter(.02f),17.f,14.f,shade.darker(.22f),38.f,39.f,false));
        g.fillPath(cranium);
        g.setColour(bone.withAlpha(.58f));
        g.strokePath(cranium,juce::PathStrokeType(.85f));
        // High cheekbone engraving gives the tiny silhouette depth.
        g.setColour(shade.withAlpha(.52f));
        juce::Path cheek;
        cheek.startNewSubPath(13.5f,30.f);
        cheek.quadraticTo(18.9f,36.6f,20.3f,37.2f);
        cheek.startNewSubPath(40.f,30.f);
        cheek.quadraticTo(34.8f,36.6f,33.2f,37.2f);
        g.strokePath(cheek,juce::PathStrokeType(1.1f));
        // Inset sockets taper toward the nose rather than two blank circles.
        juce::Path sockets;
        sockets.startNewSubPath(15.7f,24.f);
        sockets.cubicTo(16.3f,20.f,23.2f,19.9f,24.3f,24.5f);
        sockets.cubicTo(25.f,28.6f,21.7f,31.8f,18.4f,31.4f);
        sockets.cubicTo(15.5f,31.f,14.8f,27.9f,15.7f,24.f);
        sockets.closeSubPath();
        sockets.startNewSubPath(29.2f,24.5f);
        sockets.cubicTo(30.4f,19.9f,37.1f,20.f,37.8f,24.f);
        sockets.cubicTo(38.7f,27.9f,38.f,31.f,35.1f,31.4f);
        sockets.cubicTo(31.8f,31.8f,28.5f,28.6f,29.2f,24.5f);
        sockets.closeSubPath();
        g.setColour(ink);g.fillPath(sockets);
        g.setColour(shade.darker(.28f).withAlpha(.55f));
        g.strokePath(sockets,juce::PathStrokeType(.72f));
        juce::Path nasal;
        nasal.startNewSubPath(26.7f,30.f);
        nasal.lineTo(23.6f,35.4f);
        nasal.quadraticTo(26.7f,37.8f,29.9f,35.4f);
        nasal.closeSubPath();
        g.setColour(ink);g.fillPath(nasal);
        // A single shallow maxilla and ONE compact jaw, no long neck.
        juce::Path jaw;
        jaw.startNewSubPath(19.9f,38.f);
        jaw.lineTo(33.6f,38.f);
        jaw.lineTo(33.0f,45.7f);
        jaw.quadraticTo(26.7f,48.4f,20.5f,45.7f);
        jaw.closeSubPath();
        g.setColour(bone.darker(.075f));g.fillPath(jaw);
        g.setColour(shade.withAlpha(.72f));
        g.drawLine(20.f,39.9f,33.5f,39.9f,.85f);
        g.setColour(ink.withAlpha(.9f));
        for(int i=1;i<=5;++i) {
            const float tx=20.1f+13.2f*(float)i/6.f;
            g.drawLine(tx,40.f,tx,46.8f-1.2f*std::abs(tx-26.7f)/7.f,.85f);
        }
        g.setColour(shade.withAlpha(.47f));
        g.drawLine(20.f,45.9f,33.2f,45.9f,.75f);

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
