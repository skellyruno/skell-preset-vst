#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

// Skell Preset skull v3: original minimalist split-jaw bone mark.
// No borrowed Skell Key silhouette, no hood, no rack rails, and no
// painted face patch. Geometry stays crisp at the existing 56px button size.
// Changing the symbol must never affect the theme-click accessibility.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton() : juce::Button("Cycle Skell colour theme") {
        setTooltip("Change SKELL theme / Obsidian, Bone, Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool hovered,bool pressed) override {
        Theme::select(variant);
        const auto frame=getLocalBounds().toFloat().reduced(.75f);
        g.setColour(Theme::bg.brighter(.13f));
        g.fillRoundedRectangle(frame,8.f);
        g.setColour(Theme::green.withAlpha(hovered?.76f:.27f));
        g.drawRoundedRectangle(frame,8.f,hovered?1.6f:1.0f);

        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::scale(
            (float)getWidth()/56.f,(float)getHeight()/56.f));
        const auto bone=Theme::white,shade=Theme::muted,ink=Theme::bg,accent=Theme::green;

        // A heavier, slightly asymmetrical cranial outline — unmistakably
        // a skull even at small size, but not cartoon-like or hooded.
        juce::Path cranium;
        cranium.startNewSubPath(28.f,7.7f);
        cranium.cubicTo(12.f,7.7f,9.2f,15.f,10.3f,25.f);
        cranium.cubicTo(10.6f,30.8f,13.7f,34.f,17.7f,37.5f);
        cranium.lineTo(18.7f,41.5f);
        cranium.lineTo(37.3f,41.5f);
        cranium.lineTo(38.3f,37.5f);
        cranium.cubicTo(43.1f,34.3f,45.3f,29.3f,45.f,24.f);
        cranium.cubicTo(44.4f,13.1f,39.4f,7.7f,28.f,7.7f);
        cranium.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            bone.darker(.05f),16.f,10.f,shade.darker(.56f),40.f,42.f,false));
        g.fillPath(cranium);
        g.setColour(bone.withAlpha(.5f));
        g.strokePath(cranium,juce::PathStrokeType(1.f));

        // High-contrast sockets, hollow rather than angry neon slits.
        juce::Path leftEye;
        leftEye.startNewSubPath(15.f,23.f);
        leftEye.quadraticTo(18.f,19.2f,25.f,23.9f);
        leftEye.quadraticTo(24.f,31.8f,20.2f,32.7f);
        leftEye.quadraticTo(15.4f,32.1f,15.f,23.f);
        leftEye.closeSubPath();
        juce::Path rightEye;
        rightEye.startNewSubPath(41.f,23.5f);
        rightEye.quadraticTo(37.2f,19.8f,31.4f,24.f);
        rightEye.quadraticTo(31.f,31.4f,34.8f,32.6f);
        rightEye.quadraticTo(40.8f,32.1f,41.f,23.5f);
        rightEye.closeSubPath();
        g.setColour(ink);
        g.fillPath(leftEye);g.fillPath(rightEye);

        // A slender fracture through the crown is Skell Preset's distinct
        // mark. Accent stays subtle except when the button is hovered.
        g.setColour(accent.withAlpha(hovered?.95f:.57f));
        g.drawLine(30.5f,9.5f,27.f,16.f,1.65f);
        g.drawLine(27.f,16.f,30.3f,20.f,1.65f);
        g.drawLine(30.3f,20.f,27.8f,24.1f,1.25f);
        juce::Path nasal;
        nasal.addTriangle(28.f,30.7f,24.8f,37.1f,31.1f,37.1f);
        g.setColour(ink);g.fillPath(nasal);

        // Separately carved jaw with five clear teeth. An actual gap
        // above the jaw prevents the old blocky/rounded mascot appearance.
        g.setColour(ink);
        g.fillRect(18,40,20,2);
        g.setColour(bone.darker(.13f));
        g.fillRoundedRectangle(19.f,42.5f,18.f,6.7f,1.35f);
        g.setColour(ink.withAlpha(.96f));
        for(int i=1;i<6;++i) {
            const float x=19.f+3.f*i;
            g.drawLine(x,43.1f,x,48.8f,.95f);
        }
        g.setColour(bone.withAlpha(.7f));
        g.drawLine(19.2f,43.f,36.8f,43.f,.8f);

        if(pressed) {
            g.setColour(ink.withAlpha(.22f));
            g.fillRoundedRectangle(5.f,5.f,46.f,46.f,6.f);
        }
    }
private:
    int variant=Theme::Obsidian;
};
