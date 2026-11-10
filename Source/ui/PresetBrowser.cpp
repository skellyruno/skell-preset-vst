#include "PresetBrowser.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace {const juce::StringArray banks{"All presets","Main Vocals","Adlibs"};}
PresetBrowser::PresetBrowser(SkellPresetAudioProcessor& p):processor(p) {
    search.setMultiLine(false);
    search.setTextToShowWhenEmpty("Search presets or banks",Theme::muted);
    applyTheme();
    search.onTextChange=[this]{scroll=0;repaint();};
    search.onEscapeKey=[this]{setVisible(false);if(onClose)onClose();};
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
            repaint();
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
    Theme::text(g,"ORIGINAL SKELL FACTORY BANKS",{18,369,175,30},9,Theme::muted);
    Theme::text(g,"Select a sound to audition.",{18,405,172,19},11,Theme::muted);
    Theme::text(g,"Native preset import / export",{18,425,172,19},10,Theme::amber);
    Theme::text(g,"Browser stays open on load.",{18,441,172,19},10,Theme::muted);
    for(int i=0;i<6;i++){
        const int itemIndex=i+scroll;
        if(itemIndex>=(int)entries.size())break;
        const auto& item=skellpreset::factoryPresets[(size_t)entries[(size_t)itemIndex]];
        const int y=110+i*61;
        auto rect=juce::Rectangle<float>(207.f,(float)y,(float)getWidth()-224.f,56.f);
        g.setColour(Theme::bg);g.fillRoundedRectangle(rect,6);
        // A restrained themed gradient and original signal trace replace the
        // repeated raster skull on every single card.
        const auto bankTint=juce::String(item.bank)=="Adlibs"?Theme::green:Theme::blue;
        juce::ColourGradient fade(Theme::panelAlt.withAlpha(.03f),rect.getRight()-230.f,rect.getCentreY(),
                                  bankTint.withAlpha(.20f),rect.getRight(),rect.getCentreY(),false);
        g.setGradientFill(fade);g.fillRoundedRectangle(rect.reduced(.6f),5.f);
        juce::Path wave;
        for(int px=0;px<154;px+=4) {
            const float x=rect.getRight()-173.f+px;
            const float decay=1.f-std::abs(px-77.f)/77.f;
            const float y=rect.getCentreY()+std::sin(px*.19f+item.colour*2.f)*decay*(9.f+item.colour);
            if(px==0)wave.startNewSubPath(x,y);else wave.lineTo(x,y);
        }
        g.setColour(bankTint.withAlpha(.37f));g.strokePath(wave,juce::PathStrokeType(1.6f));
        const bool loaded=entries[(size_t)itemIndex]==processor.getCurrentProgram();
        g.setColour(loaded?Theme::green:Theme::stroke);
        g.drawRoundedRectangle(rect,6,1.2f);
        Theme::text(g,item.name,{221,y+8,240,23},16,Theme::white,true);
        const auto bankColour=juce::String(item.bank)=="Adlibs"?Theme::green:juce::String(item.bank)=="Vocoding"?Theme::pink:Theme::blue;
        Theme::text(g,item.bank,{222,y+34,230,13},10,bankColour,true);
        if(loaded)Theme::text(g,"ACTIVE",{getWidth()-99,y+19,68,16},9,Theme::green,true,juce::Justification::centredRight);
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
            const int previousMode=(int)processor.state.getRawParameterValue("mode")->load();
            processor.loadFactoryPreset(entries[(size_t)(scroll+row)]);
            // Loading a sound should not unexpectedly switch a Simple audition
            // into Advanced or disable the user's existing global bypass.
            if(previousMode==0 || previousMode==3) {
                if(auto* mode=processor.state.getParameter("mode"))
                    mode->setValueNotifyingHost(mode->convertTo0to1((float)previousMode));
            }
            if(onLoaded)onLoaded();
            // Keep bank, scroll position, and search query visible for fast A/B.
            repaint();
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
