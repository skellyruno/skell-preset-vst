#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include <cmath>

namespace skellpreset::stage {

// Procedural full-body skeletal vocalist. Model space is 345 x 372 with the
// floor at y=368 and the figure centred on x=151, matching the stage rectangle
// reserved by SkellIdleDeck::layoutFor(). Every contour is native JUCE geometry
// drawn on the message thread: no imported art, no textures, no audio-thread
// work.
//
// Proportions are a real seven-and-a-half-head figure rather than the old
// bobble-head build: skull 13..78, shoulders 100, ribcage 100..158, pelvis
// 186..218, knees 285, ankles 347, feet on the floor at 368. Limbs are tapered
// capsules whose joint spheres are always smaller than the shaft that meets
// them, so nothing reads as beads on a string. The ribcage is built from
// downward-bowed hoops that sweep from the spine out to a costal angle and back
// to the sternum, which removes the ladder look.
class Performer {
public:
    void draw(juce::Graphics& g,float originX,float originY,float scale,
              float activity,float phase,float vocal=-1.f) const {
        const float react=juce::jlimit(0.f,1.f,activity);
        const float sway=.72f*std::sin(phase*.62f)+.28f*std::sin(phase*.39f+1.4f);
        const float beat=std::sin(phase*1.45f);
        const float breath=juce::jlimit(0.f,1.f,vocal<0.f?react:vocal);
        // Mouth opening follows measured vocal energy rather than a timed
        // sine wave. Body sway remains theatrical set animation.
        const float jawDrop=6.5f*std::sqrt(vocal<0.f?react:juce::jlimit(0.f,1.f,vocal));
        const auto ink=Theme::bg;
        const auto ivory=juce::Colour(0xffdfd7c5);
        const auto stone=juce::Colour(0xff454e4b);
        const auto alive=Theme::green;
        const auto cool=Theme::blue;

        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::rotation(
                (sway*.65f+beat*.35f)*react*.023f,151.f,368.f)
            .scaled(scale,scale*(1.f+.004f*react))
            .translated(originX,originY));

