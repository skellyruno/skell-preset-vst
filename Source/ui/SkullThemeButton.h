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

        // Sculpted mini-skull, not a perfect oval + striped jaw. One
        // connected bone silhouette with temple indent, high cheekbones,
        // faceted jaw, deep anatomical cavities and individual teeth.
        // All graphics remain vector-based at every plugin UI scale.
        juce::Path skull;
        skull.startNewSubPath(26.7f,8.2f);
        skull.cubicTo(17.2f,7.2f,11.2f,13.8f,11.8f,23.5f);
        skull.cubicTo(11.2f,28.1f,13.7f,31.4f,17.4f,34.0f);
        skull.lineTo(18.7f,38.1f);
        skull.lineTo(19.7f,43.1f);
        skull.lineTo(22.1f,46.6f);
        skull.quadraticTo(26.7f,48.5f,31.3f,46.6f);
        skull.lineTo(33.7f,43.1f);
        skull.lineTo(34.7f,38.1f);
        skull.lineTo(36.1f,34.0f);
        skull.cubicTo(39.5f,31.3f,42.4f,27.5f,41.8f,23.2f);
        skull.cubicTo(42.1f,13.8f,36.2f,7.2f,26.7f,8.2f);
        skull.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            bone.brighter(.06f),15.7f,12.f,
            shade.darker(.22f),39.2f,43.9f,false));
        g.fillPath(skull);
        g.setColour(bone.withAlpha(.59f));
        g.strokePath(skull,juce::PathStrokeType(.8f));
        // Brow arches imply brow depth without angry cartoon eyebrows.
        juce::Path brows;
        brows.startNewSubPath(15.1f,22.1f);
        brows.quadraticTo(19.5f,17.9f,24.f,21.3f);
        brows.startNewSubPath(29.4f,21.3f);
        brows.quadraticTo(33.8f,17.9f,38.2f,22.1f);
        g.setColour(shade.darker(.15f).withAlpha(.75f));
        g.strokePath(brows,juce::PathStrokeType(1.3f));
        // Teardrop eye sockets follow the angled cheekbones instead of
        // circular emoji eyes.
        juce::Path sockets;
        sockets.startNewSubPath(16.1f,23.9f);
        sockets.cubicTo(17.7f,21.1f,22.6f,21.5f,24.1f,25.1f);
        sockets.cubicTo(24.9f,27.9f,22.f,31.2f,19.3f,31.3f);
        sockets.cubicTo(16.1f,31.4f,14.7f,27.4f,16.1f,23.9f);
        sockets.closeSubPath();
        sockets.startNewSubPath(29.3f,25.1f);
        sockets.cubicTo(30.8f,21.5f,35.7f,21.1f,37.3f,23.9f);
        sockets.cubicTo(38.7f,27.4f,37.3f,31.4f,34.1f,31.3f);
        sockets.cubicTo(31.4f,31.2f,28.5f,27.9f,29.3f,25.1f);
        sockets.closeSubPath();
        g.setColour(ink);g.fillPath(sockets);
        g.setColour(shade.withAlpha(.33f));
        g.strokePath(sockets,juce::PathStrokeType(.72f));
        // Narrow black nose, lower septum and carved cheek ridges.
        juce::Path nose;
        nose.startNewSubPath(26.7f,29.9f);
        nose.lineTo(23.5f,35.4f);
        nose.quadraticTo(24.2f,37.3f,26.7f,36.8f);
        nose.quadraticTo(29.2f,37.3f,29.9f,35.4f);
        nose.closeSubPath();
        g.setColour(ink);g.fillPath(nose);
        juce::Path cheeks;
        cheeks.startNewSubPath(13.8f,30.5f);
        cheeks.quadraticTo(18.1f,34.5f,20.9f,35.9f);
        cheeks.startNewSubPath(39.6f,30.5f);
        cheeks.quadraticTo(35.3f,34.5f,32.5f,35.9f);
        g.setColour(shade.darker(.12f).withAlpha(.56f));
        g.strokePath(cheeks,juce::PathStrokeType(.9f));
        // Teeth are separate short bone incisors inside one dark mouth.
        // No thin vertical piano-key bars running to the chin.
        g.setColour(ink.withAlpha(.94f));
        g.fillRoundedRectangle(20.7f,39.2f,12.0f,6.0f,1.0f);
        for(int i=0;i<6;++i) {
            const float x=21.1f+(float)i*1.9f;
            const float shortness=(i==0||i==5)?1.0f:0.f;
            g.setColour(bone.darker(.075f+shortness*.07f));
            g.fillRoundedRectangle(x,39.7f,1.45f,4.5f-shortness, .35f);
        }
        // Faceted jaw edge and nasal bridge highlights for tiny-size depth.
        g.setColour(shade.withAlpha(.53f));
        g.drawLine(22.2f,45.8f,26.7f,46.6f,.65f);
        g.drawLine(26.7f,46.6f,31.2f,45.8f,.65f);
        g.drawLine(25.3f,31.4f,24.6f,33.1f,.65f);
        g.drawLine(28.1f,31.4f,28.8f,33.1f,.65f);

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
