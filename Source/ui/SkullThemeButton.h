#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

// New original Skell Preset skull: dark machined bone with narrow neon eyes,
// angular hood and dual rack rails. Not the old bright bubble / patch-forehead
// skull, nor Skell Key's key-themed identity. Click still cycles host UI skins.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton() : juce::Button("Cycle Skell colour theme") {
        setTooltip("Change SKELL theme / Obsidian, Bone, Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        Theme::select(variant);
        const auto rect=getLocalBounds().toFloat().reduced(.7f);
        g.setColour(Theme::bg.brighter(.12f));
        g.fillRoundedRectangle(rect,8.f);
        g.setColour(Theme::green.withAlpha(over?.82f:.29f));
        g.drawRoundedRectangle(rect,8.f,over?1.55f:1.05f);
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::scale(
            (float)getWidth()/56.f,(float)getHeight()/56.f));
        const auto bone=Theme::white,accent=Theme::green,shadow=Theme::bg;
        // Deep hood with a pointed bridge; deliberately different silhouette.
        juce::Path hood;
        hood.startNewSubPath(7.f,42.f);
        hood.lineTo(9.f,20.f);hood.lineTo(15.f,9.f);
        hood.lineTo(28.f,4.5f);hood.lineTo(41.f,9.f);
        hood.lineTo(47.f,20.f);hood.lineTo(49.f,42.f);
        hood.lineTo(42.f,37.f);hood.lineTo(42.f,20.f);
        hood.lineTo(36.f,13.f);hood.lineTo(20.f,13.f);
        hood.lineTo(14.f,20.f);hood.lineTo(14.f,37.f);
        hood.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            shadow.brighter(.69f),7.f,5.f,shadow.darker(.12f),46.f,47.f,false));
        g.fillPath(hood);
        g.setColour(accent.withAlpha(over?.84f:.48f));
        g.strokePath(hood,juce::PathStrokeType(1.18f));
        // Sculpted angled cranium and deliberately separated lower jaw.
        juce::Path skull;
        skull.startNewSubPath(28.f,11.7f);
        skull.lineTo(39.2f,15.6f);
        skull.lineTo(42.3f,22.f);
        skull.lineTo(40.4f,32.f);
        skull.lineTo(36.1f,38.f);
        skull.lineTo(36.1f,42.f);
        skull.lineTo(19.9f,42.f);
        skull.lineTo(19.9f,38.f);
        skull.lineTo(15.6f,32.f);
        skull.lineTo(13.7f,22.f);
        skull.lineTo(16.8f,15.6f);
        skull.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            bone.darker(.27f),15.f,15.f,Theme::muted.darker(.56f),40.f,39.f,false));
        g.fillPath(skull);
        g.setColour(bone.withAlpha(.26f));
        g.strokePath(skull,juce::PathStrokeType(.8f));
        // Hollow triangular sockets, crisp rather than cartoon circles.
        juce::Path eyes;
        eyes.startNewSubPath(17.f,25.f);
        eyes.lineTo(26.2f,27.7f);eyes.lineTo(24.4f,32.f);
        eyes.lineTo(18.3f,30.f);eyes.closeSubPath();
        eyes.startNewSubPath(39.f,25.f);
        eyes.lineTo(29.8f,27.7f);eyes.lineTo(31.6f,32.f);
        eyes.lineTo(37.7f,30.f);eyes.closeSubPath();
        g.setColour(shadow);
        g.fillPath(eyes);
        // Tiny inset neon apertures, no cartoon pupils.
        g.setColour(accent.withAlpha(over?1.f:.84f));
        g.drawLine(19.1f,27.4f,23.7f,29.1f,1.65f);
        g.drawLine(36.9f,27.4f,32.3f,29.1f,1.65f);
        juce::Path nose;
        nose.addTriangle(28.f,32.f,24.6f,37.2f,31.4f,37.2f);
        g.setColour(shadow);
        g.fillPath(nose);
        g.setColour(bone.darker(.26f));
        g.fillRoundedRectangle(19.f,42.f,18.f,5.8f,1.5f);
        g.setColour(shadow);
        for(int i=0;i<5;++i)
            g.drawLine(21.f+3.5f*i,42.6f,21.f+3.5f*i,47.1f,.9f);
        // Rack identity lives OUTSIDE the skull, not stamped over its face.
        g.setColour(accent.withAlpha(.69f));
        for(int side=0;side<2;++side) {
            const float x=side==0?7.f:46.f;
            for(int k=0;k<3;++k)
                g.fillRoundedRectangle(x,23.f+5.6f*k,2.f,3.2f,.75f);
        }
        if(down) {
            g.setColour(shadow.withAlpha(.25f));
            g.fillRoundedRectangle(5.f,6.f,46.f,45.f,7.f);
        }
    }
private:
    int variant=Theme::Obsidian;
};
