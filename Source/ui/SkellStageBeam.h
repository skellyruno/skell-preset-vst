#pragma once
#include <JuceHeader.h>
#include <cmath>

// Reusable native alpha mask for continuous soft spotlight falloff. Built
// once on the UI thread; moving light position and real signal brightness
// are independent of mask generation. No per-frame path tessellation.
class SkellStageBeam {
public:
    static constexpr int width=128,height=256;
    SkellStageBeam():mask(juce::Image::ARGB,width,height,true) {
        juce::Image::BitmapData pixels(mask,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<height;++y){
            const float t=(float)y/(height-1),extent=.045f+.955f*t;
            const float fade=(1.f-t)*(1.f-.24f*t);
            for(int x=0;x<width;++x){
                const float side=std::abs(2.f*x/(width-1)-1.f)/extent;
                const float feather=juce::jlimit(0.f,1.f,(1.f-side)*3.5f);
                const float alpha=fade*std::exp(-2.6f*side*side)*feather*feather*(3.f-2.f*feather);
                pixels.setPixelColour(x,y,juce::Colours::white.withAlpha(alpha));
            }
        }
    }
    void draw(juce::Graphics& g,float sourceX,float sourceY,float targetX,float targetY,float halfWidth,juce::Colour tint,float alpha)const{
        if(targetY<=sourceY||halfWidth<=0.f||alpha<=0.f)return;
        const float w=2.f*halfWidth,h=targetY-sourceY,shift=targetX-sourceX;
        // The mask axis follows the actual moving source/target line.
        const juce::AffineTransform transform(w/width,shift/height,sourceX-halfWidth,0.f,h/height,sourceY);
        juce::Graphics::ScopedSaveState restore(g);
        g.setColour(tint.withAlpha(juce::jlimit(0.f,1.f,alpha)));
        g.drawImageTransformed(mask,transform,true);
    }
private:
    juce::Image mask;
};
