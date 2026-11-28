#pragma once
#include <JuceHeader.h>
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <algorithm>
#include <iterator>

namespace skellpreset::artistart {
struct Art {
    juce::Image portrait, soft;
    juce::String digest;
};
using Cache=std::map<juce::String,Art>;

inline juce::String key(const juce::String& artist){return artist.trim().toLowerCase();}

// Prepared on the existing library worker, never from paint or the audio thread.
inline Art prepare(const juce::Image& image,const juce::String& digest={}){
    if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096)return {};
    const int side=juce::jmin(image.getWidth(),image.getHeight());
    auto square=image.getClippedImage({(image.getWidth()-side)/2,(image.getHeight()-side)/2,side,side});
    Art out;out.digest=digest;
    out.portrait=square.rescaled(192,192,juce::Graphics::highResamplingQuality);
    // Downsample before painting; a single shared blurred texture per artist.
    out.soft=out.portrait.rescaled(12,12,juce::Graphics::highResamplingQuality)
                         .rescaled(160,160,juce::Graphics::mediumResamplingQuality);
    return out;
}

inline Cache decode(const juce::var& bank,const Cache& previous){
    Cache result;
    const auto art=bank.getProperty("artwork",{});
    const auto* rows=art.getArray();
    if(!rows || rows->size()>128)return result;
    for(const auto& row:*rows){
        const auto name=key(row.getProperty("artist",{}).toString());
        const auto hash=row.getProperty("sha256",{}).toString();
        if(name.isEmpty() || name.length()>60 || name.containsAnyOf("\r\n\t") ||
           hash.length()!=64 || !hash.containsOnly("0123456789abcdef") || result.count(name))continue;
        const auto found=previous.find(name);
        // Only reuse after confirming the current payload's hash, so a malformed
        // new resource cannot borrow a previous trusted image.
        const auto encoded=row.getProperty("payload",{}).toString();
        if(encoded.isEmpty() || encoded.length()>1400000)continue;
        juce::MemoryOutputStream data;
        if(!juce::Base64::convertFromBase64(data,encoded))continue;
        const auto bytes=data.getMemoryBlock();
        if(bytes.getSize()<24 || bytes.getSize()>1024*1024 || juce::SHA256(bytes).toHexString()!=hash)continue;
        const auto* b=static_cast<const unsigned char*>(bytes.getData());
        constexpr unsigned char png[]{137,80,78,71,13,10,26,10};
        if(!std::equal(std::begin(png),std::end(png),b))continue;
        auto be32=[](const unsigned char* p){return (unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|unsigned(p[3]);};
        if(be32(b+8)!=13 || b[12]!='I' || b[13]!='H' || b[14]!='D' || b[15]!='R')continue;
        const auto w=be32(b+16),h=be32(b+20);
        if(w==0 || h==0 || w>1024 || h>1024)continue;
        if(found!=previous.end() && found->second.digest==hash && found->second.portrait.isValid()){
            result.emplace(name,found->second);continue;
        }
        auto image=juce::ImageFileFormat::loadFrom(bytes.getData(),bytes.getSize());
        auto ready=prepare(image,hash);
        if(ready.portrait.isValid())result.emplace(name,std::move(ready));
    }
    return result;
}
Cache embedded(const Cache& previous);
// Same geometry for directory and preset rows; reserve existing favorite/count
// controls at the right. The sharp square never overlaps text even at 620 px.
inline juce::Rectangle<float> portraitBounds(juce::Rectangle<float> row){
    return {row.getRight()-130.f,row.getY()+3.f,50.f,50.f};
}
inline void cinematic(juce::Graphics& g,juce::Rectangle<float> row,const Art& art,
                      juce::Colour base,juce::Colour accent,bool hover){
    juce::Graphics::ScopedSaveState saved(g);
    juce::Path clip;clip.addRoundedRectangle(row,7.f);g.reduceClipRegion(clip);
    g.setGradientFill(juce::ColourGradient(base.brighter(hover?.12f:.035f),row.getTopLeft(),
                                           base.darker(.22f),row.getBottomRight(),false));
    g.fillRect(row);
    if(art.soft.isValid()){
        // Broad, deliberately soft cinematic strip; right edge retains the art.
        auto back=row.withLeft(row.getX()+row.getWidth()*.18f);
        g.setOpacity(.76f);
        g.drawImageWithin(art.soft,(int)back.getX(),(int)back.getY(),(int)back.getWidth(),(int)back.getHeight(),
                          juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.f);
    }
    const auto dark=juce::Colour(0xff101216);
    juce::ColourGradient veil(dark.withAlpha(.97f),row.getX(),row.getCentreY(),
                              dark.withAlpha(.10f),row.getRight()-76.f,row.getCentreY(),false);
    veil.addColour(.58,dark.withAlpha(.88f));veil.addColour(.82,dark.withAlpha(.36f));
    g.setGradientFill(veil);g.fillRect(row);
    const auto portrait=portraitBounds(row);
    if(art.portrait.isValid()){
        juce::Graphics::ScopedSaveState img(g);
        juce::Path rounded;rounded.addRoundedRectangle(portrait,4.f);g.reduceClipRegion(rounded);
        g.drawImageWithin(art.portrait,(int)portrait.getX(),(int)portrait.getY(),(int)portrait.getWidth(),(int)portrait.getHeight(),
                          juce::RectanglePlacement::fillDestination);
    }
    g.setColour(accent.withAlpha(hover?.18f:.075f));g.fillRect(row.withHeight(1.f));
}
} // namespace skellpreset::artistart
