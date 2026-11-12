#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Distinct angular SKELL skull, drawn in one stable coordinate system for all skins.
// Thin neon outlines replace the old solid cartoon skull; the button cycles themes.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton():juce::Button("Cycle Skell colour theme") {
        setTooltip("Change theme: Midnight / Cream / Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id){variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool hovered,bool down) override {
        Theme::select(variant);
        auto outer=getLocalBounds().toFloat().reduced(1.25f);
        g.setColour(Theme::panelAlt);g.fillRoundedRectangle(outer,7.f);
        g.setColour(Theme::green.withAlpha(hovered?1.f:.64f));
        g.drawRoundedRectangle(outer.reduced(.5f),7.f,down?2.f:1.3f);
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::scale(outer.getWidth()/56.f,outer.getHeight()/56.f)
                          .translated(outer.getX(),outer.getY()));
        juce::Path contour;
        contour.startNewSubPath(28.f,7.f);
        contour.cubicTo(18.f,7.f,11.f,14.f,11.f,25.f);
        contour.lineTo(13.f,33.f);
        contour.lineTo(18.f,38.f);
        contour.lineTo(20.f,47.f);
        contour.quadraticTo(28.f,51.f,36.f,47.f);
        contour.lineTo(38.f,38.f);
        contour.lineTo(43.f,33.f);
        contour.lineTo(45.f,25.f);
        contour.cubicTo(45.f,14.f,38.f,7.f,28.f,7.f);
        contour.closeSubPath();
        g.setColour(Theme::bg);g.fillPath(contour);
        g.setColour(Theme::green);g.strokePath(contour,juce::PathStrokeType(2.5f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // Swept angular orbital sockets give the mask a clear brand silhouette.
        juce::Path eyeL,eyeR;
        eyeL.startNewSubPath(16.f,26.f);eyeL.lineTo(24.5f,25.f);
        eyeL.lineTo(25.f,33.f);eyeL.lineTo(19.f,32.f);eyeL.closeSubPath();
        eyeR.startNewSubPath(40.f,26.f);eyeR.lineTo(31.5f,25.f);
        eyeR.lineTo(31.f,33.f);eyeR.lineTo(37.f,32.f);eyeR.closeSubPath();
        g.fillPath(eyeL);g.fillPath(eyeR);
        juce::Path nose;nose.startNewSubPath(28.f,33.f);nose.lineTo(24.5f,39.f);
        nose.lineTo(31.5f,39.f);nose.closeSubPath();g.fillPath(nose);
        g.drawLine(20.5f,43.f,35.5f,43.f,2.f);
        for(int t=0;t<3;++t)g.drawLine(24.f+t*4.f,43.5f,24.f+t*4.f,47.f,1.4f);
    }
private:
    int variant=Theme::Obsidian;
};
