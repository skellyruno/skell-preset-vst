#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

// Skell Preset skull v4: stronger cheekbones, defined nose and one
// continuous jaw, inspired by the shared Skell bone silhouette. No key,
// forehead fracture, decorative patch or second mouth. Vector is crisp
// at the existing 56px button size without touching other plugin UIs.
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
        cranium.startNewSubPath(28.f,8.2f);
        cranium.cubicTo(17.f,7.6f,10.7f,15.7f,11.f,25.2f);
        cranium.cubicTo(11.3f,31.8f,13.1f,34.2f,18.2f,37.9f);
        cranium.lineTo(19.4f,42.4f);
        cranium.lineTo(36.6f,42.4f);
        cranium.lineTo(37.9f,37.9f);
        cranium.cubicTo(42.9f,34.2f,44.9f,30.8f,44.9f,24.9f);
        cranium.cubicTo(45.f,14.5f,39.3f,8.1f,28.f,8.2f);
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

        // Pronounced cheekbones form the organic bridge from the cranial
        // sockets to a single jaw instead of a second stylised mouth.
        g.setColour(shade.darker(.34f).withAlpha(.83f));
        juce::Path leftCheek;
        leftCheek.startNewSubPath(12.9f,32.f);
        leftCheek.quadraticTo(17.3f,35.8f,24.f,36.5f);
        leftCheek.lineTo(20.4f,40.3f);
        leftCheek.quadraticTo(13.9f,38.9f,12.9f,32.f);
        leftCheek.closeSubPath();
        juce::Path rightCheek;
        rightCheek.startNewSubPath(43.1f,32.f);
        rightCheek.quadraticTo(38.7f,35.8f,32.f,36.5f);
        rightCheek.lineTo(35.6f,40.3f);
        rightCheek.quadraticTo(42.1f,38.9f,43.1f,32.f);
        rightCheek.closeSubPath();
        g.fillPath(leftCheek);g.fillPath(rightCheek);
        juce::Path nasal;
        nasal.startNewSubPath(28.f,31.f);
        nasal.lineTo(24.9f,36.6f);
        nasal.quadraticTo(26.2f,38.1f,28.f,37.2f);
        nasal.quadraticTo(29.8f,38.1f,31.1f,36.6f);
        nasal.closeSubPath();
        g.setColour(ink);g.fillPath(nasal);

        // One continuous lower jaw and six carved teeth; not a separate
        // duplicate mouth. Theme tint sits on the cheek contours only.
        g.setColour(bone.darker(.14f));
        juce::Path jaw;
        jaw.startNewSubPath(19.5f,40.5f);
        jaw.lineTo(36.5f,40.5f);
        jaw.lineTo(36.f,47.4f);
        jaw.quadraticTo(28.f,52.5f,20.f,47.4f);
        jaw.closeSubPath();
        g.fillPath(jaw);
        g.setColour(ink.withAlpha(.95f));
        g.drawLine(20.f,42.2f,36.f,42.2f,1.f);
        for(int i=1;i<=5;++i) {
            const float x=20.f+16.f*(float)i/6.f;
            g.drawLine(x,42.4f,x,49.f-1.4f*std::abs(x-28.f)/8.f,.9f);
        }
        g.setColour(accent.withAlpha(hovered?.65f:.16f));
        g.drawLine(13.f,33.f,18.f,37.4f,1.f);
        g.drawLine(43.f,33.f,38.f,37.4f,1.f);

        if(pressed) {
            g.setColour(ink.withAlpha(.22f));
            g.fillRoundedRectangle(5.f,5.f,46.f,46.f,6.f);
        }
    }
private:
    int variant=Theme::Obsidian;
};
