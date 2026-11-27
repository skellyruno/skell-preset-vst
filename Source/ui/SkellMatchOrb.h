#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <array>
#include <cmath>
#include <cstdint>

// Native UI-only particle volume, not a video or pre-rendered image.
// Deterministic positions avoid frame-to-frame flicker. No DSP callbacks,
// per-frame RNG, GPU context, or heap allocation for the particles.
class SkellMatchOrb {
public:
    SkellMatchOrb() {
        uint32_t seed=0x6c8e9cf5u;
        auto random=[&]() noexcept {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return (float)(seed>>8)*(1.f/16777216.f);};
        constexpr float tau=juce::MathConstants<float>::twoPi;
        for(size_t i=0;i<points.size();++i){
            const float z=random()*2.f-1.f, az=random()*tau;
            const float xy=std::sqrt(juce::jmax(0.f,1.f-z*z));
            // Two-thirds shell, one-third volumetric core; scattered dust
            // separates the silhouette from its background.
            const float rad=i<1080?.79f+.20f*random():i<1460?std::cbrt(random())*.84f:1.02f+.18f*random();
            points[i]={rad*xy*std::cos(az),rad*z,rad*xy*std::sin(az),random(),random()};
        }
        for(auto& b:buckets)b.ensureStorageAllocated((int)points.size());
    }
    void draw(juce::Graphics& g,juce::Rectangle<float> bounds,float angle,float level,float progress,bool refining)const{
        juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(bounds.toNearestInt());
        const auto c=bounds.getCentre();const float radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*.45f;
        const auto green=Theme::green;
        const float voice=std::sqrt(juce::jlimit(0.f,1.f,level));
        const float breathe=.5f+.5f*std::sin(angle*4.f-.9f);
        const float size=radius*(.66f+.21f*breathe+.13f*voice+(refining?.05f*progress:0.f));
        const float sy=std::sin(angle*2.f),cy=std::cos(angle*2.f),sx=std::sin(angle),cx=std::cos(angle);
        const float twist=.035f+.040f*voice;
        g.setGradientFill(juce::ColourGradient(green.withAlpha(.09f+.08f*voice),c.x,c.y,green.withAlpha(0.f),c.x+radius,c.y,true));
        g.fillEllipse(c.x-radius,c.y-radius,2.f*radius,2.f*radius);
        // Batch into five brightness groups. A bounded fixed-size rectangle
        // array is rendered as individual tiny dots, not a fake waveform.
        const std::array<juce::Colour,5> colours{{green.withAlpha(.11f),green.withAlpha(.26f),green.withAlpha(.46f),green.withAlpha(.72f),green.interpolatedWith(Theme::white,.38f).withAlpha(.92f)}};
        for(auto& b:buckets)b.clear();
        for(size_t i=0;i<points.size();++i){
            const auto& p=points[i];
            const float x=p.x*cy+p.z*sy,z=-p.x*sy+p.z*cy;
            const float y=p.y*cx-z*sx,depth=p.y*sx+z*cx;
            const float ripple=1.f+twist*std::sin(angle*5.f+6.f*p.phase+3.f*y);
            const float perspective=1.f/(1.f-.15f*depth);
            const float px=c.x+x*size*ripple*perspective,py=c.y+y*size*ripple*perspective;
            const float front=juce::jlimit(0.f,1.f,.5f+.5f*depth);
            const float shimmer=.5f+.5f*std::sin(angle*6.f+p.phase*juce::MathConstants<float>::twoPi);
            const float light=juce::jlimit(0.f,1.f,.44f*front+.30f*p.light+.16f*shimmer+.10f*voice);
            const int bucket=juce::jlimit(0,4,(int)(light*5.f)-(i>=1460?1:0));
            const float dot=(.56f+.65f*front+.32f*p.light)*(i>=1460?.72f:1.f);
            buckets[(size_t)bucket].addWithoutMerging({px-dot*.5f,py-dot*.5f,dot,dot});
        }
        for(size_t i=0;i<buckets.size();++i){g.setColour(colours[i]);g.fillRectList(buckets[i]);}
    }
private:
    struct Point {float x,y,z,phase,light;};
    std::array<Point,1580> points{};
    mutable std::array<juce::RectangleList<float>,5> buckets;
};
