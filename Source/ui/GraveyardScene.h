#pragma once
#include "Theme.h"
#include <cmath>
#include <cstdint>

namespace skellpreset::stage {
// Deterministic 0..1 hash so static scenery never shimmers between frames and
// no random_device/global state touches the paint path.
inline float hash01(int n) noexcept {
    std::uint32_t x=(std::uint32_t)n;
    x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;
    return (float)(x&0xffffffU)/16777215.f;
}

// Static cemetery geometry, sky and atmosphere are rasterized once per
// size/theme/horizon. Authoring is entirely native JUCE geometry on the
// message thread: no imported art, no audio-thread work, no per-frame cost.
//
// Composition, back to front: sickly sky glow, torn cloud banks, a large low
// moon, a far treeline and ruined chapel, an iron fence and gate, three banks
// of headstones, ground fog, then the boards the performer stands on. The
// palette is deliberately desaturated: cold blue-grey sky, dead bone-grey
// stone, and one warm amber window as the only saturated note.
//
// `horizon` is the visual skyline of the cemetery, which sits well above the
// performer's feet. Keeping the two lines separate is what gives the graves
// real depth instead of squeezing them into a strip under the crowd pit.
class Backdrop {
public:
    // One shared moon rectangle drives both the cached artwork and live hit target.
    static juce::Rectangle<float> moonDisc(float width,float horizon) noexcept {
        const float radius=juce::jmax(18.f,juce::jmin(64.f,width*.054f));
        const float x=width*.760f,y=juce::jmax(radius+9.f,horizon*.30f);
        return {x-radius,y-radius,2.f*radius,2.f*radius};
    }
    // Paint the cached plate in local coordinates; callers use canvas-space
    // skyline positions. One conversion is shared by art, moon and atmosphere.
    static float localSkyline(juce::Rectangle<float> bounds,float horizon) noexcept {
        return juce::jlimit(28.f,juce::jmax(28.f,bounds.getHeight()-24.f),horizon-bounds.getY());
    }
    static juce::Rectangle<float> moonBounds(juce::Rectangle<float> bounds,float horizon) noexcept {
        return moonDisc(bounds.getWidth(),localSkyline(bounds,horizon)).translated(bounds.getX(),bounds.getY());
    }
    void draw(juce::Graphics& target,juce::Rectangle<float> bounds,float horizon) {
        const int w=juce::jmax(1,(int)bounds.getWidth()),h=juce::jmax(1,(int)bounds.getHeight());
        const float skyline=localSkyline(bounds,horizon);
        if(image.getWidth()!=w || image.getHeight()!=h || lastTheme!=Theme::current
           || lastHorizon!=skyline) {
            image=juce::Image(juce::Image::ARGB,w,h,true);
            lastTheme=Theme::current;lastHorizon=skyline;
            juce::Graphics g(image);const float width=(float)w,height=(float)h;
            const float horizonY=skyline;
            const auto sky=juce::Colour(0xff152c39),stone=juce::Colour(0xff596861);
            const auto deep=juce::Colour(0xff070c12);
            const auto fog=juce::Colour(0xff9ab5b5);
            const auto disc=moonDisc(width,horizonY);
            const float moonX=disc.getCentreX(),moonY=disc.getCentreY(),moonR=disc.getWidth()*.5f;

            // --- Sky: midnight gradient with a sickly green bloom behind the
            // moon, which is what makes the scene feel diseased rather than
            // merely dark.
            g.setGradientFill(juce::ColourGradient(sky,width*.5f,0.f,deep,width*.5f,height,false));
            g.fillAll();
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xff274d50).withAlpha(.62f),moonX,moonY-moonR,
                juce::Colour(0xff0a1218).withAlpha(0.f),moonX,horizonY*1.10f,true));
            g.fillRect(0.f,0.f,width,horizonY);

