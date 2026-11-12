#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// SkellPreset's bone-and-metal skull mark. Theme swapping still uses the
// existing clickable skull; no generic key icon or externally loaded artwork.
class SkullThemeButton final : public juce::Button {
public:
    SkullThemeButton():juce::Button("Cycle Skell colour theme") {
        setTooltip("Cycle SKELL theme / Obsidian, Bone or Crimson");
        setWantsKeyboardFocus(true);
    }
    void setVariant(int id) {variant=Theme::clampVariant(id);repaint();}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        Theme::select(variant);
        auto rect=getLocalBounds().toFloat().reduced(.7f);
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(.12f),rect.getTopLeft(),
            Theme::bg,rect.getBottomRight(),false));
        g.fillRoundedRectangle(rect,6.4f);
        g.setColour(Theme::stroke.brighter(over?.6f:.16f));
        g.drawRoundedRectangle(rect.reduced(.5f),6.4f,1.05f);
        g.setColour(Theme::white.withAlpha(.15f));
        g.drawLine(rect.getX()+6.f,rect.getY()+2.f,
                   rect.getRight()-6.f,rect.getY()+2.f,1.f);

        juce::Graphics::ScopedSaveState state(g);
        const auto bounds=getLocalBounds().toFloat();
        g.addTransform(juce::AffineTransform::scale(bounds.getWidth()/56.f,
                                                   bounds.getHeight()/56.f));
        const auto bone=Theme::white;
        const auto shell=Theme::panelAlt.brighter(down?.10f:.23f);
        const auto phosphor=Theme::green;
        if(over) {
            g.setColour(phosphor.withAlpha(.095f));
            g.fillEllipse(6.f,5.f,44.f,45.f);
        }
        // Flat upper cranium, angled cheek guards, separated lower jaw.
        juce::Path skull;
        skull.startNewSubPath(28.f,7.f);
        skull.cubicTo(15.f,7.f,10.5f,13.f,10.5f,25.f);
        skull.lineTo(12.f,32.f);
        skull.lineTo(17.f,35.f);
        skull.lineTo(20.f,43.f);
        skull.lineTo(21.f,47.f);
        skull.lineTo(35.f,47.f);
        skull.lineTo(36.f,43.f);
        skull.lineTo(39.f,35.f);
        skull.lineTo(44.f,32.f);
        skull.lineTo(45.5f,25.f);
        skull.cubicTo(45.5f,13.f,41.f,7.f,28.f,7.f);
        skull.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            shell.brighter(.35f),15.f,12.f,
            Theme::bg,43.f,47.f,false));
        g.fillPath(skull);
        g.setColour(phosphor.withAlpha(over?1.f:.83f));
        g.strokePath(skull,juce::PathStrokeType(1.75f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // Bone brow ridges, dark inset eye cavities and rear cheek panels.
        juce::Path leftEye,rightEye;
        leftEye.startNewSubPath(15.f,23.f);leftEye.lineTo(25.5f,26.f);
        leftEye.lineTo(24.f,34.f);leftEye.lineTo(19.f,34.f);
        leftEye.lineTo(15.f,29.f);leftEye.closeSubPath();
        rightEye.startNewSubPath(41.f,23.f);rightEye.lineTo(30.5f,26.f);
        rightEye.lineTo(32.f,34.f);rightEye.lineTo(37.f,34.f);
        rightEye.lineTo(41.f,29.f);rightEye.closeSubPath();
        g.setColour(Theme::bg);g.fillPath(leftEye);g.fillPath(rightEye);
        g.setColour(phosphor.withAlpha(.95f));
        g.strokePath(leftEye,juce::PathStrokeType(1.f));
        g.strokePath(rightEye,juce::PathStrokeType(1.f));
        // Eye glints are deliberately tiny, not cartoon googly eyes.
        g.fillEllipse(20.5f,29.f,1.8f,1.8f);
        g.fillEllipse(33.7f,29.f,1.8f,1.8f);
        juce::Path nose;
        nose.startNewSubPath(28.f,33.5f);nose.lineTo(24.5f,40.f);
        nose.lineTo(31.5f,40.f);nose.closeSubPath();
        g.setColour(bone.withAlpha(.85f));g.fillPath(nose);
        g.setColour(Theme::bg.withAlpha(.85f));
        g.drawLine(20.f,43.f,36.f,43.f,1.75f);
        for(int i=0;i<4;++i)
            g.drawLine(22.5f+3.7f*i,43.5f,22.5f+3.7f*i,47.f,1.25f);
        // Small cranial fracture / metal highlight: SKELL identity detail.
        g.setColour(bone.withAlpha(.42f));
        g.drawLine(24.f,11.f,21.f,15.f,1.f);
        g.drawLine(21.f,15.f,24.f,18.f,1.f);
        g.setColour(phosphor.withAlpha(.53f));
        g.drawLine(13.f,22.f,12.5f,27.f,.8f);
        g.drawLine(43.f,22.f,43.5f,27.f,.8f);
    }
private:
    int variant=Theme::Obsidian;
};
