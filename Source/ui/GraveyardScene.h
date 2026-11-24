#pragma once
#include "Theme.h"

namespace skellpreset::stage {
// Static cemetery geometry and haze are rasterized once per size/theme.
// Cache ownership is per editor, entirely on the message thread.
class Backdrop {
public:
    void draw(juce::Graphics& target,juce::Rectangle<float> bounds,float ground) {
        const int w=juce::jmax(1,(int)bounds.getWidth()),h=juce::jmax(1,(int)bounds.getHeight());
        const float horizon=ground-bounds.getY();
        if(image.getWidth()!=w || image.getHeight()!=h || lastTheme!=Theme::current || lastHorizon!=horizon) {
            image=juce::Image(juce::Image::ARGB,w,h,true);lastTheme=Theme::current;lastHorizon=horizon;
            juce::Graphics g(image);const float width=(float)w,height=(float)h;
            const auto sky=juce::Colour(0xff10161b),stone=juce::Colour(0xff25302f);
            g.setGradientFill(juce::ColourGradient(sky,width*.60f,0.f,Theme::bg,width*.5f,height,false));g.fillAll();
            // Moon and two broad atmospheric halos, deliberately low contrast.
            const float mx=width*.77f,my=height*.18f;
            for(int i=5;i>0;--i) {
                const float r=17.f+12.f*i;g.setColour(Theme::blue.withAlpha(.009f*(6-i)));
                g.fillEllipse(mx-r,my-r,r*2.f,r*2.f);
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffaebbb7).withAlpha(.52f),mx-12.f,my-13.f,
                juce::Colour(0xff5e7373).withAlpha(.25f),mx+13.f,my+14.f,false));
            g.fillEllipse(mx-17.f,my-17.f,34.f,34.f);
            for(int i=0;i<8;++i) {
                const float x=mx-10.f+(i%3)*8.f,y=my-10.f+(i/3)*7.f;
                g.setColour(Theme::bg.withAlpha(.09f));g.fillEllipse(x,y,5.f+(i%2)*3.f,4.f+(i%3)*2.f);
            }
            // Distant mausoleums and chapel spires, behind the fence and graves.
            for(int side=0;side<2;++side) {
                const float x=width*(side==0?.13f:.87f),base=horizon-25.f,scale=side==0?1.f:.74f;
                g.setColour(juce::Colour(0xff151d20));g.fillRect(x-40.f*scale,base-93.f*scale,80.f*scale,93.f*scale);
                juce::Path roof;roof.addTriangle(x-49.f*scale,base-91.f*scale,x,base-124.f*scale,x+49.f*scale,base-91.f*scale);
                g.fillPath(roof);g.fillRect(x-4.f*scale,base-139.f*scale,8.f*scale,23.f*scale);
                g.fillRect(x-11.f*scale,base-134.f*scale,22.f*scale,5.f*scale);
                g.setColour(Theme::bg);g.fillRoundedRectangle(x-13.f*scale,base-46.f*scale,26.f*scale,46.f*scale,12.f*scale);
                g.setColour(stone.withAlpha(.25f));
                g.drawLine(x-37.f*scale,base-84.f*scale,x+37.f*scale,base-84.f*scale,1.f);
                for(int pillar:{-1,1})g.fillRect(x+pillar*30.f*scale-3.f,base-78.f*scale,6.f*scale,78.f*scale);
            }
            // Twisted dead trees. Unequal branch lengths keep the silhouette organic.
            for(int side:{-1,1}) {
                const float x=width*(side<0?.065f:.95f),base=horizon-12.f;
                juce::Path trunk;trunk.startNewSubPath(x-9.f,base);trunk.cubicTo(x+8.f,base-60.f,x-20.f,base-122.f,x+side*14.f,base-211.f);
                trunk.cubicTo(x-2.f,base-141.f,x+15.f,base-65.f,x+7.f,base);trunk.closeSubPath();
                g.setColour(juce::Colour(0xff101617));g.fillPath(trunk);
                for(int branch=0;branch<7;++branch) {
                    const float y=base-55.f-branch*21.f,dir=(branch%2==0?-1.f:1.f),reach=35.f+(branch%3)*17.f;
                    juce::Path limb;limb.startNewSubPath(x,y);limb.cubicTo(x+dir*13.f,y-27.f,x+dir*reach*.6f,y-6.f,x+dir*reach,y-48.f);
                    g.strokePath(limb,juce::PathStrokeType(juce::jmax(1.f,5.f-branch*.5f)));
                    g.drawLine(x+dir*reach*.68f,y-26.f,x+dir*(reach+15.f),y-30.f,1.f);
                }
            }
            // Wrought-iron fence and cemetery depth layers.
            g.setColour(stone.withAlpha(.28f));
            for(int i=0;i<52;++i) {
                const float x=width*i/51.f,y=horizon-30.f;
                g.drawLine(x,y-27.f,x,y+8.f,1.f);
                juce::Path spear;spear.addTriangle(x-2.f,y-26.f,x,y-32.f,x+2.f,y-26.f);g.fillPath(spear);
            }
            g.drawHorizontalLine((int)horizon-42,0.f,width);g.drawHorizontalLine((int)horizon-23,0.f,width);
            for(int layer=0;layer<3;++layer)for(int i=0;i<15;++i) {
                const float x=(i+.37f+(layer%2)*.4f)*width/15.f;
                // Keep the brightest subject's silhouette free of foreground markers.
                if(layer==2 && std::abs(x-width*.5f)<width*.19f)continue;
                const float scale=.48f+.25f*layer,y=horizon-28.f+layer*10.f;
                const float tw=(13.f+(i%3)*5.f)*scale,th=(28.f+(i*7%5)*6.f)*scale;
                g.setColour(Theme::bg.withAlpha(.7f));g.fillEllipse(x-tw,y-2.f,tw*2.f,7.f*scale);
                const auto r=juce::Rectangle<float>(x-tw*.5f,y-th,tw,th);
                g.setGradientFill(juce::ColourGradient(stone.withAlpha(.32f+.16f*layer),r.getTopLeft(),
                    juce::Colour(0xff101618),r.getBottomRight(),false));
                if(i%4==0){g.fillRect(x-2.5f*scale,y-th,5.f*scale,th);g.fillRect(x-9.f*scale,y-th+8.f*scale,18.f*scale,5.f*scale);}
                else {g.fillRoundedRectangle(r,tw*.40f);g.fillRect(r.withTop(r.getBottom()-th*.55f));}
                g.setColour(Theme::muted.withAlpha(.10f+.025f*layer));
                g.drawLine(x-tw*.25f,y-th+9.f*scale,x+tw*.25f,y-th+9.f*scale,.7f);
                g.drawLine(x,y-th+5.f*scale,x,y-th+17.f*scale,.7f);
            }
            // Weathered raised stage in perspective, with contact light at its lip.
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff242a28),width*.5f,horizon-6.f,
                                                  Theme::bg,width*.5f,height,false));
            g.fillRect(0.f,horizon,width,height-horizon);
            g.setColour(Theme::muted.withAlpha(.10f));
            for(int i=0;i<19;++i)g.drawLine(width*.5f+(i-9)*15.f,horizon,width*.5f+(i-9)*64.f,height,.8f);
            for(int i=0;i<5;++i)g.drawHorizontalLine((int)(horizon+i*i*1.7f),0.f,width);
            for(int i=0;i<7;++i) {
                const float y=horizon-45.f+i*9.f;
                g.setGradientFill(juce::ColourGradient(Theme::blue.withAlpha(.012f+(i%3)*.009f),width*.35f,y,
                                                      Theme::bg.withAlpha(0.f),width*.85f,y,true));
                g.fillEllipse(-width*.12f,y,width*1.25f,26.f);
            }
            g.setColour(Theme::green.withAlpha(.12f));g.drawHorizontalLine((int)horizon+1,12.f,width-12.f);
        }
        target.drawImageAt(image,(int)bounds.getX(),(int)bounds.getY());
    }
private:
    juce::Image image;
    int lastTheme=-1;float lastHorizon=-1.f;
};
}
