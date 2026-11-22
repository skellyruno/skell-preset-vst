#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Studio-grade SKELL signal monogram and original microphone accessory.
// Keeps the established theme-toggle button, hitbox and rack microphone.
// Deliberately NO skull, facial parts or bone artwork.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton():juce::Button("Cycle Skell colour theme") {
        setTooltip("SKELL signal mark / Cycle Obsidian, Bone or Crimson theme");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool hovered,bool pressed) override {
        Theme::select(variant);
        auto bounds=getLocalBounds().toFloat().reduced(.7f);
        g.setColour(Theme::bg.brighter(.14f));g.fillRoundedRectangle(bounds,8.f);
        g.setColour(Theme::green.withAlpha(hovered?.8f:.30f));
        g.drawRoundedRectangle(bounds,8.f,hovered?1.55f:1.05f);
        juce::Graphics::ScopedSaveState scoped(g);
        g.addTransform(juce::AffineTransform::scale(
            (float)getWidth()/56.f,(float)getHeight()/56.f));
        // A compact audio-signal / S mark instead of a pictorial mascot.
        juce::Path signalMark;
        signalMark.startNewSubPath(35.f,14.f);
        signalMark.cubicTo(28.f,8.f,17.f,12.f,15.f,19.f);
        signalMark.cubicTo(13.f,24.f,18.f,28.f,27.f,28.f);
        signalMark.cubicTo(37.f,28.f,39.f,36.f,34.f,41.f);
        signalMark.cubicTo(28.f,47.f,18.f,45.f,14.f,40.f);
        g.setColour(Theme::white);
        g.strokePath(signalMark,juce::PathStrokeType(4.2f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // A single green waveform cut through the letterform.
        juce::Path wave;
        wave.startNewSubPath(10.f,32.f);
        wave.lineTo(17.f,32.f);wave.lineTo(20.f,27.f);
        wave.lineTo(24.f,37.f);wave.lineTo(29.f,22.f);
        wave.lineTo(33.f,32.f);wave.lineTo(39.f,32.f);
        g.setColour(Theme::green);
        g.strokePath(wave,juce::PathStrokeType(1.65f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // Preserve the previous small broadcast microphone on the right.
        const auto ink=Theme::bg;
        g.setColour(ink);
        g.fillRoundedRectangle(43.9f,30.2f,6.2f,10.2f,3.f);
        g.setColour(Theme::green);
        g.drawRoundedRectangle(44.4f,30.7f,5.2f,9.2f,2.6f,.95f);
        for(int k=0;k<3;++k) {
            g.setColour(Theme::green.withAlpha(.65f));
            g.drawLine(45.5f,33.f+1.65f*k,48.5f,33.f+1.65f*k,.65f);
        }
        g.setColour(Theme::green);
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
