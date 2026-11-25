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
              float activity,float phase) const {
        const float react=juce::jlimit(0.f,1.f,activity);
        const float sway=std::sin(phase*.62f);
        const float beat=std::sin(phase*1.45f);
        const float jawDrop=react*(2.2f+4.0f*(beat*.5f+.5f));
        const auto ink=Theme::bg;
        const auto ivory=Theme::white;
        const auto stone=Theme::muted;
        const auto alive=Theme::green;
        const auto cool=Theme::blue;

        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::rotation(
                (sway*.55f+beat*.45f)*react*.010f,151.f,368.f)
            .scaled(scale,scale*(1.f+.004f*react))
            .translated(originX,originY));

        // Tapered capsule between two joint centres. The gradient runs across
        // the bone, so every limb reads as a cylinder lit from the upper left.
        auto limb=[&](float x0,float y0,float r0,float x1,float y1,float r1,
                      float gloss) {
            const float dx=x1-x0,dy=y1-y0;
            const float len=juce::jmax(.001f,std::sqrt(dx*dx+dy*dy));
            const float nx=-dy/len,ny=dx/len;
            const float mx=(x0+x1)*.5f,my=(y0+y1)*.5f;
            const float mr=(r0+r1)*.5f;
            const float cap=std::atan2(ny,nx);
            juce::Path p;
            p.startNewSubPath(x0+nx*r0,y0+ny*r0);
            p.quadraticTo(mx+nx*mr*1.10f,my+ny*mr*1.10f,x1+nx*r1,y1+ny*r1);
            p.addCentredArc(x1,y1,r1,r1,0.f,cap,
                cap+juce::MathConstants<float>::pi,true);
            p.lineTo(x0-nx*r0,y0-ny*r0);
            p.addCentredArc(x0,y0,r0,r0,0.f,
                cap+juce::MathConstants<float>::pi,
                cap+juce::MathConstants<float>::twoPi,true);
            p.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(gloss*.98f),mx-nx*mr*1.05f,my-ny*mr*1.05f,
                stone.withAlpha(gloss*.74f),mx+nx*mr*1.20f,my+ny*mr*1.20f,false));
            g.fillPath(p);
            g.setColour(ink.withAlpha(.58f));
            g.strokePath(p,juce::PathStrokeType(juce::jmax(.45f,r1*.16f)));
            if(r1>1.2f){
                g.setColour(ivory.withAlpha(.34f*gloss+.06f));
                g.drawLine(x0-nx*r0*.40f,y0-ny*r0*.40f,
                           x1-nx*r1*.40f,y1-ny*r1*.40f,
                           juce::jmax(.5f,juce::jmin(r0,r1)*.44f));
            }
        };
        // Joint spheres are kept below the radius of the shaft they cap, so the
        // silhouette stays continuous instead of turning into a bead chain.
        auto joint=[&](float x,float y,float r,float gloss) {
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(gloss*.86f),x-r*.5f,y-r*.6f,
                stone.withAlpha(gloss*.62f),x+r*.6f,y+r*.65f,false));
            g.fillEllipse(x-r,y-r,r*2.f,r*2.f);
            g.setColour(ink.withAlpha(.62f));
            g.drawEllipse(x-r,y-r,r*2.f,r*2.f,juce::jmax(.5f,r*.16f));
            g.setColour(ivory.withAlpha(.20f*gloss+.03f));
            g.fillEllipse(x-r*.50f,y-r*.62f,r*.48f,r*.44f);
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

        // ---- Legs. Weight carried on the left leg, right leg planted a little
        // wider, which gives the pose a working stance instead of a mannequin.
        // Shafts first, then small caps at the actual articulations.
        limb(134.f,214.f,7.2f,118.f,286.f,5.4f,.96f);
        limb(168.f,214.f,7.1f,194.f,284.f,5.3f,.94f);
        joint(133.f,213.f,6.0f,.90f);
        joint(169.f,213.f,5.9f,.88f);
        // Knees are a single wide condyle mass, not a pair of beads.
        auto condyle=[&](float x,float y,float w,float h,float gloss){
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(gloss*.90f),x-w*.22f,y-h*.58f,
                stone.withAlpha(gloss*.66f),x+w*.36f,y+h*.55f,false));
            g.fillEllipse(x-w*.5f,y-h*.5f,w,h);
            g.setColour(ink.withAlpha(.62f));
            g.drawEllipse(x-w*.5f,y-h*.5f,w,h,juce::jmax(.5f,h*.12f));
            g.setColour(ivory.withAlpha(.22f));
            g.fillEllipse(x-w*.30f,y-h*.42f,w*.34f,h*.30f);
            g.setColour(ink.withAlpha(.34f));
            g.drawLine(x-w*.26f,y+h*.12f,x+w*.26f,y+h*.12f,.9f);
        };
        condyle(119.f,288.f,13.5f,11.f,.90f);
        condyle(195.f,286.f,13.f,10.5f,.90f);
        limb(118.f,293.f,6.2f,114.f,348.f,4.4f,.96f);
        limb(194.f,291.f,6.1f,200.f,346.f,4.3f,.94f);
        limb(126.f,296.f,1.9f,121.f,344.f,1.5f,.80f);   // fibulae
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
                wing.cubicTo(151.f+s*20.f,176.f,151.f+s*32.f,183.f,
                             151.f+s*31.f,195.f);
                wing.cubicTo(151.f+s*30.f,206.f,151.f+s*20.f,213.f,
                             151.f+s*9.f,212.f);
                wing.cubicTo(151.f+s*7.f,202.f,151.f+s*6.f,191.f,
                             151.f+s*6.f,181.f);
                wing.closeSubPath();
                g.setGradientFill(juce::ColourGradient(
                    ivory.withAlpha(.80f),151.f+s*12.f,179.f,
                    stone.withAlpha(.54f),151.f+s*31.f,211.f,false));
                g.fillPath(wing);
                g.setColour(ink.withAlpha(.66f));
                g.strokePath(wing,juce::PathStrokeType(.9f));
                // Bright iliac crest along the top of each wing.
                juce::Path crest;
                crest.startNewSubPath(151.f+s*7.f,181.f);
                crest.cubicTo(151.f+s*20.f,176.f,151.f+s*31.f,183.f,
                              151.f+s*31.f,194.f);
                g.setColour(ivory.withAlpha(.34f));
                g.strokePath(crest,juce::PathStrokeType(2.f,
                    juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                // Obturator foramen: a slot low on the wing, angled outward,
                // which reads as pelvic anatomy instead of a round eye.
                juce::Graphics::ScopedSaveState hole(g);
                g.addTransform(juce::AffineTransform::rotation(
                    s*.30f,151.f+s*17.f,204.f));
                g.setColour(ink.withAlpha(.92f));
                g.fillRoundedRectangle(151.f+s*13.f,199.f,9.f,12.f,4.f);
                g.setColour(stone.withAlpha(.26f));
                g.drawRoundedRectangle(151.f+s*13.f,199.f,9.f,12.f,4.f,.7f);
            }
            // Sacrum recessed between the wings, plus the coccyx tip.
            juce::Path sacrum;
            sacrum.startNewSubPath(146.f,183.f);
            sacrum.lineTo(156.f,183.f);
            sacrum.lineTo(154.f,197.f);
            sacrum.lineTo(151.f,201.f);
            sacrum.lineTo(148.f,197.f);
            sacrum.closeSubPath();
            g.setGradientFill(juce::ColourGradient(
                stone.withAlpha(.60f),146.f,183.f,
                ink.withAlpha(.50f),156.f,200.f,false));
            g.fillPath(sacrum);
            g.setColour(ivory.withAlpha(.12f));
            for(int plate=0;plate<3;++plate)
                g.drawLine(148.f,187.f+plate*3.4f,154.f,187.f+plate*3.4f,.7f);
            // Pubic arch linking the two wings across the front.
            juce::Path pubis;
            pubis.startNewSubPath(135.f,211.f);
            pubis.cubicTo(140.f,217.f,147.f,219.f,151.f,219.f);
            pubis.cubicTo(155.f,219.f,162.f,217.f,167.f,211.f);
            g.setColour(stone.withAlpha(.54f));
            g.strokePath(pubis,juce::PathStrokeType(2.6f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            g.setColour(ivory.withAlpha(.18f));
            g.strokePath(pubis,juce::PathStrokeType(1.1f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }

        // ---- Lumbar vertebrae, narrower than the thoracic stack above them.
        for(int v=0;v<3;++v){
            const float y=166.f+v*6.6f;
            const float w=14.f+v*.8f;
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
        const float ribReach[6]{36.f,38.f,37.f,34.f,30.f,26.f};
        const float ribDrop[6]{4.6f,6.2f,8.0f,9.8f,11.4f,12.8f};
        for(int r=0;r<6;++r){
            const float ys=102.f+r*8.2f;
            const float reach=ribReach[r]*(1.f+.020f*react);
            const float drop=ribDrop[r];
            for(int side:{-1,1}){
                const float s=(float)side;
                // Bony rib: spine -> lateral margin -> forward, ending in open
                // space well short of the sternum.
                juce::Path p;
                p.startNewSubPath(151.f+s*4.f,ys);
                p.cubicTo(151.f+s*reach*.50f,ys+drop*.10f,
                          151.f+s*reach*.98f,ys+drop*.48f,
                          151.f+s*reach,ys+drop*.74f);
                p.cubicTo(151.f+s*reach*.84f,ys+drop*1.04f,
                          151.f+s*reach*.52f,ys+drop*1.02f,
                          151.f+s*(reach*.34f+9.f),ys+drop*.82f);
                g.setColour(ink.withAlpha(.86f));
                g.strokePath(p,juce::PathStrokeType(4.3f,
                    juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                g.setColour(ivory.withAlpha(.64f-.022f*r));
                g.strokePath(p,juce::PathStrokeType(2.5f,
                    juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                g.setColour(alive.withAlpha(.03f+.08f*react));
                g.strokePath(p,juce::PathStrokeType(.7f,
                    juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
                // Costal cartilage: faint continuation into the sternum.
                juce::Path cart;
                cart.startNewSubPath(151.f+s*(reach*.34f+9.f),ys+drop*.82f);
                cart.cubicTo(151.f+s*(reach*.24f+6.f),ys+drop*.92f,
                             151.f+s*10.f,ys+drop*.86f,
                             151.f+s*6.5f,ys+drop*.70f);
                g.setColour(stone.withAlpha(.34f));
                g.strokePath(cart,juce::PathStrokeType(1.5f,
                    juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            }
        }
        // Sternum: manubrium, body and a short xiphoid.
        {
            juce::Path st;
            st.startNewSubPath(146.f,98.f);
            st.lineTo(156.f,98.f);
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

        // ---- Right arm: shoulder, elbow dropped and pushed out, forearm
        // rising back in to the wrist so the fist can close on the mic barrel.
        // The wrist is placed at the exact anchor the microphone block uses.
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
            g.addTransform(juce::AffineTransform::rotation(
                .22f,205.f,124.f));
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
        // ---- The fist, drawn upright (not in the mic frame) so the fingers
        // visibly wrap the sleeve at the wrist anchor.
        {
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
        // Slack cable falling from the mic base, past the ribs and away.
        {
            juce::Path cable;
            cable.startNewSubPath(209.f,133.f);
            cable.cubicTo(224.f,150.f,232.f,176.f,228.f,206.f);
            cable.cubicTo(225.f,232.f,238.f,250.f,250.f,268.f);
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
                std::sin(phase*1.7f)*react*.045f,151.f,70.f));
            juce::Path skull;
            skull.startNewSubPath(151.f,13.f);
            skull.cubicTo(133.f,13.f,127.f,24.f,127.f,36.f);
            skull.cubicTo(127.f,46.f,130.f,53.f,134.f,57.f);
            skull.cubicTo(136.f,60.f,137.f,63.f,137.f,66.f);
            skull.cubicTo(139.f,74.f,145.f,78.f,151.f,78.f);
            skull.cubicTo(157.f,78.f,163.f,74.f,165.f,66.f);
            skull.cubicTo(165.f,63.f,166.f,60.f,168.f,57.f);
            skull.cubicTo(172.f,53.f,175.f,46.f,175.f,36.f);
            skull.cubicTo(175.f,24.f,169.f,13.f,151.f,13.f);
            skull.closeSubPath();
            g.setColour(ink);
            g.strokePath(skull,juce::PathStrokeType(3.8f));
            g.setGradientFill(juce::ColourGradient(
                ivory.withAlpha(.88f),129.f,20.f,
                stone.withAlpha(.54f),173.f,72.f,false));
            g.fillPath(skull);
            {
                juce::Graphics::ScopedSaveState texture(g);
                g.reduceClipRegion(skull);
                for(int fleck=0;fleck<80;++fleck){
                    const float x=126.f+(float)((fleck*43)%50);
                    const float y=16.f+(float)((fleck*61)%58);
                    g.setColour((fleck%3==0?ink:ivory).withAlpha(.045f));
                    g.fillEllipse(x,y,.9f+(fleck%3)*.35f,.65f+(fleck%2)*.35f);
                }
                // Temporal fossa shading pulls the cranium into a skull.
                g.setGradientFill(juce::ColourGradient(
                    ink.withAlpha(.28f),127.f,42.f,
                    ink.withAlpha(0.f),140.f,58.f,false));
                g.fillRect(125.f,36.f,20.f,26.f);
                g.setGradientFill(juce::ColourGradient(
                    ink.withAlpha(.24f),175.f,42.f,
                    ink.withAlpha(0.f),162.f,58.f,true));
                g.fillRect(157.f,36.f,20.f,26.f);
            }
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
            // Brow ridge.
            juce::Path brow;
            brow.startNewSubPath(128.f,42.f);
            brow.cubicTo(135.f,38.f,146.f,39.f,151.f,41.f);
            brow.cubicTo(157.f,39.f,169.f,38.f,174.f,41.f);
            g.setColour(ink.withAlpha(.54f));
            g.strokePath(brow,juce::PathStrokeType(2.8f));
            g.setColour(ivory.withAlpha(.38f));
            g.strokePath(brow,juce::PathStrokeType(1.2f));
            // Sockets and nasal aperture.
            juce::Path sockets;
            sockets.startNewSubPath(131.f,45.f);
            sockets.lineTo(146.f,42.f);
            sockets.lineTo(147.f,53.f);
            sockets.lineTo(137.f,57.f);
            sockets.lineTo(131.f,52.f);
            sockets.closeSubPath();
            sockets.startNewSubPath(156.f,42.f);
            sockets.lineTo(171.f,45.f);
            sockets.lineTo(171.f,53.f);
            sockets.lineTo(162.f,57.f);
            sockets.lineTo(156.f,52.f);
            sockets.closeSubPath();
            sockets.startNewSubPath(146.f,55.f);
            sockets.lineTo(156.f,55.f);
            sockets.lineTo(151.f,64.f);
            sockets.closeSubPath();
            g.setColour(ink.withAlpha(.95f));
            g.fillPath(sockets);
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
            // Maxilla and the upper teeth.
            g.setColour(ink.withAlpha(.90f));
            g.fillRoundedRectangle(139.f,64.f,24.f,6.6f+jawDrop*.28f,2.f);
            for(int tooth=0;tooth<7;++tooth){
                const float x=140.f+3.4f*tooth;
                g.setColour(ivory.withAlpha(.78f));
                g.fillRect(x,64.f,2.9f,4.f);
                g.setColour(ink.withAlpha(.50f));
                g.drawLine(x+2.9f,64.f,x+2.9f,68.f,.5f);
            }
            // Mandible follows the jaw opening.
            juce::Path jaw;
            jaw.startNewSubPath(137.f,68.f);
            jaw.cubicTo(139.f,76.f+jawDrop*.55f,146.f,81.f+jawDrop,
                        151.f,81.f+jawDrop);
            jaw.cubicTo(157.f,81.f+jawDrop,164.f,76.f+jawDrop*.6f,165.f,68.f);
            g.setColour(ink.withAlpha(.58f));
            g.strokePath(jaw,juce::PathStrokeType(3.8f));
            g.setColour(ivory.withAlpha(.56f));
            g.strokePath(jaw,juce::PathStrokeType(2.f));
            for(int tooth=0;tooth<6;++tooth){
                const float x=140.f+3.6f*tooth;
                const float drop=jawDrop*.55f;
                g.setColour(ivory.withAlpha(.64f));
                g.fillRect(x,70.6f+drop,2.7f,3.2f);
            }
            g.setColour(alive.withAlpha(.05f+.17f*react));
            g.drawLine(140.f,70.f,163.f,70.f,1.f);
            // Sutures and the cold rim on the shadow side.
            g.setColour(stone.withAlpha(.46f));
            g.drawLine(143.f,17.f,141.f,27.f,.9f);
            g.drawLine(141.f,27.f,144.f,34.f,.9f);
            g.drawLine(168.f,23.f,164.f,33.f,.8f);
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
};
} // namespace skellpreset::stage
