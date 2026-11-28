#include "PresetBrowser.h"
#include "Theme.h"
#include "PresetCoverArt.h"
#include <cstdio>
#include "../chain/AlchemistNativePreset.h"

void PresetBrowser::setListView(bool list,bool persist) {
    const bool wasShowcase=showcaseMode;const auto previousDirectory=directoryMode();showcaseMode=false;
    if(wasShowcase&&directoryMode()!=previousDirectory){scroll=0;keyboardCard=-1;}
    if(listMode==list&&!wasShowcase)return;
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
    // Legacy floating-window coordinates are intentionally ignored. The editor
    // now owns the browser bounds; retain list preference and recents.
}
void PresetBrowser::saveUIPreferences() const {
    if(preferencesReadOnly)return;
    auto* object=new juce::DynamicObject;
    juce::Array<juce::var> recent;for(const auto& key:recentKeys)recent.add(key);
    object->setProperty("recent",juce::var(recent));
    object->setProperty("list",listMode);
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
    // The vocal tabs own this footer on artist/genre leaves, just as in list view.
    if(!showVocalTabs())
        Theme::text(g,juce::String(scroll+1)+"-"+juce::String(juce::jmin((int)entries.size(),scroll+pageCapacity()))+
                    " / "+juce::String(entries.size()),{207,getHeight()-27,getWidth()-227,18},10.f,Theme::muted);
}
bool PresetBrowser::testViewSwitchForTest() {
    const bool savedMode=listMode,savedReadOnly=preferencesReadOnly,savedShowcase=showcaseMode;
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
    if(!ok)std::fprintf(stderr,"Browser view switch: card geometry, hit testing or filters changed\n");
    // Render the same factory motif in two sizes on the real graphics path.
    // Artwork must have spatial variation and different seeds must remain
    // distinguishable; a blank or identical placeholder fails this check.
    auto sampleCover=[](int seed,int w,int h){
        // This is a pixel-content test, not a GPU/backend test. Render to a
        // software surface and end the graphics lifetime before readback.
        juce::Image img(juce::Image::ARGB,w,h,true,juce::SoftwareImageType{});
        {juce::Graphics g(img);
         skellpreset::art::paintCover(g,img.getBounds().toFloat(),seed,Theme::green); }
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
    const bool motifOk=distinct>100 && nonUniform>100 && large.getPixelAt(130,52).getAlpha()>0;
    if(!motifOk)std::fprintf(stderr,"Browser motif: distinct=%d nonuniform=%d alpha=%d\n",distinct,nonUniform,(int)large.getPixelAt(130,52).getAlpha());
    ok=ok && motifOk;
    // Exercise real image decoding, shared cache reuse and cinematic row paint
    // with a synthetic QA texture. Never misrepresent it as artist artwork.
    juce::Image source(juce::Image::RGB,128,128,true,juce::SoftwareImageType{});
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
    auto* artRow=new juce::DynamicObject;
    artRow->setProperty("artist","QA Texture");
    artRow->setProperty("sha256",digest);
    artRow->setProperty("payload",juce::Base64::toBase64(png.getData(),png.getDataSize()));
    juce::var rowVar(artRow);
    auto* artworkBank=new juce::DynamicObject;
    juce::var bankVar(artworkBank);
    artworkBank->setProperty("artwork",juce::Array<juce::var>{rowVar});
    const auto cache=skellpreset::artistart::decode(bankVar,{});
    if(!written || cache.size()!=1)std::fprintf(stderr,"Browser artwork: PNG encode/decode or digest rejected\n");
    ok=ok && written && cache.size()==1;
    if(cache.size()==1){
        const auto& artwork=cache.begin()->second;
        const auto again=skellpreset::artistart::decode(bankVar,cache);
        ok=ok && artwork.portrait.getWidth()==192 && artwork.soft.getWidth()==160 &&
            again.size()==1 && again.begin()->second.digest==digest;
        for(int w:{396,776}){
            // Native Direct2D may defer drawing until Graphics is destroyed.
            // The assertions below read completed pixels, never an active target.
            juce::Image canvas(juce::Image::ARGB,w,56,true,juce::SoftwareImageType{});
            {juce::Graphics g(canvas);
             skellpreset::artistart::cinematic(g,canvas.getBounds().toFloat(),artwork,
                 Theme::panelAlt,Theme::green,false); }
            const auto portrait=skellpreset::artistart::portraitBounds(canvas.getBounds().toFloat());
            const auto dark=canvas.getPixelAt(20,28),bright=canvas.getPixelAt((int)portrait.getCentreX(),28);
            const bool rowOk=portrait.getX()>155.f && portrait.getRight()<=w-76 &&
                dark.getBrightness()<.35f && bright.getBrightness()>dark.getBrightness()+.2f;
            if(!rowOk)std::fprintf(stderr,"Browser cinematic row: width=%d dark=%.3f bright=%.3f\n",w,(double)dark.getBrightness(),(double)bright.getBrightness());
            ok=ok && rowOk;
        }
    }
    artRow->setProperty("sha256",juce::String::repeatedString("0",64));
    ok=ok && skellpreset::artistart::decode(bankVar,cache).empty();
    // Showcase focus and side cards never trigger a host program change.
    const int originalProgram=processor.loadedFactoryPresetIndex();
    setShowcase(true);const int center=showcaseSelection();
    if(center>=0){ok=ok&&resultAt(showcaseCardBounds(0).getCentre())==center;
        if(center+1<resultCount()){moveShowcase(1);ok=ok&&showcaseSelection()==center+1;}
        ok=ok&&processor.loadedFactoryPresetIndex()==originalProgram;
    }
    ok=ok&&showcaseLoad.isVisible()&&!showcaseCardBounds(0).intersects(showcaseCardBounds(1))&&getLocalBounds().contains(showcaseLoad.getBounds());
    // Circular artwork navigation is symmetric only when there are three unique cards.
    ok=ok&&!showcaseCardBounds(-2).intersects(showcaseCardBounds(-1))&&!showcaseCardBounds(2).intersects(showcaseCardBounds(1))&&showcaseIndex(-2,5,0)==3&&showcaseIndex(2,5,4)==1&&showcaseIndex(-1,3,0)==2&&showcaseIndex(1,3,2)==0&&showcaseIndex(-1,2,0)==-1&&showcaseIndex(1,1,0)==-1&&showcaseIndex(0,0,-1)==-1;
    setListView(savedMode,false);if(savedShowcase)setShowcase(true);keyboardCard=oldFocus;scroll=oldScroll;resized();
    preferencesReadOnly=savedReadOnly;
    return ok;
}

void PresetBrowser::rememberRecent(int entry) {
    const auto key=favoriteKey(entry);if(key.isEmpty())return;
    recentKeys.removeString(key);recentKeys.insert(0,key);
    while(recentKeys.size()>40)recentKeys.remove(recentKeys.size()-1);
    saveUIPreferences();
}

void PresetBrowser::setShowcase(bool enabled){
    const auto previousDirectory=directoryMode();showcaseMode=enabled;
    if(previousDirectory!=directoryMode()){keyboardCard=-1;scroll=0;}
    hoveredCard=hoveredStar=-1;browserWheelCarry=0;resized();repaint();
}
int PresetBrowser::showcaseSelection() const {return auditionTargetForTest(keyboardCard,scroll,resultCount());}
int PresetBrowser::showcaseIndex(int offset,int count,int selected) const noexcept {
    if(selected<0||selected>=count)return -1;
    if(count>=3)return (selected+offset+count)%count;
    const int index=selected+offset;return index>=0&&index<count?index:-1;
}
void PresetBrowser::moveShowcase(int delta){const int n=showcaseIndex(delta,resultCount(),showcaseSelection());if(n>=0)focusCard(n);}
void PresetBrowser::activateShowcase(){
    if(!showcaseMode||detailsOpen||chooserActive||loadInFlight||saveInFlight)return;
    const int n=showcaseSelection();if(n<0)return;
    if(directoryMode()!=DirectoryMode::none){activateDirectoryRow(n);return;}
    const auto entries=filtered();if(n<(int)entries.size())auditionEntry(entries[(size_t)n]);
}
juce::Rectangle<int> PresetBrowser::showcaseCardBounds(int offset) const {
    const int w=getWidth()-100,mid=getWidth()/2,hero=juce::jmin(240,w*36/100);
    const bool five=resultCount()>=5;
    // Never let a neighboring card outweigh the featured portrait, even
    // with two results or when the rack is hidden.
    const int side=juce::jmax(35,juce::jmin(hero*76/100,five?(w-hero)*31/100:(w-hero)/2-14));
    const int far=juce::jmax(24,juce::jmin(hero*55/100,(w-hero-2*side)/2-16));
    const int height=juce::jlimit(128,250,getHeight()-(showVocalTabs()?334:294));
    if(offset==0)return {mid-hero/2,154,hero,height};
    const int x=offset<0?mid-hero/2-side-8:mid+hero/2+8;
    if(std::abs(offset)==1)return {x,175,side,height-42};
    return {offset<0?x-far-6:x+side+6,193,far,height-78};
}
void PresetBrowser::paintShowcase(juce::Graphics& g,const std::vector<int>& entries,const std::vector<DirectoryCard>& cards){
    const bool directory=directoryMode()!=DirectoryMode::none;const int count=directory?(int)cards.size():(int)entries.size(),n=showcaseSelection();
    const int width=getWidth()-36,mid=getWidth()/2;
    if(n<0||n>=count){
        const bool artists=directoryMode()==DirectoryMode::artists&&search.getText().trim().isEmpty();
        Theme::fittedText(g,artists?"NO ARTIST COLLECTIONS YET":"NO MATCHING SOUNDS",{18,210,width,36},19.f,Theme::white,true,juce::Justification::centred);
        Theme::fittedText(g,artists?"Import a personal preset and assign its artist in Manage Library.":"Try another category or clear your search.",{28,250,width-20,38},13.f,Theme::muted,false,juce::Justification::centred);return;
    }
    const auto hero=showcaseCardBounds(0);
    const auto gallery=juce::Rectangle<float>(18.f,145.f,(float)width,(float)(getHeight()-181));
    g.setGradientFill(juce::ColourGradient(Theme::panelAlt.withAlpha(.7f),gallery.getTopLeft(),Theme::bg.withAlpha(.85f),gallery.getBottomLeft(),false));g.fillRoundedRectangle(gallery,12.f);
    g.setGradientFill(juce::ColourGradient(Theme::green.withAlpha(.15f),(float)mid,(float)hero.getCentreY(),Theme::plot.withAlpha(0.f),(float)(mid+width/2),(float)hero.getBottom(),true));g.fillRoundedRectangle(gallery,12.f);
    g.setGradientFill(juce::ColourGradient(Theme::green.withAlpha(.065f),(float)mid,(float)hero.getBottom(),Theme::green.withAlpha(0.f),(float)mid+width*.38f,(float)hero.getBottom()+28.f,true));g.fillRect(gallery);
    juce::String title,subtitle;
    for(int d:{-2,2,-1,1,0}){
        if(std::abs(d)==2&&count<5)continue;
        const int i=showcaseIndex(d,count,n);if(i<0)continue;const auto rect=showcaseCardBounds(d);const auto area=rect.toFloat();juce::Image cover;juce::String label;int seed=0;
        if(directory){const auto& card=cards[(size_t)i];label=card.label;seed=card.id;cover=artworkForUser(card.artIndex).portrait;if(directoryMode()==DirectoryMode::artists){const auto found=artistArtwork.find(skellpreset::artistart::key(card.label));if(found!=artistArtwork.end())cover=found->second.portrait;}}
        else {const int entry=entries[(size_t)i];seed=entry<0?-entry:entry+1;label=entry<0?userDisplayName(-entry-1):processor.getProgramName(entry);cover=entry<0?artworkForUser(-entry-1).portrait:entry==skellpreset::alchemist::factoryIndex?alchemistArtwork:juce::Image{};}
        g.setColour(juce::Colours::black.withAlpha(.50f));g.fillRoundedRectangle(area.translated(0.f,5.f).expanded(d==0?4.f:2.f),12.f);
        if(d==0){g.setColour(Theme::green.withAlpha(.06f));g.fillRoundedRectangle(area.expanded(4.f),13.f);}
        g.setColour(Theme::panelAlt);g.fillRoundedRectangle(area,10.f);
        {juce::Graphics::ScopedSaveState state(g);juce::Path clip;clip.addRoundedRectangle(area.reduced(2.f),7.f);g.reduceClipRegion(clip);
            if(cover.isValid())g.drawImageWithin(cover,rect.getX()+2,rect.getY()+2,rect.getWidth()-4,rect.getHeight()-4,juce::RectanglePlacement::fillDestination);
            else {const std::array<juce::Colour,4> accents{{Theme::green,Theme::blue,Theme::amber,Theme::pink}};skellpreset::art::paintCover(g,area.reduced(2.f),seed,accents[(size_t)artworkPaletteIndexForTest(seed)].withAlpha(.85f));}
            g.setGradientFill(juce::ColourGradient(Theme::bg.withAlpha(d==0?.02f:.26f),area.getTopLeft(),Theme::bg.withAlpha(d==0?.24f:.72f),area.getBottomLeft(),false));g.fillRect(area);
        }
        g.setColour(d==0?Theme::green.withAlpha(.72f):hoveredCard==i?Theme::green.withAlpha(.5f):Theme::stroke.withAlpha(.6f));g.drawRoundedRectangle(area.reduced(.6f),9.f,d==0?1.4f:1.f);
        g.setColour(Theme::white.withAlpha(d==0?.15f:.055f));g.drawRoundedRectangle(area.reduced(2.2f),7.f,.75f);
        if(d!=0)Theme::fittedText(g,label,rect.withTop(rect.getBottom()-29).reduced(4,0),std::abs(d)==2?9.f:10.f,Theme::muted,true,juce::Justification::centred);
        else{title=label;if(directory){const auto& card=cards[(size_t)i];subtitle=juce::String(card.main)+" MAIN  /  "+juce::String(card.adlib)+" ADLIB  /  "+juce::String(card.other)+" OTHER";}else{const int entry=entries[(size_t)i];subtitle=entry<0?artistForUser(-entry-1):juce::String("FACTORY PRESET");}}
    }
    Theme::fittedText(g,title,{18,hero.getBottom()+5,width,28},22.f,Theme::white,true,juce::Justification::centred);
    Theme::fittedText(g,subtitle,{18,hero.getBottom()+34,width,18},10.5f,Theme::muted,true,juce::Justification::centred);
    const int footer=getHeight()-(showVocalTabs()?65:25);
    const int marks=juce::jmin(count,9);const int activeMark=count<=9?n:juce::roundToInt((float)n*(marks-1)/(float)(count-1));
    for(int mark=0;mark<marks;++mark){g.setColour(mark==activeMark?Theme::green:Theme::muted.withAlpha(.34f));g.fillRoundedRectangle((float)(mid-marks*11/2+mark*11),(float)footer-8.f,mark==activeMark?8.f:5.f,3.f,1.5f);}
    Theme::fittedText(g,juce::String(n+1)+" / "+juce::String(count)+"  |  BROWSE WITHOUT LOADING",{18,footer,width,16},10.f,Theme::muted,false,juce::Justification::centred);
}
