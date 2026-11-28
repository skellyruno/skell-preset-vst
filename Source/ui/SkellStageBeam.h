#pragma once
#include <JuceHeader.h>
#include <cmath>

// The software renderer pays heavily for affine alpha-mask fills. Use
// three feathered, bounded native paths instead. Supports up- and down-
// lighting with a zero-alpha tip, without a transformed image resample.
class SkellStageBeam {
public:
    void draw(juce::Graphics& g,float sourceX,float sourceY,float targetX,float targetY,float halfWidth,juce::Colour tint,float alpha)const{
        if(std::abs(targetY-sourceY)<1.f||halfWidth<=0.f||alpha<=0.f)return;
        juce::Graphics::ScopedSaveState restore(g);
        for(int layer=0;layer<3;++layer){
            const float w=halfWidth*(1.f-.26f*layer);
            juce::Path path;path.startNewSubPath(sourceX-1.5f,sourceY);path.lineTo(sourceX+1.5f,sourceY);
            path.lineTo(targetX+w,targetY);path.lineTo(targetX-w,targetY);path.closeSubPath();
            juce::ColourGradient gradient(tint.withAlpha(juce::jlimit(0.f,1.f,alpha*(.21f+.05f*layer))),sourceX,sourceY,
                tint.withAlpha(0.f),targetX,targetY,false);
            gradient.addColour(.58,tint.withAlpha(juce::jlimit(0.f,1.f,alpha*.045f)));
            g.setGradientFill(gradient);g.fillPath(path);
        }
    }
};
