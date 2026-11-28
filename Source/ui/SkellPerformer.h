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
    const C bone{0xfff3f0e8}, ink{0xff111614}, edge{0xffb7bcb5};
    static Path poly(std::initializer_list<P> points){Path p;if(points.size()<3)return p;const auto* v=points.begin();const size_t count=points.size();auto before=[&](size_t i){return v[i]+(v[(i+count-1)%count]-v[i])*.23f;};auto after=[&](size_t i){return v[i]+(v[(i+1)%count]-v[i])*.23f;};p.startNewSubPath(before(0));for(size_t i=0;i<count;++i){p.lineTo(before(i));p.quadraticTo(v[i],after(i));}p.closeSubPath();return p;}
    static void stroke(juce::Graphics& g,const Path& p,float w,C colour){g.setColour(colour);g.strokePath(p,juce::PathStrokeType(w,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
    void fill(juce::Graphics& g,const Path& p,C colour)const{g.setColour(colour);g.fillPath(p);}
    void plate(juce::Graphics& g,const Path& p)const{fill(g,p,bone);stroke(g,p,.72f,ink);}
    void hole(juce::Graphics& g,std::initializer_list<P> points)const{fill(g,poly(points),ink);}
    // Broad, irregular bone silhouette with ink-cut marrow and flared ends.
    void longBone(juce::Graphics& g,P a,P b,float ra,float rb,float bow=0.f)const{
        const P delta=b-a;const float len=juce::jmax(1.f,delta.getDistanceFromOrigin());const P u=delta/len,n{-u.y,u.x};
        const P m=(a+b)*.5f+n*bow;const float w=(ra+rb)*.46f;
        Path p;p.startNewSubPath(a-u*ra*.52f+n*ra*.7f);p.lineTo(a+n*ra);p.quadraticTo(a+u*len*.2f+n*w,m+n*w*.86f);p.lineTo(b-u*len*.19f+n*w*.82f);p.quadraticTo(b-u*rb*.55f+n*rb,b+n*rb);p.lineTo(b+u*rb*.65f+n*rb*.55f);p.lineTo(b+u*rb*.58f-n*rb*.64f);p.quadraticTo(b-n*rb,b-u*len*.17f-n*w);p.quadraticTo(m-n*w*.7f,a+u*len*.15f-n*w);p.lineTo(a-n*ra*.92f);p.lineTo(a-u*ra*.68f-n*ra*.44f);p.closeSubPath();plate(g,p);
        // One tapered anatomical shadow rather than separate geometric surface marks.
        if(len>13.f&&ra>2.f){
            Path marrow;marrow.startNewSubPath(a+u*len*.10f-n*ra*.60f);
            marrow.cubicTo(a+u*len*.25f-n*w*.85f,m-n*w*.58f,b-u*len*.12f-n*rb*.56f);
            marrow.cubicTo(b-u*len*.26f-n*w*.02f,m+u*len*.14f+n*w*.22f,m-n*w*.16f);
            marrow.quadraticTo(a+u*len*.23f+n*w*.11f,a+u*len*.10f-n*ra*.60f);marrow.closeSubPath();fill(g,marrow,ink);
        }
    }
    void knuckle(juce::Graphics& g,P p,float r)const{plate(g,poly({p+P{-r,-r*.35f},p+P{-r*.75f,-r*.85f},p+P{-r*.1f,-r},p+P{r*.65f,-r*.8f},p+P{r,r*.1f},p+P{r*.4f,r*.85f},p+P{-r*.3f,r},p+P{-r*.8f,r*.55f}}));if(r>3.f)hole(g,{p+P{-r*.3f,-r*.3f},p+P{r*.5f,-r*.45f},p+P{r*.2f,r*.3f}});}
    void shoeBones(juce::Graphics& g,P ankle,float direction)const{
        const auto q=[&](float x,float y){return ankle+P{x*direction,y};};
        plate(g,poly({q(-5,-3),q(3,-5),q(7,1),q(13,6),q(20,8),q(23,13),q(17,16),q(8,13),q(2,9),q(-6,10),q(-8,4)}));
        hole(g,{q(-3,1),q(2,0),q(3,5),q(-2,7)});
        // Compact, blunt phalanges extend from a coherent forefoot rather than
        // splaying into long spikes at normal plugin scale.
        for(int i=0;i<4;++i){const float k=(float)i;const auto toeStart=q(12+k*2.8f,9+k*.65f),tip=q(19+k*2.1f,15+k*.5f);longBone(g,toeStart,tip,2.6f-.19f*k,2.0f-.14f*k);}
        longBone(g,q(3,8),q(13,17),3.1f,2.6f);
    }
    void bodyArt(juce::Graphics& g)const{
        // Far leg, then the foreground crossed leg. Knees and ankles are chunky.
        longBone(g,{188,203},{223,263},7.8f,7.1f,-1);knuckle(g,{223,263},7.f);
        longBone(g,{222,270},{226,325},6.6f,5.2f,2);longBone(g,{229,271},{232,325},2.8f,2.4f);shoeBones(g,{229,327},1);
        longBone(g,{139,205},{193,269},8.4f,7.5f,-8);knuckle(g,{193,269},7.5f);
        longBone(g,{189,276},{145,351},6.2f,5.0f,-4);longBone(g,{197,277},{153,351},3.4f,2.7f);shoeBones(g,{147,350},1);
        // The near iliac crest sweeps forward; the far wing is foreshortened.
        Path hip;hip.startNewSubPath(140,170);hip.cubicTo(132,162,120,167,123,181);hip.cubicTo(121,190,128,195,131,197);hip.cubicTo(123,203,130,216,141,221);hip.cubicTo(152,228,158,215,165,214);hip.cubicTo(177,216,190,222,195,208);hip.cubicTo(198,198,190,194,192,187);hip.cubicTo(201,176,192,163,183,166);hip.cubicTo(174,166,168,179,171,191);hip.cubicTo(165,200,157,194,153,188);hip.cubicTo(148,181,147,174,140,170);hip.closeSubPath();plate(g,hip);
        Path ilium;ilium.startNewSubPath(132,172);ilium.cubicTo(140,170,143,178,147,186);ilium.cubicTo(150,195,144,194,140,190);ilium.cubicTo(134,188,135,181,131,181);ilium.quadraticTo(127,177,132,172);ilium.closeSubPath();fill(g,ilium,ink);
        Path farIlium;farIlium.startNewSubPath(181,175);farIlium.cubicTo(186,168,193,172,192,181);farIlium.cubicTo(188,186,189,192,183,194);farIlium.cubicTo(178,192,177,180,181,175);farIlium.closeSubPath();fill(g,farIlium,ink);
        Path sockets;sockets.startNewSubPath(132,202);sockets.cubicTo(137,196,146,199,150,205);sockets.cubicTo(152,210,143,214,137,212);sockets.quadraticTo(131,209,132,202);sockets.closeSubPath();fill(g,sockets,ink);
        Path farSocket;farSocket.startNewSubPath(175,205);farSocket.cubicTo(178,199,185,198,190,201);farSocket.cubicTo(195,209,181,214,177,210);farSocket.closeSubPath();fill(g,farSocket,ink);
        hole(g,{{153,193},{161,200},{167,196},{172,202},{168,208},{161,211},{155,205}});
        // Connected vertebral column, with tightly interlocking facets.
        Path spine;spine.startNewSubPath(190,49);spine.cubicTo(184,74,167,120,157,192);stroke(g,spine,11.f,ink);
        for(int i=0;i<18;++i){const float t=i/17.f,y=51.f+140.f*t,x=190.f-33.f*t+3.f*std::sin(t*6.f),w=3.5f+2.5f*t;
            plate(g,poly({{x-w-1,y-3},{x-1,y-2.6f},{x+w,y-3.6f},{x+w+1,y+.5f},{x+w-1,y+3.1f},{x,y+4.2f},{x-w,y+2.5f},{x-w-1,y+.2f}}));
            if(i%4==0)hole(g,{{x-2,y-.8f},{x+2,y-1.5f},{x+1,y+1.2f}});
        }
        // Oblique thorax, tapering into an open costal arch.
        Path cage;cage.startNewSubPath(150,65);cage.cubicTo(133,61,124,80,124,99);cage.cubicTo(121,119,127,145,134,157);cage.quadraticTo(147,156,166,137);cage.lineTo(179,127);cage.cubicTo(186,136,183,150,190,154);cage.cubicTo(205,144,208,115,219,98);cage.quadraticTo(209,82,192,82);cage.cubicTo(175,72,164,67,150,65);cage.closeSubPath();fill(g,cage,ink);
        // Individually authored rib contours rather than evenly spaced bars.
        struct Rib {P root, outer, turn, tip;float thickness;};
        const Rib ribs[]={
            {{149,71},{135,76},{150,94},{183,95},3.1f},
            {{143,79},{129,88},{146,106},{182,104},3.9f},
            {{138,89},{124,100},{142,116},{178,115},4.4f},
            {{134,99},{123,113},{139,130},{174,125},4.0f},
            {{132,111},{124,126},{140,141},{166,135},4.4f},
            {{131,123},{129,138},{143,148},{157,145},3.5f},
            {{133,136},{133,149},{143,154},{151,149},3.0f}
        };
        for(const auto& rib:ribs){Path crescent;crescent.startNewSubPath(rib.root);crescent.cubicTo(rib.outer+P{-1,-1},rib.outer,rib.turn);crescent.quadraticTo(rib.turn+P{10,6},rib.tip);crescent.lineTo(rib.tip+P{-1,-rib.thickness});crescent.quadraticTo(rib.turn+P{8,6-rib.thickness},rib.turn+P{2,-rib.thickness});crescent.cubicTo(rib.outer+P{4,1},rib.outer+P{6,-2},rib.root+P{2,0});crescent.closeSubPath();fill(g,crescent,bone);}
        Path outer;outer.startNewSubPath(143,73);outer.cubicTo(126,76,123,101,128,124);outer.quadraticTo(129,138,136,149);stroke(g,outer,2.8f,bone);
        const float farRibs[6][4]={{192,88,214,97},{191,98,213,106},{188,107,209,117},{186,117,205,126},{183,126,200,137},{181,134,194,146}};
        for(const auto& r:farRibs){const float x=r[0],y=r[1],sideX=r[2],tipY=r[3];Path crescent;crescent.startNewSubPath(x,y);crescent.cubicTo(sideX,y-3,sideX+2,tipY-2,x-3,tipY+3);crescent.lineTo(x-4,tipY-.5f);crescent.cubicTo(sideX-4,tipY-4,sideX-5,y+1,x,y+2.5f);crescent.closeSubPath();fill(g,crescent,bone);}
        Path sternum;sternum.startNewSubPath(185,82);sternum.quadraticTo(195,83,193,91);sternum.cubicTo(189,103,184,116,183,129);sternum.quadraticTo(180,139,173,142);sternum.lineTo(171,134);sternum.cubicTo(179,120,177,103,180,93);sternum.closeSubPath();plate(g,sternum);hole(g,{{183,110},{186,108},{184,119}});
        Path costal;costal.startNewSubPath(136,149);costal.cubicTo(143,156,156,143,175,130);costal.quadraticTo(186,137,191,149);stroke(g,costal,4.7f,bone);
        longBone(g,{144,68},{184,85},5.6f,4.3f,3);longBone(g,{190,85},{216,91},4.2f,5.6f,-3);
        // Mic-side elbow and gripping hand, static relative to the microphone.
        longBone(g,{215,94},{262,135},7.f,5.8f,2);knuckle(g,{262,135},6.f);
        longBone(g,{260,131},{237,84},6.3f,4.3f,1);longBone(g,{267,130},{243,84},2.8f,2.4f);
        plate(g,poly({{229,73},{238,68},{246,72},{249,78},{247,83},{241,93},{232,90},{228,82}}));hole(g,{{231,76},{238,74},{244,77},{242,80},{234,80}});hole(g,{{233,84},{241,82},{242,86},{235,89}});hole(g,{{243,73},{246,74},{246,77},{243,76}});
    }
    void armArt(juce::Graphics& g)const{
        // Open palm silhouette, splayed curved fingers and a nearly level upper arm.
        longBone(g,{145,68},{109,78},6.6f,6.2f,2);knuckle(g,{109,78},6.1f);
        longBone(g,{105,75},{66,60},5.6f,4.2f,0);longBone(g,{106,82},{64,65},2.8f,2.7f);
        plate(g,poly({{64,54},{57,45},{47,45},{38,51},{39,61},{49,67},{60,66},{70,62}}));hole(g,{{43,51},{48,50},{53,53},{50,58},{44,56}});hole(g,{{55,58},{60,54},{63,59},{59,63}});hole(g,{{47,61},{51,60},{53,64},{49,65}});
        const P bases[4]{{43,54},{45,49},{50,46},{55,48}};const P mids[4]{{32,49},{32,43},{37,39},{46,38}};const P ends[4]{{21,41},{20,33},{29,31},{38,30}};
        for(int i=0;i<4;++i){longBone(g,bases[i],mids[i],3.4f,2.8f);knuckle(g,mids[i],2.65f);longBone(g,mids[i],ends[i],2.7f,2.0f);}
        longBone(g,{62,51},{59,31},3.9f,3.0f);knuckle(g,{59,31},2.9f);longBone(g,{59,31},{46,13},3.0f,2.0f);knuckle(g,{145,68},7.4f);
    }
    void skullArt(juce::Graphics& g)const{
        // Rounded cranial vault, asymmetric temporal fossa and cheek arch.
        Path cr;cr.startNewSubPath(170,43);cr.cubicTo(162,33,166,17,178,9);cr.cubicTo(187,2,200,1,211,6);cr.cubicTo(222,10,229,19,230,30);cr.quadraticTo(234,35,232,40);cr.lineTo(228,43);cr.quadraticTo(235,48,230,53);cr.lineTo(224,55);cr.lineTo(214,52);cr.lineTo(208,48);cr.cubicTo(202,46,198,53,191,54);cr.cubicTo(182,53,174,49,170,43);cr.closeSubPath();plate(g,cr);
        Path temple;temple.startNewSubPath(169,32);temple.cubicTo(173,29,174,23,177,19);temple.lineTo(180,22);temple.cubicTo(176,29,179,35,184,36);temple.cubicTo(190,33,194,28,194,20);temple.quadraticTo(198,16,201,21);temple.cubicTo(202,30,194,37,190,43);temple.quadraticTo(185,52,179,46);temple.cubicTo(172,45,169,41,169,32);temple.closeSubPath();fill(g,temple,ink);
        Path orbit;orbit.startNewSubPath(206,22);orbit.cubicTo(211,17,221,17,221,24);orbit.cubicTo(221,30,217,34,211,34);orbit.cubicTo(204,33,203,27,206,22);orbit.closeSubPath();fill(g,orbit,ink);
        Path nasal;nasal.startNewSubPath(228,30);nasal.quadraticTo(224,31,222,36);nasal.quadraticTo(219,42,224,44);nasal.lineTo(230,40);nasal.quadraticTo(229,35,228,30);nasal.closeSubPath();fill(g,nasal,ink);
        Path cheek;cheek.startNewSubPath(187,42);cheek.cubicTo(196,34,202,35,210,37);cheek.quadraticTo(215,39,211,44);cheek.cubicTo(206,46,200,43,197,49);cheek.quadraticTo(191,51,187,47);cheek.closeSubPath();fill(g,cheek,ink);
        hole(g,{{214,45},{217,42},{220,47},{223,45},{229,50},{223,51},{217,49}});
        Path suture;suture.startNewSubPath(178,13);suture.quadraticTo(183,8,190,7);suture.lineTo(188,10);suture.quadraticTo(184,9,178,13);fill(g,suture,ink);
        hole(g,{{173,39},{177,37},{180,40},{177,43}});hole(g,{{182,46},{185,44},{187,47},{184,49}});
        for(int i=0;i<5;++i){const float x=209.f+i*4.f,y=48.f+(i%2)*.9f;plate(g,poly({{x,y},{x+3.5f,y+.8f},{x+3.f,y+5.7f},{x+.5f,y+5.1f}}));}
    }
    void jawArt(juce::Graphics& g)const{
        Path jawContour;jawContour.startNewSubPath(190,43);jawContour.quadraticTo(195,43,197,48);jawContour.cubicTo(195,55,198,65,204,70);jawContour.quadraticTo(213,78,221,69);jawContour.lineTo(225,72);jawContour.cubicTo(224,79,216,82,210,80);jawContour.cubicTo(199,76,190,65,188,55);jawContour.quadraticTo(188,49,190,43);jawContour.closeSubPath();plate(g,jawContour);
        Path ramus;ramus.startNewSubPath(191,50);ramus.quadraticTo(195,49,195,56);ramus.quadraticTo(194,61,199,66);ramus.quadraticTo(193,64,191,57);ramus.closeSubPath();fill(g,ramus,ink);
        hole(g,{{204,71},{211,76},{219,73},{216,77},{210,78}});
        for(int i=0;i<4;++i){const float x=207.f+i*3.5f,y=70.f-(i>1?1.2f:0.f);plate(g,poly({{x,y},{x+.6f,y-4.5f},{x+2.9f,y-4.1f},{x+3.f,y}}));}
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
        g.setGradientFill(juce::ColourGradient(ink.withAlpha(.85f),151,365,ink.withAlpha(0.f),151,377,false));g.fillEllipse(128,359,146,15);
        cache(body,{112,43,170,329},[this](auto& art){bodyArt(art);});cache(arm,{10,5,147,87},[this](auto& art){armArt(art);});cache(head,{162,0,78,62},[this](auto& art){skullArt(art);});cache(jaw,{184,40,48,42},[this](auto& art){jawArt(art);});
        g.addTransform(bodyLean(react,phase));cableAndStand(g,react,phase);blit(g,body,112,43);
        {juce::Graphics::ScopedSaveState armPose(g);g.addTransform(juce::AffineTransform::rotation(.06f*react*std::sin(phase*.68f),145,68));blit(g,arm,10,5);}
        {juce::Graphics::ScopedSaveState skullPose(g);g.addTransform(juce::AffineTransform::rotation(-.02f+.04f*react*std::sin(phase*.73f),193,53));blit(g,head,162,0);g.addTransform(juce::AffineTransform::rotation(.17f+.27f*std::sqrt(voice),193,46));blit(g,jaw,184,40);}
        microphone(g,react);
    }
private:mutable juce::Image body,arm,head,jaw;
};
} // namespace skellpreset::stage
