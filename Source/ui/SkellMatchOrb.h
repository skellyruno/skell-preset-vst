#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <array>
#include <cmath>
#include <cstdint>

// Deterministic native particle volume. UI thread only. Precompute phases,
// batch reusable rectangles and use a cached analytic glow per paint.
class SkellMatchOrb {
public:
    SkellMatchOrb(){
        uint32_t seed=0x6c8e9cf5u;
        auto random=[&]() noexcept {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return (float)(seed>>8)*(1.f/16777216.f);};
        constexpr float tau=juce::MathConstants<float>::twoPi;
        for(size_t i=0;i<points.size();++i){
            const float z=random()*2.f-1.f,az=random()*tau,xy=std::sqrt(juce::jmax(0.f,1.f-z*z));
            const float rad=i<2700?.76f+.24f*random():i<3900?std::cbrt(random())*.85f:1.02f+.22f*random();
            const float phase=random()*tau;
            points[i]={rad*xy*std::cos(az),rad*z,rad*xy*std::sin(az),std::sin(phase),std::cos(phase),random()};
        }
        for(auto& b:buckets)b.ensureStorageAllocated((int)points.size());
        halos.ensureStorageAllocated(256);
    }
    void draw(juce::Graphics& g,juce::Rectangle<float> bounds,float angle,float level,float progress,bool refining)const{
        juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(bounds.toNearestInt());
        const auto c=bounds.getCentre();const float radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*.43f;
        const auto green=Theme::green;
        const float voice=std::sqrt(juce::jlimit(0.f,1.f,level));
        const float breathe=.5f+.5f*std::sin(angle*4.f-.9f);
        const float size=radius*(.72f+.18f*breathe+.12f*voice+(refining?.04f*progress:0.f));
        const float sy=std::sin(angle*2.f),cy=std::cos(angle*2.f),sx=std::sin(angle),cx=std::cos(angle);
        const float waveSin=std::sin(angle*6.f),waveCos=std::cos(angle*6.f);
        const float twist=.055f+.055f*voice;
        // Local dark lens separates the luminous volume from the transparent
        // stage. A soft core and rim glow add optical depth without a blur pass.
        g.setGradientFill(juce::ColourGradient(Theme::bg.withAlpha(.90f),c.x,c.y,Theme::bg.withAlpha(0.f),c.x+radius*1.15f,c.y,true));
        g.fillEllipse(bounds);
        juce::ColourGradient aura(green.withAlpha(.14f+.12f*voice),c.x,c.y,green.withAlpha(0.f),c.x+radius*1.12f,c.y,true);
        aura.addColour(.55,green.withAlpha(.065f+.05f*voice));aura.addColour(.83,green.withAlpha(.08f+.07f*voice));
        g.setGradientFill(aura);g.fillEllipse(bounds);
        const std::array<juce::Colour,7> colours{{green.withMultipliedBrightness(.45f).withAlpha(.22f),green.withMultipliedBrightness(.72f).withAlpha(.35f),green.withAlpha(.42f),green.withAlpha(.64f),green.withAlpha(.85f),green.interpolatedWith(Theme::white,.24f).withAlpha(.94f),green.interpolatedWith(Theme::white,.64f)}};
        for(auto& bucket:buckets)bucket.clear();
        halos.clear();
        for(size_t i=0;i<points.size();++i){
            const auto& p=points[i];
            const float x=p.x*cy+p.z*sy,z=-p.x*sy+p.z*cy;
            const float y=p.y*cx-z*sx,depth=p.y*sx+z*cx;
            const float pulse=waveSin*p.cosPhase+waveCos*p.sinPhase;
            const float ripple=1.f+twist*(pulse+.34f*y);
            const float perspective=1.f/(1.f-.22f*depth);
            const float px=c.x+x*size*ripple*perspective,py=c.y+y*size*ripple*perspective;
            const float front=juce::jlimit(0.f,1.f,.5f+.5f*depth);
            const float rim=1.f-std::abs(2.f*front-1.f);
            const float light=juce::jlimit(0.f,1.f,.50f*front+.24f*p.light+.10f*(.5f+.5f*pulse)+.10f*rim+.10f*voice);
            const int bucket=juce::jlimit(0,6,(int)(light*7.f)-(i>=3900?2:0));
            const float dot=(.52f+.78f*front+.43f*p.light)*perspective*(i>=3900?.60f:1.f);
            buckets[(size_t)bucket].addWithoutMerging({px-dot*.5f,py-dot*.5f,dot,dot});
            if(bucket>=5 && i%17==0){const float halo=dot*2.6f;halos.addWithoutMerging({px-halo*.5f,py-halo*.5f,halo,halo});}
        }
        g.setColour(green.withAlpha(.13f));g.fillRectList(halos);
        for(size_t i=0;i<buckets.size();++i){g.setColour(colours[i]);g.fillRectList(buckets[i]);}
    }
private:
    struct Point {float x,y,z,sinPhase,cosPhase,light;};
    std::array<Point,4200> points{};
    mutable std::array<juce::RectangleList<float>,7> buckets;
    mutable juce::RectangleList<float> halos;
};
