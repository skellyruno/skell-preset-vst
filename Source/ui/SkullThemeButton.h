#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Theme-cycling skull, fully drawn in native JUCE vectors; no signal monogram.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton():juce::Button("Cycle Skell colour theme") {
        setTooltip("Skell skull / Cycle Obsidian, Bone or Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics&g,bool hovered,bool pressed) override {
        Theme::select(variant);
        const auto bounds=getLocalBounds().toFloat().reduced(.7f);
        g.setColour(Theme::bg.brighter(.14f));g.fillRoundedRectangle(bounds,8.f);
        g.setColour(Theme::green.withAlpha(hovered?.84f:.35f));
        g.drawRoundedRectangle(bounds,8.f,hovered?1.6f:1.f);
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(
            (float)getWidth()/56.f,(float)getHeight()/56.f));
        juce::Path skull;
        skull.startNewSubPath(28.f,7.f);
        skull.cubicTo(15.f,7.f,9.f,16.f,10.f,25.f);
        skull.cubicTo(9.f,31.f,14.f,35.f,16.f,38.f);
        skull.lineTo(16.f,44.f);
        skull.quadraticTo(28.f,51.f,40.f,44.f);
        skull.lineTo(40.f,38.f);
        skull.cubicTo(43.f,35.f,47.f,31.f,46.f,25.f);
        skull.cubicTo(47.f,16.f,41.f,7.f,28.f,7.f);
        skull.closeSubPath();
        g.setColour(Theme::green.withAlpha(hovered?.24f:.10f));
        g.fillEllipse(8.f,8.f,40.f,41.f);
        g.setColour(Theme::white.withAlpha(pressed?.75f:.94f));g.fillPath(skull);
        g.setColour(Theme::bg);
        g.fillEllipse(15.f,24.f,11.f,10.f);
        g.fillEllipse(30.f,24.f,11.f,10.f);
        juce::Path nose;
        nose.startNewSubPath(28.f,33.f);
        nose.lineTo(23.f,39.f);nose.lineTo(33.f,39.f);nose.closeSubPath();
        g.fillPath(nose);
        g.fillRoundedRectangle(19.f,42.f,18.f,2.f,1.f);
        for(int i=0;i<4;++i)
            g.drawLine(22.f+i*4.f,43.f,22.f+i*4.f,48.f,.9f);
        g.setColour(Theme::green);
        g.drawLine(13.f,18.f,17.f,12.f,1.15f);
        g.fillEllipse(40.f,17.f,3.f,3.f);
    }
private:
    int variant=Theme::Obsidian;
};
