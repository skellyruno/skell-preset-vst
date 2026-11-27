#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>
namespace skellpreset::stage {
// Original three-quarter anatomical performer; see OpenStax A&P 2e 7.2–8.4.
// Model coordinates remain 345 x 372; only presentation changes.
class Performer {
    using P=juce::Point<float>;using Path=juce::Path;using C=juce::Colour;
    const C bone{0xffd8d5c9}, shade{0xff485650}, dark{0xff0c1416};
    static Path poly(std::initializer_list<P> pts){Path p;bool first=true;for(auto v:pts){if(first){p.startNewSubPath(v);first=false;}else p.lineTo(v);}p.closeSubPath();return p;}
    void line(juce::Graphics& g,const Path& p,float w,C c)const{g.setColour(c);g.strokePath(p,juce::PathStrokeType(w,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
    void solid(juce::Graphics& g,const Path& p,P a,P b,float light=1.f)const{
        juce::ColourGradient grad(bone.interpolatedWith(shade,1.f-light),a,shade.interpolatedWith(dark,.18f),b,false);
        grad.addColour(.18,bone.interpolatedWith(shade,1.f-light*.98f));grad.addColour(.42,bone.interpolatedWith(shade,1.f-light*.72f));grad.addColour(.72,bone.interpolatedWith(shade,1.f-light*.43f));g.setGradientFill(grad);g.fillPath(p);line(g,p,.60f,dark.withAlpha(.86f));
    }
    void limb(juce::Graphics& g,P a,P b,float ra,float rb,float light=1.f,float bend=0.f)const{
        const P d=b-a;const float len=juce::jmax(1.f,d.getDistanceFromOrigin());const P u=d/len,n{-u.y,u.x},m=(a+b)*.5f+n*bend;const float w=(ra+rb)*.31f;
        Path p;p.startNewSubPath(a+n*ra);p.cubicTo(a+u*len*.19f+n*ra*.65f,m-u*len*.15f+n*w,m+n*w);p.cubicTo(m+u*len*.17f+n*w,b-u*len*.17f+n*rb*.7f,b+n*rb);p.cubicTo(b+u*rb+n*rb,b+u*rb-n*rb,b-n*rb);p.cubicTo(b-u*len*.17f-n*rb*.7f,m+u*len*.17f-n*w,m-n*w);p.cubicTo(m-u*len*.15f-n*w,a+u*len*.19f-n*ra*.65f,a-n*ra);p.cubicTo(a-u*ra-n*ra,a-u*ra+n*ra,a+n*ra);p.closeSubPath();solid(g,p,m-n*juce::jmax(ra,rb),m+n*juce::jmax(ra,rb),light);
        Path ridge;ridge.startNewSubPath(a+u*len*.20f-n*w*.60f);ridge.quadraticTo(m-n*w*.65f,b-u*len*.20f-n*w*.40f);line(g,ridge,.65f,bone.withAlpha(.46f*light));
        Path groove;groove.startNewSubPath(a+u*len*.24f+n*w*.56f);groove.quadraticTo(m+n*w*.60f,b-u*len*.23f+n*w*.38f);line(g,groove,.42f,shade.withAlpha(.34f*light));
    }
    void joint(juce::Graphics& g,P p,float x,float y,float light=1.f)const{Path a;a.addEllipse(p.x-x,p.y-y,x*2,y*2);solid(g,a,p-P{x*.6f,y},p+P{x,y},light);}
    void feet(juce::Graphics& g,P a,float dir,float light)const{joint(g,a+P{0.f,0.f},4.3f,4.7f,light);limb(g,a,a+P{-dir*4.f,7.f},3.5f,2.8f,light);joint(g,a+P{-dir*4.f,7.f},4,3,light);for(int i=0;i<5;++i){const P b=a+P{dir*(20.f-i*2.4f),10.f+i*.4f};limb(g,a+P{dir*(2.f+i*1.4f),4.f+i*.5f},b,1.7f,1.1f,light);limb(g,b,b+P{dir*(5.f-i*.5f),1},1,.72f,light);}}
    void bodyArt(juce::Graphics& g)const{
        // Bent leading leg and foreshortened trailing leg, not a mirrored A-pose.
        limb(g,{165,210},{196,280},6.7f,5.7f,.73f,-3);joint(g,{196,280},5.8f,5,.76f);joint(g,{194,280},3.5f,4.5f,.82f);limb(g,{197,284},{214,349},4.9f,3.6f,.75f);limb(g,{203,287},{218,345},1.8f,1.6f,.64f);feet(g,{214,351},1,.74f);
        limb(g,{131,212},{111,282},7.3f,6.2f,1,3.4f);joint(g,{111,284},6.7f,5.3f);joint(g,{109,284},3.9f,4.8f,.96f);limb(g,{111,288},{131,350},5.6f,4.2f,.96f,-2.4f);limb(g,{118,290},{136,348},2.1f,1.8f,.77f);feet(g,{131,352},-1,.94f);
        // Vertebral column behind the cage, with transverse processes.
        for(int k=0;k<20;++k){const float t=k/19.f,y=67.f+130.f*t,x=147.f+6.f*std::sin(t*3.7f),w=4.1f+3.f*t;auto p=poly({{x-w,y-2},{x-w-3,y+1},{x-w,y+2},{x,y+4},{x+w,y+2},{x+w+3,y+1},{x+w,y-2},{x,y-3}});solid(g,p,{x-w,y-3},{x+w,y+4},.77f);}
        solid(g,poly({{140,179},{157,176},{165,192},{154,210},{147,205}}),{141,178},{160,211},.67f);
        for(int side:{-1,1}){const float s=(float)side,w=side<0?1.f:.74f,y=side<0?3.f:-2.f;auto q=[&](float x,float v){return P{151+s*x*w,v+y};};Path hip;hip.startNewSubPath(q(5,178));hip.cubicTo(q(11,168),q(33,166),q(34,181));hip.cubicTo(q(36,193),q(24,199),q(24,209));hip.cubicTo(q(19,223),q(5,217),q(2,207));hip.quadraticTo(q(14,195),q(5,178));hip.closeSubPath();Path hole;hole.startNewSubPath(q(8,201));hole.cubicTo(q(17,195),q(22,198),q(20,205));hole.cubicTo(q(17,213),q(9,214),q(8,201));hole.closeSubPath();hip.addPath(hole);hip.setUsingNonZeroWinding(false);solid(g,hip,q(24,171),q(6,217),side<0?.95f:.79f);Path crest;crest.startNewSubPath(q(7,178));crest.cubicTo(q(17,168),q(32,170),q(34,181));line(g,crest,1.5f,bone.withAlpha(.65f));Path iliac;iliac.startNewSubPath(q(8,180));iliac.cubicTo(q(21,176),q(31,181),q(24,192));iliac.quadraticTo(q(16,197),q(10,192));iliac.closeSubPath();g.setGradientFill(juce::ColourGradient(shade.withAlpha(.48f),q(13,180),shade.withAlpha(0.f),q(25,191),false));g.fillPath(iliac);}
        limb(g,{142,211},{155,213},2.7f,2.1f,.83f);
        // Rotated thorax: the posterior ribs remain visible through the gaps.
        const float reach[12]{15,22,29,34,38,40,40,38,35,30,23,15};
        for(int pass=0;pass<2;++pass)for(int k=0;k<12;++k)for(int side:{-1,1}){const float s=(float)side,w=reach[k]*(side<0?1.f:.65f),y=88.f+k*6.1f+(side>0?-3.f:0.f),inner=side<0?154.f:157.f,edge=inner+s*w,fy=y+10.f+k*1.38f;Path p;if(pass==0){p.startNewSubPath(150.f,y);p.cubicTo(150.f+s*w*.5f,y-5,edge,y-2,edge,y+4);line(g,p,1.8f,shade.withAlpha(.75f));}else{const float end=k<7?inner+s*4.7f:k<10?inner+s*(10.f+(k-7)*5.f):edge-s*7.f;p.startNewSubPath(edge,y+3);p.cubicTo(edge+s*1.5f,y+12,inner+s*w*.62f,fy+3,end,fy);line(g,p,3.4f,dark);line(g,p,k<2?2.20f:k>9?1.65f:2.65f,bone.interpolatedWith(shade,side<0?.18f:.51f));Path glint;glint.startNewSubPath(edge,y+4);glint.quadraticTo(edge,fy-1,inner+s*w*.53f,fy+1);line(g,glint,.65f,bone.withAlpha(side<0?.68f:.32f));if(k>=7&&k<10){Path cart;cart.startNewSubPath(end,fy);cart.quadraticTo(inner+s*8,fy-2,inner+s*5,151.f+(k-7)*2);line(g,cart,1.4f,bone.interpolatedWith(shade,.50f));}}}
        Path st;st.startNewSubPath(151,94);st.quadraticTo(155,90,160,94);st.lineTo(162,105);st.quadraticTo(157,126,159,145);st.lineTo(155,155);st.lineTo(151,146);st.lineTo(153,108);st.closeSubPath();solid(g,st,{151,94},{162,151},.95f);
        Path cl;cl.startNewSubPath(155,96);cl.cubicTo(140,88,119,85,107,94);cl.cubicTo(123,91,140,99,155,102);cl.closeSubPath();solid(g,cl,{110,88},{155,104});Path cr;cr.startNewSubPath(158,94);cr.cubicTo(172,86,181,84,192,88);cr.cubicTo(180,91,168,101,158,101);cr.closeSubPath();solid(g,cr,{161,86},{192,102},.79f);
    }
    void craniumArt(juce::Graphics& g)const{
        Path cr;cr.startNewSubPath(126,40);cr.cubicTo(120,29,126,15,139,11);cr.cubicTo(153,6,168,12,174,23);cr.cubicTo(179,33,175,42,173,47);cr.quadraticTo(178,50,172,54);cr.lineTo(169,57);cr.quadraticTo(168,61,166,64);cr.lineTo(151,66);cr.quadraticTo(143,60,139,56);cr.cubicTo(129,57,124,49,126,40);cr.closeSubPath();solid(g,cr,{133,13},{174,60});
        {juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(cr);g.setGradientFill(juce::ColourGradient(bone.withAlpha(.48f),136,17,bone.withAlpha(0.f),158,34,true));g.fillEllipse(123,7,45,38);g.setGradientFill(juce::ColourGradient(shade.withAlpha(.58f),125,44,shade.withAlpha(0.f),145,23,true));g.fillRect(119.f,9.f,48.f,49.f);Path suture;suture.startNewSubPath(139,10);suture.lineTo(141,14);suture.lineTo(138,18);suture.lineTo(140,21);suture.lineTo(138,25);suture.lineTo(140,29);line(g,suture,.65f,shade.withAlpha(.40f));for(int k=0;k<90;++k){float x=124.f+(float)((k*47)%51),y=9.f+(float)((k*31)%58);g.setColour((k%4?shade:bone).withAlpha(.12f));g.fillEllipse(x,y,.65f+(k%3)*.2f,.55f);}}
        Path orbit;orbit.startNewSubPath(145,36);orbit.cubicTo(149,31,157,29,161,34);orbit.cubicTo(163,39,158,44,154,45);orbit.cubicTo(147,47,143,42,145,36);orbit.closeSubPath();Path far;far.startNewSubPath(168,34);far.quadraticTo(174,32,175,37);far.cubicTo(174,43,171,45,168,44);far.quadraticTo(165,40,168,34);far.closeSubPath();for(const auto* hole:{&orbit,&far}){auto b=hole->getBounds();g.setGradientFill(juce::ColourGradient(dark.darker(.8f),b.getCentreX(),b.getY(),shade.darker(.65f),b.getCentreX(),b.getBottom(),false));g.fillPath(*hole);line(g,*hole,.85f,shade.darker(.28f));}
        Path temple;temple.startNewSubPath(128,37);temple.cubicTo(134,32,140,37,139,45);temple.quadraticTo(133,52,128,47);temple.closeSubPath();g.setGradientFill(juce::ColourGradient(dark.withAlpha(.50f),128,44,dark.withAlpha(.03f),141,33,false));g.fillPath(temple);
        Path cheek;cheek.startNewSubPath(132,48);cheek.quadraticTo(143,44,150,48);cheek.quadraticTo(157,50,155,54);cheek.quadraticTo(145,53,138,56);cheek.closeSubPath();solid(g,cheek,{137,47},{151,56},.94f);Path nose;nose.startNewSubPath(164,42);nose.cubicTo(160,46,157,51,160,54);nose.quadraticTo(163,56,165,52);nose.quadraticTo(169,55,170,51);nose.quadraticTo(168,45,164,42);nose.closeSubPath();g.setColour(dark.darker(.7f));g.fillPath(nose);
        Path brow;brow.startNewSubPath(143,34);brow.quadraticTo(151,27,161,32);brow.startNewSubPath(167,31);brow.quadraticTo(171,29,175,33);line(g,brow,.85f,bone.brighter(.06f));
        Path cheekRidge;cheekRidge.startNewSubPath(130,45);cheekRidge.cubicTo(136,43,144,47,147,50);line(g,cheekRidge,1.05f,bone.withAlpha(.56f));
        // Temporal and zygomatic contours follow the socket rather than
        // a single flat cheek plate. These details are rasterized once.
        Path arch;arch.startNewSubPath(130,43);arch.cubicTo(131,39,137,40,141,44);arch.quadraticTo(145,46,147,47);
        line(g,arch,.80f,shade.withAlpha(.62f));
        Path maxilla;maxilla.startNewSubPath(154,54);maxilla.quadraticTo(158,51,162,54);
        line(g,maxilla,.72f,bone.withAlpha(.60f));
    }
    void skull(juce::Graphics& g,float jaw,float turn,float react)const{
        juce::Graphics::ScopedSaveState pose(g);g.addTransform(juce::AffineTransform::scale(.90f,.94f,150.f,65.f).followedBy(juce::AffineTransform::rotation(turn,150,65)));
        if(!cranium.isValid()){
            cranium=juce::Image(juce::Image::ARGB,144,144,true);
            juce::Graphics art(cranium);
            art.addTransform(juce::AffineTransform::translation(-112.f,0.f).scaled(2.f));
            craniumArt(art);
        }
        g.setOpacity(1.f);g.drawImageTransformed(cranium,juce::AffineTransform::scale(.5f).translated(112.f,0.f));
        // Jaw rotates around condyle, teeth retain dimensions at every level.
        const float gape=.003f+.17f*std::sqrt(jaw);auto mouth=poly({{146,55},{170,55},{170,65+jaw*5},{153,71+jaw*6},{140,56}});g.setColour(dark.darker(.7f));g.fillPath(mouth);
        for(int k=0;k<8;++k){const float x=150.f+k*2.45f,y=59.4f-std::sin(k*.29f)*1.6f;Path t;t.addRoundedRectangle(x,y,1.88f,3.30f-(k>5?.55f:0.f)+(k%3)*.12f,.38f);solid(g,t,{x,y},{x+2,y+4},.97f);}
        {juce::Graphics::ScopedSaveState mandible(g);g.addTransform(juce::AffineTransform::rotation(gape,138,48));Path lower;lower.startNewSubPath(136,47);lower.quadraticTo(141,47,142,53);lower.lineTo(144,63);lower.quadraticTo(151,69,165,66);lower.lineTo(171,63);lower.quadraticTo(174,70,164,73);lower.cubicTo(151,77,139,69,137,61);lower.closeSubPath();solid(g,lower,{141,51},{169,75},.90f);for(int k=0;k<7;++k){float x=151.f+k*2.45f,y=64.2f-std::sin(k*.3f)*1.6f;Path t;t.addRoundedRectangle(x,y,1.92f,2.95f+(k%2)*.18f,.4f);solid(g,t,{x,y},{x+2,y+3},.88f);}}
        g.setGradientFill(juce::ColourGradient(Theme::green.withAlpha(.06f+.19f*react),153,39,Theme::green.withAlpha(0.f),160,42,true));g.fillEllipse(146,32,15,15);
        // Small rim glints make the head read as lit bone, not a flat cartoon.
        Path edge;edge.startNewSubPath(128,30);edge.cubicTo(128,16,143,10,153,12);line(g,edge,.8f,C(0xffa9c6c0).withAlpha(.64f));
    }
    void arms(juce::Graphics& g,float activity,float phase)const{
        const float sway=std::sin(phase*.68f)*activity;
        const float lift=(.5f+.5f*std::sin(phase*.46f+1.2f))*activity;
        const P sh{107,95};
        P wr{47-sway*7-9.f*lift,172-sway*12-37.f*lift};
        // Two-link arm with fixed bone lengths. Solve the elbow on the
        // same side as the resting pose, rather than stretching the humerus.
        constexpr float upper=56.6f,lower=41.7f;
        P delta=wr-sh;const float distance=juce::jlimit(upper-lower+.1f,upper+lower-.1f,delta.getDistanceFromOrigin());
        const P direction=delta/juce::jmax(.001f,delta.getDistanceFromOrigin());
        wr=sh+direction*distance;
        const float along=(upper*upper-lower*lower+distance*distance)/(2.f*distance);
        const float bend=std::sqrt(juce::jmax(0.f,upper*upper-along*along));
        const P el=sh+direction*along+P{direction.y,-direction.x}*bend;
        joint(g,sh,5.6f,5.2f);limb(g,sh,el,6.3f,4.8f,.96f,-2);joint(g,el,4.3f,4.2f);limb(g,el+P{1,1},wr,3.4f,2.6f,.92f);limb(g,el+P{5,1},wr+P{4,3},2.4f,2.1f,.79f);joint(g,wr,3.3f,2.9f);for(int k=0;k<4;++k){const P a=wr+P{-4.f,((float)k-1.5f)*2.2f},b=wr+P{-12.f,((float)k-1.5f)*3.1f+7.f};limb(g,wr,a,1.35f,1.2f,.97f);limb(g,a,b,1.1f,.85f,.92f);limb(g,b,b+P{-3,3},.85f,.65f,.88f);}limb(g,wr+P{1,3},wr+P{-4,9},1.4f,1,.93f);
        const P rs{191,89},re{213,139},rw{191,83};joint(g,rs,5.2f,4.8f,.77f);limb(g,rs,re,5.7f,4.5f,.77f,2);joint(g,re,4.4f,4.1f,.75f);limb(g,re,rw,3.8f,2.9f,.88f);limb(g,re+P{4,-1},rw+P{4,1},2.5f,2.1f,.71f);
        {juce::Graphics::ScopedSaveState mic(g);g.addTransform(juce::AffineTransform::rotation(-.48f,191,83));g.setGradientFill(juce::ColourGradient(C(0xffa5aaa4),187,75,C(0xff232a2b),195,75,false));g.fillRoundedRectangle(187,61,8,32,2);g.setColour(Theme::green.withAlpha(.42f+.42f*activity));g.fillRect(187,67,8,2);g.setGradientFill(juce::ColourGradient(C(0xffe0ded1),187,54,C(0xff323c3c),196,65,false));g.fillEllipse(183,49,16,19);Path cap;cap.addEllipse(183,49,16,19);g.reduceClipRegion(cap);g.setColour(dark.withAlpha(.68f));for(int k=0;k<12;++k){g.drawLine(180.f+k*2,49,188.f+k*2,68,.55f);g.drawLine(180.f+k*2,68,188.f+k*2,49,.55f);}}
        for(int k=0;k<4;++k){const P a{187.f+k*.8f,77.f+k*3.1f};limb(g,a,a+P{8,1.7f},1.45f,1.05f,.96f);limb(g,a+P{8,1.7f},a+P{9,4},1.05f,.8f,.82f);}
        Path cable;cable.startNewSubPath(196,92);cable.cubicTo(229,136,236,222,237,307);cable.quadraticTo(236,366,270,367);line(g,cable,1.7f,dark);line(g,cable,.55f,shade.withAlpha(.63f));
    }
public:
    static juce::AffineTransform bodyLean(float activity,float phase) noexcept {
        const float lean=(.012f*std::sin(phase*.41f)+.028f*std::sin(phase*.71f))*juce::jlimit(0.f,1.f,activity);
        return juce::AffineTransform::rotation(lean,151.f,365.f);
    }
    // Same microphone geometry, pose and transforms as arms(), not an
    // unrelated fixed rectangle over the character or its neighbours.
    static juce::Rectangle<int> microphoneHitBounds(float x,float y,float scale,float activity,float phase) noexcept {
        return juce::Rectangle<float>(183.f,49.f,16.f,44.f)
            .transformedBy(juce::AffineTransform::rotation(-.48f,191.f,83.f)
                .followedBy(bodyLean(activity,phase))
                .followedBy(juce::AffineTransform::scale(scale).translated(x,y)))
            .expanded(5.f).toNearestInt();
    }
    void draw(juce::Graphics& g,float x,float y,float scale,float activity,float phase,float vocal=-1.f)const{
        const float react=juce::jlimit(0.f,1.f,activity),voice=juce::jlimit(0.f,1.f,vocal<0.f?react:vocal);juce::Graphics::ScopedSaveState state(g);g.addTransform(juce::AffineTransform::scale(scale).translated(x,y));
        g.setGradientFill(juce::ColourGradient(dark.withAlpha(.86f),151,365,dark.withAlpha(0.f),151,377,false));g.fillEllipse(85,359,155,15);
        if(!body.isValid()||cachedTheme!=Theme::current){body=juce::Image(juce::Image::ARGB,690,744,true);cachedTheme=Theme::current;juce::Graphics art(body);art.addTransform(juce::AffineTransform::scale(2.f));bodyArt(art);}// Small coherent body lean pivots at the feet; jaw and hands move with the torso.
        g.addTransform(bodyLean(react,phase));
        g.setOpacity(1.f);g.drawImageTransformed(body,juce::AffineTransform::scale(.5f));arms(g,react,phase);skull(g,voice,-.075f+.075f*std::sin(phase*.73f)*react,react);
    }
private: mutable juce::Image body,cranium;mutable int cachedTheme=-1;
};
} // namespace skellpreset::stage