        // Tapered capsule between two joint centres. The gradient runs across
        // the bone, so every limb reads as a cylinder lit from the upper left.
        juce::Graphics* boneTarget=&g;
        auto limb=[&](float x0,float y0,float r0,float x1,float y1,float r1,
                      float gloss) {
            auto& bone=*boneTarget;
            const float dx=x1-x0,dy=y1-y0;
            const float len=juce::jmax(.001f,std::sqrt(dx*dx+dy*dy));
            const float nx=-dy/len,ny=dx/len;
            const float mx=(x0+x1)*.5f,my=(y0+y1)*.5f;
            const float mr=(r0+r1)*.5f;
            // Single continuous contour. addCentredArc(..., true) started
            // separate subpaths and used an incompatible angular convention,
            // producing disconnected caps and flat, angular-looking bones.
            const float tx=dx/len,ty=dy/len;
            constexpr float halfCircle=4.f/3.f;
            juce::Path p;
            p.startNewSubPath(x0+nx*r0,y0+ny*r0);
            p.cubicTo(x0+tx*len*.30f+nx*mr*.63f,y0+ty*len*.30f+ny*mr*.63f,
                      x1-tx*len*.25f+nx*mr*.63f,y1-ty*len*.25f+ny*mr*.63f,x1+nx*r1,y1+ny*r1);
            p.cubicTo(x1+nx*r1+tx*r1*halfCircle,y1+ny*r1+ty*r1*halfCircle,
                      x1-nx*r1+tx*r1*halfCircle,y1-ny*r1+ty*r1*halfCircle,
                      x1-nx*r1,y1-ny*r1);
            p.cubicTo(x1-tx*len*.25f-nx*mr*.63f,y1-ty*len*.25f-ny*mr*.63f,
                      x0+tx*len*.30f-nx*mr*.63f,y0+ty*len*.30f-ny*mr*.63f,x0-nx*r0,y0-ny*r0);
            p.cubicTo(x0-nx*r0-tx*r0*halfCircle,y0-ny*r0-ty*r0*halfCircle,
                      x0+nx*r0-tx*r0*halfCircle,y0+ny*r0-ty*r0*halfCircle,
                      x0+nx*r0,y0+ny*r0);
            p.closeSubPath();
            juce::ColourGradient marrow(stone.brighter(.18f).withAlpha(gloss),mx-nx*mr,my-ny*mr,
                stone.interpolatedWith(cool,.12f).withAlpha(gloss),mx+nx*mr,my+ny*mr,false);
            marrow.addColour(.26,ivory.withAlpha(gloss));
            marrow.addColour(.56,ivory.interpolatedWith(stone,.34f).withAlpha(gloss));
            bone.setGradientFill(marrow);bone.fillPath(p);
            if(len>18.f && r0>2.8f){
                juce::Graphics::ScopedSaveState texture(bone);bone.reduceClipRegion(p);
                // Sparse deterministic cortical pitting and irregular end
                // shadows: no new image allocation or random state per frame.
                for(int mark=0;mark<14;++mark){
                    const float u=.10f+.80f*(float)((mark*37)%101)/100.f;
                    const float v=((float)((mark*19)%29)/28.f-.5f)*mr*1.5f;
                    bone.setColour((mark%4==0?ivory:ink).withAlpha(.06f+.025f*(mark%3)));
                    bone.fillEllipse(x0+dx*u+nx*v,y0+dy*u+ny*v,.65f+mr*.10f,.65f+mr*.13f);
                }
            }
            bone.setColour(ink.withAlpha(.58f));
            bone.strokePath(p,juce::PathStrokeType(juce::jmax(.45f,r1*.16f)));
            if(r1>1.2f){
                bone.setColour(alive.withAlpha(.025f+.07f*(boneTarget==&g?react:0.f)));
                bone.drawLine(x0+nx*r0*.72f,y0+ny*r0*.72f,x1+nx*r1*.72f,y1+ny*r1*.72f,juce::jmax(.5f,r1*.20f));
                bone.setColour(ivory.withAlpha(.09f*gloss+.02f));
                bone.drawLine(x0-nx*r0*.40f,y0-ny*r0*.40f,
                           x1-nx*r1*.40f,y1-ny*r1*.40f,
                           juce::jmax(.45f,juce::jmin(r0,r1)*.32f));
            }
        };
        // Joint spheres are kept below the radius of the shaft they cap, so the
        // silhouette stays continuous instead of turning into a bead chain.
        auto joint=[&](float x,float y,float r,float gloss) {
            auto& bone=*boneTarget;
            bone.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(gloss*.86f),x-r*.5f,y-r*.6f,
                stone.withAlpha(gloss*.62f),x+r*.6f,y+r*.65f,false));
            bone.fillEllipse(x-r,y-r,r*2.f,r*2.f);
            bone.setColour(ink.withAlpha(.62f));
            bone.drawEllipse(x-r,y-r,r*2.f,r*2.f,juce::jmax(.5f,r*.16f));
            bone.setColour(ivory.withAlpha(.20f*gloss+.03f));
            bone.fillEllipse(x-r*.50f,y-r*.62f,r*.48f,r*.44f);
        };

        // ---- Contact shadow, then a lit apron under the feet. Without these
        // the figure floats above the boards.
        g.setGradientFill(juce::ColourGradient(
            ink.withAlpha(.92f),151.f,362.f,ink.withAlpha(0.f),151.f,374.f,false));
        g.fillEllipse(92.f,361.f,120.f,13.f);
        g.setGradientFill(juce::ColourGradient(
            alive.withAlpha(.07f+.09f*react),151.f,366.f,
            ink.withAlpha(0.f),151.f,352.f,false));
        g.fillEllipse(96.f,353.f,112.f,17.f);

        // Fixed weight-bearing anatomy: cached as a compact model-space plate.
        // The moving ribs, upper limbs, head, jaw and cable are still vectors.
        if(!lowerBody.isValid() || lowerTheme!=Theme::current){
            lowerBody=juce::Image(juce::Image::ARGB,176,212,true);lowerTheme=Theme::current;
            juce::Graphics lowerArt(lowerBody);lowerArt.addTransform(juce::AffineTransform::translation(-65.f,-166.f));
            boneTarget=&lowerArt;
        // ---- Legs. Weight carried on the left leg, right leg planted a little
        // wider, which gives the pose a working stance instead of a mannequin.
        // Shafts first, then small caps at the actual articulations.
        limb(134.f,214.f,7.2f,124.f,287.f,5.4f,.96f);
        limb(168.f,214.f,7.1f,194.f,284.f,5.3f,.94f);
        joint(133.f,213.f,6.0f,.90f);
        joint(169.f,213.f,5.9f,.88f);
        // Knees are a single wide condyle mass, not a pair of beads.
        auto condyle=[&](float x,float y,float w,float h,float gloss){
            lowerArt.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(gloss*.90f),x-w*.22f,y-h*.58f,
                stone.withAlpha(gloss*.66f),x+w*.36f,y+h*.55f,false));
            lowerArt.fillEllipse(x-w*.5f,y-h*.5f,w,h);
            lowerArt.setColour(ink.withAlpha(.62f));
            lowerArt.drawEllipse(x-w*.5f,y-h*.5f,w,h,juce::jmax(.5f,h*.12f));
            lowerArt.setColour(ivory.withAlpha(.22f));
            lowerArt.fillEllipse(x-w*.30f,y-h*.42f,w*.34f,h*.30f);
            lowerArt.setColour(ink.withAlpha(.34f));
            lowerArt.drawLine(x-w*.26f,y+h*.12f,x+w*.26f,y+h*.12f,.9f);
        };
        condyle(124.f,289.f,13.5f,11.f,.90f);
        condyle(195.f,286.f,13.f,10.5f,.90f);
        limb(124.f,294.f,6.2f,114.f,348.f,4.4f,.96f);
        limb(194.f,291.f,6.1f,200.f,346.f,4.3f,.94f);
        limb(131.f,297.f,1.9f,121.f,344.f,1.5f,.80f);   // fibulae
        limb(188.f,294.f,1.8f,194.f,342.f,1.4f,.78f);
        // Ankles are a single malleolus knob either side of the talus.
        condyle(112.f,350.f,9.5f,8.f,.88f);
        condyle(201.f,348.f,9.2f,7.6f,.86f);
        // Feet: talus, then a flat arch of metatarsals and toes on the floor.
        limb(114.f,352.f,4.0f,106.f,362.f,3.2f,.88f);
        limb(106.f,362.f,3.0f,95.f,365.f,2.3f,.85f);
        limb(200.f,350.f,3.9f,209.f,360.f,3.1f,.86f);
        limb(209.f,360.f,2.9f,220.f,363.f,2.2f,.83f);
        for(int toe=0;toe<4;++toe){
            const float t=(float)toe;
            limb(99.f-t*.8f,364.f,1.6f,88.f-t*1.4f,366.f,1.2f,.84f);
            limb(216.f+t*.7f,362.f,1.5f,227.f+t*1.2f,364.f,1.1f,.82f);
        }

        // ---- Pelvis: a single continuous ring (ilium, ischium, pubis) with a
        // ---- Pelvis. Two flared iliac wings joined by a low pubic arch, with
        // the sacrum recessed between them. Wings are drawn as separate lobes
        // that open outward and upward, so the whole assembly reads as a bowl
        // of bone rather than a rounded mask with two holes.
        {
            for(int side:{-1,1}){
                const float s=(float)side;
                juce::Path wing;
                wing.startNewSubPath(151.f+s*6.f,181.f);
                wing.cubicTo(151.f+s*17.f,171.f,151.f+s*33.f,177.f,
                             151.f+s*31.f,192.f);
                wing.cubicTo(151.f+s*28.f,201.f,151.f+s*18.f,215.f,
                             151.f+s*9.f,212.f);
                wing.cubicTo(151.f+s*7.f,202.f,151.f+s*6.f,191.f,
                             151.f+s*6.f,181.f);
                wing.closeSubPath();
                lowerArt.setGradientFill(juce::ColourGradient(
                    ivory.withAlpha(.80f),151.f+s*12.f,179.f,
                    stone.withAlpha(.54f),151.f+s*31.f,211.f,false));
                // Cut an anatomical obturator opening out of the bone, rather
                // than painting a rounded black spot onto the wing.
                juce::Path foramen;
                foramen.startNewSubPath(151.f+s*11.f,198.f);
                foramen.cubicTo(151.f+s*22.f,195.f,151.f+s*23.f,201.f,151.f+s*19.f,205.f);
                foramen.cubicTo(151.f+s*15.f,211.f,151.f+s*10.f,208.f,151.f+s*11.f,198.f);
                foramen.closeSubPath();
                auto rim=wing;rim.addPath(foramen);rim.setUsingNonZeroWinding(false);
                lowerArt.fillPath(rim);
                juce::Path iliacFossa;
                iliacFossa.startNewSubPath(151.f+s*10.f,182.f);
                iliacFossa.cubicTo(151.f+s*20.f,177.f,151.f+s*28.f,184.f,151.f+s*25.f,191.f);
                iliacFossa.quadraticTo(151.f+s*17.f,199.f,151.f+s*10.f,182.f);
                iliacFossa.closeSubPath();
                lowerArt.setGradientFill(juce::ColourGradient(ink.withAlpha(.38f),151.f+s*12.f,181.f,stone.withAlpha(.04f),151.f+s*28.f,196.f,false));
                lowerArt.fillPath(iliacFossa);
                lowerArt.setColour(ink.withAlpha(.66f));
                lowerArt.strokePath(wing,juce::PathStrokeType(.9f));
                // Bright iliac crest along the top of each wing.
                juce::Path crest;
                crest.startNewSubPath(151.f+s*7.f,181.f);
                crest.cubicTo(151.f+s*20.f,176.f,151.f+s*31.f,183.f,
                              151.f+s*31.f,194.f);
                lowerArt.setColour(ivory.withAlpha(.34f));
                lowerArt.strokePath(crest,juce::PathStrokeType(2.f,
                    juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                lowerArt.setColour(stone.withAlpha(.48f));
                lowerArt.strokePath(foramen,juce::PathStrokeType(.8f));
            }
            // Sacrum recessed between the wings, plus the coccyx tip.
            juce::Path sacrum;
            sacrum.startNewSubPath(146.f,183.f);
            sacrum.lineTo(156.f,183.f);
            sacrum.lineTo(154.f,197.f);
            sacrum.lineTo(151.f,201.f);
            sacrum.lineTo(148.f,197.f);
            sacrum.closeSubPath();
            lowerArt.setGradientFill(juce::ColourGradient(
                stone.withAlpha(.60f),146.f,183.f,
                ink.withAlpha(.50f),156.f,200.f,false));
            lowerArt.fillPath(sacrum);
            lowerArt.setColour(ivory.withAlpha(.12f));
            for(int plate=0;plate<3;++plate)
                lowerArt.drawLine(148.f,187.f+plate*3.4f,154.f,187.f+plate*3.4f,.7f);
            // Pubic arch linking the two wings across the front.
            juce::Path pubis;
            pubis.startNewSubPath(135.f,211.f);
            pubis.cubicTo(140.f,217.f,147.f,219.f,151.f,219.f);
            pubis.cubicTo(155.f,219.f,162.f,217.f,167.f,211.f);
            lowerArt.setColour(stone.withAlpha(.54f));
            lowerArt.strokePath(pubis,juce::PathStrokeType(2.6f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            lowerArt.setColour(ivory.withAlpha(.18f));
            lowerArt.strokePath(pubis,juce::PathStrokeType(1.1f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }

            boneTarget=&g;
        }
        g.setOpacity(1.f);g.drawImageAt(lowerBody,65,166);

        // ---- Lumbar vertebrae, narrower than the thoracic stack above them.
        for(int v=0;v<5;++v){
            const float y=151.f+v*6.7f;
            const float w=11.f+v*.9f;
            g.setColour(ink.withAlpha(.84f));
            g.fillRoundedRectangle(151.f-w*.5f,y-1.f,w+1.f,8.f,2.f);
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.66f),151.f-w*.4f,y,
                stone.withAlpha(.50f),151.f+w*.5f,y+7.f,false));
            g.fillRoundedRectangle(151.f-w*.42f,y,w*.84f,6.f,1.5f);
        }

        // ---- Scapulae sit behind the cage, then the ribs wrap in front.
        for(int side:{-1,1}){
            const float s=(float)side;
            juce::Path scap;
            scap.startNewSubPath(151.f+s*24.f,100.f);
            scap.cubicTo(151.f+s*40.f,104.f,151.f+s*42.f,120.f,
                         151.f+s*34.f,134.f);
            scap.lineTo(151.f+s*23.f,142.f);
            scap.cubicTo(151.f+s*23.f,126.f,151.f+s*23.f,112.f,
                         151.f+s*24.f,100.f);
            scap.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                stone.withAlpha(.42f),151.f+s*24.f,100.f,
                ink.withAlpha(.56f),151.f+s*38.f,142.f,false));
            g.fillPath(scap);
            g.setColour(stone.withAlpha(.28f));
            juce::Path ridge;
            ridge.startNewSubPath(151.f+s*28.f,104.f);
            ridge.quadraticTo(151.f+s*38.f,116.f,151.f+s*32.f,134.f);
            g.strokePath(ridge,juce::PathStrokeType(1.3f));
        }

        // ---- Ribcage. A dark thoracic cavity first, then six open rib arcs per
        // side that stop short of the sternum. The gap between each arc and the
        // midline is the costal cartilage, suggested with a much fainter line.
        // Leaving that gap open is what stops the ribs reading as closed rings.
        {
            juce::Path cavity;
            cavity.startNewSubPath(151.f,99.f);
            cavity.cubicTo(135.f,99.f,120.f,106.f,117.f,118.f);
            cavity.cubicTo(114.f,132.f,122.f,146.f,136.f,152.f);
            cavity.cubicTo(142.f,155.f,147.f,158.f,151.f,159.f);
            cavity.cubicTo(155.f,158.f,160.f,155.f,166.f,152.f);
            cavity.cubicTo(180.f,146.f,188.f,132.f,185.f,118.f);
            cavity.cubicTo(182.f,106.f,167.f,99.f,151.f,99.f);
            cavity.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xff0a0f10),151.f,104.f,
                juce::Colour(0xff020405),151.f,156.f,false));
            g.fillPath(cavity);
        }
        // Seven true ribs: subdued posterior arcs, brighter descending
        // anterior bone, and distinct cartilage at the sternum.
        const float ribReach[7]{24.f,30.f,35.f,38.f,40.f,39.f,36.f};
        for(int r=0;r<7;++r){
            const float y=101.f+r*6.f,reach=ribReach[r]*(1.f+.02f*react);
            const float frontY=y+6.f+r*1.25f;
            for(int side:{-1,1}){
                const float d=(float)side;
                juce::Path rear;
                rear.startNewSubPath(151.f+d*5.f,y-2.5f);
                rear.cubicTo(151.f+d*reach*.45f,y-5.f,151.f+d*reach*.92f,y-1.f,151.f+d*reach,y+3.f);
                g.setColour(stone.withAlpha(.42f));
                g.strokePath(rear,juce::PathStrokeType(1.6f));
                juce::Path front;
                front.startNewSubPath(151.f+d*reach,y+2.f);
                front.cubicTo(151.f+d*(reach-.5f),y+7.f,151.f+d*reach*.62f,frontY+2.f,151.f+d*12.f,frontY);
                g.setColour(ink.withAlpha(.88f));
                g.strokePath(front,juce::PathStrokeType(4.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                g.setGradientFill(juce::ColourGradient(ivory.withAlpha(.92f),151.f+d*12.f,frontY,stone.brighter(.35f).withAlpha(.92f),151.f+d*reach,y+2.f,false));
                g.strokePath(front,juce::PathStrokeType(3.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                juce::Path cartilage;
                cartilage.startNewSubPath(151.f+d*12.f,frontY);
                cartilage.quadraticTo(151.f+d*8.f,frontY+.5f,151.f+d*5.5f,frontY-2.f);
                g.setColour(stone.interpolatedWith(ivory,.32f).withAlpha(.72f));
                g.strokePath(cartilage,juce::PathStrokeType(1.7f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            }
        }
        // Three false ribs form the costal arch; two floating ribs stop short.
        for(int side:{-1,1}){
            const float d=(float)side;
            const float widths[]{35.f,31.f,27.f,22.f,17.f};
            for(int r=0;r<5;++r){
                const float y=142.f+r*5.2f,reach=widths[r];
                juce::Path rib;
                rib.startNewSubPath(151.f+d*5.f,y);
                rib.cubicTo(151.f+d*reach*.45f,y-3.f,151.f+d*reach,y+1.f,151.f+d*reach,y+6.f);
                g.setColour(stone.withAlpha(.45f));
                g.strokePath(rib,juce::PathStrokeType(1.5f));
                juce::Path arc;
                arc.startNewSubPath(151.f+d*reach,y+6.f);
                arc.quadraticTo(151.f+d*reach*.78f,y+12.f,151.f+d*(r<3?11.f+r*3.f:reach*.87f),y+9.f);
                g.setGradientFill(juce::ColourGradient(stone.withAlpha(.65f),151.f+d*reach,y+6.f,ivory.withAlpha(.78f-r*.08f),151.f+d*12.f,y+9.f,false));
                g.strokePath(arc,juce::PathStrokeType(2.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                if(r<3){
                    juce::Path cart;
                    cart.startNewSubPath(151.f+d*(11.f+r*3.f),y+9.f);
                    cart.quadraticTo(151.f+d*(10.f+r*2.f),y+4.f,151.f+d*(5.f+r*3.f),151.f+r*2.f);
                    g.setColour(stone.interpolatedWith(ivory,.2f).withAlpha(.53f));
                    g.strokePath(cart,juce::PathStrokeType(1.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                }
            }
        }
        // Sternum: manubrium, body and a short xiphoid.
        {
            juce::Path st;
            st.startNewSubPath(145.f,99.f);
            st.quadraticTo(148.f,97.f,149.f,99.5f);
            st.quadraticTo(151.f,102.f,153.f,99.5f);
            st.quadraticTo(154.f,97.f,157.f,99.f);
            st.lineTo(157.f,108.f);
            st.lineTo(154.f,146.f);
            st.lineTo(151.f,156.f);
            st.lineTo(148.f,146.f);
            st.lineTo(146.f,108.f);
            st.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.86f),146.f,100.f,
                stone.withAlpha(.56f),158.f,154.f,false));
            g.fillPath(st);
            g.setColour(ink.withAlpha(.64f));
            g.strokePath(st,juce::PathStrokeType(.9f));
            g.setColour(stone.withAlpha(.40f));
            g.drawLine(147.f,107.f,156.f,107.f,.8f);
            g.drawLine(147.f,124.f,155.f,124.f,.7f);
            g.drawLine(148.f,140.f,154.f,140.f,.7f);
        }
        // Clavicles from the manubrium to each acromion.
        for(int side:{-1,1}){
            const float s=(float)side;
            juce::Path cl;
            cl.startNewSubPath(150.f,101.f);
            cl.cubicTo(150.f+s*18.f,96.f,150.f+s*30.f,95.f,150.f+s*40.f,97.f);
            cl.cubicTo(150.f+s*30.f,101.f,150.f+s*18.f,104.f,150.f,106.f);
            cl.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.80f),150.f,99.f,
                stone.withAlpha(.50f),151.f+s*36.f,103.f,false));
            g.fillPath(cl);
            g.setColour(ink.withAlpha(.64f));
            g.strokePath(cl,juce::PathStrokeType(.85f));
        }
        // Cervical vertebrae linking skull to thorax.
        for(int v=0;v<3;++v){
            const float y=79.f+v*6.6f;
            const float w=9.f+v*1.6f;
            g.setColour(ink.withAlpha(.86f));
            g.fillRoundedRectangle(151.f-w*.5f,y-1.f,w+1.f,7.f,2.f);
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.68f),151.f-w*.4f,y,
                stone.withAlpha(.50f),151.f+w*.5f,y+5.6f,false));
            g.fillRoundedRectangle(151.f-w*.42f,y,w*.84f,5.2f,1.4f);
        }

        // ---- Left arm hangs loose with a soft curl in the fingers that
        // breathes with the measured level.
        {
        juce::Graphics::ScopedSaveState arm(g);
        g.addTransform(juce::AffineTransform::rotation((.035f+.09f*breath)*react*std::sin(phase*.82f),112.f,101.f));
        joint(112.f,101.f,5.0f,.92f);
        limb(113.f,102.f,6.8f,86.f,158.f,4.8f,.96f);
        joint(86.f,159.f,4.0f,.88f);
        limb(87.f,162.f,4.0f,79.f,212.f,3.0f,.92f);
        limb(89.f,161.f,4.4f,82.f,211.f,3.2f,.94f);
        joint(80.f,215.f,3.2f,.86f);
        {
            const float drift=react*beat*1.5f;
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.76f),75.f,214.f,
                stone.withAlpha(.52f),84.f,224.f,false));
            g.fillEllipse(72.f,215.f+drift,13.f,10.f);
            g.setColour(ink.withAlpha(.66f));
            g.drawEllipse(72.f,215.f+drift,13.f,10.f,.8f);
            for(int finger=0;finger<4;++finger){
                const float f=(float)finger;
                const float sx=75.f+f*2.1f,sy=221.f+drift+f*.5f;
                const float kx=68.f+f*2.7f,ky=229.f+drift+f*.4f;
                const float curl=1.4f+react*(beat*.5f+.5f)*2.0f;
                limb(sx,sy,1.6f,kx,ky,1.35f,.84f);
                limb(kx,ky,1.25f,kx+1.2f,ky+5.4f+curl+f*.35f,1.f,.78f);
            }
            limb(79.f,217.f+drift,1.5f,72.f,224.f+drift,1.2f,.80f);
        }

        }
        // ---- Right arm: shoulder, elbow dropped and pushed out, forearm
        // rising back in to the wrist so the fist can close on the mic barrel.
        // The wrist is placed at the exact anchor the microphone block uses.
        const float armGesture=(.016f+.048f*breath)*std::sin(phase*.57f+.8f)+
            .016f*react*std::sin(phase*1.21f);
        const auto rightArmFrame=juce::AffineTransform::rotation(armGesture,190.f,101.f);
        constexpr float micAngle=-.46f;
        const auto micFrame=juce::AffineTransform::rotation(micAngle,205.f,124.f);
        {
        juce::Graphics::ScopedSaveState rightArm(g);g.addTransform(rightArmFrame);
        joint(190.f,101.f,5.0f,.92f);
        limb(191.f,102.f,6.8f,222.f,164.f,5.0f,.96f);
        joint(222.f,164.f,4.4f,.90f);
        limb(220.f,161.f,4.4f,204.f,125.f,3.3f,.94f);
        limb(224.f,160.f,4.8f,208.f,123.f,3.6f,.92f);
        joint(205.f,124.f,3.6f,.88f);

        // ---- Microphone: capsule beside the jaw, barrel angled down-right to
        // meet the fist at the wrist anchor (205,124). The whole assembly is
        // rotated about the wrist so the grip and the arm share one pivot.
        {
            juce::Graphics::ScopedSaveState mic(g);
            g.addTransform(micFrame);
            // Barrel along the local -y axis from the wrist.
            juce::Path barrel;
            barrel.addRoundedRectangle(196.f,80.f,18.f,48.f,8.f);
            g.setColour(ink);
            g.fillPath(barrel.createPathWithRoundedCorners(9.f));
            juce::Path inner;
            inner.addRoundedRectangle(197.5f,81.5f,15.f,45.f,7.5f);
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.88f),198.f,84.f,
                stone.withAlpha(.60f),212.f,126.f,false));
            g.fillPath(inner);
            // Grille ball at the top, sitting just off the jaw.
            juce::Path ball;
            ball.addEllipse(193.f,60.f,24.f,26.f);
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.95f),195.f,64.f,
                stone.withAlpha(.58f),215.f,84.f,false));
            g.fillPath(ball);
            g.setColour(ink.withAlpha(.62f));
            g.strokePath(ball,juce::PathStrokeType(1.f));
            {
                juce::Graphics::ScopedSaveState mesh(g);
                g.reduceClipRegion(ball);
                g.setColour(ink.withAlpha(.36f));
                for(int line=0;line<6;++line)
                    g.drawHorizontalLine(63+line*4,191.f,219.f);
                for(int line=0;line<5;++line)
                    g.drawVerticalLine(196+line*4,58.f,88.f);
            }
            // Green status ring under the ball.
            g.setColour(alive.withAlpha(.85f));
            g.fillRoundedRectangle(196.f,88.f,18.f,3.6f,1.4f);
            // Matte handle sleeve and the cable socket.
            g.setColour(Theme::stroke.withAlpha(.84f));
            g.fillRoundedRectangle(198.f,96.f,14.f,32.f,4.5f);
            g.setColour(ivory.withAlpha(.17f));
            g.drawLine(200.f,99.f,200.f,125.f,.9f);
            g.setColour(ink.withAlpha(.94f));
            g.fillRoundedRectangle(199.f,127.f,12.f,8.f,3.f);
        }
        // The fist rotates with the microphone, keeping the fingers wrapped
        // around its handle instead of floating independently.
        {
            juce::Graphics::ScopedSaveState grip(g);g.addTransform(micFrame);
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.74f),200.f,112.f,
                stone.withAlpha(.50f),214.f,134.f,false));
            g.fillEllipse(198.f,112.f,17.f,15.f);
            g.setColour(ink.withAlpha(.62f));
            g.drawEllipse(198.f,112.f,17.f,15.f,.85f);
            for(int finger=0;finger<4;++finger){
                const float y=112.f+(float)finger*3.6f;
                limb(199.f,y,1.9f,213.f,y+1.1f,1.6f,.86f);
                g.setColour(ivory.withAlpha(.52f));
                g.fillEllipse(197.f,y-1.2f,2.9f,2.9f);
            }
            // Thumb crossing the front of the sleeve on the near side.
            limb(204.f,124.f,1.9f,196.f,117.f,1.5f,.84f);
        }
        } // the wrist, grip, mic and arm share the shoulder gesture
        // Slack cable falling from the moving mic base, past the ribs and away.
        {
            juce::Path cable;
            const auto anchor=juce::Point<float>(205.f,133.f).transformedBy(micFrame).transformedBy(rightArmFrame);
            cable.startNewSubPath(anchor);
            cable.cubicTo(224.f,150.f,232.f,176.f,228.f,206.f);
            cable.cubicTo(225.f,251.f,245.f,318.f,254.f,357.f);
            cable.quadraticTo(260.f,369.f,282.f,366.f);
            g.setColour(ink.withAlpha(.88f));
            g.strokePath(cable,juce::PathStrokeType(2.3f));
            g.setColour(stone.withAlpha(.28f));
            g.strokePath(cable,juce::PathStrokeType(.9f));
        }

        // ---- Skull. Seven-head proportion, three-quarter planes, sunken
        // sockets, and a mandible that opens with the measured output.
        {
            juce::Graphics::ScopedSaveState head(g);
            g.addTransform(juce::AffineTransform::rotation(
                (.018f+.048f*react)*std::sin(phase*.81f)+.015f*react*std::sin(phase*1.7f),151.f,70.f)
                .followedBy(juce::AffineTransform::scale(.90f,.86f,151.f,79.f)));
            juce::Path skull;
            skull.startNewSubPath(151.f,13.f);
            skull.cubicTo(134.f,11.f,125.5f,23.f,126.f,35.f);
            skull.cubicTo(125.5f,44.f,127.f,49.f,130.f,52.f);
            skull.quadraticTo(129.f,55.f,134.f,57.f);
            skull.cubicTo(136.f,60.f,137.f,63.f,137.f,66.f);
            // The fixed cranium stops at the maxilla; the lower jaw is
            // a separate articulated shape rather than a line over bone.
            skull.cubicTo(140.f,69.f,146.f,70.f,151.f,70.f);
            skull.cubicTo(156.f,70.f,162.f,69.f,165.f,66.f);
            skull.cubicTo(165.f,63.f,166.f,60.f,168.f,57.f);
            skull.quadraticTo(173.f,55.f,172.f,52.f);
            skull.cubicTo(175.f,49.f,176.5f,44.f,176.f,35.f);
            skull.cubicTo(176.f,22.f,168.f,11.f,151.f,13.f);
            skull.closeSubPath();
            g.setColour(ink);
            g.strokePath(skull,juce::PathStrokeType(3.8f));
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.98f),137.f,19.f,
                stone.withAlpha(.88f),173.f,72.f,false));
            g.fillPath(skull);
            {
                juce::Graphics::ScopedSaveState rim(g);g.reduceClipRegion(skull);
                g.setGradientFill(juce::ColourGradient(cool.withAlpha(.16f+.19f*react),174.f,32.f,cool.withAlpha(0.f),155.f,38.f,false));
                g.fillRect(150.f,13.f,26.f,65.f);
                g.setGradientFill(juce::ColourGradient(alive.withAlpha(.12f+.17f*breath),148.f,76.f,alive.withAlpha(0.f),148.f,48.f,false));
                g.fillRect(127.f,46.f,49.f,32.f);
            }
            if(!skullWeathering.isValid() || skullTheme!=Theme::current){
                skullWeathering=juce::Image(juce::Image::ARGB,108,130,true);skullTheme=Theme::current;
                juce::Graphics art(skullWeathering);
                art.addTransform(juce::AffineTransform::translation(-124.f,-10.f).scaled(2.f));
                art.reduceClipRegion(skull);
                for(int fleck=0;fleck<156;++fleck){
                    const float x=126.f+(float)((fleck*43)%50);
                    const float y=16.f+(float)((fleck*61)%58);
                    art.setColour((fleck%3==0?ink:ivory).withAlpha(.065f));
                    art.fillEllipse(x,y,.9f+(fleck%3)*.35f,.65f+(fleck%2)*.35f);
                }
                // Bone planes and porous weathering, not flat white plastic.
                art.setGradientFill(juce::ColourGradient(juce::Colour(0xffb6a48e).withAlpha(.24f),142.f,20.f,ink.withAlpha(0.f),154.f,39.f,true));
                art.fillRect(127.f,13.f,48.f,50.f);
                for(int stain=0;stain<16;++stain){const float x=130.f+(float)((stain*19)%43),y=19.f+(float)((stain*13)%49);art.setColour(stone.withAlpha(.08f));art.fillEllipse(x,y,1.4f+(stain%3)*.4f,2.5f);}
                // Coronal suture and layered frontal/parietal bone planes.
                juce::Path suture;suture.startNewSubPath(128.5f,25.f);
                suture.lineTo(135.f,23.f);suture.lineTo(139.f,24.2f);suture.lineTo(143.f,21.8f);
                suture.lineTo(148.f,23.f);suture.lineTo(152.f,21.f);suture.lineTo(157.f,23.5f);
                suture.lineTo(162.f,22.5f);suture.lineTo(167.f,25.f);suture.lineTo(173.f,27.f);
                art.setColour(ink.withAlpha(.13f));art.strokePath(suture,juce::PathStrokeType(.55f));
                art.setGradientFill(juce::ColourGradient(ivory.withAlpha(.20f),143.f,16.f,
                    ivory.withAlpha(0.f),155.f,33.f,true));art.fillEllipse(130.f,13.f,37.f,30.f);
                // Temporal fossa shading pulls the cranium into a skull.
                art.setGradientFill(juce::ColourGradient(
                    ink.withAlpha(.28f),127.f,42.f,
                    ink.withAlpha(0.f),140.f,58.f,false));
                art.fillRect(125.f,36.f,20.f,26.f);
                art.setGradientFill(juce::ColourGradient(
                    ink.withAlpha(.24f),175.f,42.f,
                    ink.withAlpha(0.f),162.f,58.f,true));
                art.fillRect(157.f,36.f,20.f,26.f);
            }
            g.setOpacity(1.f);g.drawImageTransformed(skullWeathering,juce::AffineTransform::scale(.5f).translated(124.f,10.f));
            // Forehead and cheekbone planes.
            juce::Path forehead;
            forehead.startNewSubPath(133.f,32.f);
            forehead.quadraticTo(151.f,19.f,166.f,29.f);
            forehead.lineTo(160.f,41.f);
            forehead.lineTo(139.f,42.f);
            forehead.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.18f),136.f,24.f,
                stone.withAlpha(.05f),164.f,41.f,false));
            g.fillPath(forehead);
            juce::Path cheek;
            cheek.startNewSubPath(164.f,55.f);
            cheek.lineTo(174.f,52.f);
            cheek.lineTo(170.f,62.f);
            cheek.lineTo(165.f,64.f);
            cheek.closeSubPath();
            g.setColour(ink.withAlpha(.20f));
            g.fillPath(cheek);
            // Temple and cheekbone planes distinguish the skull from a mask.
            for(int side:{-1,1}){
                const float d=(float)side;
                juce::Path temple;temple.startNewSubPath(151.f+d*20.f,32.f);
                temple.cubicTo(151.f+d*24.f,41.f,151.f+d*24.f,53.f,151.f+d*17.f,57.f);
                temple.quadraticTo(151.f+d*13.f,47.f,151.f+d*20.f,32.f);temple.closeSubPath();
                g.setGradientFill(juce::ColourGradient(stone.withAlpha(.44f),151.f+d*23.f,39.f,
                    ink.withAlpha(.04f),151.f+d*13.f,49.f,false));g.fillPath(temple);
                juce::Path arch;arch.startNewSubPath(151.f+d*21.f,52.f);
                arch.quadraticTo(151.f+d*17.f,59.f,151.f+d*10.f,61.f);
                g.setColour(ivory.withAlpha(.30f));g.strokePath(arch,juce::PathStrokeType(1.4f));
            }
            // Brow ridge.
            juce::Path brow;
            brow.startNewSubPath(128.f,42.f);
            brow.cubicTo(135.f,36.5f,145.f,38.2f,151.f,41.5f);
            brow.cubicTo(157.f,37.7f,168.f,38.8f,174.f,41.8f);
            g.setColour(ink.withAlpha(.54f));
            g.strokePath(brow,juce::PathStrokeType(2.8f));
            g.setColour(ivory.withAlpha(.38f));
            g.strokePath(brow,juce::PathStrokeType(1.2f));
            // Sockets and nasal aperture.
            juce::Path sockets;
            sockets.startNewSubPath(131.f,45.f);
            sockets.cubicTo(134.f,40.5f,143.f,40.f,146.7f,43.4f);
            sockets.quadraticTo(149.f,47.f,145.7f,51.8f);
            sockets.cubicTo(143.f,56.f,138.f,58.f,133.5f,55.f);
            sockets.quadraticTo(129.f,52.f,131.f,45.f);
            sockets.closeSubPath();
            sockets.startNewSubPath(156.f,42.f);
            sockets.cubicTo(160.f,39.8f,168.5f,41.f,172.f,45.5f);
            sockets.quadraticTo(174.f,50.f,169.f,54.8f);
            sockets.cubicTo(166.f,57.6f,160.f,57.8f,157.f,53.f);
            sockets.quadraticTo(153.f,48.f,156.f,42.f);
            sockets.closeSubPath();
            sockets.startNewSubPath(151.f,53.f);
            sockets.cubicTo(148.f,56.f,145.f,61.f,146.f,63.f);
            sockets.quadraticTo(149.f,64.f,151.f,61.f);
            sockets.quadraticTo(154.f,65.f,157.f,63.f);
            sockets.cubicTo(157.f,59.f,153.f,55.f,151.f,53.f);
            sockets.closeSubPath();
            g.setColour(ink.withAlpha(.99f));
            g.fillPath(sockets);
            g.setColour(stone.withAlpha(.55f));
            g.strokePath(sockets,juce::PathStrokeType(.65f));
            for(float cx:{139.f,163.f}){
                g.setGradientFill(juce::ColourGradient(stone.withAlpha(.43f),cx,55.f,ink.withAlpha(0.f),cx,47.f,false));
                g.fillEllipse(cx-5.f,48.f,10.f,7.f);
            }
            g.setColour(ivory.withAlpha(.30f));
            g.drawLine(133.f,44.f,145.f,42.f,1.f);
            g.drawLine(158.f,43.f,170.f,45.f,1.f);
            // Spectral glimmer deep in the sockets, tied to measured level.
            g.setGradientFill(juce::ColourGradient(
                alive.withAlpha(.04f+.26f*react),139.f,50.f,
                alive.withAlpha(0.f),144.f,54.f,true));
            g.fillEllipse(133.f,46.f,13.f,10.f);
            g.setGradientFill(juce::ColourGradient(
                alive.withAlpha(.04f+.20f*react),165.f,50.f,
                alive.withAlpha(0.f),160.f,54.f,true));
            g.fillEllipse(157.f,46.f,13.f,10.f);
            // Zygomatic arches.
            g.setColour(ivory.withAlpha(.46f));
            g.drawLine(129.f,56.f,139.f,60.f,1.5f);
            g.drawLine(173.f,55.f,163.f,60.f,1.5f);
            g.setColour(ink.withAlpha(.26f));
            g.drawLine(131.f,59.f,140.f,62.f,.8f);
            g.drawLine(171.f,58.f,163.f,62.f,.8f);
            // Fixed maxilla and the real dark oral cavity; upper teeth
            // never stretch with vocal level.
            juce::Path mouth;
            mouth.startNewSubPath(137.f,65.f);
            mouth.quadraticTo(151.f,63.f,165.f,65.f);
            mouth.cubicTo(165.f,73.f+jawDrop,158.f,78.f+jawDrop,151.f,78.f+jawDrop);
            mouth.cubicTo(144.f,78.f+jawDrop,137.f,73.f+jawDrop,137.f,65.f);
            mouth.closeSubPath();
            g.setColour(ink.withAlpha(.98f));
            g.fillPath(mouth);
            // Lower mandibular ramus, chin and dental ridge form one closed
            // piece. Its lower teeth follow the same displacement.
            juce::Path jaw;
            jaw.startNewSubPath(136.f,61.f);
            jaw.cubicTo(134.f,70.f,138.f,78.f+jawDrop,144.f,80.f+jawDrop);
            jaw.quadraticTo(151.f,83.f+jawDrop,158.f,80.f+jawDrop);
            jaw.cubicTo(164.f,78.f+jawDrop,168.f,70.f,166.f,61.f);
            jaw.lineTo(163.f,65.f);
            jaw.cubicTo(163.f,72.f,158.f,74.f+jawDrop,151.f,74.f+jawDrop);
            jaw.cubicTo(144.f,74.f+jawDrop,139.f,72.f,139.f,65.f);
            jaw.closeSubPath();
            g.setGradientFill(juce::ColourGradient(ivory.withAlpha(.96f),138.f,65.f,
                                                    stone,165.f,83.f+jawDrop,false));
            g.fillPath(jaw);
            g.setColour(ink.withAlpha(.64f));
            g.strokePath(jaw,juce::PathStrokeType(.9f));
            for(int tooth=0;tooth<8;++tooth) {
                const float x=139.4f+3.f*tooth+((tooth%3)-1)*.16f;
                const float edge=std::abs((float)tooth-3.5f)/3.5f;
                const float top=64.f+edge*.62f+(tooth%2)*.17f;
                const float lower=70.1f+jawDrop-edge*1.35f;
                g.setGradientFill(juce::ColourGradient(ivory,x,top,stone,x+2.65f,top+4.8f,false));
                g.fillRoundedRectangle(x,top,2.55f+(tooth%3)*.12f,4.8f-edge*.9f-(tooth%2)*.27f,.55f);
                g.setGradientFill(juce::ColourGradient(ivory.withAlpha(.84f),x,lower,
                                                        stone,x+2.65f,lower+3.5f,false));
                g.fillRoundedRectangle(x,lower,2.65f,3.5f,.60f);
            }
            juce::Path chin;
            chin.startNewSubPath(141.f,76.f+jawDrop*.8f);
            chin.quadraticTo(151.f,81.f+jawDrop,161.f,76.f+jawDrop*.8f);
            g.setColour(ivory.withAlpha(.28f));
            g.strokePath(chin,juce::PathStrokeType(1.1f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            // Sutures and the cold rim on the shadow side.
            g.setColour(stone.withAlpha(.47f));
            juce::Path suture;suture.startNewSubPath(143.f,14.f);suture.lineTo(142.f,19.f);suture.lineTo(144.f,22.f);suture.lineTo(141.f,26.f);suture.lineTo(143.f,30.f);suture.lineTo(141.f,34.f);
            suture.startNewSubPath(169.f,22.f);suture.lineTo(165.f,26.f);suture.lineTo(166.f,29.f);suture.lineTo(163.f,34.f);
            g.strokePath(suture,juce::PathStrokeType(.72f));
            g.setColour(cool.withAlpha(.10f+.09f*react));
            juce::Path rim;
            rim.startNewSubPath(127.f,38.f);
            rim.cubicTo(126.f,50.f,130.f,58.f,136.f,63.f);
            g.strokePath(rim,juce::PathStrokeType(1.3f));
            g.setColour(ivory.withAlpha(.17f));
            g.drawLine(166.f,26.f,171.f,38.f,1.1f);
        }

        // ---- Final spectral pass: a green underlight that ties the figure to
        // the same boards and beams the stage is lit with.
        g.setColour(alive.withAlpha(.04f+.13f*react));
        g.drawLine(138.f,75.f,163.f,75.f,.9f);
        g.drawLine(126.f,196.f,133.f,208.f,.9f);
        g.drawLine(175.f,195.f,169.f,207.f,.9f);
        g.drawLine(120.f,216.f,124.f,282.f,.9f);
        g.drawLine(180.f,215.f,192.f,280.f,.9f);
    }
private:
    mutable juce::Image skullWeathering,lowerBody;
    mutable int skullTheme=-1,lowerTheme=-1;
};
} // namespace skellpreset::stage
