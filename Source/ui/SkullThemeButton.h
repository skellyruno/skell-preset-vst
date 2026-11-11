#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Official Skell-Preset vector mark. Small-size legibility comes first:
// clean cranium, two deep sockets, simple nasal cavity and a four-tooth jaw.
// This is drawn from paths, not a raster screenshot; click opens the skin menu.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton() : juce::Button("Change Skell colour theme") {
        setTooltip("SKELL themes  /  Obsidian, Cream, Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) { variant=Theme::clampVariant(id); repaint(); }

    void paintButton(juce::Graphics& g,bool hovered,bool down) override {
        Theme::select(variant);
        const auto bounds=getLocalBounds().toFloat().reduced(1.f);
        if(hovered || down) {
            g.setColour(Theme::green.withAlpha(down?.16f:.08f));
            g.fillRoundedRectangle(bounds,10.f);
            g.setColour(Theme::green.withAlpha(.7f));
            g.drawRoundedRectangle(bounds,10.f,1.f);
        }
        juce::Graphics::ScopedSaveState guard(g);
        g.addTransform(juce::AffineTransform::scale(bounds.getWidth()/56.f,bounds.getHeight()/56.f)
                           .translated(bounds.getX(),bounds.getY()));
        // A restrained floating halo anchors the mark without heavy wings.
        g.setColour(Theme::green.withAlpha(.15f));
        g.fillEllipse(5.5f,4.f,45.f,46.f);
        g.setColour(Theme::green.withAlpha(.55f));
        g.drawEllipse(5.5f,4.f,45.f,46.f,1.1f);

        juce::Path skull;
        skull.startNewSubPath(28.f,7.f);
        skull.cubicTo(16.f,7.f,11.f,15.f,11.f,26.f);
        skull.cubicTo(11.f,35.f,16.f,38.f,19.f,39.f);
        skull.lineTo(19.f,46.f);
        skull.quadraticTo(19.f,49.f,23.f,49.f);
        skull.lineTo(33.f,49.f);
        skull.quadraticTo(37.f,49.f,37.f,46.f);
        skull.lineTo(37.f,39.f);
        skull.cubicTo(42.f,36.f,45.f,32.f,45.f,26.f);
        skull.cubicTo(45.f,15.f,40.f,7.f,28.f,7.f);
        skull.closeSubPath();
        g.setColour(Theme::green);
        g.fillPath(skull);

        // Cut-outs remain strong at a 70%-scaled plugin window.
        g.setColour(Theme::plot);
        juce::Path leftEye,rightEye;
        leftEye.startNewSubPath(17.f,26.f);
        leftEye.quadraticTo(18.f,20.f,23.f,23.f);
        leftEye.quadraticTo(26.f,25.f,25.f,31.f);
        leftEye.quadraticTo(20.f,34.f,17.f,29.f);
        leftEye.closeSubPath();
        rightEye.startNewSubPath(39.f,26.f);
        rightEye.quadraticTo(38.f,20.f,33.f,23.f);
        rightEye.quadraticTo(30.f,25.f,31.f,31.f);
        rightEye.quadraticTo(36.f,34.f,39.f,29.f);
        rightEye.closeSubPath();
        g.fillPath(leftEye); g.fillPath(rightEye);
        juce::Path nose;
        nose.startNewSubPath(28.f,32.f);nose.lineTo(24.f,38.f);
        nose.quadraticTo(28.f,40.f,32.f,38.f);
        nose.closeSubPath();g.fillPath(nose);
        for(int tooth=0;tooth<4;++tooth)
            g.fillRoundedRectangle(22.f+tooth*3.25f,42.f,1.7f,5.1f,.5f);

        // Original angular highlight gives the otherwise minimal symbol a signature.
        g.setColour(Theme::white.withAlpha(.30f));
        juce::Path sheen;sheen.startNewSubPath(17.f,18.f);
        sheen.quadraticTo(19.f,13.f,24.f,12.f);
        g.strokePath(sheen,juce::PathStrokeType(1.65f));
    }
private:
    int variant=Theme::Obsidian;
};
