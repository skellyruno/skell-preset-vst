#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Small original vector-drawn winged skull; doubles as a keyboard-accessible
// theme selector. No bitmap scaling/blur or UI timer is involved.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton():juce::Button("Choose colour theme") {setTooltip("Change colour theme: Obsidian, Cream or Crimson");}
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        Theme::select(variant);
        auto b=getLocalBounds().toFloat().reduced(1.f);
        g.setColour(over?Theme::green.withAlpha(.11f):Theme::panelAlt.withAlpha(.22f));
        g.fillRoundedRectangle(b,9.f);
        if(over || down) {g.setColour(Theme::green.withAlpha(.65f));g.drawRoundedRectangle(b,9.f,1.f);}
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::scale(b.getWidth()/58.f,b.getHeight()/58.f).translated(b.getX(),b.getY()));
        g.setColour(Theme::green);
        // Angular wings frame the skull, mirroring the SKELL mark without a jagged raster icon.
        juce::Path wings;
        wings.startNewSubPath(17,19);wings.lineTo(9,13);wings.lineTo(3,20);wings.lineTo(3,34);
        wings.lineTo(10,28);wings.lineTo(15,31);wings.lineTo(20,26);wings.closeSubPath();
        wings.startNewSubPath(41,19);wings.lineTo(49,13);wings.lineTo(55,20);wings.lineTo(55,34);
        wings.lineTo(48,28);wings.lineTo(43,31);wings.lineTo(38,26);wings.closeSubPath();
        g.fillPath(wings);
        juce::Path cranium;
        cranium.startNewSubPath(16.f,23.f);
        cranium.cubicTo(16.f,11.f,21.f,7.f,29.f,7.f);
        cranium.cubicTo(37.f,7.f,42.f,13.f,42.f,23.f);
        cranium.cubicTo(42.f,30.f,38.f,34.f,36.f,35.f);
        cranium.lineTo(36.f,45.f);cranium.lineTo(22.f,45.f);cranium.lineTo(22.f,35.f);
        cranium.cubicTo(18.f,32.f,16.f,29.f,16.f,23.f);
        cranium.closeSubPath();g.fillPath(cranium);
        g.setColour(Theme::plot);
        juce::Path eye;
        eye.addEllipse(21.f,24.f,7.f,8.f);eye.addEllipse(31.f,24.f,7.f,8.f);g.fillPath(eye);
        juce::Path nose;nose.startNewSubPath(29.5f,32.f);nose.lineTo(26.f,38.f);nose.lineTo(33.f,38.f);nose.closeSubPath();g.fillPath(nose);
        g.setColour(Theme::plot.withAlpha(.8f));
        for(int i=0;i<3;i++)g.fillRoundedRectangle(24.f+i*4.f,40.f,2.f,5.f,.7f);
    }
private:
    int variant=Theme::Obsidian;
};
