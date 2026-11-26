#pragma once
#include "Theme.h"
#include <cmath>

namespace skellpreset::art {
// Each factory sound has deterministic original cover art; no bitmap decoding
// or file I/O during a repaint. User-selected images override this motif.
inline void paintCover(juce::Graphics& g,juce::Rectangle<float> r,int seed,
                juce::Colour accent) {
    g.setGradientFill(juce::ColourGradient(Theme::plot,r.getX(),r.getY(),
                     accent.darker(.65f),r.getRight(),r.getBottom(),false));
    g.fillRoundedRectangle(r,5.f);
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(r.toNearestInt());
    // Use a consistent 65x46 design motif, scaled to fit and centred.
    // A large thumbnail and a compact list therefore show the same identity.
    const float scale=juce::jmax(.01f,juce::jmin(r.getWidth()/65.f,r.getHeight()/46.f));
    const float x=r.getCentreX()-32.5f*scale,y=r.getCentreY()-23.f*scale;
    g.addTransform(juce::AffineTransform::scale(scale).translated(x,y));
    r={0.f,0.f,65.f,46.f};
    const int variation=(seed<0?-seed:seed)%4;
    const float cx=r.getCentreX(),cy=r.getCentreY();
    g.setColour(accent.withAlpha(.13f));
    for(int i=1;i<=4;++i)
        g.drawEllipse(cx-9.f*i,cy-9.f*i,18.f*i,18.f*i,1.f);
    g.setColour(accent.withAlpha(.85f));
    if(variation==0) {
        for(int i=0;i<7;++i){
            const float h=11.f+(float)((seed*17+i*29)%31);
            g.fillRoundedRectangle(r.getX()+6.f+i*8.6f,cy-h*.5f,4.f,h,1.3f);
        }
    } else if(variation==1) {
        juce::Path wave;
        for(int i=0;i<=42;++i){
            const float x=r.getX()+3.f+i*(r.getWidth()-6.f)/42.f;
            const float y=cy+std::sin((float)i*.5f+seed*.21f)*
                (11.f+5.f*std::cos((float)i*.13f));
            if(i==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);
        }
        g.strokePath(wave,juce::PathStrokeType(2.5f));
    } else if(variation==2) {
        const float twist=(float)((seed*7)%13)-6.f;
        const float step=2.6f+(float)(seed%4)*.22f;
        for(int i=0;i<5;++i) {
            const float margin=5.f+i*step;
            g.drawRoundedRectangle(r.getX()+margin+twist*.23f,r.getY()+margin,
                                   r.getWidth()-2.f*margin,r.getHeight()-2.f*margin,
                                   2.5f+(float)(seed%3),1.4f);
        }
    } else {
        const float span=16.f+(float)((seed*3)%11);
        const float h=12.f+(float)((seed*5)%13);
        juce::Path poly;
        poly.startNewSubPath(cx,cy-h);
        poly.lineTo(cx+span,cy-3.f);
        poly.lineTo(cx+3.f,cy+h);
        poly.lineTo(cx-span,cy+3.f);
        poly.closeSubPath();
        g.strokePath(poly,juce::PathStrokeType(2.4f));
        const float core=3.f+(float)(seed%7);
        g.fillEllipse(cx-core,cy-core,core*2.f,core*2.f);
    }
}
}
