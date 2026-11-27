#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#if defined(SKELL_MATCH_HUB_ART)
#include <SkellHubMatchArtData.h>
#endif

class SkellHubRadar {
public:
    SkellHubRadar(){
#if defined(SKELL_MATCH_HUB_ART)
        const auto original=juce::ImageFileFormat::loadFrom(SkellHubMatchArtData::skellyradar_png,SkellHubMatchArtData::skellyradar_pngSize);
        if(original.isValid()){
            // Same source artwork and split as SkellyMotion: original rings
            // rotate, central skull stays upright. Cache both masked layers.
            const int w=original.getWidth(),h=original.getHeight();
            ring=juce::Image(juce::Image::ARGB,w,h,true);centre=juce::Image(juce::Image::ARGB,w,h,true);
            const float cx=w*.4897f,cy=h*.495f,rr=std::min(cx,cy);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                const auto c=original.getPixelAt(x,y);
                const float radius=std::hypot(x-cx,y-cy)/rr;
                const float middle=juce::jlimit(0.f,1.f,(.31f-radius)/.02f);
                const float edge=juce::jlimit(0.f,1.f,(1.f-radius)/.08f);
                // Convert the original dark plate into an alpha layer for
                // screen-like composition, preserving the hue/luminance.
                const float light=std::max({c.getFloatRed(),c.getFloatGreen(),c.getFloatBlue()});
                if(light<.001f)continue;
                const auto lit=juce::Colour::fromFloatRGBA(c.getFloatRed()/light,c.getFloatGreen()/light,c.getFloatBlue()/light,light);
                ring.setPixelAt(x,y,lit.withMultipliedAlpha((1.f-middle)*edge));
                centre.setPixelAt(x,y,lit.withMultipliedAlpha(middle));
            }
        }
#endif
    }
    bool usesOriginalArt() const noexcept {return ring.isValid()&&centre.isValid();}
    void draw(juce::Graphics& g,juce::Rectangle<float> b,float angle,bool active)const{
        const auto tint=juce::Colour(0xffaaff1a);
        const auto c=b.getCentre();const float r=b.getWidth()*.5f;
        g.setGradientFill(juce::ColourGradient(tint.withAlpha(active?.11f:.05f),c.x,c.y,tint.withAlpha(0.f),c.x+r,c.y,true));
        g.fillEllipse(b);
        if(usesOriginalArt()){
            const float scale=std::min(b.getWidth()/ring.getWidth(),b.getHeight()/ring.getHeight());
            const float x=c.x-ring.getWidth()*.4897f*scale,y=c.y-ring.getHeight()*.495f*scale;
            const auto t=juce::AffineTransform::scale(scale).translated(x,y);
            g.drawImageTransformed(ring,t.followedBy(juce::AffineTransform::rotation(angle,c.x,c.y)));
            g.drawImageTransformed(centre,t);
            return;
        }
        // Clean-build fallback: visibly a native ring indicator, not a
        // purported copy of the licensed/source Hub artwork.
        g.setColour(tint.withAlpha(.28f));g.drawEllipse(b.reduced(3.f),1.f);g.drawEllipse(b.reduced(r*.43f),.8f);
        for(int k=0;k<3;++k){
            const float q=r*(.80f-k*.19f);juce::Path p;p.addCentredArc(c.x,c.y,q,q,0.f,angle+k*.85f,angle+k*.85f+3.9f,true);
            g.setColour(tint.withAlpha(.70f-k*.14f));g.strokePath(p,juce::PathStrokeType(k==0?1.6f:1.f));
        }
        g.setColour(tint);g.setFont(Theme::font(13.f,true));g.drawText("S",b.toNearestInt(),juce::Justification::centred);
    }
private:
    juce::Image ring,centre;
};