            // A cool aurora behind the ruined skyline, layered into the
            // cached plate, never evaluated in the audio callback.
            for(int band=0;band<3;++band){
                juce::Path veil;veil.startNewSubPath(0.f,horizonY*(.36f+band*.08f));
                veil.cubicTo(width*.22f,horizonY*.03f,width*.50f,horizonY*.54f,width,horizonY*(.15f+band*.08f));
                veil.cubicTo(width*.77f,horizonY*.66f,width*.38f,horizonY*.16f,0.f,horizonY*(.47f+band*.08f));veil.closeSubPath();
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xff527b78).withAlpha(.055f-band*.012f),width*.48f,horizonY*.36f,juce::Colour(0xff112020).withAlpha(0.f),width*.92f,horizonY*.17f,false));g.fillPath(veil);
            }
            // Deterministic depth layers cached with the static environment.
            for(int layer=0;layer<3;++layer){
                const float baseline=horizonY*(.73f+.095f*layer);
                juce::Path ridge;ridge.startNewSubPath(0.f,baseline);
                for(int k=0;k<=24;++k){
                    const float x=width*k/24.f;
                    const float y=baseline-(8.f+hash01(layer*421+k*47)*20.f)*(1.f-.20f*layer);
                    ridge.lineTo(x,y);
                }
                ridge.lineTo(width,horizonY+18.f);ridge.lineTo(0.f,horizonY+18.f);ridge.closeSubPath();
                g.setColour(juce::Colour(0xff18282c).interpolatedWith(deep,.32f+.20f*layer).withAlpha(.72f));
                g.fillPath(ridge);
            }
            g.setGradientFill(juce::ColourGradient(fog.withAlpha(.15f),moonX,horizonY*.91f,
                fog.withAlpha(0.f),moonX,horizonY*.48f,false));
            g.fillRect(0.f,horizonY*.48f,width,horizonY*.45f);
            // Stars, denser and brighter at the zenith, dimmed near the moon.
            for(int i=0;i<120;++i) {
                const float sx=hash01(i*3741)*width;
                const float sy=hash01(i*9173+11)*horizonY*.90f;
                const float depth=1.f-sy/juce::jmax(1.f,horizonY);
                const float dm=juce::jlimit(0.f,1.f,
                    std::sqrt((sx-moonX)*(sx-moonX)+(sy-moonY)*(sy-moonY))/(moonR*7.f));
                const float r=.30f+.95f*hash01(i*553)*depth;
                const float a=(.06f+.44f*depth*(.5f+.5f*hash01(i*31)))*dm;
                g.setColour(juce::Colours::white.withAlpha(a));
                g.fillEllipse(sx,sy,r*2.f,r*2.f);
                if(hash01(i*77)>.968f) {
                    g.setColour(juce::Colours::white.withAlpha(a*.60f));
                    g.drawLine(sx-r*3.f,sy,sx+r*3.f,sy,.6f);
                    g.drawLine(sx,sy-r*3.f,sx,sy+r*3.f,.6f);
                }
            }

            // Torn cloud banks drifting across the moon. Each band is a chain
            // of soft ellipses so the silhouette stays ragged, not a smear.
            for(int band=0;band<8;++band) {
                const float y=horizonY*(.04f+.082f*band);
                const float drift=-width*.12f+hash01(band*61)*width*.3f;
                const float a=.20f-.014f*band;
                for(int puff=0;puff<7;++puff) {
                    const float px=drift+width*(.06f*puff+.09f*hash01(band*211+puff));
                    const float pw=width*(.14f+.10f*hash01(band*97+puff*13));
                    const float ph=(9.f+7.f*hash01(band*43+puff))*1.7f;
                    g.setGradientFill(juce::ColourGradient(
                        juce::Colour(0xff2b4046).withAlpha(a),px,y,
                        juce::Colour(0xff0a1218).withAlpha(0.f),px+pw*.5f,y+ph,true));
                    g.fillEllipse(px,y,px+pw>width?width-px:pw,ph);
                }
            }

            // --- Moon: heavy silver disc with layered halo, craters and a
            // green rim from the fog.
            const float halo=moonR*3.4f;
            g.setGradientFill(juce::ColourGradient(fog.withAlpha(.25f),moonX,moonY,
                fog.withAlpha(0.f),moonX+halo,moonY,true));
            g.fillEllipse(moonX-halo,moonY-halo,halo*2.f,halo*2.f);
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xffe6f1e5).withAlpha(.98f),moonX-moonR*.45f,moonY-moonR*.55f,
                juce::Colour(0xff637c80).withAlpha(.80f),moonX+moonR*.6f,moonY+moonR*.75f,false));
            g.fillEllipse(moonX-moonR,moonY-moonR,moonR*2.f,moonR*2.f);
            // Real relief is clipped to the disc. No horizontal stripes.
            {
            juce::Graphics::ScopedSaveState lunar(g);
            juce::Path discClip;discClip.addEllipse(moonX-moonR,moonY-moonR,2.f*moonR,2.f*moonR);
            g.reduceClipRegion(discClip);
            for(int c=0;c<40;++c) {
                const float a=hash01(c*211)*6.2831f,r=moonR*.70f*std::sqrt(hash01(c*97));
                const float cx=moonX+std::cos(a)*r,cy=moonY+std::sin(a)*r;
                const float cr=moonR*(.07f+.20f*hash01(c*43));
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303b3b).withAlpha(.12f+.15f*hash01(c*13)),cx-cr*.35f,cy-cr*.3f,
                    juce::Colour(0xff7a8884).withAlpha(0.f),cx+cr,cy+cr,true));
                g.fillEllipse(cx-cr,cy-cr,cr*2.f,cr*2.f);
                if(cr>2.f){
                    juce::Path rim;rim.addCentredArc(cx,cy,cr,cr,0.f,3.1f,5.0f,true);
                    g.setColour(juce::Colours::white.withAlpha(.10f));g.strokePath(rim,juce::PathStrokeType(.7f));
                }
            }
            }
            g.setColour(fog.withAlpha(.25f));
            g.drawEllipse(moonX-moonR,moonY-moonR,moonR*2.f,moonR*2.f,.9f);

            // Layered mist gives the dark ridge atmospheric perspective.
            g.setGradientFill(juce::ColourGradient(fog.withAlpha(.22f),width*.30f,horizonY-65.f,
                fog.withAlpha(0.f),width*.30f,horizonY-150.f,false));
            g.fillRect(0.f,juce::jmax(0.f,horizonY-150.f),width,150.f);
            // Distant ridge, then a blacker treeline silhouette.
            juce::Path ridge;
            ridge.startNewSubPath(0.f,horizonY);
            for(int i=0;i<=30;++i) {
                const float x=width*i/30.f;
                const float y=horizonY-14.f-20.f*hash01(i*37)-9.f*std::sin(i*.9f);
                ridge.lineTo(x,y);
            }
            ridge.lineTo(width,horizonY);ridge.closeSubPath();
            g.setColour(juce::Colour(0xff0a1214));g.fillPath(ridge);
            for(int tree=0;tree<52;++tree) {
                const float x=hash01(tree*617)*width;
                const float th=9.f+15.f*hash01(tree*83);
                const float base=horizonY-6.f;
                juce::Path fir;
                fir.startNewSubPath(x,base-th);
                fir.lineTo(x-th*.30f,base);
                fir.lineTo(x+th*.30f,base);
                fir.closeSubPath();
                g.setColour(juce::Colour(0xff060b0d));
                g.fillPath(fir);
            }

            // --- Ruined chapel on the right shoulder, warm window the only
            // saturated colour in the scene, plus a broken bell tower.
            {
                const float x=width*.855f,base=horizonY-10.f,s=.62f;
                const auto wall=juce::Colour(0xff131c1d);
                g.setColour(wall);
                g.fillRect(x-54.f*s,base-104.f*s,108.f*s,104.f*s);
                juce::Path roof;
                roof.addTriangle(x-64.f*s,base-100.f*s,x,base-150.f*s,x+64.f*s,base-100.f*s);
                g.fillPath(roof);
                // Broken tower and spire.
                g.fillRect(x-72.f*s,base-176.f*s,26.f*s,176.f*s);
                juce::Path spire;
                spire.addTriangle(x-76.f*s,base-174.f*s,x-59.f*s,base-224.f*s,x-42.f*s,base-174.f*s);
                g.fillPath(spire);
                g.setColour(juce::Colour(0xff0b1113));
                g.fillRect(x-70.f*s,base-150.f*s,9.f*s,10.f*s); // belfry opening
                // Warm windows, one blown out and one guttering.
                const auto lamp=juce::Colour(0xffe8b478);
                for(int wx=0;wx<3;++wx) {
                    const float bx=x-30.f*s+wx*26.f*s;
                    const float glow=wx==1?.72f:.44f;
                    g.setColour(lamp.withAlpha(.18f*glow));
                    g.fillRoundedRectangle(bx-5.f,base-72.f*s,24.f*s,36.f*s,4.f);
                    g.setColour(lamp.withAlpha(glow));
                    g.fillRoundedRectangle(bx,base-70.f*s,15.f*s,31.f*s,3.f);
                    g.setColour(juce::Colour(0xff241a10));
                    g.drawLine(bx,base-56.f*s,bx+15.f*s,base-56.f*s,.8f);
                    g.drawLine(bx+7.5f*s,base-70.f*s,bx+7.5f*s,base-39.f*s,.8f);
                }
                // Door arch and stonework shadows.
                g.setColour(juce::Colour(0xff04080a));
                juce::Path door;
                door.startNewSubPath(x-15.f*s,base);
                door.lineTo(x-15.f*s,base-36.f*s);
                door.cubicTo(x-15.f*s,base-54.f*s,x+15.f*s,base-54.f*s,x+15.f*s,base-36.f*s);
                door.lineTo(x+15.f*s,base);
                door.closeSubPath();
                g.fillPath(door);
                g.setColour(stone.withAlpha(.30f));
                g.drawLine(x-52.f*s,base-92.f*s,x+52.f*s,base-92.f*s,1.f);
                for(int pillar=0;pillar<5;++pillar)
                    g.drawLine(x-46.f*s+pillar*23.f*s,base-92.f*s,
                               x-46.f*s+pillar*23.f*s,base,1.f);
                // Iron cross on the roof ridge.
                g.setColour(juce::Colour(0xff1a2326));
                g.fillRect(x-1.6f*s,base-172.f*s,3.2f*s,20.f*s);
                g.fillRect(x-7.f*s,base-166.f*s,14.f*s,3.f*s);
            }

            // --- Dead trees: heavy trunks with forked, clawing limbs.
            for(int side:{-1,1}) {
                const float x=width*(side<0?.050f:.955f),base=horizonY-4.f;
                const float dir=(float)side;
                juce::Path trunk;
                trunk.startNewSubPath(x-11.f,base);
                trunk.cubicTo(x+9.f,base-56.f,x-21.f,base-102.f,x+dir*15.f,base-168.f);
                trunk.cubicTo(x-3.f,base-112.f,x+16.f,base-58.f,x+9.f,base);
                trunk.closeSubPath();
                g.setColour(juce::Colour(0xff070c0d));g.fillPath(trunk);
                for(int branch=0;branch<8;++branch) {
                    const float y=base-34.f-branch*13.f;
                    const float bdir=(branch%2==0?-dir:dir);
                    const float reach=24.f+(branch%4)*12.f+16.f*hash01(side*31+branch*7);
                    juce::Path limb;
                    limb.startNewSubPath(x,y);
                    limb.cubicTo(x+bdir*12.f,y-22.f,x+bdir*reach*.60f,y-8.f,
                                 x+bdir*reach,y-40.f);
                    g.setColour(juce::Colour(0xff070c0d));
                    g.strokePath(limb,juce::PathStrokeType(juce::jmax(1.f,5.0f-branch*.44f)));
                    // Clawed twigs at each tip.
                    for(int twig=0;twig<3;++twig) {
                        const float t=twig*.6f;
                        g.drawLine(x+bdir*reach*(.62f+t*.16f),y-24.f-t*10.f,
                                   x+bdir*(reach+12.f+twig*5.f),y-32.f-t*14.f,
                                   juce::jmax(.6f,1.5f-branch*.1f));
                    }
                }
                // Hanging rope and a crow on the near tree.
                if(side<0) {
                    g.setColour(juce::Colour(0xff121a1c));
                    juce::Path rope;
                    rope.startNewSubPath(x+18.f,base-112.f);
                    rope.cubicTo(x+25.f,base-90.f,x+22.f,base-70.f,x+26.f,base-50.f);
                    g.strokePath(rope,juce::PathStrokeType(1.1f));
                    g.setColour(juce::Colour(0xff0a1012));
                    g.fillEllipse(x+22.f,base-49.f,8.f,10.f);
                    g.fillEllipse(x+27.f,base-55.f,5.f,5.f);
                }
            }

            // --- Wrought-iron fence with a central gate arch.
            const float fenceY=horizonY-54.f;
            for(int i=0;i<52;++i) {
                const float x=width*i/51.f;
                const bool gate=std::abs(x-width*.5f)<width*.060f;
                const float barTop=gate?fenceY-40.f:fenceY-25.f-hash01(i*61)*8.f;
                g.setColour(juce::Colour(0xff101a1c).withAlpha(.60f));
                g.drawLine(x,barTop,x+(hash01(i*41)-.5f)*2.0f,fenceY+10.f,i%8==0?2.4f:1.f);
                juce::Path spear;
                spear.addTriangle(x-2.2f,barTop+1.f,x,barTop-7.f,x+2.2f,barTop+1.f);
                g.fillPath(spear);
            }
            g.setColour(juce::Colour(0xff101a1c));
            g.drawHorizontalLine((int)fenceY-42,0.f,width);
            g.drawHorizontalLine((int)fenceY-24,0.f,width);
            g.drawHorizontalLine((int)fenceY-6,0.f,width);
            {
                const float gx=width*.5f,gw=width*.064f;
                g.setColour(juce::Colour(0xff0e1618));
                for(int s:{-1,1}) {
                    g.fillRect(gx+s*gw-4.f,fenceY-56.f,8.f,fenceY+12.f-(fenceY-56.f));
                    g.fillRect(gx+s*gw-8.f,fenceY-60.f,16.f,7.f);
                }
                juce::Path arch;
                arch.startNewSubPath(gx-gw-4.f,fenceY-52.f);
                arch.cubicTo(gx-gw*.55f,fenceY-80.f,gx+gw*.55f,fenceY-80.f,gx+gw+4.f,fenceY-52.f);
                g.strokePath(arch,juce::PathStrokeType(3.6f));
                g.setColour(stone.withAlpha(.34f));
                g.strokePath(arch,juce::PathStrokeType(1.1f));
                // Iron finial spikes along the arch.
                for(int spike=0;spike<7;++spike) {
                    const float t=spike/6.f;
                    const float ax=gx-gw+2.f*gw*t;
                    const float ay=fenceY-52.f-27.f*std::sin(t*juce::MathConstants<float>::pi);
                    g.setColour(juce::Colour(0xff0e1618));
                    g.drawLine(ax,ay,ax,ay-7.f,1.f);
                }
            }

            // --- Graves in three depth banks. Far bank is small and dense,
            // near bank is large and sparse, all pushed off-centre so the
            // performer's silhouette stays clean.
            for(int layer=0;layer<3;++layer) {
                const int count=layer==0?24:layer==1?17:12;
                const float s=.42f+.30f*layer;
                const float base=horizonY-34.f+layer*13.f;
                for(int i=0;i<count;++i) {
                    const float slot=(i+.5f+(hash01(layer*97+i*13)-.5f)*.6f)*width/count;
                    const float x=slot+(layer==2?(hash01(i*211)-.5f)*width*.02f:0.f);
                    if(std::abs(x-width*.5f)<width*(.16f-.03f*layer))continue;
                    const int kind=(int)(hash01(layer*53+i*911)*5.f)%5;
                    const float tw=(11.f+hash01(i*29)*8.f)*s;
                    const float th=(24.f+hash01(i*71)*20.f)*s;
                    const auto shade=stone.withAlpha(.44f+.10f*layer);
                    g.setColour(Theme::bg.withAlpha(.72f));
                    g.fillEllipse(x-tw*.95f,base-2.f,tw*1.9f,8.f*s);
                    // Soft moonlit top edge on every stone.
                    const auto topLight=juce::Colours::white.withAlpha(.12f+.04f*layer);
                    if(kind==0) { // Rounded headstone
                        const auto r=juce::Rectangle<float>(x-tw*.5f,base-th,tw,th);
                        g.setColour(shade);g.fillRoundedRectangle(r,tw*.44f);
                        g.setColour(Theme::bg.withAlpha(.46f));
                        g.fillRect(r.withTop(r.getBottom()-th*.44f));
                        g.setColour(topLight);
                        g.drawLine(x-tw*.30f,base-th+tw*.22f,x+tw*.30f,base-th+tw*.22f,.9f);
                        g.setColour(Theme::muted.withAlpha(.13f+.02f*layer));
                        for(int line=0;line<2;++line)
                            g.drawLine(x-tw*.22f,base-th*.70f+line*th*.14f,
                                       x+tw*.22f,base-th*.70f+line*th*.14f,.7f);
                    } else if(kind==1) { // Cross
                        g.setColour(shade);
                        g.fillRect(x-2.6f*s,base-th,5.2f*s,th);
                        g.fillRect(x-10.f*s,base-th+9.f*s,20.f*s,5.f*s);
                        g.setColour(topLight);
                        g.fillRect(x-10.f*s,base-th+9.f*s,20.f*s,1.f*s);
                    } else if(kind==2) { // Obelisk
                        juce::Path ob;
                        ob.startNewSubPath(x-5.f*s,base);
                        ob.lineTo(x-2.8f*s,base-th*.86f);
                        ob.lineTo(x,base-th);
                        ob.lineTo(x+2.8f*s,base-th*.86f);
                        ob.lineTo(x+5.f*s,base);
                        ob.closeSubPath();
                        g.setColour(shade);g.fillPath(ob);
                        g.setColour(topLight);
                        g.drawLine(x-2.2f*s,base-th*.84f,x,base-th+1.f,.9f);
                    } else if(kind==3) { // Pedestal with urn
                        g.setColour(shade);
                        g.fillRect(x-6.5f*s,base-12.f*s,13.f*s,12.f*s);
                        g.fillRect(x-4.8f*s,base-19.f*s,9.6f*s,7.f*s);
                        g.fillEllipse(x-4.2f*s,base-27.f*s,8.4f*s,8.4f*s);
                        g.setColour(topLight);
                        g.drawLine(x-3.f*s,base-26.f*s,x+3.f*s,base-26.f*s,.8f);
                    } else { // Tilted slab
                        juce::Graphics::ScopedSaveState tilt(g);
                        g.addTransform(juce::AffineTransform::rotation(
                            (hash01(i*367)-.5f)*.16f,x,base));
                        g.setColour(shade);
                        g.fillRect(x-tw*.52f,base-th,tw*1.04f,th);
                        g.setColour(topLight);
                        g.drawLine(x-tw*.46f,base-th+1.f,x+tw*.46f,base-th+1.f,.9f);
                    }
                    // Weathered crack / moss.
                    g.setColour(juce::Colour(0xff2c3d33).withAlpha(.30f));
                    g.drawLine(x-tw*.24f,base-th*.62f,x-tw*.10f,base-2.f,.8f);
                }
            }

            // --- Dead grass and iron grave markers along the fence line.
            for(int i=0;i<190;++i) {
                const float x=hash01(i*733)*width;
                const float base=horizonY-20.f+hash01(i*97)*22.f;
                if(std::abs(x-width*.5f)<width*.10f)continue;
                g.setColour(juce::Colour(0xff243026).withAlpha(.50f+.24f*hash01(i*13)));
                for(int blade=0;blade<4;++blade) {
                    const float bx=x+(blade-1.5f)*1.7f;
                    const float hgt=3.f+7.f*hash01(i*31+blade);
                    g.drawLine(bx,base,bx+(blade-1.5f)*2.1f,base-hgt,.7f);
                }
            }

            // --- Ground fog: several soft horizontal banks of pale mist that
            // make the cemetery feel damp and cold.
            for(int bank=0;bank<13;++bank) {
                const float y=horizonY-64.f+bank*11.f;
                g.setGradientFill(juce::ColourGradient(
                    fog.withAlpha(.048f+(bank%3)*.016f),width*.20f,y,
                    juce::Colour(0xff0a1218).withAlpha(0.f),width*.96f,y,true));
                g.fillEllipse(-width*.18f,y,width*1.36f,24.f+bank*2.f);
            }
            // Cold moonlight wash across the near graves so the stones are
            // readable instead of sinking into the black.
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xff9fc4c0).withAlpha(.16f),moonX,horizonY-40.f,
                juce::Colour(0xff9fc4c0).withAlpha(0.f),moonX,horizonY+14.f,false));
            g.fillRect(0.f,horizonY-52.f,width,72.f);

            // --- Raised boards in perspective with plank seams and lip light.
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff24312e),width*.5f,horizonY-4.f,
                                                   Theme::bg,width*.5f,height,false));
            g.fillRect(0.f,horizonY,width,height-horizonY);
            // Broken slate seams and irregular, fine-grain mottling replace
            // the uniform wire grid. This plate is cached, not redrawn per frame.
            for(int tile=0;tile<11;++tile){
                const float t=(float)(tile+1)/12.f;
                const float y=horizonY+(height-horizonY)*t*t;
                const float next=horizonY+(height-horizonY)*std::pow((float)(tile+2)/12.f,2.f);
                g.setColour(stone.withAlpha(.070f+.045f*t));
                g.drawLine(0.f,y,width,y+hash01(tile*91)*2.f,.7f);
                const float span=40.f+130.f*t;
                for(float x=-span+hash01(tile*72)*span;x<width;x+=span){
                    g.setColour(Theme::bg.withAlpha(.14f));
                    g.drawLine(x,y,x+(x-width*.5f)*.055f,next,.7f);
                }
            }
            for(int grain=0;grain<420;++grain){
                const float t=hash01(grain*137+3),y=horizonY+t*(height-horizonY);
                const float x=hash01(grain*571+9)*width;
                g.setColour((grain%3==0?fog:Theme::bg).withAlpha(.025f+.045f*t));
                g.fillEllipse(x,y,1.f+4.f*t, .6f+1.1f*t);
            }
            // Damp reflection of the moon on the boards.
            g.setGradientFill(juce::ColourGradient(
                juce::Colours::white.withAlpha(.075f),moonX,horizonY+2.f,
                juce::Colours::white.withAlpha(0.f),moonX,horizonY+40.f,true));
            g.fillEllipse(moonX-moonR*1.1f,horizonY,moonR*2.2f,36.f);
            // Wet lip highlight: a soft band, never a one-pixel laser line.
            g.setGradientFill(juce::ColourGradient(
                Theme::green.withAlpha(.22f),0.f,horizonY-1.f,
                Theme::green.withAlpha(.03f),0.f,horizonY+7.f,false));
            g.fillRect(0.f,horizonY-1.f,width,8.f);
            g.setGradientFill(juce::ColourGradient(
                Theme::green.withAlpha(.075f),width*.5f,horizonY,
                Theme::bg.withAlpha(0.f),width*.5f,horizonY+70.f,false));
            g.fillRect(0.f,horizonY,width,70.f);

            // Crows wheeling over the graves, small enough to read as motion
            // even in a still frame.
            for(int crow=0;crow<9;++crow) {
                const float cx=width*(.14f+.10f*crow)+40.f*hash01(crow*57);
                const float cy=horizonY*(.24f+.060f*(crow%4));
                const float s=1.5f+.9f*hash01(crow*91);
                juce::Path bird;
                bird.startNewSubPath(cx-s*4.f,cy);
                bird.quadraticTo(cx-s*1.6f,cy-s*2.6f,cx,cy-s*.3f);
                bird.quadraticTo(cx+s*1.6f,cy-s*2.6f,cx+s*4.f,cy);
                g.setColour(juce::Colour(0xff05090b).withAlpha(.86f));
                g.strokePath(bird,juce::PathStrokeType(1.05f));
            }

            // Two asymmetrical foreground gravestones flank the boards.
            // Their chipped outlines, side faces and damp stone highlights
            // establish scale between the distant cemetery and the singer.
            for(int side:{-1,1}){
                const float x=width*(side<0?.13f:.89f);
                const float base=horizonY+juce::jmin(69.f,(height-horizonY)*.30f);
                const float tall=juce::jmin(104.f,horizonY*.47f)*(side<0?1.f:.74f);
                const float wide=tall*.38f;
                if(tall<18.f)continue;
                juce::Path slab;
                slab.startNewSubPath(x-wide*.5f,base);
                slab.lineTo(x-wide*.55f,base-tall*.65f);
                slab.quadraticTo(x-wide*.38f,base-tall,x+wide*.06f,base-tall);
                slab.quadraticTo(x+wide*.50f,base-tall*.94f,x+wide*.52f,base-tall*.66f);
                slab.lineTo(x+wide*.52f,base);slab.closeSubPath();
                g.setGradientFill(juce::ColourGradient(stone.withAlpha(.72f),x-wide*.55f,base-tall,
                    deep,x+wide*.52f,base,false));g.fillPath(slab);
                g.setColour(fog.withAlpha(.22f));g.strokePath(slab,juce::PathStrokeType(.9f));
                g.setColour(deep.withAlpha(.67f));
                g.fillRoundedRectangle(x-wide*.65f,base-4.f,wide*1.3f,8.f,1.3f);
                for(int line=0;line<4;++line){
                    const float y=base-tall*.61f+line*tall*.092f;
                    g.setColour(deep.withAlpha(.45f));g.drawLine(x-wide*.30f,y,x+wide*.28f,y,.9f);
                }
                g.setColour(deep.withAlpha(.63f));g.fillRoundedRectangle(x-1.5f,base-tall*.87f,3.f,tall*.16f,.6f);
                g.fillRoundedRectangle(x-wide*.20f,base-tall*.81f,wide*.4f,2.f,.6f);
            }
            // Edge vignette draws the eye to the performer without crushing
            // the cemetery detail the way a heavy overlay would.
            g.setGradientFill(juce::ColourGradient(Theme::bg.withAlpha(0.f),width*.5f,height*.44f,
                                                   Theme::bg.withAlpha(.20f),0.f,height*.44f,true));
            g.fillRect(0.f,0.f,width,height);
            // Re-lift the lower centre so the boards under the figure stay lit.
            g.setGradientFill(juce::ColourGradient(
                Theme::bg.withAlpha(.16f),width*.5f,horizonY-30.f,
                Theme::bg.withAlpha(0.f),width*.5f,height,false));
            g.fillRect(0.f,horizonY-30.f,width,height-horizonY+30.f);
        }
        target.drawImageAt(image,(int)bounds.getX(),(int)bounds.getY());
    }

    // Live atmosphere drawn on top of the cached plate: the chapel windows
    // gutter, cold mist drifts sideways and grave-fireflies pulse. All three
    // are cheap gradients and dots, so they can move every frame while the
    // cemetery itself stays a cached bitmap.
    void atmosphere(juce::Graphics& g,juce::Rectangle<float> bounds,float horizon,
                    float phase,float activity) const {
        const float width=bounds.getWidth(),height=bounds.getHeight();
        if(width<40.f || height<40.f)return;
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(bounds.toNearestInt());
        const float react=juce::jlimit(0.f,1.f,activity);
        const auto lamp=juce::Colour(0xffe8b478);
        const float chapelX=bounds.getX()+width*.855f;
        const float worldSkyline=bounds.getY()+localSkyline(bounds,horizon);
        const float chapelBase=worldSkyline-10.f;
        const float s=.62f;
        // Three guttering windows, each with its own slow irregular rhythm.
        for(int wx=0;wx<3;++wx) {
            const float flicker=.60f+.40f*std::sin(phase*(.42f+.17f*wx)+wx*2.1f)
                +.12f*std::sin(phase*(1.9f+wx*.4f));
            const float level=juce::jlimit(.10f,1.f,flicker)*(.72f+.28f*react);
            const float bx=chapelX-30.f*s+wx*26.f*s;
            const float w=15.f*s,h=31.f*s;
            g.setColour(lamp.withAlpha(.10f*level));
            g.fillEllipse(bx+w*.5f-26.f*s,chapelBase-70.f*s+h*.5f-26.f*s,
                          52.f*s,52.f*s);
            g.setColour(lamp.withAlpha(.30f*level));
            g.fillRoundedRectangle(bx,chapelBase-70.f*s,w,h,3.f);
        }
        // Distant storm: a soft, slowly breathing fork in the far sky, not a strobe.
        if(width>450.f && worldSkyline-bounds.getY()>115.f){
            const float pulse=.10f+.20f*std::pow(juce::jmax(0.f,std::sin(phase*.36f+1.2f)),6.f)+.08f*react;
            const float x=bounds.getX()+width*.22f,y=bounds.getY()+8.f,h=juce::jmin(112.f,(worldSkyline-y)*.62f);
            juce::Path bolt;bolt.startNewSubPath(x,y);bolt.lineTo(x+13.f,y+h*.24f);bolt.lineTo(x+2.f,y+h*.37f);bolt.lineTo(x+20.f,y+h*.57f);bolt.lineTo(x+9.f,y+h*.70f);bolt.lineTo(x+27.f,y+h);
            bolt.startNewSubPath(x+20.f,y+h*.57f);bolt.lineTo(x+42.f,y+h*.70f);bolt.lineTo(x+35.f,y+h*.89f);
            g.setColour(Theme::blue.withAlpha(pulse*.19f));g.strokePath(bolt,juce::PathStrokeType(3.1f));
            g.setColour(Theme::blue.withAlpha(pulse*.62f));g.strokePath(bolt,juce::PathStrokeType(.75f));
        }
        // Drifting mist: three soft bands moving at different speeds.
        const auto mist=juce::Colour(0xff8aa6a6);
        for(int band=0;band<3;++band) {
            const float speed=.055f+.028f*band;
            const float offset=std::fmod(phase*speed*width*.30f+band*width*.42f,width*1.5f);
            const float y=worldSkyline-70.f+band*26.f;
            const float a=.045f+.018f*band;
            g.setGradientFill(juce::ColourGradient(
                mist.withAlpha(0.f),bounds.getX()+offset-width*.7f,y,
                mist.withAlpha(a),bounds.getX()+offset-width*.2f,y+14.f,false));
            g.fillEllipse(bounds.getX()+offset-width*.9f,y,width*.9f,30.f+band*8.f);
        }
        // Grave fireflies: a dozen slow pulses just above the headstones.
        for(int i=0;i<14;++i) {
            const float seed=hash01(i*613);
            const float x=bounds.getX()+width*(.06f+.88f*hash01(i*271));
            const float y=worldSkyline-58.f+34.f*hash01(i*443);
            const float pulse=.5f+.5f*std::sin(phase*(.5f+.6f*seed)+i*1.7f);
            const float a=pulse*pulse*(.16f+.30f*react);
            if(a<.01f)continue;
            const float r=1.f+.8f*seed;
            g.setColour(juce::Colour(0xffc9ff7a).withAlpha(a*.30f));
            g.fillEllipse(x-r*2.6f,y-r*2.6f,r*5.2f,r*5.2f);
            g.setColour(juce::Colour(0xffdcffa8).withAlpha(a));
            g.fillEllipse(x-r*.5f,y-r*.5f,r,r);
        }
    }
private:
    juce::Image image;
    int lastTheme=-1;float lastHorizon=-1.f;
};
}
