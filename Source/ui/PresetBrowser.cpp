#include "PresetBrowser.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <array>

namespace {
const juce::StringArray banks{"All presets","Main Vocals","Adlibs","My Presets"};
// Each factory sound has deterministic original cover art; no bitmap decoding
// or file I/O during a repaint. User-selected images override this motif.
void paintCover(juce::Graphics& g,juce::Rectangle<float> r,int seed,
                juce::Colour accent) {
    g.setGradientFill(juce::ColourGradient(Theme::plot,r.getX(),r.getY(),
                     accent.darker(.65f),r.getRight(),r.getBottom(),false));
    g.fillRoundedRectangle(r,5.f);
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(r.toNearestInt());
    const int variation=(seed<0?-seed:seed)%4;
    const float cx=r.getCentreX(),cy=r.getCentreY();
    g.setColour(accent.withAlpha(.13f));
    for(int i=1;i<=4;++i)
        g.drawEllipse(cx-9.f*i,cy-9.f*i,18.f*i,18.f*i,1.f);
    g.setColour(accent.withAlpha(.85f));
    if(variation==0) {
        for(int i=0;i<7;++i){
            const float h=11.f+(float)((seed*17+i*29)%31);
            g.fillRoundedRectangle(r.getX()+6.f+i*8.6f,cy-h*.5f,4.f,h,1.3f);
        }
    } else if(variation==1) {
        juce::Path wave;
        for(int i=0;i<=42;++i){
            const float x=r.getX()+3.f+i*(r.getWidth()-6.f)/42.f;
            const float y=cy+std::sin((float)i*.5f+seed*.21f)*
                (11.f+5.f*std::cos((float)i*.13f));
            if(i==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);
        }
        g.strokePath(wave,juce::PathStrokeType(2.5f));
    } else if(variation==2) {
        const float twist=(float)((seed*7)%13)-6.f;
        const float step=3.f+(float)(seed%4)*.32f;
        for(int i=0;i<5;++i) {
            const float margin=8.f+i*step;
            g.drawRoundedRectangle(r.getX()+margin+twist*.23f,r.getY()+margin,
                                   r.getWidth()-2.f*margin,r.getHeight()-2.f*margin,
                                   2.5f+(float)(seed%3),1.4f);
        }
    } else {
        const float span=16.f+(float)((seed*3)%11);
        const float h=12.f+(float)((seed*5)%13);
        juce::Path poly;
        poly.startNewSubPath(cx,cy-h);
        poly.lineTo(cx+span,cy-3.f);
        poly.lineTo(cx+3.f,cy+h);
        poly.lineTo(cx-span,cy+3.f);
        poly.closeSubPath();
        g.strokePath(poly,juce::PathStrokeType(2.4f));
        const float core=3.f+(float)(seed%7);
        g.fillEllipse(cx-core,cy-core,core*2.f,core*2.f);
    }
}
}
PresetBrowser::PresetBrowser(SkellPresetAudioProcessor& p):processor(p) {
    search.setMultiLine(false);
    search.setTextToShowWhenEmpty("Search presets or banks",Theme::muted);
    applyTheme();
    search.onTextChange=[this]{scroll=0;hoveredCard=-1;repaint();};
    search.onEscapeKey=[this]{setVisible(false);if(onClose)onClose();};
    // Enter auditions the first filtered result without dismissing the browser.
    search.onReturnKey=[this]{
        const auto entries=filtered();
        if(!entries.empty())auditionEntry(entries[(size_t)std::min(scroll,(int)entries.size()-1)]);
    };
    addAndMakeVisible(search);
    for (auto* button : {&importButton,&exportButton,&artworkButton}) {
        addAndMakeVisible(*button);
    }
    importButton.setTooltip("Open a validated native .skellpreset ZIP archive");
    exportButton.setTooltip("Save the active chain, slot order, macros and global controls");
    artworkButton.setTooltip("Add custom PNG/JPG cover art to a saved My Presets entry");
    artworkButton.setEnabled(false);
    importButton.onClick=[this] {chooseImport();};
    exportButton.onClick=[this] {chooseExport();};
    artworkButton.onClick=[this] {chooseArtwork();};
    setWantsKeyboardFocus(true);
}
void PresetBrowser::applyTheme() {
    Theme::selectFor(processor.state);
    search.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    search.setColour(juce::TextEditor::textColourId,Theme::white);
    search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    search.setTextToShowWhenEmpty("Search presets or banks",Theme::muted);
    for(auto* button:{&importButton,&exportButton,&artworkButton}) {
        button->setColour(juce::TextButton::buttonColourId,Theme::panel);
        button->setColour(juce::TextButton::textColourOffId,Theme::white);
    }
    repaint();
}
juce::File PresetBrowser::libraryDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Skell Audio").getChildFile("Skell-Preset")
        .getChildFile("Presets");
}
void PresetBrowser::scanLibrary() {
    userPresets.clear();
    const auto folder=libraryDirectory();
    if(folder.createDirectory().failed())return;
    juce::Array<juce::File> files;
    folder.findChildFiles(files,juce::File::findFiles,false,"*.skellpreset");
    for(const auto& item:files)
        if(item.getSize()>0 && item.getSize()<=4*1024*1024)
            userPresets.push_back(item);
    std::sort(userPresets.begin(),userPresets.end(),
        [](const juce::File& a,const juce::File& b) {
            return a.getFileName().compareIgnoreCase(b.getFileName())<0;
        });
    if(userPresets.size()>200)userPresets.resize(200);
    userArtwork.clear();userArtwork.resize(userPresets.size());
    for(size_t i=0;i<userPresets.size();++i) {
        const auto base=userPresets[i];
        auto cover=base.withFileExtension(".png");
        if(!cover.existsAsFile())cover=base.withFileExtension(".jpg");
        if(cover.existsAsFile() && cover.getSize()>0 && cover.getSize()<=2*1024*1024) {
            auto image=juce::ImageFileFormat::loadFrom(cover);
            if(image.isValid() && image.getWidth()<=4096 && image.getHeight()<=4096)
                userArtwork[i]=image.rescaled(128,96,juce::Graphics::mediumResamplingQuality);
        }
    }
}
void PresetBrowser::copyToLibrary(const juce::File& source) {
    const auto folder=libraryDirectory();
    if(folder.createDirectory().failed())return;
    if(source.getParentDirectory()==folder) {
        activeUserPreset=source;scanLibrary();refreshArtwork();return;
    }
    const auto dest=folder.getNonexistentChildFile(
        juce::File::createLegalFileName(source.getFileNameWithoutExtension()),
        ".skellpreset",true);
    if(source.copyFileTo(dest))activeUserPreset=dest;
    scanLibrary();
    refreshArtwork();
}
void PresetBrowser::open(){scanLibrary();refreshArtwork();bank=0;scroll=0;hoveredCard=-1;search.setText({},juce::dontSendNotification);setVisible(true);toFront(true);search.grabKeyboardFocus();repaint();}
void PresetBrowser::resized(){
    search.setBounds(19,17,getWidth()-38,42);
    importButton.setBounds(18,419,166,31);
    exportButton.setBounds(18,456,166,31);
    artworkButton.setBounds(18,493,166,31);
}
void PresetBrowser::setChooserActive(bool active) {
    chooserActive=active;
    importButton.setEnabled(!active);
    exportButton.setEnabled(!active);
    artworkButton.setEnabled(!active && activeUserPreset.existsAsFile());
}
void PresetBrowser::chooseImport() {
    if(chooserActive)return;
    setChooserActive(true);
    fileChooser=std::make_unique<juce::FileChooser>("Import native Skell-Preset", juce::File{}, "*.skellpreset");
    juce::Component::SafePointer<PresetBrowser> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return; // editor/plugin may have closed while native dialog was open
            safe->setChooserActive(false);
            const auto file=chooser.getResult();
            if(file==juce::File{})return;
            const auto result=safe->processor.importPreset(file);
            if(result.failed()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Cannot import preset",result.getErrorMessage());
                return;
            }
            safe->copyToLibrary(file);
            if(safe->onLoaded)safe->onLoaded();
            safe->repaint();
        });
}
void PresetBrowser::chooseExport() {
    if(chooserActive)return;
    setChooserActive(true);
    const auto base=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto initial=base.getChildFile(juce::File::createLegalFileName(processor.presetName())+".skellpreset");
    fileChooser=std::make_unique<juce::FileChooser>("Save native Skell-Preset",initial,"*.skellpreset");
    juce::Component::SafePointer<PresetBrowser> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return;
            safe->setChooserActive(false);
            auto file=chooser.getResult();
            if(file==juce::File{})return;
            if(!file.hasFileExtension(".skellpreset"))file=file.withFileExtension(".skellpreset");
            const auto result=safe->processor.exportPreset(file);
            if(result.failed()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Cannot save preset",result.getErrorMessage());
                return;
            }
            safe->copyToLibrary(file);
            safe->refreshArtwork();
            safe->repaint();
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"Preset saved",file.getFullPathName());
        });
}
std::vector<int> PresetBrowser::filtered() const {
    std::vector<int> list;
    const auto q=search.getText().toLowerCase().trim();
    if(bank!=3)for(int i=0;i<(int)skellpreset::factoryPresets.size();++i) {
        const auto& item=skellpreset::factoryPresets[(size_t)i];
        const juce::String name(item.name),b(item.bank),desc(item.description);
        if(b=="Vocoding")continue; // Reserved/unfinished source presets are hidden.
        if(bank>0 && b!=banks[bank])continue;
        bool found=q.isEmpty() || name.containsIgnoreCase(q) ||
                   b.containsIgnoreCase(q) || desc.containsIgnoreCase(q);
        if(q.startsWith("bank:"))
            found=b.containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("tag:"))
            found=desc.containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("author:"))
            found=juce::String("skell audio").containsIgnoreCase(
                q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("fav:")||q.startsWith("match:"))found=false;
        if(found)list.push_back(i);
    }
    if(bank==0 || bank==3)for(int i=0;i<(int)userPresets.size();++i) {
        const auto label=userPresets[(size_t)i].getFileNameWithoutExtension();
        bool found=q.isEmpty() || label.containsIgnoreCase(q) ||
                   juce::String("My Presets").containsIgnoreCase(q);
        if(q.startsWith("bank:"))
            found=juce::String("My Presets").containsIgnoreCase(
                q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("author:")||q.startsWith("fav:")||
           q.startsWith("match:")||q.startsWith("tag:"))found=false;
        if(found)list.push_back(-i-1);
    }
    return list;
}
void PresetBrowser::refreshArtwork() {
    artworkButton.setEnabled(!chooserActive && activeUserPreset.existsAsFile());
    repaint();
}
void PresetBrowser::chooseArtwork() {
    if(chooserActive || !activeUserPreset.existsAsFile())return;
    setChooserActive(true);
    fileChooser=std::make_unique<juce::FileChooser>(
        "Choose artwork for "+activeUserPreset.getFileNameWithoutExtension(),
        juce::File{}, "*.png;*.jpg;*.jpeg");
    juce::Component::SafePointer<PresetBrowser> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return;
            safe->setChooserActive(false);
            const auto file=chooser.getResult();
            if(!file.existsAsFile())return;
            if(file.getSize()<=0 || file.getSize()>2*1024*1024){
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Artwork too large","Use a PNG/JPG image smaller than 2 MB.");return;
            }
            auto image=juce::ImageFileFormat::loadFrom(file);
            if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096){
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Invalid artwork","Choose a valid image up to 4096 x 4096.");return;
            }
            const auto destination=safe->activeUserPreset.withFileExtension(".png");
            // Write-then-replace: a failed save must not destroy the previous cover.
            juce::TemporaryFile staging(destination);
            juce::PNGImageFormat png;
            const auto thumbnail=image.rescaled(240,180,juce::Graphics::mediumResamplingQuality);
            bool written=false;
            {
                juce::FileOutputStream output(staging.getFile());
                if(output.openedOk()) {
                    written=png.writeImageToStream(thumbnail,output);
                    output.flush();
                }
            }
            if(!written || !staging.overwriteTargetFileWithTemporary()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Cannot save artwork","Your previous cover is unchanged. Check library permissions.");return;
            }
            safe->scanLibrary();
            safe->refreshArtwork();
        });
}
void PresetBrowser::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    g.setColour(Theme::panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),10.f);
    g.setColour(Theme::stroke);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),10.f,1.2f);
    g.setColour(Theme::green.withAlpha(.82f));g.fillRoundedRectangle(juce::Rectangle<float>(18.f,67.f,3.f,23.f),1.5f);
    Theme::text(g,"LIBRARY",{30,73,154,20},12,Theme::white,true);
    const auto entries=filtered();
    Theme::text(g,juce::String((int)entries.size())+" SOUNDS",{207,73,246,20},12,Theme::white,true);
    Theme::text(g,"ESC TO CLOSE",{getWidth()-121,75,100,20},10,Theme::muted,false,juce::Justification::centredRight);
    for(int i=0;i<banks.size();i++){
        auto b=juce::Rectangle<float>(17.f,111.f+i*60.f,168.f,52.f);
        g.setColour(i==bank?Theme::green.withAlpha(.11f):Theme::panel);g.fillRoundedRectangle(b,6);
        g.setColour(i==bank?Theme::green.withAlpha(.7f):Theme::stroke);g.drawRoundedRectangle(b,6,1);
        Theme::text(g,banks[i],{29,(int)b.getY()+14,145,22},14,Theme::white,i==bank);
    }
    Theme::text(g,juce::String((int)userPresets.size())+" PERSONAL PRESETS",
        {18,368,175,22},11,Theme::green,true);
    Theme::text(g,"Select a sound to audition.",{18,390,175,20},11,Theme::muted);
    for(int i=0;i<6;i++){
        const int itemIndex=i+scroll;
        if(itemIndex>=(int)entries.size())break;
        const int entry=entries[(size_t)itemIndex];
        const bool custom=entry<0;
        const juce::String itemName=custom
            ?userPresets[(size_t)(-entry-1)].getFileNameWithoutExtension()
            :juce::String(skellpreset::factoryPresets[(size_t)entry].name);
        const juce::String itemBank=custom?"My Presets":
            juce::String(skellpreset::factoryPresets[(size_t)entry].bank);
        const int y=110+i*61;
        auto rect=juce::Rectangle<float>(207.f,(float)y,(float)getWidth()-224.f,56.f);
        g.setColour(Theme::panelAlt);g.fillRoundedRectangle(rect,7.f);
        const std::array<juce::Colour,5> accents{{Theme::green,Theme::blue,Theme::amber,Theme::pink,Theme::white}};
        const int seed=custom?-entry:entry+1;
        const auto accent=accents[(size_t)(seed%5)];
        const auto cover=juce::Rectangle<float>(215.f,(float)y+5.f,67.f,46.f);
        if(custom && (size_t)(-entry-1)<userArtwork.size() && userArtwork[(size_t)(-entry-1)].isValid()) {
            g.drawImageWithin(userArtwork[(size_t)(-entry-1)],(int)cover.getX(),(int)cover.getY(),
                              (int)cover.getWidth(),(int)cover.getHeight(),juce::RectanglePlacement::centred);
        } else paintCover(g,cover,seed,accent);
        g.setColour(Theme::stroke.withAlpha(.8f));g.drawRoundedRectangle(cover,5.f,1.f);
        const bool loaded=custom
            ?activeUserPreset==userPresets[(size_t)(-entry-1)]
            :entry==processor.loadedFactoryPresetIndex();
        const bool hovered=itemIndex==hoveredCard;
        g.setColour(loaded?Theme::green:hovered?Theme::blue:Theme::stroke);
        g.drawRoundedRectangle(rect,7,hovered?1.6f:1.f);
        Theme::text(g,itemName,{294,y+7,260,24},15,Theme::white,true);
        const auto bankColour=custom?Theme::green:Theme::blue;
        Theme::text(g,itemBank,{294,y+33,210,16},11,bankColour,true);
        if(loaded){g.setColour(Theme::green);g.fillEllipse((float)getWidth()-29.f,(float)y+25.f,6.f,6.f);}
    }
    if(entries.empty())Theme::text(g,"NO MATCHING PRESETS",{246,222,350,40},15,Theme::muted,true);
    if((int)entries.size()>6){
        const float railTop=110.f,railHeight=361.f;
        const float thumbHeight=juce::jmax(30.f,railHeight*6.f/(float)entries.size());
        const float travel=railHeight-thumbHeight;
        const float thumbY=railTop+travel*(float)scroll/(float)(entries.size()-6);
        g.setColour(Theme::stroke.withAlpha(.6f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)getWidth()-12.f,railTop,3.f,railHeight),1.5f);
        g.setColour(Theme::green.withAlpha(.85f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)getWidth()-12.f,thumbY,3.f,thumbHeight),1.5f);
        Theme::text(g,"SCROLL TO SEE MORE",{260,getHeight()-20,300,16},10,Theme::muted);
    }
}
void PresetBrowser::loadFactoryFromBrowser(int index) {
    if(index<0 || index>=(int)skellpreset::factoryPresets.size())return;
    activeUserPreset=juce::File{};
    refreshArtwork();
    const int previousMode=(int)processor.state.getRawParameterValue("mode")->load();
    processor.loadFactoryPreset(index);
    // Preserve Simple workspace and bypass while A/B auditioning.
    if(previousMode==0 || previousMode==3) {
        if(auto* mode=processor.state.getParameter("mode"))
            mode->setValueNotifyingHost(mode->convertTo0to1((float)previousMode));
    }
    if(onLoaded)onLoaded();
    repaint();
}
void PresetBrowser::loadUserFromBrowser(int index) {
    if(index<0 || index>=(int)userPresets.size())return;
    const int previousMode=(int)processor.state.getRawParameterValue("mode")->load();
    const auto file=userPresets[(size_t)index];
    const auto result=processor.importPreset(file);
    if(result.failed()) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Cannot load saved preset",result.getErrorMessage());
        return;
    }
    if(previousMode==0 || previousMode==3)
        if(auto* mode=processor.state.getParameter("mode"))
            mode->setValueNotifyingHost(mode->convertTo0to1((float)previousMode));
    activeUserPreset=file;
    refreshArtwork();
    if(onLoaded)onLoaded();
    repaint();
}
void PresetBrowser::auditionEntry(int entry) {
    if(entry>=0)loadFactoryFromBrowser(entry);
    else loadUserFromBrowser(-entry-1);
}
void PresetBrowser::mouseDown(const juce::MouseEvent&e) {
    if(e.y>=111 && e.x>=17 && e.x<=185){
        const int idx=(e.y-111)/60;
        if(idx>=0&&idx<banks.size()){bank=idx;scroll=0;repaint();}return;
    }
    if(e.y>=110&&e.x>=207&&e.y<476){
        const auto entries=filtered();const int row=(e.y-110)/61;
        if(row>=0&&row<6 && scroll+row<(int)entries.size()){
            auditionEntry(entries[(size_t)(scroll+row)]);
            // Keep bank, scroll position and query visible for fast A/B.
        }
    }
}
void PresetBrowser::mouseMove(const juce::MouseEvent& e){
    const int candidate=e.x>=207&&e.y>=110&&e.y<476 ? (e.y-110)/61+scroll : -1;
    const int next=candidate>=0&&candidate<(int)filtered().size()?candidate:-1;
    if(next!=hoveredCard){hoveredCard=next;repaint();}
    setMouseCursor(next>=0?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);
}
void PresetBrowser::mouseExit(const juce::MouseEvent&){hoveredCard=-1;repaint();}
void PresetBrowser::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    if(e.x<197 || e.y<102 || e.y>486)return;
    const int maxScroll=std::max(0,(int)filtered().size()-6);
    const int next=juce::jlimit(0,maxScroll,scroll+(wheel.deltaY<0?2:-2));
    if(scroll!=next){scroll=next;hoveredCard=-1;repaint();}
}
bool PresetBrowser::keyPressed(const juce::KeyPress& key){
    if(key==juce::KeyPress::escapeKey){setVisible(false);if(onClose)onClose();return true;}
    return false;
}
