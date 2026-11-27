#include "PresetBrowser.h"
#include "Theme.h"
#include "PresetCoverArt.h"
#include "../chain/AlchemistNativePreset.h"

void PresetBrowser::setListView(bool list,bool persist) {
    if(listMode==list)return;
    const int anchor=keyboardCard>=0?keyboardCard:scroll;
    listMode=list;
    hoveredCard=hoveredStar=-1;browserWheelCarry=0.f;
    resized();
    if(anchor>=0 && keyboardCard>=0)focusCard(anchor);
    if(persist)saveUIPreferences();
    repaint();
}
void PresetBrowser::loadUIPreferences() {
    const auto file=libraryDirectory().getChildFile("BrowserUI.json");
    if(!file.existsAsFile() || file.getSize()>4096)return;
    const auto settings=juce::JSON::parse(file.loadFileAsString());
    if(!settings.isObject())return;
    listMode=(bool)settings.getProperty("list",true);
    const auto stored=settings.getProperty("recent",{});
    if(const auto* recents=stored.getArray())for(const auto& entry:*recents)
        if(entry.isString() && entry.toString().length()<=270 && recentKeys.size()<40)recentKeys.addIfNotAlreadyThere(entry.toString());
    const int w=(int)settings.getProperty("width",650),h=(int)settings.getProperty("height",538);
    if(w>=620 && w<=1000 && h>=530 && h<=610) {
        setBounds((int)settings.getProperty("x",445),(int)settings.getProperty("y",77),w,h);
        userPlaced=true;
    }
}
void PresetBrowser::saveUIPreferences() const {
    if(preferencesReadOnly)return;
    auto* object=new juce::DynamicObject;
    juce::Array<juce::var> recent;for(const auto& key:recentKeys)recent.add(key);
    object->setProperty("recent",juce::var(recent));
    object->setProperty("list",listMode);object->setProperty("x",getX());object->setProperty("y",getY());
    object->setProperty("width",getWidth());object->setProperty("height",getHeight());
    const juce::var settings(object);
    const auto destination=libraryDirectory().getChildFile("BrowserUI.json");
    if(destination.getParentDirectory().createDirectory().failed())return;
    juce::TemporaryFile pending(destination);
    if(pending.getFile().replaceWithText(juce::JSON::toString(settings)))
        pending.overwriteTargetFileWithTemporary();
}
void PresetBrowser::paintPresetGrid(juce::Graphics& g,const std::vector<int>& entries) {
    if(entries.empty()) {
        Theme::fittedText(g,"NO PRESETS HERE YET",{224,210,getWidth()-248,30},17.f,Theme::white,true);
        Theme::fittedText(g,"Try another search, or import a preset in Manage Library.",
                          {224,254,getWidth()-248,38},12.f,Theme::muted);
        return;
    }
    for(int i=0;i<pageCapacity() && scroll+i<(int)entries.size();++i) {
        const int index=scroll+i,entry=entries[(size_t)index];
        const auto bounds=directoryCardBounds(i).toFloat();
        const bool selected=entry>=0?processor.loadedFactoryPresetIndex()==entry:
            userPresets[(size_t)(-entry-1)]==activeUserPreset;
        const bool focused=keyboardCard==index;
        Theme::panelBox(g,bounds,7.f,selected);
        if(focused){
            g.setColour(Theme::blue.withAlpha(.90f));
            g.drawRoundedRectangle(bounds.reduced(1.1f),7.f,1.5f);
        }
        auto art=bounds.reduced(6.f).removeFromTop(bounds.getHeight()-49.f);
        juce::Image cover;
        if(entry<0)cover=artworkForUser(-entry-1).portrait;
        else if(entry==skellpreset::alchemist::factoryIndex)cover=alchemistArtwork;
        if(cover.isValid()) {
            juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(art.toNearestInt());
            g.drawImageWithin(cover,(int)art.getX(),(int)art.getY(),(int)art.getWidth(),(int)art.getHeight(),
                              juce::RectanglePlacement::fillDestination);
        } else {
            const std::array<juce::Colour,4> accents{{Theme::green,Theme::amber,Theme::pink,Theme::white}};
            const int seed=entry<0?-entry:entry+1;
            skellpreset::art::paintCover(g,art,seed,accents[(size_t)artworkPaletteIndexForTest(seed)]);
        }
        const auto star=bounds.withLeft(bounds.getRight()-35.f).withHeight(35.f);
        g.setColour(Theme::bg.withAlpha(.8f));g.fillRoundedRectangle(star.reduced(3.f),5.f);
        juce::Path glyph;
        glyph.addStar(star.getCentre(),5,5.f,10.f);
        g.setColour(isFavorite(entry)?Theme::green:Theme::white);
        if(isFavorite(entry))g.fillPath(glyph);
        else g.strokePath(glyph,juce::PathStrokeType(1.5f));
        const auto name=entry>=0?processor.getProgramName(entry):userDisplayName(-entry-1);
        const auto card=directoryCardBounds(i);
        const int nameWidth=card.getWidth()-(selected?34:20);
        Theme::fittedText(g,name,{card.getX()+10,card.getBottom()-43,nameWidth,22},
                          14.f,Theme::white,true);
        if(selected){
            g.setColour(Theme::green);
            g.fillEllipse((float)card.getRight()-15.f,(float)card.getBottom()-35.f,6.f,6.f);
        }
        Theme::fittedText(g,entry>=0?juce::String("FACTORY PRESET"):artistForUser(-entry-1),
            {card.getX()+10,card.getBottom()-21,card.getWidth()-20,16},10.5f,Theme::muted);
        if(index==hoveredCard) {
            g.setColour(Theme::green.withAlpha(.6f));g.drawRoundedRectangle(card.toFloat().reduced(.7f),7.f,1.3f);
        }
    }
    Theme::text(g,juce::String(scroll+1)+"-"+juce::String(juce::jmin((int)entries.size(),scroll+pageCapacity()))+
                " / "+juce::String(entries.size()),{207,getHeight()-27,getWidth()-227,18},10.f,Theme::muted);
}
bool PresetBrowser::testViewSwitchForTest() {
    const bool savedMode=listMode,savedReadOnly=preferencesReadOnly;
    preferencesReadOnly=true;
    const int oldBank=bank,tier=artistTierFilter.getSelectedId(),artist=artistFilter.getSelectedId(),
        genre=genreFilter.getSelectedId(),type=typeFilter.getSelectedId(),oldFocus=keyboardCard,oldScroll=scroll;
    const auto query=search.getText();
    setListView(false,false);const auto tile=directoryCardBounds(0);
    setListView(true,false);const auto row=directoryCardBounds(0);
    bool ok=tile.getHeight()>row.getHeight() && row.getHeight()==56 && directoryColumns()==1 &&
        row.getWidth()>tile.getWidth() && resultAt(row.getCentre())==scroll &&
        resultAt({row.getX(),row.getBottom()+2})==-1 &&
        bank==oldBank && artistTierFilter.getSelectedId()==tier && artistFilter.getSelectedId()==artist &&
        genreFilter.getSelectedId()==genre && typeFilter.getSelectedId()==type && search.getText()==query;
    // Render the same factory motif in two sizes on the real graphics path.
    // Artwork must have spatial variation and different seeds must remain
    // distinguishable; a blank or identical placeholder fails this check.
    auto sampleCover=[](int seed,int w,int h){
        juce::Image img(juce::Image::ARGB,w,h,true);
        juce::Graphics g(img);
        skellpreset::art::paintCover(g,img.getBounds().toFloat(),seed,Theme::green);
        return img;
    };
    const auto first=sampleCover(1,65,46),second=sampleCover(2,65,46);
    const auto large=sampleCover(1,260,104);
    int distinct=0,nonUniform=0;
    const auto corner=first.getPixelAt(4,4);
    for(int y=4;y<42;++y)for(int x=4;x<61;++x){
        if(first.getPixelAt(x,y)!=second.getPixelAt(x,y))++distinct;
        if(first.getPixelAt(x,y)!=corner)++nonUniform;
    }
    ok=ok && distinct>100 && nonUniform>100 &&
        large.getPixelAt(130,52).getAlpha()>0;
    // Exercise real image decoding, shared cache reuse and cinematic row paint
    // with a synthetic QA texture. Never misrepresent it as artist artwork.
    juce::Image source(juce::Image::RGB,128,128,true);
    {
        juce::Graphics graphics(source);
        graphics.setGradientFill(juce::ColourGradient(juce::Colours::cyan,0.f,0.f,
            juce::Colours::magenta,128.f,128.f,false));
        graphics.fillAll();
    }
    juce::MemoryOutputStream png;
    juce::PNGImageFormat format;
    const bool written=format.writeImageToStream(source,png);
    const auto digest=juce::SHA256(png.getMemoryBlock()).toHexString();
    auto* row=new juce::DynamicObject;
    row->setProperty("artist","QA Texture");
    row->setProperty("sha256",digest);
    row->setProperty("payload",juce::Base64::toBase64(png.getData(),png.getDataSize()));
    juce::var rowVar(row);
    auto* bank=new juce::DynamicObject;
    juce::var bankVar(bank);
    bank->setProperty("artwork",juce::Array<juce::var>{rowVar});
    const auto cache=skellpreset::artistart::decode(bankVar,{});
    ok=ok && written && cache.size()==1;
    if(cache.size()==1){
        const auto& artwork=cache.begin()->second;
        const auto again=skellpreset::artistart::decode(bankVar,cache);
        ok=ok && artwork.portrait.getWidth()==192 && artwork.soft.getWidth()==160 &&
            again.size()==1 && again.begin()->second.digest==digest;
        for(int w:{396,776}){
            juce::Image canvas(juce::Image::ARGB,w,56,true);juce::Graphics g(canvas);
            skellpreset::artistart::cinematic(g,canvas.getBounds().toFloat(),artwork,
                Theme::panelAlt,Theme::green,false);
            const auto portrait=skellpreset::artistart::portraitBounds(canvas.getBounds().toFloat());
            const auto dark=canvas.getPixelAt(20,28),bright=canvas.getPixelAt((int)portrait.getCentreX(),28);
            ok=ok && portrait.getX()>155.f && portrait.getRight()<=w-76 &&
                dark.getBrightness()<.35f && bright.getBrightness()>dark.getBrightness()+.2f;
        }
    }
    row->setProperty("sha256",juce::String::repeatedString("0",64));
    ok=ok && skellpreset::artistart::decode(bankVar,cache).empty();
    setListView(savedMode,false);keyboardCard=oldFocus;scroll=oldScroll;resized();
    preferencesReadOnly=savedReadOnly;
    return ok;
}

void PresetBrowser::rememberRecent(int entry) {
    const auto key=favoriteKey(entry);if(key.isEmpty())return;
    recentKeys.removeString(key);recentKeys.insert(0,key);
    while(recentKeys.size()>40)recentKeys.remove(recentKeys.size()-1);
    saveUIPreferences();
}
