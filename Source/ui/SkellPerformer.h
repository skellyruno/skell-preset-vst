#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>
namespace skellpreset::stage {
// Original three-quarter anatomical performer; see OpenStax A&P 2e 7.2–8.4.
// Model coordinates remain 345 x 372; only presentation changes.
class Performer {
    using P=juce::Point<float>;using Path=juce::Path;using C=juce::Colour;
    const C bone{0xfff5f0df}, shade{0xff626c64}, dark{0xff101715};
    static Path poly(std::initializer_list<P> pts){Path p;bool first=true;for(auto v:pts){if(first){p.startNewSubPath(v);first=false;}else p.lineTo(v);}p.closeSubPath();return p;}
    void line(juce::Graphics& g,const Path& p,float w,C c)const{g.setColour(c);g.strokePath(p,juce::PathStrokeType(w,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
    void solid(juce::Graphics& g,const Path& p,P a,P b,float light=1.f)const{
        juce::ColourGradient grad(bone.interpolatedWith(shade,1.f-light),a,bone.interpolatedWith(shade,.68f),b,false);
        grad.addColour(.18,bone.interpolatedWith(shade,1.f-light*.98f));grad.addColour(.42,bone.interpolatedWith(shade,1.f-light*.72f));grad.addColour(.72,bone.interpolatedWith(shade,1.f-light*.80f));g.setGradientFill(grad);g.fillPath(p);line(g,p,.80f,dark.withAlpha(.92f));
    }
    // Deterministic coarse ink erosion, baked into the existing anatomical caches.
    // Preserve antialiasing and transparent gaps; never animate random noise.
    void weather(juce::Image& image) const {
        juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readWrite);
        auto hash=[](unsigned x,unsigned y){unsigned h=x*374761393u+y*668265263u;h=(h^(h>>13))*1274126177u;return h^(h>>16);};
        for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x){
            auto c=pixels.getPixelColour(x,y);if(c.getAlpha()<210||c.getBrightness()<.30f)continue;
            const auto jitter=hash((unsigned)x/13u,(unsigned)y/11u);
            const auto coarse=hash(((unsigned)x+jitter%5u)/5u,((unsigned)y+(jitter>>8)%4u)/4u)%100u, fine=hash((unsigned)x,(unsigned)y)%100u;
            if(coarse<9u)c=c.interpolatedWith(dark,.43f);else if(coarse<20u)c=c.interpolatedWith(shade,.22f);
            if(fine<13u)c=c.interpolatedWith(dark,.19f);
            pixels.setPixelColour(x,y,c);
        }
    }
    void limb(juce::Graphics& g,P a,P b,float ra,float rb,float light=1.f,float bend=0.f)const{
        const P d=b-a;const float len=juce::jmax(1.f,d.getDistanceFromOrigin());const P u=d/len,n{-u.y,u.x},m=(a+b)*.5f+n*bend;const float w=(ra+rb)*.31f;
        Path p;p.startNewSubPath(a+n*ra);p.cubicTo(a+u*len*.19f+n*ra*.65f,m-u*len*.15f+n*w,m+n*w);p.cubicTo(m+u*len*.17f+n*w,b-u*len*.17f+n*rb*.7f,b+n*rb);p.cubicTo(b+u*rb+n*rb,b+u*rb-n*rb,b-n*rb);p.cubicTo(b-u*len*.17f-n*rb*.7f,m+u*len*.17f-n*w,m-n*w);p.cubicTo(m-u*len*.15f-n*w,a+u*len*.19f-n*ra*.65f,a-n*ra);p.cubicTo(a-u*ra-n*ra,a-u*ra+n*ra,a+n*ra);p.closeSubPath();solid(g,p,m-n*juce::jmax(ra,rb),m+n*juce::jmax(ra,rb),light);
        Path ridge;ridge.startNewSubPath(a+u*len*.20f-n*w*.60f);ridge.quadraticTo(m-n*w*.65f,b-u*len*.20f-n*w*.40f);line(g,ridge,.65f,bone.withAlpha(.46f*light));
        Path groove;groove.startNewSubPath(a+u*len*.24f+n*w*.56f);groove.quadraticTo(m+n*w*.60f,b-u*len*.23f+n*w*.38f);line(g,groove,.68f,shade.withAlpha(.70f*light));
        if(len>24.f&&ra>3.f){ // bony ridges and irregular cortical pits, not uniform cylinders
            const auto chip=poly({a+u*len*.13f-n*ra*.36f,a+u*len*.19f+n*ra*.12f,a+u*len*.29f-n*w*.26f,a+u*len*.20f-n*w*.64f});
            g.setColour(dark.withAlpha(.60f));g.fillPath(chip);
            const auto fleck=poly({m+u*len*.11f-n*w*.16f,m+u*len*.16f+n*w*.34f,m+u*len*.25f+n*w*.11f,m+u*len*.19f-n*w*.34f});
            g.setColour(shade.withAlpha(.72f));g.fillPath(fleck);
        }
    }
    void joint(juce::Graphics& g,P p,float x,float y,float light=1.f)const{Path a;a.addEllipse(p.x-x,p.y-y,x*2,y*2);solid(g,a,p-P{x*.6f,y},p+P{x,y},light);}
    void feet(juce::Graphics& g,P a,float dir,float light)const{joint(g,a+P{0.f,0.f},4.3f,4.7f,light);limb(g,a,a+P{-dir*4.f,7.f},3.5f,2.8f,light);joint(g,a+P{-dir*4.f,7.f},4,3,light);for(int i=0;i<5;++i){const P b=a+P{dir*(20.f-i*2.4f),10.f+i*.4f};limb(g,a+P{dir*(2.f+i*1.4f),4.f+i*.5f},b,1.7f,1.1f,light);limb(g,b,b+P{dir*(5.f-i*.5f),1},1,.72f,light);}}
    void bodyArt(juce::Graphics& g)const{
        // Bent leading leg and foreshortened trailing leg, not a mirrored A-pose.
        limb(g,{165,210},{207,272},7.f,6.f,.78f,-3);joint(g,{207,272},6.1f,5.4f,.80f);joint(g,{205,272},3.5f,4.5f,.88f);limb(g,{207,277},{212,347},5.f,3.8f,.79f);limb(g,{213,277},{218,345},2.1f,1.7f,.74f);feet(g,{212,349},1,.79f);
        limb(g,{131,212},{182,281},8.2f,7.f,1,-4.1f);joint(g,{182,281},7.f,5.8f);joint(g,{180,281},4.2f,4.8f,.96f);limb(g,{179,285},{139,350},5.8f,4.2f,.98f,-2.4f);limb(g,{187,286},{145,349},2.3f,1.8f,.87f);feet(g,{139,352},1,.98f);
        juce::Graphics::ScopedSaveState torso(g);g.addTransform(torsoPose());
        // Vertebral column behind the cage, with transverse processes.
        for(int k=0;k<20;++k){const float t=k/19.f,y=67.f+130.f*t,x=147.f+6.f*std::sin(t*3.7f),w=4.1f+3.f*t;auto p=poly({{x-w,y-2},{x-w-3,y+1},{x-w,y+2},{x,y+4},{x+w,y+2},{x+w+3,y+1},{x+w,y-2},{x,y-3}});solid(g,p,{x-w,y-3},{x+w,y+4},.77f);}
        solid(g,poly({{140,179},{157,176},{165,192},{154,210},{147,205}}),{141,178},{160,211},.67f);
        for(int side:{-1,1}){const float s=(float)side,w=side<0?1.f:.74f,y=side<0?3.f:-2.f;auto q=[&](float x,float v){return P{151+s*x*w,v+y};};Path hip;hip.startNewSubPath(q(5,178));hip.cubicTo(q(11,168),q(33,166),q(34,181));hip.cubicTo(q(36,193),q(24,199),q(24,209));hip.cubicTo(q(19,223),q(5,217),q(2,207));hip.quadraticTo(q(14,195),q(5,178));hip.closeSubPath();Path hole;hole.startNewSubPath(q(8,201));hole.cubicTo(q(17,195),q(22,198),q(20,205));hole.cubicTo(q(17,213),q(9,214),q(8,201));hole.closeSubPath();hip.addPath(hole);hip.setUsingNonZeroWinding(false);solid(g,hip,q(24,171),q(6,217),side<0?.95f:.79f);Path crest;crest.startNewSubPath(q(7,178));crest.cubicTo(q(17,168),q(32,170),q(34,181));line(g,crest,1.5f,bone.withAlpha(.65f));Path iliac;iliac.startNewSubPath(q(8,180));iliac.cubicTo(q(21,176),q(31,181),q(24,192));iliac.quadraticTo(q(16,197),q(10,192));iliac.closeSubPath();g.setGradientFill(juce::ColourGradient(shade.withAlpha(.48f),q(13,180),shade.withAlpha(0.f),q(25,191),false));g.fillPath(iliac);}
        limb(g,{142,211},{155,213},2.7f,2.1f,.83f);
        // Rotated thorax: the posterior ribs remain visible through the gaps.
        const float reach[12]{15,22,29,34,38,40,40,38,35,30,23,15};
        for(int pass=0;pass<2;++pass)for(int k=0;k<12;++k)for(int side:{-1,1}){const float s=(float)side,w=reach[k]*(side<0?1.f:.42f),y=88.f+k*6.1f+(side>0?-3.f:0.f),inner=side<0?154.f:157.f,edge=inner+s*w,fy=y+10.f+k*1.38f;Path p;if(pass==0){p.startNewSubPath(150.f,y);p.cubicTo(150.f+s*w*.5f,y-5,edge,y-2,edge,y+4);line(g,p,1.8f,shade.withAlpha(.75f));}else{const float end=k<7?inner+s*4.7f:k<10?inner+s*(10.f+(k-7)*5.f):edge-s*7.f;p.startNewSubPath(edge,y+3);p.cubicTo(edge+s*1.5f,y+12,inner+s*w*.62f,fy+3,end,fy);line(g,p,3.4f,dark);line(g,p,k<2?3.1f:k>9?2.1f:3.25f,bone.interpolatedWith(shade,side<0?.05f:.28f));Path glint;glint.startNewSubPath(edge,y+4);glint.quadraticTo(edge,fy-1,inner+s*w*.53f,fy+1);line(g,glint,.65f,bone.withAlpha(side<0?.68f:.32f));if(k>=7&&k<10){Path cart;cart.startNewSubPath(end,fy);cart.quadraticTo(inner+s*8,fy-2,inner+s*5,151.f+(k-7)*2);line(g,cart,1.4f,bone.interpolatedWith(shade,.50f));}}}
        Path st;st.startNewSubPath(151,94);st.quadraticTo(155,90,160,94);st.lineTo(162,105);st.quadraticTo(157,126,159,145);st.lineTo(155,155);st.lineTo(151,146);st.lineTo(153,108);st.closeSubPath();solid(g,st,{151,94},{162,151},.95f);
        Path cl;cl.startNewSubPath(155,96);cl.cubicTo(140,88,119,85,107,94);cl.cubicTo(123,91,140,99,155,102);cl.closeSubPath();solid(g,cl,{110,88},{155,104});Path cr;cr.startNewSubPath(158,94);cr.cubicTo(172,86,181,84,192,88);cr.cubicTo(180,91,168,101,158,101);cr.closeSubPath();solid(g,cr,{161,86},{192,102},.79f);
    }
    void craniumArt(juce::Graphics& g)const{
        // Right-facing cranium: occiput, brow projection, cheek arch and projecting maxilla.
        Path cr;cr.startNewSubPath(126,43);cr.cubicTo(118,34,122,19,134,13);cr.cubicTo(146,5,163,9,170,20);cr.quadraticTo(176,29,174,35);cr.lineTo(179,40);cr.lineTo(175,43);cr.lineTo(179,49);cr.lineTo(175,52);cr.lineTo(179,58);cr.lineTo(175,62);cr.lineTo(154,61);cr.lineTo(147,55);cr.cubicTo(135,59,124,52,126,43);cr.closeSubPath();solid(g,cr,{132,12},{174,61});
        // High-contrast angular planes read at plugin scale, with asymmetric hollows.
        auto ink=[&](std::initializer_list<P> pts,float opacity){g.setColour(dark.withAlpha(opacity));g.fillPath(poly(pts));};
        ink({{126,28},{132,16},{139,13},{135,22},{137,29},{131,32}},.68f);
        ink({{147,10},{156,12},{160,17},{155,21},{149,18}},.75f);
        ink({{128,40},{132,34},{140,36},{144,44},{138,49},{131,47}},.91f);
        Path orbit;orbit.startNewSubPath(153,32);orbit.lineTo(163,29);orbit.quadraticTo(172,31,171,38);orbit.lineTo(166,45);orbit.lineTo(158,44);orbit.lineTo(151,39);orbit.closeSubPath();g.setColour(dark);g.fillPath(orbit);
        ink({{173,43},{169,46},{168,52},{174,53},{176,50}},.98f);
        ink({{143,50},{150,46},{159,49},{164,54},{154,54},{148,57}},.82f);
        ink({{161,55},{169,54},{171,58},{166,60},{159,58}},.66f);
        Path brow;brow.startNewSubPath(150,31);brow.lineTo(160,26);brow.lineTo(169,28);brow.lineTo(174,34);line(g,brow,1.65f,bone);
        Path arch;arch.startNewSubPath(132,46);arch.cubicTo(140,43,145,43,151,46);arch.lineTo(158,49);line(g,arch,2.1f,bone);
        Path seam;seam.startNewSubPath(143,12);seam.lineTo(145,16);seam.lineTo(142,20);seam.lineTo(145,23);seam.lineTo(142,28);line(g,seam,.85f,dark.withAlpha(.72f));
        Path rear;rear.startNewSubPath(126,31);rear.quadraticTo(124,21,136,15);line(g,rear,1.15f,bone);

    }
    void skull(juce::Graphics& g,float jaw,float turn,float react)const{
        juce::Graphics::ScopedSaveState pose(g);g.addTransform(juce::AffineTransform::scale(.98f,.98f,150.f,65.f).followedBy(juce::AffineTransform::rotation(turn,150,65)));
        if(!cranium.isValid()){
            cranium=juce::Image(juce::Image::ARGB,144,144,true);
            juce::Graphics art(cranium);
            art.addTransform(juce::AffineTransform::translation(-112.f,0.f).scaled(2.f));
            craniumArt(art);
            weather(cranium);
        }
        g.setOpacity(1.f);g.drawImageTransformed(cranium,juce::AffineTransform::scale(.5f).translated(112.f,0.f));
        // Jaw rotates around condyle, teeth retain dimensions at every level.
        const float gape=.13f+.24f*std::sqrt(jaw);
        auto mouth=poly({{143,48},{153,56},{176,59},{176,68+jaw*6},{157,75+jaw*6},{139,57}});g.setColour(dark);g.fillPath(mouth);
        for(int k=0;k<7;++k){const float x=154.f+k*3.1f,y=60.f-(k%3)*.40f;auto tooth=poly({{x,y},{x+2.6f,y-.3f},{x+2.3f,y+3.3f},{x+.5f,y+3.8f}});g.setColour(bone.interpolatedWith(shade,.12f));g.fillPath(tooth);}
        {juce::Graphics::ScopedSaveState mandible(g);g.addTransform(juce::AffineTransform::rotation(gape,138,48));
            auto lower=poly({{135,46},{140,47},{144,57},{149,64},{161,67},{174,64},{176,68},{169,73},{157,76},{146,72},{139,64},{134,54}});solid(g,lower,{139,48},{169,76},.96f);
            g.setColour(dark.withAlpha(.78f));g.fillPath(poly({{138,51},{142,56},{143,63},{140,60}}));g.fillPath(poly({{151,70},{160,71},{166,69},{161,73},{154,74}}));
            for(int k=0;k<6;++k){const float x=155.f+k*3.1f,y=66.5f-(k%3)*.4f;g.setColour(bone);g.fillPath(poly({{x,y},{x+.4f,y-3.3f},{x+2.4f,y-3.1f},{x+2.6f,y}}));}}

        g.setGradientFill(juce::ColourGradient(Theme::green.withAlpha(.02f+.07f*react),153,39,Theme::green.withAlpha(0.f),160,42,true));g.fillEllipse(146,32,15,15);
        // Small rim glints make the head read as lit bone, not a flat cartoon.
        Path edge;edge.startNewSubPath(128,30);edge.cubicTo(128,16,143,10,153,12);line(g,edge,.8f,C(0xffa9c6c0).withAlpha(.64f));
    }
    void arms(juce::Graphics& g,float activity,float phase)const{
        const float sway=std::sin(phase*.68f)*activity;
        const float lift=(.5f+.5f*std::sin(phase*.46f+1.2f))*activity;
        const P sh{107,95};
        P wr{24-sway*4-4.f*lift,67-sway*8-14.f*lift};
        // Two-link arm with fixed bone lengths. Solve the elbow on the
        // same side as the resting pose, rather than stretching the humerus.
        constexpr float upper=56.6f,lower=41.7f;
        P delta=wr-sh;const float distance=juce::jlimit(upper-lower+.1f,upper+lower-.1f,delta.getDistanceFromOrigin());
        const P direction=delta/juce::jmax(.001f,delta.getDistanceFromOrigin());
        wr=sh+direction*distance;
        const float along=(upper*upper-lower*lower+distance*distance)/(2.f*distance);
        const float bend=std::sqrt(juce::jmax(0.f,upper*upper-along*along));
        const P el=sh+direction*along+P{direction.y,-direction.x}*bend;
        joint(g,sh,5.6f,5.2f);limb(g,sh,el,6.3f,4.8f,.96f,-2);joint(g,el,4.3f,4.2f);limb(g,el+P{1,1},wr,3.4f,2.6f,.92f);limb(g,el+P{5,1},wr+P{4,3},2.4f,2.1f,.79f);joint(g,wr,3.3f,2.9f);{juce::Graphics::ScopedSaveState hand(g);g.addTransform(juce::AffineTransform::scale(1.28f,1.22f,wr.x,wr.y));const P palm=wr+P{-7,-5};
            auto carpal=poly({palm+P{5,4},palm+P{-3,5},palm+P{-6,0},palm+P{-3,-5},palm+P{4,-3}});solid(g,carpal,palm-P{4,4},palm+P{4,4});
            for(int k=0;k<4;++k){const P a=palm+P{-4.f+k*2.4f,-3.f},b=a+P{-8.f+k*2.6f,-8.f-k*1.9f},tip=b+P{-3.f+k*1.5f,-5.f};limb(g,palm,a,1.95f,1.6f,.99f);limb(g,a,b,1.85f,1.45f,.97f);joint(g,b,1.55f,1.40f,.88f);limb(g,b,tip,1.40f,.96f,.96f);}
            limb(g,palm+P{2,2},palm+P{8,-3},2.1f,1.6f,.97f);limb(g,palm+P{8,-3},palm+P{11,-8},1.5f,1.0f,.97f);}
        const P rs{185,95},re{240,126},rw{191,83};joint(g,rs,5.2f,4.8f,.77f);limb(g,rs,re,6.2f,4.8f,.88f,2);joint(g,re,4.4f,4.1f,.75f);limb(g,re,rw,3.8f,2.9f,.88f);limb(g,re+P{4,-1},rw+P{4,1},2.5f,2.1f,.71f);
        {juce::Graphics::ScopedSaveState mic(g);g.addTransform(juce::AffineTransform::rotation(-.73f,191,83));g.setGradientFill(juce::ColourGradient(C(0xffa5aaa4),187,75,C(0xff232a2b),195,75,false));g.fillRoundedRectangle(187,61,8,32,2);g.setColour(Theme::green.withAlpha(.42f+.42f*activity));g.fillRect(187,67,8,2);g.setGradientFill(juce::ColourGradient(C(0xffe0ded1),187,54,C(0xff323c3c),196,65,false));g.fillEllipse(183,49,16,19);g.setColour(C(0xffd8d7c9));g.drawEllipse(183.5f,49.5f,15.f,18.f,1.15f);Path cap;cap.addEllipse(183,49,16,19);g.reduceClipRegion(cap);g.setColour(dark.withAlpha(.43f));for(int k=0;k<12;++k){g.drawLine(180.f+k*2,49,188.f+k*2,68,.55f);g.drawLine(180.f+k*2,68,188.f+k*2,49,.55f);}}
        for(int k=0;k<4;++k){const P a{187.f+k*.8f,77.f+k*3.1f};limb(g,a,a+P{8,1.7f},1.45f,1.05f,.96f);limb(g,a+P{8,1.7f},a+P{9,4},1.05f,.8f,.82f);}
    }
    static juce::AffineTransform torsoPose() noexcept {return juce::AffineTransform::rotation(.20f,151.f,210.f);}
    void drawCable(juce::Graphics& g,float activity,float phase)const{
        const auto plug=P{191.f,93.f}.transformedBy(juce::AffineTransform::rotation(-.73f,191.f,83.f).followedBy(torsoPose()));
        const float sway=5.f*activity*std::sin(phase*.60f);
        Path cable;cable.startNewSubPath(plug);cable.cubicTo(301.f,133.f,306.f+sway,191.f,254.f+sway,228.f);cable.cubicTo(214.f,255.f,184.f,273.f,188.f,310.f);cable.cubicTo(190.f,347.f,205.f,367.f,244.f,366.f);
        line(g,cable,3.1f,dark.withAlpha(.95f));line(g,cable,1.45f,bone.interpolatedWith(shade,.30f));
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
            .transformedBy(juce::AffineTransform::rotation(-.73f,191.f,83.f)
                .followedBy(torsoPose())
                .followedBy(bodyLean(activity,phase))
                .followedBy(juce::AffineTransform::scale(scale).translated(x,y)))
            .expanded(5.f).toNearestInt();
    }
    void draw(juce::Graphics& g,float x,float y,float scale,float activity,float phase,float vocal=-1.f)const{
        const float react=juce::jlimit(0.f,1.f,activity),voice=juce::jlimit(0.f,1.f,vocal<0.f?react:vocal);juce::Graphics::ScopedSaveState state(g);g.addTransform(juce::AffineTransform::scale(scale).translated(x,y));
        g.setGradientFill(juce::ColourGradient(dark.withAlpha(.86f),151,365,dark.withAlpha(0.f),151,377,false));g.fillEllipse(112,359,148,15);
        if(!body.isValid()||cachedTheme!=Theme::current){body=juce::Image(juce::Image::ARGB,690,744,true);cachedTheme=Theme::current;juce::Graphics art(body);art.addTransform(juce::AffineTransform::scale(2.f));bodyArt(art);weather(body);}// Small coherent body lean pivots at the feet; jaw and hands move with the torso.
        g.addTransform(bodyLean(react,phase));
        drawCable(g,react,phase);g.setOpacity(1.f);g.drawImageTransformed(body,juce::AffineTransform::scale(.5f));g.addTransform(torsoPose());arms(g,react,phase);skull(g,voice,-.075f+.075f*std::sin(phase*.73f)*react,react);
    }
private: mutable juce::Image body,cranium;mutable int cachedTheme=-1;
};
} // namespace skellpreset::stage
