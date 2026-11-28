#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>
#include <functional>
namespace skellpreset::stage {
// Original stencil-style performer. Geometry is in the existing 345 x 372 stage
// coordinate system. Pose, ink plates and interactive mic share that space.
class Performer {
    using P=juce::Point<float>; using Path=juce::Path; using C=juce::Colour;
    const C bone{0xffeee9dc}, ink{0xff18221f}, edge{0xffbcbdb0};
    static Path poly(std::initializer_list<P> points){Path p;if(points.size()<3)return p;const auto* v=points.begin();const size_t count=points.size();auto before=[&](size_t i){return v[i]+(v[(i+count-1)%count]-v[i])*.23f;};auto after=[&](size_t i){return v[i]+(v[(i+1)%count]-v[i])*.23f;};p.startNewSubPath(before(0));for(size_t i=0;i<count;++i){p.lineTo(before(i));p.quadraticTo(v[i],after(i));}p.closeSubPath();return p;}
    static void stroke(juce::Graphics& g,const Path& p,float w,C colour){g.setColour(colour);g.strokePath(p,juce::PathStrokeType(w,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
    void fill(juce::Graphics& g,const Path& p,C colour)const{g.setColour(colour);g.fillPath(p);}
    void plate(juce::Graphics& g,const Path& p)const{fill(g,p,bone);stroke(g,p,.85f,ink);}
    void hole(juce::Graphics& g,std::initializer_list<P> points)const{fill(g,poly(points),ink);}
    // Broad, irregular bone silhouette with ink-cut marrow and flared ends.
    void longBone(juce::Graphics& g,P a,P b,float ra,float rb,float bow=0.f)const{
        const P delta=b-a;const float len=juce::jmax(1.f,delta.getDistanceFromOrigin());const P u=delta/len,n{-u.y,u.x};
        const P m=(a+b)*.5f+n*bow;const float w=(ra+rb)*.40f;
        Path p;p.startNewSubPath(a-u*ra*.52f+n*ra*.7f);p.lineTo(a+n*ra);p.quadraticTo(a+u*len*.2f+n*w,m+n*w*.86f);p.lineTo(b-u*len*.19f+n*w*.82f);p.quadraticTo(b-u*rb*.55f+n*rb,b+n*rb);p.lineTo(b+u*rb*.65f+n*rb*.55f);p.lineTo(b+u*rb*.58f-n*rb*.64f);p.quadraticTo(b-n*rb,b-u*len*.17f-n*w);p.quadraticTo(m-n*w*.7f,a+u*len*.15f-n*w);p.lineTo(a-n*ra*.92f);p.lineTo(a-u*ra*.68f-n*ra*.44f);p.closeSubPath();plate(g,p);
        if(len>13.f&&ra>2.f){hole(g,{a+u*len*.12f-n*ra*.55f,a+u*len*.24f-n*w*.42f,m-u*len*.08f-n*w*.22f,m-u*len*.17f+n*w*.25f,a+u*len*.19f+n*w*.33f});hole(g,{m+u*len*.08f-n*w*.25f,b-u*len*.15f-n*w*.38f,b-u*len*.10f+n*w*.14f,m+u*len*.16f+n*w*.28f});
            hole(g,{a-u*ra*.21f+n*ra*.35f,a+u*ra*.6f+n*ra*.66f,a+u*ra*.75f+n*ra*.14f});
            hole(g,{b-u*rb*.58f-n*rb*.47f,b+u*rb*.15f-n*rb*.62f,b+u*rb*.29f+n*rb*.02f});}
    }
    void knuckle(juce::Graphics& g,P p,float r)const{plate(g,poly({p+P{-r,-r*.35f},p+P{-r*.75f,-r*.85f},p+P{-r*.1f,-r},p+P{r*.65f,-r*.8f},p+P{r,r*.1f},p+P{r*.4f,r*.85f},p+P{-r*.3f,r},p+P{-r*.8f,r*.55f}}));if(r>3.f)hole(g,{p+P{-r*.3f,-r*.3f},p+P{r*.5f,-r*.45f},p+P{r*.2f,r*.3f}});}
    void shoeBones(juce::Graphics& g,P ankle,float direction)const{
        const auto q=[&](float x,float y){return ankle+P{x*direction,y};};
        plate(g,poly({q(-5,-3),q(4,-4),q(8,3),q(19,8),q(26,10),q(29,15),q(23,19),q(13,17),q(5,13),q(-6,11),q(-8,4)}));
        for(int i=0;i<4;++i){const float x=7.f+i*4.5f;hole(g,{q(x,5+i*.9f),q(x+3,7+i*.9f),q(x+1,12+i*.8f),q(x-1,10+i*.8f)});}hole(g,{q(-3,1),q(2,2),q(2,7),q(-4,8)});
    }
    void bodyArt(juce::Graphics& g)const{
        // Far leg, then the foreground crossed leg. Knees and ankles are chunky.
        longBone(g,{191,203},{220,266},8.5f,7.2f,-1);knuckle(g,{220,266},7.3f);
        longBone(g,{220,273},{220,329},7.f,5.8f,2);longBone(g,{226,274},{225,329},2.8f,2.4f);shoeBones(g,{221,331},1);
        longBone(g,{145,205},{191,269},10.f,8.f,-5);knuckle(g,{191,269},8.f);
        longBone(g,{187,276},{151,351},6.7f,5.5f,-2);longBone(g,{194,278},{158,351},3.4f,2.7f);shoeBones(g,{151,350},1);
        // Broad pelvic wings, dark iliac fossae and open obturator foramina.
        Path hip;hip.startNewSubPath(146,173);hip.cubicTo(128,162,119,174,123,190);hip.lineTo(136,196);hip.cubicTo(128,208,133,221,147,222);hip.lineTo(164,213);hip.lineTo(182,215);hip.cubicTo(196,227,208,216,202,204);hip.lineTo(197,191);hip.cubicTo(205,176,196,167,184,168);hip.quadraticTo(172,171,175,190);hip.lineTo(161,197);hip.quadraticTo(153,187,146,173);hip.closeSubPath();plate(g,hip);
        hole(g,{{129,176},{137,174},{142,179},{148,188},{144,195},{139,191},{136,184},{132,184}});hole(g,{{183,175},{190,172},{196,175},{198,182},{193,191},{186,197},{181,190},{183,185}});
        hole(g,{{139,204},{145,200},{154,203},{155,211},{146,216},{138,212}});hole(g,{{177,204},{185,199},{195,202},{196,210},{189,216},{179,211}});
        hole(g,{{155,194},{161,200},{168,195},{175,202},{168,211},{160,209}});
        // Visible irregular vertebrae and the neck, in front of a narrow spine.
        Path spine;spine.startNewSubPath(190,53);spine.cubicTo(182,83,165,123,158,192);stroke(g,spine,9.5f,ink);for(int i=0;i<15;++i){const float t=i/14.f,y=52.f+139.f*t,x=190.f-32.f*t+3.f*std::sin(t*6.f),w=3.7f+2.1f*t;plate(g,poly({{x-w-2,y-2},{x+w,y-3},{x+w+2,y},{x+w,y+4},{x-w,y+3},{x-w-2,y+1}}));if(i%3==0)hole(g,{{x-2,y-1},{x+2,y-2},{x+1,y+1}});}
        // Oblique thorax. Dark interior and broad, irregular inked rib crescents.
        Path cage;cage.startNewSubPath(152,66);cage.cubicTo(132,65,127,94,129,121);cage.quadraticTo(125,145,139,156);cage.lineTo(159,141);cage.lineTo(178,130);cage.lineTo(190,151);cage.cubicTo(213,143,220,117,219,98);cage.quadraticTo(210,83,191,83);cage.quadraticTo(175,68,152,66);cage.closeSubPath();fill(g,cage,ink);
        for(int i=0;i<8;++i){const float t=i/7.f;const float x=143.f-8.f*std::sin(t*3.14f),y=76.f+8.f*i+(i%3-1)*1.1f;Path r;r.startNewSubPath(x,y);r.cubicTo(x-6,y+12,149.f-6.f*t,y+24,181.f-19.f*t,y+17);stroke(g,r,5.1f-1.2f*t,bone);if(i%2==1)hole(g,{{x-1,y+8},{x+4,y+11},{x+3,y+14}});}
        for(int i=0;i<6;++i){const float t=i/5.f,y=88.f+8.f*i;Path r;r.startNewSubPath(192.f-7.f*t,y);r.cubicTo(217.f-4.f*t,y-2,213.f-4.f*t,y+10,188.f-4.f*t,y+12);stroke(g,r,4.3f-.6f*t,bone);}
        plate(g,poly({{187,82},{194,87},{189,103},{185,124},{179,138},{174,130},{179,109},{180,96}}));hole(g,{{183,112},{187,108},{184,119}});
        Path margin;margin.startNewSubPath(139,147);margin.quadraticTo(151,156,176,130);margin.quadraticTo(188,135,193,147);stroke(g,margin,5.1f,bone);
        longBone(g,{144,68},{184,85},5.8f,4.6f,3);longBone(g,{190,85},{216,91},4.2f,5.6f,-3);
        // Mic-side elbow and gripping hand, static relative to the microphone.
        longBone(g,{215,94},{262,135},7.f,5.8f,2);knuckle(g,{262,135},6.f);
        longBone(g,{260,131},{237,84},6.3f,4.3f,1);longBone(g,{267,130},{243,84},2.8f,2.4f);
        plate(g,poly({{229,73},{238,68},{246,72},{247,83},{241,93},{232,90},{228,82}}));hole(g,{{231,77},{238,75},{243,78},{241,81},{234,81}});hole(g,{{233,85},{241,83},{241,87},{235,89}});
    }
    void armArt(juce::Graphics& g)const{
        // Open palm silhouette, splayed curved fingers and a nearly level upper arm.
        longBone(g,{145,68},{109,78},6.6f,6.2f,2);knuckle(g,{109,78},6.1f);
        longBone(g,{105,75},{66,60},5.6f,4.2f,0);longBone(g,{106,82},{64,65},2.8f,2.7f);
        plate(g,poly({{64,54},{57,45},{47,45},{38,51},{39,61},{49,67},{60,66},{70,62}}));hole(g,{{43,51},{48,50},{53,53},{50,58},{44,56}});hole(g,{{55,58},{60,54},{63,59},{59,63}});hole(g,{{47,61},{51,60},{53,64},{49,65}});
        const P bases[4]{{43,52},{46,47},{51,44},{57,46}};const P mids[4]{{30,45},{33,39},{44,32},{55,35}};const P ends[4]{{19,34},{25,28},{36,21},{51,23}};
        for(int i=0;i<4;++i){longBone(g,bases[i],mids[i],3.4f,2.7f);knuckle(g,mids[i],2.4f);longBone(g,mids[i],ends[i],2.5f,1.8f);}
        longBone(g,{62,51},{61,32},3.3f,2.5f);longBone(g,{61,32},{49,12},2.5f,1.6f);knuckle(g,{145,68},7.4f);
    }
    void skullArt(juce::Graphics& g)const{
        // Large cranium, tapered occiput, projecting maxilla and open cheek arch.
        Path cr;cr.startNewSubPath(171,43);cr.cubicTo(166,24,178,7,194,5);cr.cubicTo(213,2,229,14,230,31);cr.lineTo(234,36);cr.lineTo(229,43);cr.lineTo(234,47);cr.lineTo(230,53);cr.lineTo(217,54);cr.lineTo(210,49);cr.lineTo(201,49);cr.lineTo(193,55);cr.cubicTo(180,55,172,50,171,43);cr.closeSubPath();plate(g,cr);
        hole(g,{{172,35},{178,22},{181,26},{179,32},{183,36},{176,41}});hole(g,{{181,42},{190,35},{196,30},{197,20},{201,18},{203,23},{200,34},{192,43},{190,48},{183,48}});
        Path orbit;orbit.startNewSubPath(208,21);orbit.cubicTo(219,17,225,23,219,32);orbit.cubicTo(211,37,203,31,208,21);orbit.closeSubPath();fill(g,orbit,ink);
        hole(g,{{228,29},{225,35},{221,40},{226,43},{232,40}});hole(g,{{193,43},{202,36},{211,38},{207,44},{200,45}});hole(g,{{211,45},{216,42},{219,47},{224,46},{227,50},{217,51}});
        hole(g,{{183,11},{190,7},{197,7},{191,10}});hole(g,{{204,6},{211,7},{215,10},{210,9}});hole(g,{{181,46},{184,45},{186,48},{183,50}});hole(g,{{217,37},{219,36},{220,39}});
        for(int i=0;i<5;++i){const float x=211.f+i*3.7f;plate(g,poly({{x,49},{x+3.4f,50},{x+2.7f,56},{x+.6f,55}}));}
    }
    void jawArt(juce::Graphics& g)const{
        Path jaw;jaw.startNewSubPath(192,44);jaw.lineTo(197,45);jaw.lineTo(198,56);jaw.quadraticTo(201,64,212,68);jaw.lineTo(222,64);jaw.lineTo(225,67);jaw.lineTo(222,73);jaw.lineTo(212,76);jaw.cubicTo(200,72,190,61,190,52);jaw.closeSubPath();plate(g,jaw);
        hole(g,{{194,49},{197,57},{200,62},{195,59},{192,54}});hole(g,{{205,68},{211,71},{219,68},{215,73},{210,73}});
        for(int i=0;i<4;++i){const float x=209.f+i*3.2f;plate(g,poly({{x,67},{x+.6f,62},{x+2.9f,62},{x+3.f,67}}));}
    }
    void microphone(juce::Graphics& g,float activity)const{
        // Diagonal mic pointing directly at the open mouth. Cable starts at its plug.
        longBone(g,{232,51},{251,64},5.5f,3.8f);plate(g,poly({{226,44},{234,43},{240,49},{239,55},{233,59},{226,55},{223,49}}));
        for(int i=0;i<3;++i){Path grille;grille.startNewSubPath(226.f+i*3.f,46);grille.lineTo(231.f+i*3.f,55);stroke(g,grille,.9f,ink);}
        g.setColour(Theme::green.withAlpha(.45f+.45f*activity));g.fillEllipse(244,60,2.8f,2.8f);
    }
    void cableAndStand(juce::Graphics& g,float activity,float phase)const{
        const float sway=4.f*activity*std::sin(phase*.6f);Path cable;cable.startNewSubPath(254,66);cable.cubicTo(307,90,323+sway,154,294+sway,200);cable.cubicTo(275,228,245,247,220,261);cable.cubicTo(194,277,194,311,202,332);stroke(g,cable,3.2f,ink);stroke(g,cable,1.6f,edge);
        Path mount;mount.startNewSubPath(242,60);mount.lineTo(236,75);stroke(g,mount,4.f,ink);stroke(g,mount,2.f,edge);Path stand;stand.startNewSubPath(235,89);stand.lineTo(201,337);stroke(g,stand,4.f,ink);stroke(g,stand,2.2f,edge);g.setColour(ink);g.fillEllipse(177,336,49,12);g.setColour(edge);g.drawEllipse(177,336,49,12,1.4f);
    }
    static void cache(juce::Image& image,juce::Rectangle<int> area,const std::function<void(juce::Graphics&)>& paint){if(image.isValid())return;image=juce::Image(juce::Image::ARGB,area.getWidth()*2,area.getHeight()*2,true);juce::Graphics art(image);art.addTransform(juce::AffineTransform::translation(-(float)area.getX(),-(float)area.getY()).scaled(2.f));paint(art);}
    static void blit(juce::Graphics& g,const juce::Image& image,float x,float y){g.setOpacity(1.f);g.drawImageTransformed(image,juce::AffineTransform::scale(.5f).translated(x,y));}
public:
    static juce::AffineTransform bodyLean(float activity,float phase) noexcept {const float lean=(.012f*std::sin(phase*.41f)+.028f*std::sin(phase*.71f))*juce::jlimit(0.f,1.f,activity);return juce::AffineTransform::rotation(lean,151.f,365.f);}
    static juce::Rectangle<int> microphoneHitBounds(float x,float y,float scale,float activity,float phase) noexcept {return juce::Rectangle<float>(223,43,33,25).transformedBy(bodyLean(activity,phase).followedBy(juce::AffineTransform::scale(scale).translated(x,y))).expanded(5.f).toNearestInt();}
    void draw(juce::Graphics& g,float x,float y,float scale,float activity,float phase,float vocal=-1.f)const{
        const float react=juce::jlimit(0.f,1.f,activity),voice=juce::jlimit(0.f,1.f,vocal<0.f?react:vocal);juce::Graphics::ScopedSaveState state(g);g.addTransform(juce::AffineTransform::scale(scale).translated(x,y));
        g.setGradientFill(juce::ColourGradient(ink.withAlpha(.85f),151,365,ink.withAlpha(0.f),151,377,false));g.fillEllipse(119,359,146,15);
        cache(body,{112,43,170,329},[this](auto& art){bodyArt(art);});cache(arm,{10,5,147,87},[this](auto& art){armArt(art);});cache(head,{162,0,78,62},[this](auto& art){skullArt(art);});cache(jaw,{184,40,48,42},[this](auto& art){jawArt(art);});
        g.addTransform(bodyLean(react,phase));cableAndStand(g,react,phase);blit(g,body,112,43);
        {juce::Graphics::ScopedSaveState armPose(g);g.addTransform(juce::AffineTransform::rotation(.06f*react*std::sin(phase*.68f),145,68));blit(g,arm,10,5);}
        {juce::Graphics::ScopedSaveState skullPose(g);g.addTransform(juce::AffineTransform::rotation(-.02f+.04f*react*std::sin(phase*.73f),193,53));blit(g,head,162,0);g.addTransform(juce::AffineTransform::rotation(.12f+.27f*std::sqrt(voice),193,46));blit(g,jaw,184,40);}
        microphone(g,react);
    }
private:mutable juce::Image body,arm,head,jaw;
};
} // namespace skellpreset::stage
