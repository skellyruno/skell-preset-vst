#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// Skell Preset's rack/patch skull. Shared bone proportions, custom three-row
// patch matrix across the forehead: unmistakably Preset, not Skell Key.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton() : juce::Button("Cycle Skell colour theme") {
        setTooltip("Cycle SKELL theme / Obsidian, Bone or Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        Theme::select(variant);
        const auto tile=getLocalBounds().toFloat().reduced(0.7f);
        g.setColour(juce::Colours::black.withAlpha(.38f));
        g.fillRoundedRectangle(tile.translated(0.f,1.5f),9.f);
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(.29f),
            tile.getTopLeft(),Theme::bg,tile.getBottomRight(),false));
        g.fillRoundedRectangle(tile,9.f);
        g.setColour(Theme::green.withAlpha(over?.90f:.55f));
        g.drawRoundedRectangle(tile.reduced(.65f),8.4f,over?1.6f:1.15f);
        juce::Graphics::ScopedSaveState saved(g);
        const auto b=getLocalBounds().toFloat();
        g.addTransform(juce::AffineTransform::scale(b.getWidth()/56.f,
                                                    b.getHeight()/56.f));
        const auto bone=Theme::white;
        const auto ink=Theme::bg;
        const auto accent=Theme::green;
        if(over) {g.setColour(accent.withAlpha(.10f));g.fillEllipse(7.f,6.f,42.f,45.f);}
        // One recognisable SKELL skull silhouette: domed skull, cheekbones,
        // separated jaw. Preset-specific details sit *on*, not in place of it.
        juce::Path cranium;
        cranium.startNewSubPath(28.f,7.f);
        cranium.cubicTo(13.f,6.f,10.f,14.f,11.f,26.f);
        cranium.cubicTo(11.f,33.f,16.f,35.f,18.f,38.f);
        cranium.lineTo(18.f,43.f);
        cranium.quadraticTo(18.f,47.f,23.f,47.f);
        cranium.lineTo(33.f,47.f);
        cranium.quadraticTo(38.f,47.f,38.f,43.f);
        cranium.lineTo(38.f,38.f);
        cranium.cubicTo(42.f,35.f,45.f,31.f,45.f,26.f);
        cranium.cubicTo(46.f,14.f,41.f,6.f,28.f,7.f);
        cranium.closeSubPath();
        g.setGradientFill(juce::ColourGradient(bone.brighter(.15f),17.f,12.f,
            bone.darker(.42f),41.f,46.f,false));
        g.fillPath(cranium);
        g.setColour(accent.withAlpha(.77f));
        g.strokePath(cranium,juce::PathStrokeType(1.25f));
        // Dark sculpted eye sockets, nose, and distinct teeth.
        g.setColour(ink);
        g.fillEllipse(15.f,24.f,11.f,10.f);
        g.fillEllipse(30.f,24.f,11.f,10.f);
        juce::Path nose;
        nose.addTriangle(28.f,32.f,24.4f,39.f,31.6f,39.f);
        g.fillPath(nose);
        g.fillRoundedRectangle(21.f,42.5f,14.f,1.7f,.8f);
        for(int i=0;i<4;++i)g.drawLine(22.3f+3.7f*i,43.f,22.3f+3.7f*i,47.f,1.f);
        g.setColour(accent.withAlpha(.93f));
        g.fillEllipse(20.f,29.f,1.9f,1.9f);
        g.fillEllipse(35.f,29.f,1.9f,1.9f);
        // Three parallel patch lanes with differing parameter positions.
        for(int row=0;row<3;++row) {
            const float yy=12.3f+row*3.2f;
            g.setColour(ink.withAlpha(.67f));g.drawLine(19.f,yy,37.f,yy,1.3f);
            g.setColour(accent.withAlpha(row==1?1.f:.71f));
            g.fillRoundedRectangle(21.5f+row*5.1f,yy-1.4f,3.3f,2.8f,1.0f);
        }
        if(down) {
            g.setColour(ink.withAlpha(.19f));
            g.fillRoundedRectangle(tile,8.f);
        }
    }
private:
    int variant=Theme::Obsidian;
};
