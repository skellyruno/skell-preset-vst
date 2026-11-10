#include "PresetBrowser.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace {const juce::StringArray banks{"All presets","Main Vocals","Adlibs","My Presets"};}
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
    for (auto* button : {&importButton,&exportButton}) {
        addAndMakeVisible(*button);
    }
    importButton.setTooltip("Open a validated native .skellpreset ZIP archive");
    exportButton.setTooltip("Save the active chain, slot order, macros and global controls");
    importButton.onClick=[this] {chooseImport();};
    exportButton.onClick=[this] {chooseExport();};
    setWantsKeyboardFocus(true);
}
void PresetBrowser::applyTheme() {
    Theme::selectFor(processor.state);
    search.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    search.setColour(juce::TextEditor::textColourId,Theme::white);
    search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    search.setTextToShowWhenEmpty("Search presets or banks",Theme::muted);
    for(auto* button:{&importButton,&exportButton}) {
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
}
void PresetBrowser::copyToLibrary(const juce::File& source) {
    const auto folder=libraryDirectory();
    if(folder.createDirectory().failed())return;
    if(source.getParentDirectory()==folder) {
        activeUserPreset=source;scanLibrary();return;
    }
    const auto dest=folder.getNonexistentChildFile(
        juce::File::createLegalFileName(source.getFileNameWithoutExtension()),
        ".skellpreset",true);
    if(source.copyFileTo(dest))activeUserPreset=dest;
    scanLibrary();
}
void PresetBrowser::open(){scanLibrary();bank=0;scroll=0;hoveredCard=-1;search.setText({},juce::dontSendNotification);setVisible(true);toFront(true);search.grabKeyboardFocus();repaint();}
void PresetBrowser::resized(){
    search.setBounds(19,17,getWidth()-38,42);
    importButton.setBounds(17,470,168,27);
    exportButton.setBounds(17,502,168,27);
}
void PresetBrowser::setChooserActive(bool active) {
    chooserActive=active;
    importButton.setEnabled(!active);
    exportButton.setEnabled(!active);
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
void PresetBrowser::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    g.setColour(Theme::panelAlt);g.fillRoundedRectangle(getLocalBounds().toFloat(),11);
    g.setColour(Theme::stroke.brighter(.5f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),11,1.4f);
    Theme::text(g,"BANKS",{19,75,174,20},10,Theme::muted,true);
    const auto entries=filtered();
    Theme::text(g,juce::String((int)entries.size())+" PRESETS",{204,75,246,20},10,Theme::muted,true);
    Theme::text(g,"ESC TO CLOSE",{getWidth()-121,75,100,20},10,Theme::muted,false,juce::Justification::centredRight);
    g.setColour(Theme::stroke);g.drawVerticalLine(195,84.f,(float)getHeight()-20);
    for(int i=0;i<banks.size();i++){
        auto b=juce::Rectangle<float>(17.f,111.f+i*60.f,168.f,52.f);
        g.setColour(i==bank?Theme::green.withAlpha(.11f):Theme::panel);g.fillRoundedRectangle(b,6);
        g.setColour(i==bank?Theme::green.withAlpha(.7f):Theme::stroke);g.drawRoundedRectangle(b,6,1);
        g.setColour((i==bank?Theme::green:Theme::blue).withAlpha(.20f));
        for(int j=0;j<4;j++)g.drawLine(b.getRight()-44.f+j*8.f,b.getY()+12.f,
                                        b.getRight()-56.f+j*8.f,b.getBottom()-9.f,1.3f);
        Theme::text(g,banks[i],{29,(int)b.getY()+14,133,22},13,Theme::white,i==bank);
    }
    if(processor.loadedFactoryPresetIndex()<0)
        Theme::text(g,"CUSTOM CHAIN ACTIVE",{18,341,177,20},10,Theme::green,true);
    Theme::text(g,"FACTORY + YOUR NATIVE PRESETS",{18,369,175,30},9,Theme::muted);
    Theme::text(g,"Select a sound to audition.",{18,405,172,19},11,Theme::muted);
    Theme::text(g,juce::String((int)userPresets.size())+" SAVED IN MY PRESETS",{18,425,172,19},10,Theme::amber);
    Theme::text(g,"Browser stays open on load.",{18,441,172,19},10,Theme::muted);
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
        const int itemColour=custom?2:skellpreset::factoryPresets[(size_t)entry].colour;
        const int y=110+i*61;
        auto rect=juce::Rectangle<float>(207.f,(float)y,(float)getWidth()-224.f,56.f);
        g.setColour(Theme::bg);g.fillRoundedRectangle(rect,6);
        // A restrained themed gradient and original signal trace replace the
        // repeated raster skull on every single card.
        const auto bankTint=(custom || itemBank=="Adlibs")?Theme::green:Theme::blue;
        juce::ColourGradient fade(Theme::panelAlt.withAlpha(.03f),rect.getRight()-230.f,rect.getCentreY(),
                                  bankTint.withAlpha(.20f),rect.getRight(),rect.getCentreY(),false);
        g.setGradientFill(fade);g.fillRoundedRectangle(rect.reduced(.6f),5.f);
        juce::Path wave;
        for(int px=0;px<154;px+=4) {
            const float x=rect.getRight()-173.f+px;
            const float decay=1.f-std::abs(px-77.f)/77.f;
            const float waveY=rect.getCentreY()+std::sin(px*.19f+itemColour*2.f)*decay*(9.f+itemColour);
            if(px==0)wave.startNewSubPath(x,waveY);else wave.lineTo(x,waveY);
        }
        g.setColour(bankTint.withAlpha(.37f));g.strokePath(wave,juce::PathStrokeType(1.6f));
        const bool loaded=custom
            ?activeUserPreset==userPresets[(size_t)(-entry-1)]
            :entry==processor.loadedFactoryPresetIndex();
        const bool hovered=itemIndex==hoveredCard;
        g.setColour(loaded?Theme::green:hovered?Theme::blue:Theme::stroke);
        g.drawRoundedRectangle(rect,6,hovered?1.8f:1.2f);
        Theme::text(g,itemName,{221,y+8,240,23},16,Theme::white,true);
        const auto bankColour=(custom || itemBank=="Adlibs")?Theme::green:Theme::blue;
        Theme::text(g,itemBank,{222,y+34,230,13},10,bankColour,true);
        if(loaded)Theme::text(g,"ACTIVE",{getWidth()-99,y+19,68,16},9,Theme::green,true,juce::Justification::centredRight);
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
