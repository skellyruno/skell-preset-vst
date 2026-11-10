#include "PresetBrowser.h"
#include "Theme.h"
#include <BinaryData.h>
#include <algorithm>

namespace {const juce::StringArray banks{"All presets","Main Vocals","Adlibs"};}
PresetBrowser::PresetBrowser(SkellPresetAudioProcessor& p):processor(p) {
    search.setMultiLine(false);
    search.setTextToShowWhenEmpty("Search presets or banks",Theme::muted);
    search.setColour(juce::TextEditor::backgroundColourId,Theme::bg);
    search.setColour(juce::TextEditor::textColourId,Theme::white);
    search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    search.onTextChange=[this]{scroll=0;repaint();};
    search.onEscapeKey=[this]{setVisible(false);if(onClose)onClose();};
    addAndMakeVisible(search);
    for (auto* button : {&importButton,&exportButton}) {
        button->setColour(juce::TextButton::buttonColourId,Theme::bg);
        button->setColour(juce::TextButton::textColourOffId,Theme::white);
        addAndMakeVisible(*button);
    }
    importButton.setTooltip("Open a validated native .skellpreset ZIP archive");
    exportButton.setTooltip("Save the active chain, slot order, macros and global controls");
    importButton.onClick=[this] {chooseImport();};
    exportButton.onClick=[this] {chooseExport();};
    const void* data[]={BinaryData::cover_0_png,BinaryData::cover_1_png,BinaryData::cover_2_png,
                        BinaryData::cover_3_png,BinaryData::cover_4_png,BinaryData::cover_5_png};
    const int size[]={BinaryData::cover_0_pngSize,BinaryData::cover_1_pngSize,BinaryData::cover_2_pngSize,
                      BinaryData::cover_3_pngSize,BinaryData::cover_4_pngSize,BinaryData::cover_5_pngSize};
    for(size_t k=0;k<covers.size();k++)covers[k]=juce::ImageCache::getFromMemory(data[k],size[k]);
    setWantsKeyboardFocus(true);
}
void PresetBrowser::open(){bank=0;scroll=0;search.setText({},juce::dontSendNotification);setVisible(true);toFront(true);search.grabKeyboardFocus();repaint();}
void PresetBrowser::resized(){
    search.setBounds(19,17,getWidth()-38,42);
    importButton.setBounds(17,470,168,27);
    exportButton.setBounds(17,502,168,27);
}
void PresetBrowser::chooseImport() {
    fileChooser=std::make_unique<juce::FileChooser>("Import native Skell-Preset", juce::File{}, "*.skellpreset");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            const auto file=chooser.getResult();
            if(file==juce::File{})return;
            const auto result=processor.importPreset(file);
            if(result.failed()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Cannot import preset",result.getErrorMessage());
                return;
            }
            if(onLoaded)onLoaded();
            setVisible(false);
            if(onClose)onClose();
        });
}
void PresetBrowser::chooseExport() {
    const auto base=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto initial=base.getChildFile(juce::File::createLegalFileName(processor.presetName())+".skellpreset");
    fileChooser=std::make_unique<juce::FileChooser>("Save native Skell-Preset",initial,"*.skellpreset");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            auto file=chooser.getResult();
            if(file==juce::File{})return;
            if(!file.hasFileExtension(".skellpreset"))file=file.withFileExtension(".skellpreset");
            const auto result=processor.exportPreset(file);
            if(result.failed()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Cannot save preset",result.getErrorMessage());
                return;
            }
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,"Preset saved",file.getFullPathName());
        });
}
std::vector<int> PresetBrowser::filtered() const {
    std::vector<int> list;
    auto q=search.getText().toLowerCase().trim();
    for(int i=0;i<(int)skellpreset::factoryPresets.size();i++) {
        const auto& item=skellpreset::factoryPresets[(size_t)i];
        const juce::String name(item.name), b(item.bank),desc(item.description);
        if(b=="Vocoding")continue; // Preserve existing preset IDs; hide unfinished vocoder content.
        if(bank>0 && b!=banks[bank])continue;
        bool found=q.isEmpty() || name.containsIgnoreCase(q) || b.containsIgnoreCase(q) || desc.containsIgnoreCase(q);
        if(q.startsWith("bank:"))found=b.containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("tag:"))found=desc.containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("author:"))found=juce::String("skell audio").containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
        if(q.startsWith("fav:")||q.startsWith("match:"))found=false; // no synthetic user favourites/matches
        if(found)list.push_back(i);
    }
    return list;
}
void PresetBrowser::paint(juce::Graphics&g) {
    g.setColour(Theme::panelAlt);g.fillRoundedRectangle(getLocalBounds().toFloat(),11);
    g.setColour(Theme::stroke.brighter(.5f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),11,1.4f);
    Theme::text(g,"BANKS",{19,75,174,20},10,Theme::muted,true);
    const auto entries=filtered();
    Theme::text(g,juce::String((int)entries.size())+" PRESETS",{204,75,246,20},10,Theme::muted,true);
    Theme::text(g,"ESC TO CLOSE",{getWidth()-121,75,100,20},10,Theme::muted,false,juce::Justification::centredRight);
    g.setColour(Theme::stroke);g.drawVerticalLine(195,84.f,(float)getHeight()-20);
    for(int i=0;i<banks.size();i++){
        auto b=juce::Rectangle<float>(17.f,111.f+i*60.f,168.f,52.f);
        g.setColour(i==bank?Theme::green.withAlpha(.13f):Theme::bg);g.fillRoundedRectangle(b,6);
        g.setColour(i==bank?Theme::green.withAlpha(.7f):Theme::stroke);g.drawRoundedRectangle(b,6,1);
        const auto& image=covers[(size_t)(i%6)];
        if(image.isValid()){
            g.saveState();g.reduceClipRegion(juce::Rectangle<int>((int)b.getRight()-56,(int)b.getY()+1,55,50));
            g.drawImage(image,juce::Rectangle<float>(b.getRight()-92,b.getY(),140,52),juce::RectanglePlacement::fillDestination);g.restoreState();
        }
        Theme::text(g,banks[i],{29,(int)b.getY()+14,133,22},13,Theme::white,i==bank);
    }
    Theme::text(g,"ORIGINAL SKELL FACTORY BANKS",{18,369,175,30},9,Theme::muted);
    Theme::text(g,"Click any preset to load.",{18,405,172,19},11,Theme::muted);
    Theme::text(g,"Native preset import / export",{18,425,172,19},10,Theme::amber);
    Theme::text(g,"Use WAV pairs for MATCH later.",{18,441,172,19},10,Theme::muted);
    for(int i=0;i<6;i++){
        const int itemIndex=i+scroll;
        if(itemIndex>=(int)entries.size())break;
        const auto& item=skellpreset::factoryPresets[(size_t)entries[(size_t)itemIndex]];
        const int y=110+i*61;
        auto rect=juce::Rectangle<float>(207.f,(float)y,(float)getWidth()-224.f,56.f);
        g.setColour(Theme::bg);g.fillRoundedRectangle(rect,6);
        const auto& image=covers[(size_t)(item.colour%6)];
        if(image.isValid()) {
            g.saveState();g.reduceClipRegion(juce::Rectangle<int>(getWidth()-214,y+1,196,54));
            g.drawImage(image,juce::Rectangle<float>((float)getWidth()-260.f,(float)y,244.f,56.f),juce::RectanglePlacement::fillDestination);
            g.restoreState();
        }
        g.setColour(entries[(size_t)itemIndex]==processor.getCurrentProgram()?Theme::green:Theme::stroke);
        g.drawRoundedRectangle(rect,6,1.2f);
        Theme::text(g,item.name,{221,y+8,240,23},16,Theme::white,true);
        const auto bankColour=juce::String(item.bank)=="Adlibs"?Theme::green:juce::String(item.bank)=="Vocoding"?Theme::pink:Theme::blue;
        Theme::text(g,item.bank,{222,y+34,230,13},10,bankColour,true);
    }
    if(entries.empty())Theme::text(g,"NO MATCHING PRESETS",{246,222,350,40},15,Theme::muted,true);
    if((int)entries.size()>6){
        Theme::text(g,"SCROLL TO SEE MORE",{260,getHeight()-20,300,16},10,Theme::muted);
    }
}
void PresetBrowser::mouseDown(const juce::MouseEvent&e) {
    if(e.y>=111 && e.x>=17 && e.x<=185){
        const int idx=(e.y-111)/60;
        if(idx>=0&&idx<banks.size()){bank=idx;scroll=0;repaint();}return;
    }
    if(e.y>=110&&e.x>=207&&e.y<476){
        const auto entries=filtered();const int row=(e.y-110)/61;
        if(row>=0&&row<6 && scroll+row<(int)entries.size()){
            processor.loadFactoryPreset(entries[(size_t)(scroll+row)]);
            if(onLoaded)onLoaded();setVisible(false);if(onClose)onClose();
        }
    }
}
void PresetBrowser::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel){
    const int maxScroll=std::max(0,(int)filtered().size()-6);
    scroll=juce::jlimit(0,maxScroll,scroll+(wheel.deltaY<0?2:-2));repaint();
}
bool PresetBrowser::keyPressed(const juce::KeyPress& key){
    if(key==juce::KeyPress::escapeKey){setVisible(false);if(onClose)onClose();return true;}
    return false;
}
