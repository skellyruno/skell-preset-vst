#include "PresetBrowser.h"
#include "Theme.h"
#include "../chain/AlchemistNativePreset.h"
#include "../chain/NativeSoundMatchTrials.h"
#include <algorithm>
#include <cmath>
#include <array>

namespace {
// Direct Main Vocals > Adlibs submenu: no fifth top-level library bank.
 // Historical factory IDs and personal genre tags remain unchanged.
const juce::StringArray banks{"Main Vocals","My Presets","Artists","Favorites"};
constexpr int alchemistIndex=skellpreset::alchemist::factoryIndex;
constexpr int deuceIndex=skellpreset::deuce_v25::factoryIndex;
// One shared geometry model for painting, click hit-testing and UI regression.
// The three genres have a visible MANUAL heading and never overlap controls.
juce::Rectangle<int> bankBounds(int index) noexcept {
    // Reserve one indented 33px row beneath Main Vocals for ADLIBS.
    return {17,109+(index==0?0:38+index*38),168,33};
}
juce::Rectangle<int> adlibsBounds() noexcept {return {30,147,155,31};}
juce::String singlePreviewName(int entry) {
    if(entry==alchemistIndex)return "ALCHEMIST";
    if(entry==deuceIndex)return "DEUCE";
    return entry<0?"YOUR PRESET":"FACTORY PRESET";
}
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
        const float step=2.6f+(float)(seed%4)*.22f;
        for(int i=0;i<5;++i) {
            const float margin=5.f+i*step;
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
    search.setTextToShowWhenEmpty("Search sounds / artist:name / genre:name",Theme::muted);
    artistEditor.setMultiLine(false);
    artistEditor.setTextToShowWhenEmpty("Artist for selected preset",Theme::muted);
    artistEditor.setTooltip("Type an artist name for your selected personal preset, then press SET ARTIST");
    genrePicker.setTooltip("Assign a category to the selected saved preset, never automatically.");
    genreFilter.setTooltip("Filter your saved presets by genre/category.");
    categoryEditor.setTextToShowWhenEmpty("New category name",Theme::muted);
    categoryEditor.setTooltip("Create your own library category; presets and automation stay unchanged.");
    applyTheme();
    search.onTextChange=[this]{
        scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;
        clearSearchButton.setEnabled(search.getText().isNotEmpty());
        repaint();
    };
    search.onEscapeKey=[this]{
        if(detailsOpen)showDetails(false);
        else {setVisible(false);if(onClose)onClose();}
    };
    // Enter auditions the first filtered result without dismissing the browser.
    search.onReturnKey=[this]{
        const auto entries=filtered();
        const int target=auditionTargetForTest(keyboardCard,scroll,(int)entries.size());
        if(target>=0)auditionEntry(entries[(size_t)target]);
    };
    addAndMakeVisible(search);
    addAndMakeVisible(artistEditor);
    addAndMakeVisible(genrePicker);
    addAndMakeVisible(genreFilter);
    addAndMakeVisible(sortPicker);
    sortPicker.addItem("Name  A-Z",1);
    sortPicker.addItem("Name  Z-A",2);
    sortPicker.addItem("Starred first",3);
    sortPicker.setSelectedId(1,juce::dontSendNotification);
    sortPicker.setTooltip("Change display order, never rename presets or modify audio.");
    sortPicker.onChange=[this]{scroll=0;hoveredCard=-1;keyboardCard=-1;repaint();};
    addAndMakeVisible(categoryEditor);
    for(auto* button:{&importButton,&exportButton,&artworkButton,
                      &artistButton,&genreButton,&detailsButton,&addCategoryButton,
                      &clearSearchButton})
        addAndMakeVisible(*button);
    detailsButton.setTooltip("Show artist, genre, and cover controls in a separate clean panel");
    importButton.setTooltip("Open a validated native .skellpreset ZIP archive");
    exportButton.setTooltip("Save the active chain, slot order, macros and global controls");
    artworkButton.setTooltip("Choose a cover for Alchemist or any selected saved preset");
    artworkButton.setEnabled(false);
    artistButton.setEnabled(false);
    artistButton.setTooltip("Save an artist label to the selected personal preset. Does not modify its .skellpreset file.");
    clearSearchButton.setTooltip("Clear the search phrase without changing the selected category.");
    clearSearchButton.setEnabled(false);
    clearSearchButton.onClick=[this] {
        search.clear();
        search.grabKeyboardFocus();
        scroll=0;hoveredCard=-1;keyboardCard=-1;repaint();
    };
    genreButton.setTooltip("Manually tag your personal preset with a genre, Ad-libs or custom category.");
    addCategoryButton.setTooltip("Add a new category to the user library; no audio or preset ZIP changes.");
    genreButton.setEnabled(false);
    reloadAlchemistArtwork();
    scanLibrary();
    loadFavorites();
    loadArtists();
    loadCustomCategories();
    refreshGenreChoices();
    loadGenres();
    refreshArtwork();
    detailsButton.onClick=[this]{showDetails(!detailsOpen);};
    importButton.onClick=[this] {chooseImport();};
    exportButton.onClick=[this] {chooseExport();};
    artworkButton.onClick=[this] {chooseArtwork();};
    artistButton.onClick=[this] {
        if(!activeUserPreset.existsAsFile())return;
        for(int i=0;i<(int)userPresets.size();++i)
            if(userPresets[(size_t)i]==activeUserPreset) {
                if(setArtistForUser(i,artistEditor.getText())) {
                    scroll=0;hoveredCard=-1;repaint();
                }
                return;
            }
    };
    artistEditor.onReturnKey=[this]{if(artistButton.isEnabled())artistButton.triggerClick();};
    genreButton.onClick=[this] {
        if(!activeUserPreset.existsAsFile())return;
        const auto label=genrePicker.getSelectedId()>1
            ?genrePicker.getText():juce::String{};
        for(int i=0;i<(int)userPresets.size();++i)
            if(userPresets[(size_t)i]==activeUserPreset) {
                if(setGenreForUser(i,label)){scroll=0;hoveredCard=-1;repaint();}
                return;
            }
    };
    genreFilter.onChange=[this]{
        scroll=0;hoveredCard=-1;keyboardCard=-1;repaint();
    };
    addCategoryButton.onClick=[this] {
        const auto label=categoryEditor.getText().trim();
        if(label.isEmpty() || label.length()>32 ||
           label.equalsIgnoreCase("All genres") ||
           label.equalsIgnoreCase("Uncategorized") ||
           label.equalsIgnoreCase("Untagged") ||
           label.containsAnyOf("\r\n\t\\/:*?\"<>|")){
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Invalid category","Use at most 32 characters, with no path separators.");
            return;
        }
        if(!isKnownCategory(label)){
            if(customCategories.size()>=64)return;
            customCategories.add(label);
            if(!saveCustomCategories()){
                customCategories.removeString(label,true);
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Category not saved","Check your preset library folder permissions.");
                return;
            }
        }
        refreshGenreChoices();
        for(int i=0;i<genrePicker.getNumItems();++i)
            if(genrePicker.getItemText(i).equalsIgnoreCase(label))
                genrePicker.setSelectedId(genrePicker.getItemId(i),juce::dontSendNotification);
        categoryEditor.clear();
        if(activeUserPreset.existsAsFile() && genreButton.isEnabled())
            genreButton.triggerClick();
        repaint();
    };
    categoryEditor.onReturnKey=[this]{addCategoryButton.triggerClick();};
    setWantsKeyboardFocus(true);
}
void PresetBrowser::applyTheme() {
    Theme::selectFor(processor.state);
    search.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    search.setColour(juce::TextEditor::textColourId,Theme::white);
    search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    search.setTextToShowWhenEmpty("Search sounds / artist:name / genre:name",Theme::muted);
    artistEditor.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    artistEditor.setColour(juce::TextEditor::textColourId,Theme::white);
    artistEditor.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    artistEditor.setTextToShowWhenEmpty("Artist for selected preset",Theme::muted);
    genrePicker.setColour(juce::ComboBox::backgroundColourId,Theme::plot);
    genrePicker.setColour(juce::ComboBox::textColourId,Theme::white);
    genrePicker.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    genrePicker.setColour(juce::ComboBox::arrowColourId,Theme::green);
    genreFilter.setColour(juce::ComboBox::backgroundColourId,Theme::plot);
    genreFilter.setColour(juce::ComboBox::textColourId,Theme::white);
    genreFilter.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    genreFilter.setColour(juce::ComboBox::arrowColourId,Theme::green);
    sortPicker.setColour(juce::ComboBox::backgroundColourId,Theme::plot);
    sortPicker.setColour(juce::ComboBox::textColourId,Theme::white);
    sortPicker.setColour(juce::ComboBox::outlineColourId,Theme::stroke);
    sortPicker.setColour(juce::ComboBox::arrowColourId,Theme::green);
    categoryEditor.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    categoryEditor.setColour(juce::TextEditor::textColourId,Theme::white);
    categoryEditor.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    for(auto* button:{&importButton,&exportButton,&artworkButton,
                       &artistButton,&genreButton,&detailsButton,&addCategoryButton,
                       &clearSearchButton}) {
        button->setColour(juce::TextButton::buttonColourId,Theme::panel);
        button->setColour(juce::TextButton::textColourOffId,Theme::white);
    }
    exportButton.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.22f));
    detailsButton.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.12f));
    addCategoryButton.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.12f));
    repaint();
}
juce::File PresetBrowser::libraryDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Skell Audio").getChildFile("Skell-Preset")
        .getChildFile("Presets");
}
void PresetBrowser::scanLibrary() {
    categoryCountsDirty=true;
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
    reloadAlchemistArtwork();
    for(size_t i=0;i<userPresets.size();++i) {
        const auto base=userPresets[i];
        auto cover=base.withFileExtension(".png");
        if(!cover.existsAsFile())cover=base.withFileExtension(".jpg");
        if(cover.existsAsFile() && cover.getSize()>0 && cover.getSize()<=8*1024*1024) {
            auto image=juce::ImageFileFormat::loadFrom(cover);
            if(image.isValid() && image.getWidth()<=4096 && image.getHeight()<=4096)
                userArtwork[i]=image.rescaled(128,96,juce::Graphics::mediumResamplingQuality);
        }
    }
}

juce::File PresetBrowser::activeArtworkDestination() const {
    if(activeUserPreset.existsAsFile())
        return activeUserPreset.withFileExtension(".png");
    if(processor.loadedFactoryPresetIndex()==alchemistIndex)
        return libraryDirectory().getChildFile("Covers").getChildFile("Alchemist.png");
    return {};
}
bool PresetBrowser::canEditCover() const {
    return activeUserPreset.existsAsFile() ||
        processor.loadedFactoryPresetIndex()==alchemistIndex;
}
void PresetBrowser::reloadAlchemistArtwork() {
    alchemistArtwork={};
    const auto path=libraryDirectory().getChildFile("Covers").getChildFile("Alchemist.png");
    if(!path.existsAsFile()||path.getSize()<=0||path.getSize()>8*1024*1024)return;
    auto img=juce::ImageFileFormat::loadFrom(path);
    if(img.isValid() && img.getWidth()<=4096 && img.getHeight()<=4096)
        alchemistArtwork=img.rescaled(1200,900,juce::Graphics::highResamplingQuality);
}
juce::Image PresetBrowser::selectedCover() const {
    if(activeUserPreset.existsAsFile()) {
        for(size_t i=0;i<userPresets.size();++i)
            if(userPresets[i]==activeUserPreset && i<userArtwork.size())
                return selectedUserBackdrop.isValid()?selectedUserBackdrop:userArtwork[i];
    }
    if(processor.loadedFactoryPresetIndex()==alchemistIndex)
        return alchemistArtwork;
    return {};
}

bool PresetBrowser::copyToLibrary(const juce::File& source) {
    const auto folder=libraryDirectory();
    if(folder.createDirectory().failed())return false;
    if(source.getParentDirectory()==folder) {
        activeUserPreset=source;scanLibrary();refreshArtwork();return true;
    }
    const auto dest=folder.getNonexistentChildFile(
        juce::File::createLegalFileName(source.getFileNameWithoutExtension()),
        ".skellpreset",true);
    if(!source.copyFileTo(dest))return false;
    activeUserPreset=dest;
    // Preserve a validated cover in the personal library if it exists.
    auto originalCover=source.withFileExtension(".png");
    if(!originalCover.existsAsFile())originalCover=source.withFileExtension(".jpg");
    if(originalCover.existsAsFile() && originalCover.getSize()>0 &&
       originalCover.getSize()<=8*1024*1024)
        originalCover.copyFileTo(dest.withFileExtension(originalCover.getFileExtension()));
    scanLibrary();
    refreshArtwork();
    return true;
}
void PresetBrowser::open() {
    scanLibrary();loadFavorites();loadArtists();
    loadCustomCategories();refreshGenreChoices();loadGenres();refreshArtwork();
    bank=0;mainAdlibs=false;scroll=0;hoveredCard=-1;keyboardCard=-1;
    hoveredBank=-1;hoveredAdlibs=false;
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    // Preserve the user's in-session display sort without saving DAW state.
    showDetails(false);
    search.setText({},juce::dontSendNotification);
    clearSearchButton.setEnabled(false);
    categoryCountsDirty=true;
    setVisible(true);toFront(true);search.grabKeyboardFocus();repaint();
}
void PresetBrowser::showDetails(bool openDetails) {
    detailsOpen=openDetails;
    detailsButton.setButtonText(detailsOpen?"BACK TO LIBRARY":"EDIT DETAILS");
    if(!detailsOpen)grabKeyboardFocus();
    resized();repaint();
}
void PresetBrowser::chooseBank(int index){
    if(index<0 || index>=banks.size())return;
    bank=index;mainAdlibs=false;
    scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;
    // Factory Main Vocals are not hidden by a previous user genre filter.
    if(bank==0)genreFilter.setSelectedId(1,juce::dontSendNotification);
    repaint();
}
void PresetBrowser::chooseAdlibs(){
    bank=0;mainAdlibs=true;
    scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    repaint();
}
void PresetBrowser::focusCard(int index){
    const int count=(int)filtered().size();
    if(count<=0){keyboardCard=-1;scroll=0;repaint();return;}
    keyboardCard=juce::jlimit(0,count-1,index);
    if(keyboardCard<scroll)scroll=keyboardCard;
    if(keyboardCard>=scroll+6)scroll=keyboardCard-5;
    hoveredCard=-1;repaint();
}
void PresetBrowser::resized(){
    search.setBounds(19,17,getWidth()-111,42);
    clearSearchButton.setBounds(getWidth()-86,22,66,31);
    sortPicker.setVisible(!detailsOpen);
    sortPicker.setBounds(getWidth()-169,69,150,29);
    detailsButton.setBounds(18,detailsOpen?112:386,166,30);
    for(auto* control:{static_cast<juce::Component*>(&artistEditor),
                       static_cast<juce::Component*>(&artistButton),
                       static_cast<juce::Component*>(&genrePicker),
                       static_cast<juce::Component*>(&genreButton),
                       static_cast<juce::Component*>(&categoryEditor),
                       static_cast<juce::Component*>(&addCategoryButton),
                       static_cast<juce::Component*>(&artworkButton)})
        control->setVisible(detailsOpen);
    genreFilter.setVisible(!detailsOpen);
    if(detailsOpen){
        artistEditor.setBounds(18,184,166,29);
        artistButton.setBounds(18,221,166,29);
        genrePicker.setBounds(18,289,107,29);
        genreButton.setBounds(132,289,52,29);
        categoryEditor.setBounds(18,359,166,29);
        addCategoryButton.setBounds(18,393,166,29);
        artworkButton.setBounds(18,429,166,29);
        importButton.setBounds(18,464,166,28);
        exportButton.setBounds(18,498,166,28);
    }else{
        genreFilter.setBounds(18,332,166,32);
        importButton.setBounds(18,433,166,30);
        exportButton.setBounds(18,476,166,30);
    }
}
void PresetBrowser::setChooserActive(bool active) {
    chooserActive=active;
    importButton.setEnabled(!active);
    exportButton.setEnabled(!active);
    artworkButton.setEnabled(!active && canEditCover());
    artistButton.setEnabled(!active && activeUserPreset.existsAsFile());
    artistEditor.setEnabled(!active && activeUserPreset.existsAsFile());
    genreButton.setEnabled(!active && activeUserPreset.existsAsFile());
    genrePicker.setEnabled(!active && activeUserPreset.existsAsFile());
    addCategoryButton.setEnabled(!active);
    categoryEditor.setEnabled(!active);
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
            if(safe->onBeforeLoad)safe->onBeforeLoad();
            const auto result=safe->processor.importPreset(file);
            if(result.failed()) {
                if(safe->onLoadFailed)safe->onLoadFailed();
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Cannot import preset",result.getErrorMessage());
                return;
            }
            const bool copied=safe->copyToLibrary(file);
            if(safe->onLoaded)safe->onLoaded();
            if(!copied)
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,"Preset loaded",
                    "The sound was imported, but a library copy could not be saved.");
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
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return;
            safe->setChooserActive(false);
            auto file=chooser.getResult();
            if(file==juce::File{})return;
            if(!file.hasFileExtension(".skellpreset"))file=file.withFileExtension(".skellpreset");
            const auto previousUser=safe->activeUserPreset;
            juce::String previousArtist,previousGenre;
            for(int i=0;i<(int)safe->userPresets.size();++i)
                if(safe->userPresets[(size_t)i]==previousUser) {
                    previousArtist=safe->artistForUser(i);
                    previousGenre=safe->genreForUser(i);
                    break;
                }
            auto oldCover=safe->activeArtworkDestination();
            if(!oldCover.existsAsFile() && previousUser.existsAsFile())
                oldCover=previousUser.withFileExtension(".jpg");
            const auto result=safe->processor.exportPreset(file);
            if(result.failed()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Cannot save preset",result.getErrorMessage());
                return;
            }
            const bool copied=safe->copyToLibrary(file);
            // Save-as should retain artist/category labels on the NEW library
            // copy, without changing the original .skellpreset or metadata.
            if(copied && safe->activeUserPreset!=previousUser) {
                for(int i=0;i<(int)safe->userPresets.size();++i)
                    if(safe->userPresets[(size_t)i]==safe->activeUserPreset) {
                        if(previousArtist.isNotEmpty())
                            safe->setArtistForUser(i,previousArtist);
                        if(previousGenre.isNotEmpty())
                            safe->setGenreForUser(i,previousGenre);
                        break;
                    }
            }
            // Saving a tweaked Alchemist or personal preset should preserve
            // its current cover even if the destination preset name changes.
            if(oldCover.existsAsFile() && safe->activeUserPreset.existsAsFile()) {
                const auto newCover=safe->activeUserPreset.withFileExtension(".png");
                if(oldCover!=newCover)
                    oldCover.copyFileTo(newCover);
            }
            safe->scanLibrary();
            safe->refreshArtwork();
            if(safe->onCoverChanged)safe->onCoverChanged();
            safe->repaint();
            juce::AlertWindow::showMessageBoxAsync(
                copied?juce::AlertWindow::InfoIcon:juce::AlertWindow::WarningIcon,
                copied?"Preset saved":"Preset exported",
                copied?file.getFullPathName():
                   file.getFullPathName()+"\nThe file was exported, but no library copy was saved.");
        });
}
int PresetBrowser::visibleFactoryPresetsForTest() const {
    int count=0;
    for(const int entry:filtered())if(entry>=0)++count;
    return count;
}
juce::File PresetBrowser::favoritesFile() const {
    // Kept outside the user's .skellpreset ZIP and VST3 APVTS; factory and
    // saved-preset identifiers remain intact across sessions and host reloads.
    return libraryDirectory().getParentDirectory().getChildFile("Favorites.txt");
}

juce::File PresetBrowser::genresFile() const {
    // Genre category exists only in this user-library sidecar, never in
    // factory programs, automation or .skellpreset archives.
    return libraryDirectory().getParentDirectory().getChildFile("GenreTags.txt");
}
juce::File PresetBrowser::categoriesFile() const {
    return libraryDirectory().getParentDirectory().getChildFile("UserCategories.txt");
}
bool PresetBrowser::isKnownCategory(const juce::String& label) const {
    return label.equalsIgnoreCase("Rage") ||
           label.equalsIgnoreCase("Indie") ||
           label.equalsIgnoreCase("R&B") ||
           label.equalsIgnoreCase("Ad-libs") ||
           label.equalsIgnoreCase("All genres") ||
           label.equalsIgnoreCase("Uncategorized") ||
           customCategories.contains(label,true);
}
void PresetBrowser::loadCustomCategories() {
    customCategories.clear();
    const auto file=categoriesFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>8192)return;
    for(auto name:juce::StringArray::fromLines(file.loadFileAsString())) {
        name=name.trim();
        if(name.isEmpty() || name.length()>32 ||
           name.containsAnyOf("\r\n\t\\/:*?\"<>|") ||
           isKnownCategory(name))continue;
        customCategories.add(name);
        if(customCategories.size()>=64)break;
    }
}
bool PresetBrowser::saveCustomCategories() const {
    const auto file=categoriesFile();
    if(file.getParentDirectory().createDirectory().failed())return false;
    juce::TemporaryFile pending(file);
    if(!pending.getFile().replaceWithText(customCategories.joinIntoString("\n")+"\n"))
        return false;
    return pending.overwriteTargetFileWithTemporary();
}
void PresetBrowser::refreshGenreChoices() {
    const auto previousTag=genrePicker.getText();
    const auto previousFilter=genreFilter.getText();
    genrePicker.clear(juce::dontSendNotification);
    genreFilter.clear(juce::dontSendNotification);
    genrePicker.addItem("Uncategorized",1);
    genreFilter.addItem("All genres",1);
    genreFilter.addItem("Untagged",2);
    for(const auto* label:{"Rage","Indie","R&B","Ad-libs"}){
        const int id=genrePicker.getNumItems()+1;
        genrePicker.addItem(label,id);
        // Adlibs is navigated directly under Main Vocals. Keep its
        // existing Edit Details genre tag for personal preset migration,
        // but do not duplicate the sound in the filter dropdown.
        if(juce::String(label)!="Ad-libs")
            genreFilter.addItem(label,id+1);
    }
    for(const auto& label:customCategories){
        const int id=genrePicker.getNumItems()+1;
        genrePicker.addItem(label,id);
        genreFilter.addItem(label,id+1);
    }
    auto lookup=[](const juce::ComboBox& picker,const juce::String& label){
        for(int i=0;i<picker.getNumItems();++i)
            if(picker.getItemText(i).equalsIgnoreCase(label))
                return picker.getItemId(i);
        return 1;
    };
    genrePicker.setSelectedId(lookup(genrePicker,previousTag),juce::dontSendNotification);
    genreFilter.setSelectedId(lookup(genreFilter,previousFilter),juce::dontSendNotification);
}
juce::String PresetBrowser::genreForUser(int index) const {
    if(index<0 || index>=(int)userPresets.size())return {};
    const int pos=genreKeys.indexOf(favoriteKey(-index-1),true);
    return pos>=0 && pos<genreNames.size()?genreNames[pos]:juce::String{};
}
void PresetBrowser::loadGenres() {
    genreKeys.clear();genreNames.clear();
    const auto file=genresFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>65536)return;
    for(const auto& line:juce::StringArray::fromLines(file.loadFileAsString())) {
        const int tab=line.indexOfChar('\t');
        if(tab<6)continue;
        const auto key=line.substring(0,tab).trim().toLowerCase();
        const auto label=line.substring(tab+1).trim();
        if(!key.startsWith("user:") || key.length()>260 ||
           key.containsAnyOf("\r\n\t") || genreKeys.contains(key,true) ||
           !isKnownCategory(label))continue;
        genreKeys.add(key);genreNames.add(label);
        if(genreKeys.size()>=200)break;
    }
}
bool PresetBrowser::saveGenres() const {
    const auto file=genresFile();
    if(file.getParentDirectory().createDirectory().failed())return false;
    juce::StringArray rows;
    for(int i=0;i<genreKeys.size() && i<genreNames.size();++i)
        rows.add(genreKeys[i]+"\t"+genreNames[i]);
    juce::TemporaryFile pending(file);
    if(!pending.getFile().replaceWithText(rows.joinIntoString("\n")+"\n"))
        return false;
    return pending.overwriteTargetFileWithTemporary();
}
bool PresetBrowser::setGenreForUser(int index,const juce::String& label) {
    if(index<0 || index>=(int)userPresets.size() ||
       (label.isNotEmpty() && !isKnownCategory(label)))
        return false;
    const auto key=favoriteKey(-index-1);
    if(key.isEmpty())return false;
    const auto oldKeys=genreKeys,oldLabels=genreNames;
    const int existing=genreKeys.indexOf(key,true);
    if(existing>=0) {
        if(label.isEmpty()){genreKeys.remove(existing);genreNames.remove(existing);}
        else genreNames.set(existing,label);
    } else if(label.isNotEmpty()) {
        if(genreKeys.size()>=200)return false;
        genreKeys.add(key);genreNames.add(label);
    }
    if(!saveGenres()) {
        genreKeys=oldKeys;genreNames=oldLabels;
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Category not saved","Check the Skell Preset library folder permissions.");
        return false;
    }
    categoryCountsDirty=true;
    return true;
}
juce::File PresetBrowser::artistsFile() const {
    // Artist tags are library metadata, not factory IDs, host automation,
    // FL Studio project state or .skellpreset ZIP contents.
    return libraryDirectory().getParentDirectory().getChildFile("ArtistTags.txt");
}
juce::String PresetBrowser::artistForUser(int index) const {
    if(index<0 || index>=(int)userPresets.size())return {};
    const auto key=favoriteKey(-index-1);
    const int found=artistKeys.indexOf(key,true);
    return found>=0 && found<artistNames.size()?artistNames[found]:juce::String{};
}
void PresetBrowser::loadArtists() {
    artistKeys.clear();artistNames.clear();
    const auto file=artistsFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>65536)return;
    for(const auto& line:juce::StringArray::fromLines(file.loadFileAsString())) {
        const int tab=line.indexOfChar('\t');
        if(tab<6)continue;
        const auto key=line.substring(0,tab).trim().toLowerCase();
        const auto artist=line.substring(tab+1).trim();
        if(!key.startsWith("user:") || key.length()>260 ||
           key.containsAnyOf("\r\n\t") || artist.isEmpty() ||
           artist.length()>60 || artist.containsAnyOf("\r\n\t") ||
           artistKeys.contains(key,true))continue;
        artistKeys.add(key);artistNames.add(artist);
        if(artistKeys.size()>=200)break;
    }
}
bool PresetBrowser::saveArtists() const {
    const auto target=artistsFile();
    if(target.getParentDirectory().createDirectory().failed())return false;
    juce::StringArray rows;
    for(int i=0;i<artistKeys.size() && i<artistNames.size();++i)
        rows.add(artistKeys[i]+"\t"+artistNames[i]);
    // Atomic replacement keeps existing user tags intact on disk failure.
    juce::TemporaryFile pending(target);
    if(!pending.getFile().replaceWithText(rows.joinIntoString("\n")+"\n"))
        return false;
    return pending.overwriteTargetFileWithTemporary();
}
bool PresetBrowser::setArtistForUser(int index,const juce::String& input) {
    const auto artist=input.trim();
    if(index<0 || index>=(int)userPresets.size())return false;
    if(artist.length()>60 || artist.containsAnyOf("\r\n\t")) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Invalid artist","Use an artist name up to 60 characters without tabs or line breaks.");
        return false;
    }
    const auto key=favoriteKey(-index-1);
    if(key.isEmpty())return false;
    const auto beforeKeys=artistKeys,beforeNames=artistNames;
    const int existing=artistKeys.indexOf(key,true);
    if(existing>=0) {
        if(artist.isEmpty()) {
            artistKeys.remove(existing);artistNames.remove(existing);
        } else artistNames.set(existing,artist);
    } else if(artist.isNotEmpty() && artistKeys.size()<200) {
        artistKeys.add(key);artistNames.add(artist);
    }
    if(!saveArtists()) {
        artistKeys=beforeKeys;artistNames=beforeNames;
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Artist not saved","Check the Skell Preset library folder permissions.");
        return false;
    }
    categoryCountsDirty=true;
    return true;
}
juce::String PresetBrowser::favoriteKey(int entry) const {
    if(entry>=0)return "factory:"+juce::String(entry);
    const auto index=-entry-1;
    if(index<0 || index>=(int)userPresets.size())return {};
    return "user:"+userPresets[(size_t)index].getFileName().toLowerCase();
}
bool PresetBrowser::isFavorite(int entry) const {
    const auto key=favoriteKey(entry);
    return key.isNotEmpty() && favoriteKeys.contains(key,true);
}
void PresetBrowser::loadFavorites() {
    favoriteKeys.clear();
    const auto file=favoritesFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>65536)return;
    for(auto key:juce::StringArray::fromLines(file.loadFileAsString())) {
        key=key.trim().toLowerCase();
        bool safeFactory=key=="factory:43" || key=="factory:46";
        if(!safeFactory && key.startsWith("factory:")){
            const auto index=key.substring(8).getIntValue();
            safeFactory=index>=0 && index<alchemistIndex &&
                juce::String(skellpreset::factoryPresets[(size_t)index].bank)
                    .equalsIgnoreCase("Adlibs");
        }
        if(key.length()>0 && key.length()<260 &&
           (key.startsWith("user:") || safeFactory) &&
           !key.containsChar('\r') && !favoriteKeys.contains(key,true))
            favoriteKeys.add(key);
        if(favoriteKeys.size()>=512)break;
    }
}
bool PresetBrowser::saveFavorites() const {
    const auto file=favoritesFile();
    if(file.getParentDirectory().createDirectory().failed())return false;
    // Atomic replace: never damage a user's previous Favorites.txt on error.
    juce::TemporaryFile pending(file);
    if(!pending.getFile().replaceWithText(favoriteKeys.joinIntoString("\n")+"\n"))
        return false;
    return pending.overwriteTargetFileWithTemporary();
}
void PresetBrowser::toggleFavorite(int entry) {
    const auto key=favoriteKey(entry);
    if(key.isEmpty())return;
    auto next=favoriteKeys;
    if(favoriteKeys.contains(key,true))favoriteKeys.removeString(key,true);
    else favoriteKeys.add(key);
    if(!saveFavorites()) {
        favoriteKeys=std::move(next);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Favorites not saved","Check the Skell Preset library folder permissions.");
        return;
    }
    scroll=juce::jlimit(0,std::max(0,(int)filtered().size()-6),scroll);
    hoveredCard=-1;hoveredStar=-1;
    categoryCountsDirty=true;
    repaint();
}
bool PresetBrowser::testBrowserSortingForTest(){
    const int oldBank=bank,oldScroll=scroll,oldMode=sortPicker.getSelectedId();
    const bool oldMainAdlibs=mainAdlibs;
    const auto oldQuery=search.getText();
    const auto oldFilter=genreFilter.getText();
    const auto oldFavorites=favoriteKeys;
    const auto oldCount=userPresets.size();
    bank=1;scroll=0;
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    search.setText("__skell_sort_probe__",juce::dontSendNotification);
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory);
    userPresets.push_back(folder.getChildFile("zz__skell_sort_probe__.skellpreset"));
    userPresets.push_back(folder.getChildFile("aa__skell_sort_probe__.skellpreset"));
    const int big=-((int)oldCount)-1,small=big-1;
    sortPicker.setSelectedId(1,juce::dontSendNotification);
    const auto az=filtered();
    const bool asc=az.size()==2 && az[0]==small && az[1]==big;
    sortPicker.setSelectedId(2,juce::dontSendNotification);
    const auto za=filtered();
    const bool desc=za.size()==2 && za[0]==big && za[1]==small;
    favoriteKeys.add(favoriteKey(big));
    sortPicker.setSelectedId(3,juce::dontSendNotification);
    const auto stars=filtered();
    const bool starred=stars.size()==2 && stars[0]==big;
    userPresets.resize(oldCount);favoriteKeys=oldFavorites;
    search.setText(oldQuery,juce::dontSendNotification);
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    for(int i=0;i<genreFilter.getNumItems();++i)
        if(genreFilter.getItemText(i)==oldFilter)
            genreFilter.setSelectedId(genreFilter.getItemId(i),juce::dontSendNotification);
    sortPicker.setSelectedId(oldMode,juce::dontSendNotification);
    bank=oldBank;mainAdlibs=oldMainAdlibs;scroll=oldScroll;repaint();
    return asc && desc && starred;
}
bool PresetBrowser::testBrowserKeyboardForTest(){
    // Synthetic library entries are never loaded or written to disk.
    const int oldBank=bank,oldScroll=scroll,oldHover=hoveredCard,
        oldFocus=keyboardCard;
    const bool oldMainAdlibs=mainAdlibs;
    const auto oldSearch=search.getText();
    const auto oldFilter=genreFilter.getText();
    const bool oldDetails=detailsOpen;
    const auto oldCount=userPresets.size();
    showDetails(false);
    chooseBank(1); // My Presets includes user entries without touching factory
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    search.setText("__skell_keynav_probe_",juce::dontSendNotification);
    for(int i=0;i<12;++i)
        userPresets.push_back(juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("__skell_keynav_probe_"+juce::String(i)+".skellpreset"));
    const bool visible=filtered().size()==12;
    keyboardCard=-1; // first Down must target the FIRST row, not skip it
    const auto unmodified=juce::ModifierKeys();
    keyPressed(juce::KeyPress(juce::KeyPress::downKey,unmodified,0));
    const bool firstDown=keyboardCard==0 && scroll==0;
    focusCard(0);
    keyPressed(juce::KeyPress(juce::KeyPress::downKey,unmodified,0));
    const bool down=keyboardCard==1 && scroll==0;
    keyPressed(juce::KeyPress(juce::KeyPress::endKey,unmodified,0));
    const bool end=keyboardCard==11 && scroll==6 &&
        auditionTargetForTest(keyboardCard,scroll,12)==11;
    keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey,unmodified,0));
    const bool pageUp=keyboardCard==5 && scroll==5 &&
        auditionTargetForTest(-1,scroll,12)==5 &&
        auditionTargetForTest(-1,999,12)==11 &&
        auditionTargetForTest(-1,0,0)==-1;
    keyPressed(juce::KeyPress(juce::KeyPress::homeKey,unmodified,0));
    const bool home=keyboardCard==0 && scroll==0;
    keyPressed(juce::KeyPress(juce::KeyPress::leftKey,unmodified,0));
    const bool left=bank==0;
    keyPressed(juce::KeyPress(juce::KeyPress::rightKey,unmodified,0));
    const bool right=bank==1;
    userPresets.resize(oldCount);
    bank=oldBank;mainAdlibs=oldMainAdlibs;
    scroll=oldScroll;hoveredCard=oldHover;keyboardCard=oldFocus;
    search.setText(oldSearch,juce::dontSendNotification);
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    for(int i=0;i<genreFilter.getNumItems();++i)
        if(genreFilter.getItemText(i)==oldFilter)
            genreFilter.setSelectedId(genreFilter.getItemId(i),juce::dontSendNotification);
    showDetails(oldDetails);
    repaint();
    return visible && firstDown && down && end && pageUp && home && left && right;
}
bool PresetBrowser::testBrowserLayoutForTest() {
    const bool oldDetails=detailsOpen;
    const auto validate=[this](bool expanded){
        showDetails(expanded);
        if(getWidth()<620 || getHeight()<530)return false;
        const auto bounds=getLocalBounds();
        if(!bounds.contains(search.getBounds()) ||
           !bounds.contains(clearSearchButton.getBounds()) ||
           search.getBounds().intersects(clearSearchButton.getBounds()) ||
           !bounds.contains(sortPicker.getBounds()) ||
           !bounds.contains(detailsButton.getBounds()) ||
           sortPicker.isVisible()==expanded)return false;
        if(artistEditor.isVisible()!=expanded ||
           artistButton.isVisible()!=expanded ||
           genrePicker.isVisible()!=expanded ||
           genreButton.isVisible()!=expanded ||
           artworkButton.isVisible()!=expanded ||
           categoryEditor.isVisible()!=expanded ||
           addCategoryButton.isVisible()!=expanded ||
           genreFilter.isVisible()==expanded)return false;
        const std::array<const juce::Component*,11> widgets{{
            &detailsButton,&genrePicker,&genreButton,&artistEditor,
            &artistButton,&importButton,&exportButton,&artworkButton,
            &categoryEditor,&addCategoryButton,&genreFilter}};
        for(size_t i=0;i<widgets.size();++i){
            if(!widgets[i]->isVisible())continue;
            if(!bounds.contains(widgets[i]->getBounds()))return false;
            for(size_t j=i+1;j<widgets.size();++j)
                if(widgets[j]->isVisible() &&
                   widgets[i]->getBounds().intersects(widgets[j]->getBounds()))
                    return false;
        }
        if(!bounds.contains(adlibsBounds()))return false;
        for(int i=0;i<banks.size();++i) {
            const auto category=bankBounds(i);
            if(!bounds.contains(category))return false;
            if(category.intersects(adlibsBounds()))return false;
            if(!expanded) {
                for(const auto* widget:widgets)
                    if(widget->isVisible() &&
                       category.intersects(widget->getBounds()))return false;
                for(int j=i+1;j<banks.size();++j)
                    if(category.intersects(bankBounds(j)))return false;
            }
        }
        return true;
    };
    const bool library=validate(false),details=validate(true);
    showDetails(oldDetails);
    return library && details && singlePreviewName(deuceIndex)=="DEUCE" &&
           singlePreviewName(alchemistIndex)=="ALCHEMIST";
}
bool PresetBrowser::testMainVocalsPresetFilterForTest() {
    const int oldBank=bank,oldScroll=scroll;
    const bool oldMainAdlibs=mainAdlibs;
    mainAdlibs=false;
    const auto oldQuery=search.getText();
    const auto oldFilter=genreFilter.getText();
    const auto oldArtistKeys=artistKeys,oldArtists=artistNames;
    const auto oldGenreKeys=genreKeys,oldGenres=genreNames;
    const auto oldFavorites=favoriteKeys,oldCustom=customCategories;
    search.setText({},juce::dontSendNotification);
    genreFilter.setSelectedId(1,juce::dontSendNotification);

    bank=0;
    const auto main=filtered();
    const bool approvedOnly=main.size()==2 &&
        main[0]==alchemistIndex && main[1]==deuceIndex;
    mainAdlibs=true;
    const auto adlibs=filtered();
    int expectedAdlibs=0;
    for(int i=0;i<alchemistIndex;++i)
        if(juce::String(skellpreset::factoryPresets[(size_t)i].bank)
              .equalsIgnoreCase("Adlibs"))++expectedAdlibs;
    // Test the factory subset independently: a real user's library may
    // already contain saved Ad-libs presets, which belong in this submenu.
    // Never require that user's valid saved presets to be absent.
    int factoryEntries=0;
    bool validSubmenu=true;
    for(const int entry:adlibs) {
        if(entry>=0) {
            ++factoryEntries;
            validSubmenu=validSubmenu && entry<alchemistIndex &&
                juce::String(skellpreset::factoryPresets[(size_t)entry].bank)
                    .equalsIgnoreCase("Adlibs");
        } else {
            validSubmenu=validSubmenu &&
                genreForUser(-entry-1).equalsIgnoreCase("Ad-libs");
        }
    }
    const bool factorySubmenu=expectedAdlibs>0 &&
        factoryEntries==expectedAdlibs && validSubmenu;
    mainAdlibs=false;
    bank=1;
    const auto personal=filtered();
    const bool personalOnly=std::all_of(personal.begin(),personal.end(),
        [](int entry){return entry<0;});
    bank=2;
    const auto artists=filtered();
    const bool artistsOnly=std::all_of(artists.begin(),artists.end(),
        [this](int entry){
            return entry<0 && artistForUser(-entry-1).isNotEmpty();
        });
    bank=3;
    const auto favorites=filtered();
    const bool favoritesOnly=std::all_of(favorites.begin(),favorites.end(),
        [this](int entry){
            if(!isFavorite(entry))return false;
            return entry<0 || entry==alchemistIndex || entry==deuceIndex ||
                (entry>=0 && entry<alchemistIndex &&
                 juce::String(skellpreset::factoryPresets[(size_t)entry].bank)
                     .equalsIgnoreCase("Adlibs"));
        });

    // A saved user preset may still have an Ad-libs genre tag. Removing
    // the extra FACTORY browser page must not delete its user metadata.
    const int index=(int)userPresets.size();
    userPresets.push_back(
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("__skell_browser_retired_adlibs_probe__.skellpreset"));
    const int entry=-index-1;
    const auto key=favoriteKey(entry);
    artistKeys.add(key);artistNames.add("__skell_artist_probe__");
    genreKeys.add(key);genreNames.add("Ad-libs");
    favoriteKeys.add(key);
    bank=1;search.setText("__skell_browser_retired_adlibs_probe__",juce::dontSendNotification);
    const auto personalHits=filtered();
    const bool personalPreserved=std::find(personalHits.begin(),
        personalHits.end(),entry)!=personalHits.end();
    bank=2;search.setText("artist:__skell_artist_probe__",juce::dontSendNotification);
    const auto artistHits=filtered();
    const bool artistPreserved=std::find(artistHits.begin(),
        artistHits.end(),entry)!=artistHits.end();
    bank=3;search.setText("fav:__skell_browser_retired_adlibs_probe__",juce::dontSendNotification);
    const auto favoriteHits=filtered();
    const bool favoritePreserved=std::find(favoriteHits.begin(),
        favoriteHits.end(),entry)!=favoriteHits.end();
    bank=0;mainAdlibs=true;
    search.setText({},juce::dontSendNotification);
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    const auto tagged=filtered();
    const bool taggedPreserved=std::find(tagged.begin(),tagged.end(),entry)
        !=tagged.end();
    mainAdlibs=false;bank=1;
    // The editable personal tag remains in Edit Details, but Adlibs
    // is no longer a genre-filter dropdown duplicate.
    bool tagEditable=false,filterDuplicate=false;
    for(int i=0;i<genrePicker.getNumItems();++i)
        tagEditable=tagEditable ||
            genrePicker.getItemText(i).equalsIgnoreCase("Ad-libs");
    for(int i=0;i<genreFilter.getNumItems();++i)
        filterDuplicate=filterDuplicate ||
            genreFilter.getItemText(i).equalsIgnoreCase("Ad-libs");

    bool knownGenres=true;
    for(const auto& name:juce::StringArray{"Rage","Indie","R&B"}) {
        genreNames.set(genreNames.size()-1,name);
        int id=1;
        for(int i=0;i<genreFilter.getNumItems();++i)
            if(genreFilter.getItemText(i)==name)
                id=genreFilter.getItemId(i);
        genreFilter.setSelectedId(id,juce::dontSendNotification);
        const auto results=filtered();
        knownGenres=knownGenres && id>1 &&
            std::find(results.begin(),results.end(),entry)!=results.end();
    }
    customCategories.add("__skell_custom_probe__");
    refreshGenreChoices();
    genreNames.set(genreNames.size()-1,"__skell_custom_probe__");
    int customId=1;
    for(int i=0;i<genreFilter.getNumItems();++i)
        if(genreFilter.getItemText(i)=="__skell_custom_probe__")
            customId=genreFilter.getItemId(i);
    genreFilter.setSelectedId(customId,juce::dontSendNotification);
    const auto customHits=filtered();
    const bool customPreserved=customId>1 &&
        std::find(customHits.begin(),customHits.end(),entry)!=customHits.end();
    genreNames.set(genreNames.size()-1,juce::String{});
    genreFilter.setSelectedId(2,juce::dontSendNotification);
    const auto untagged=filtered();
    const bool untaggedPreserved=std::find(untagged.begin(),
        untagged.end(),entry)!=untagged.end();

    userPresets.pop_back();
    artistKeys=oldArtistKeys;artistNames=oldArtists;
    genreKeys=oldGenreKeys;genreNames=oldGenres;
    favoriteKeys=oldFavorites;customCategories=oldCustom;
    refreshGenreChoices();
    for(int i=0;i<genreFilter.getNumItems();++i)
        if(genreFilter.getItemText(i)==oldFilter)
            genreFilter.setSelectedId(genreFilter.getItemId(i),juce::dontSendNotification);
    bank=oldBank;mainAdlibs=oldMainAdlibs;scroll=oldScroll;
    search.setText(oldQuery,juce::dontSendNotification);
    categoryCountsDirty=true;
    return banks.size()==4 && approvedOnly && factorySubmenu &&
        personalOnly && artistsOnly && favoritesOnly && personalPreserved &&
        artistPreserved && favoritePreserved && taggedPreserved &&
        tagEditable && !filterDuplicate &&
        knownGenres && customPreserved && untaggedPreserved;
}
std::vector<int> PresetBrowser::filtered() const {
    std::vector<int> list;
    const auto q=search.getText().toLowerCase().trim();
    const bool favoritesOnly=bank==3 || q.startsWith("fav:");
    const bool genreActive=genreFilter.getSelectedId()>1;
    const auto selectedGenre=genreFilter.getText();

    if((bank==0 && !mainAdlibs || bank==3) && (!genreActive || bank==0)) {
        // Only Alchemist and Deuce appear in the Main lead-vocal list.
        for(const int i:{alchemistIndex,deuceIndex}) {
            if(i<0 || i>=(int)skellpreset::factoryPresets.size())continue;
            const auto& item=skellpreset::factoryPresets[(size_t)i];
            const juce::String name(item.name),b(item.bank),desc(item.description);
            bool found=q.isEmpty() || name.containsIgnoreCase(q) ||
                       b.containsIgnoreCase(q) || desc.containsIgnoreCase(q);
            if(q.startsWith("bank:"))
                found=b.containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("tag:"))
                found=desc.containsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("author:"))
                found=juce::String("skell audio").containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("artist:") || q.startsWith("genre:") ||
               q.startsWith("match:"))found=false;
            if(q.startsWith("fav:")){
                const auto term=q.fromFirstOccurrenceOf(":",false,false).trim();
                found=term.isEmpty() || name.containsIgnoreCase(term);
            }
            if(favoritesOnly && !isFavorite(i))found=false;
            if(found)list.push_back(i);
        }
    }
    // A direct, indented ADLIBS section lives UNDER Main Vocals. Factory
    // program indices remain exactly as released; no preset file changes.
    // Favorites can also display explicitly starred factory Adlibs.
    if((bank==0 && mainAdlibs) || (bank==3 && !genreActive)){
        for(int i=0;i<alchemistIndex;++i){
            const auto& item=skellpreset::factoryPresets[(size_t)i];
            if(!juce::String(item.bank).equalsIgnoreCase("Adlibs"))continue;
            const juce::String name(item.name),description(item.description);
            bool found=q.isEmpty() || name.containsIgnoreCase(q) ||
                description.containsIgnoreCase(q) ||
                juce::String("Adlibs").containsIgnoreCase(q);
            if(q.startsWith("bank:"))
                found=juce::String("Adlibs").containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("artist:") || q.startsWith("genre:") ||
               q.startsWith("author:") || q.startsWith("match:"))found=false;
            if(q.startsWith("fav:")){
                const auto term=q.fromFirstOccurrenceOf(":",false,false).trim();
                found=term.isEmpty() || name.containsIgnoreCase(term);
            }
            if(favoritesOnly && !isFavorite(i))found=false;
            if(found)list.push_back(i);
        }
    }
    // Existing personal sounds tagged Ad-libs also appear directly beneath
    // Main Vocals. The original user tag is read-only here and remains
    // editable under Edit Details > Category, not via a genre filter.
    if(bank==0 && mainAdlibs){
        for(int i=0;i<(int)userPresets.size();++i){
            if(!genreForUser(i).equalsIgnoreCase("Ad-libs"))continue;
            const auto label=userPresets[(size_t)i].getFileNameWithoutExtension();
            const auto artist=artistForUser(i);
            bool found=q.isEmpty() || label.containsIgnoreCase(q) ||
                artist.containsIgnoreCase(q) ||
                juce::String("Adlibs").containsIgnoreCase(q);
            if(q.startsWith("artist:"))
                found=artist.containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("bank:"))
                found=juce::String("Adlibs").containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("genre:"))
                found=juce::String("Ad-libs").containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("fav:") || q.startsWith("tag:") ||
               q.startsWith("match:"))found=false;
            if(found)list.push_back(-i-1);
        }
    }
    if(bank>=1) {
        for(int i=0;i<(int)userPresets.size();++i) {
            const auto label=userPresets[(size_t)i].getFileNameWithoutExtension();
            const auto artist=artistForUser(i);
            const auto genre=genreForUser(i);
            bool found=q.isEmpty() || label.containsIgnoreCase(q) ||
                       artist.containsIgnoreCase(q) || genre.containsIgnoreCase(q) ||
                       (bank==1 && juce::String("My Presets").containsIgnoreCase(q));
            if(q.startsWith("artist:"))
                found=artist.isNotEmpty() && artist.containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("genre:"))
                found=genre.isNotEmpty() && genre.containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("bank:"))
                found=bank==1 && juce::String("My Presets").containsIgnoreCase(
                    q.fromFirstOccurrenceOf(":",false,false).trim());
            if(q.startsWith("fav:")) {
                const auto term=q.fromFirstOccurrenceOf(":",false,false).trim();
                found=term.isEmpty() || label.containsIgnoreCase(term) ||
                      artist.containsIgnoreCase(term);
            }
            if(q.startsWith("author:") || q.startsWith("match:") ||
               q.startsWith("tag:"))found=false;
            if(bank==2 && artist.isEmpty())found=false;
            if(bank>=1 && genreActive &&
               (genreFilter.getSelectedId()==2
                    ?genre.isNotEmpty():!genre.equalsIgnoreCase(selectedGenre)))
                found=false;
            if(favoritesOnly && !isFavorite(-i-1))found=false;
            if(found)list.push_back(-i-1);
        }
        if(bank==2)
            std::stable_sort(list.begin(),list.end(),[this](int a,int b){
                return artistForUser(-a-1).compareIgnoreCase(artistForUser(-b-1))<0;
            });
    }
    // Display sorting changes only this temporary list. The original
    // files, factory program order, APVTS IDs and saved patch are untouched.
    if(bank==1 || bank==3){
        const int mode=sortPicker.getSelectedId();
        const auto label=[this](int entry){
            return entry>=0
                ?juce::String(skellpreset::factoryPresets[(size_t)entry].name)
                :userPresets[(size_t)(-entry-1)].getFileNameWithoutExtension();
        };
        std::stable_sort(list.begin(),list.end(),
            [this,mode,&label](int a,int b){
                if(mode==3){
                    const bool favA=isFavorite(a),favB=isFavorite(b);
                    if(favA!=favB)return favA;
                }
                const int difference=label(a).compareIgnoreCase(label(b));
                return mode==2?difference>0:difference<0;
            });
    }
    return list;
}
void PresetBrowser::refreshArtwork() {
    artworkButton.setEnabled(!chooserActive && canEditCover());
    artistButton.setEnabled(!chooserActive && activeUserPreset.existsAsFile());
    artistEditor.setEnabled(!chooserActive && activeUserPreset.existsAsFile());
    genreButton.setEnabled(!chooserActive && activeUserPreset.existsAsFile());
    genrePicker.setEnabled(!chooserActive && activeUserPreset.existsAsFile());
    juce::String artist;
    for(int i=0;i<(int)userPresets.size();++i)
        if(userPresets[(size_t)i]==activeUserPreset) {artist=artistForUser(i);break;}
    artistEditor.setText(artist,juce::dontSendNotification);
    juce::String genre;
    for(int i=0;i<(int)userPresets.size();++i)
        if(userPresets[(size_t)i]==activeUserPreset) {genre=genreForUser(i);break;}
    int genreId=1;
    for(int i=0;i<genrePicker.getNumItems();++i)
        if(genrePicker.getItemText(i).equalsIgnoreCase(genre))
            genreId=genrePicker.getItemId(i);
    genrePicker.setSelectedId(genreId,juce::dontSendNotification);
    // 128x96 thumbnails are only for cards; never scale them to wallpaper.
    selectedUserBackdrop={};
    if(activeUserPreset.existsAsFile()) {
        auto candidate=activeUserPreset.withFileExtension(".png");
        if(!candidate.existsAsFile())candidate=activeUserPreset.withFileExtension(".jpg");
        if(candidate.existsAsFile() && candidate.getSize()>0 &&
            candidate.getSize()<=8*1024*1024) {
            auto img=juce::ImageFileFormat::loadFrom(candidate);
            if(img.isValid() && img.getWidth()<=4096 && img.getHeight()<=4096) {
                const float scale=std::min({1.f,1600.f/(float)img.getWidth(),
                                                1000.f/(float)img.getHeight()});
                selectedUserBackdrop=img.rescaled(
                    juce::jmax(1,juce::roundToInt(img.getWidth()*scale)),
                    juce::jmax(1,juce::roundToInt(img.getHeight()*scale)),
                    juce::Graphics::highResamplingQuality);
            }
        }
    }
    repaint();
}
void PresetBrowser::chooseArtwork() {
    if(chooserActive || !canEditCover())return;
    setChooserActive(true);
    fileChooser=std::make_unique<juce::FileChooser>(
        "Choose artwork for "+(activeUserPreset.existsAsFile()?activeUserPreset.getFileNameWithoutExtension():juce::String("Alchemist")),
        juce::File{}, "*.png;*.jpg;*.jpeg");
    juce::Component::SafePointer<PresetBrowser> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return;
            safe->setChooserActive(false);
            const auto file=chooser.getResult();
            if(!file.existsAsFile())return;
            if(file.getSize()<=0 || file.getSize()>8*1024*1024){
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Artwork too large","Use a PNG/JPG image smaller than 8 MB.");return;
            }
            auto image=juce::ImageFileFormat::loadFrom(file);
            if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096){
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Invalid artwork","Choose a valid image up to 4096 x 4096.");return;
            }
            const auto destination=safe->activeArtworkDestination();
            if(destination==juce::File{} || destination.getParentDirectory().createDirectory().failed())return;
            // Write-then-replace: a failed save must not destroy the previous cover.
            juce::TemporaryFile staging(destination);
            juce::PNGImageFormat png;
            // Preserve the source image's aspect ratio in its saved cover.
            // The browser builds its own lightweight 128x96 thumbnail.
            // Never save only a 240x180 thumbnail as full-plugin wallpaper.
            const float scale=std::min({1.f,1600.f/(float)image.getWidth(),
                                            1000.f/(float)image.getHeight()});
            auto coverImage=image.rescaled(
                juce::jmax(1,juce::roundToInt(image.getWidth()*scale)),
                juce::jmax(1,juce::roundToInt(image.getHeight()*scale)),
                juce::Graphics::highResamplingQuality);
            bool written=false;
            {
                juce::FileOutputStream output(staging.getFile());
                if(output.openedOk()) {
                    written=png.writeImageToStream(coverImage,output);
                    output.flush();
                }
            }
            if(!written || !staging.overwriteTargetFileWithTemporary()) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Cannot save artwork","Your previous cover is unchanged. Check library permissions.");return;
            }
            safe->scanLibrary();
            safe->refreshArtwork();
            if(safe->onCoverChanged)safe->onCoverChanged();
        });
}
void PresetBrowser::refreshCategoryCounts(){
    int artists=0,favorites=0;
    adlibsCount=0;
    if(isFavorite(alchemistIndex))++favorites;
    if(isFavorite(deuceIndex))++favorites;
    for(int i=0;i<alchemistIndex;++i){
        if(!juce::String(skellpreset::factoryPresets[(size_t)i].bank)
               .equalsIgnoreCase("Adlibs"))continue;
        ++adlibsCount;
        if(isFavorite(i))++favorites;
    }
    for(int i=0;i<(int)userPresets.size();++i){
        if(artistForUser(i).isNotEmpty())++artists;
        if(genreForUser(i).equalsIgnoreCase("Ad-libs"))++adlibsCount;
        if(isFavorite(-i-1))++favorites;
    }
    categoryCounts={{2,(int)userPresets.size(),artists,favorites}};
    categoryCountsDirty=false;
}
bool PresetBrowser::testCategoryCountsForTest(){
    refreshCategoryCounts();
    const int size=(int)userPresets.size();
    int factoryAdlibs=0;
    for(int i=0;i<alchemistIndex;++i)
        if(juce::String(skellpreset::factoryPresets[(size_t)i].bank)
               .equalsIgnoreCase("Adlibs"))++factoryAdlibs;
    return !categoryCountsDirty && categoryCounts[0]==2 &&
        categoryCounts[1]==size &&
        categoryCounts[2]>=0 && categoryCounts[2]<=size &&
        categoryCounts[3]>=0 && categoryCounts[3]<=size+2+factoryAdlibs &&
        adlibsCount>=factoryAdlibs &&
        adlibsCount<=factoryAdlibs+size;
}
void PresetBrowser::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    g.setGradientFill(juce::ColourGradient(
        Theme::panelAlt.withMultipliedBrightness(1.09f),0.f,0.f,
        Theme::panel,0.f,(float)getHeight(),false));
    g.fillRoundedRectangle(getLocalBounds().toFloat(),10.f);
    g.setColour(Theme::stroke);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),10.f,1.2f);
    g.setColour(Theme::green.withAlpha(.82f));g.fillRoundedRectangle(juce::Rectangle<float>(18.f,67.f,3.f,23.f),1.5f);
    Theme::text(g,detailsOpen?"EDIT DETAILS":"LIBRARY",
                {30,73,154,20},12,Theme::white,true);
    const auto entries=filtered();
    // Cache library counts on the UI thread: no O(N × sidecar tags)
    // recount on every hover repaint or every mouse movement.
    if(categoryCountsDirty)refreshCategoryCounts();
    Theme::text(g,juce::String((int)entries.size())+
        (entries.size()==1?" SOUND":" SOUNDS"),{207,73,246,20},12,Theme::white,true);
    if(detailsOpen)
        Theme::text(g,"ESC TO RETURN",{getWidth()-175,75,151,20},
            10,Theme::muted,false,juce::Justification::centredRight);
    if(!detailsOpen) {
        Theme::text(g,"CATEGORIES",{19,312,166,16},10.2f,Theme::muted,true);
        for(int i=0;i<banks.size();++i){
            const auto b=bankBounds(i).toFloat();
            const bool selected=i==bank && (i!=0 || !mainAdlibs);
            const bool hovered=i==hoveredBank;
            g.setColour(selected?Theme::green.withAlpha(.14f):
                        hovered?Theme::panelAlt.brighter(.17f):Theme::panel);
            g.fillRoundedRectangle(b,6.f);
            g.setColour(selected?Theme::green.withAlpha(.78f):
                        hovered?Theme::muted.withAlpha(.68f):Theme::stroke.withAlpha(.75f));
            g.drawRoundedRectangle(b.reduced(.5f),6.f,selected?1.25f:1.f);
            Theme::fittedText(g,banks[i],
                {29,(int)b.getY()+5,112,20},12.f,
                selected?Theme::white:Theme::muted,selected,
                juce::Justification::centredLeft);
            Theme::text(g,juce::String(categoryCounts[(size_t)i]),
                {149,(int)b.getY()+6,28,18},10.f,
                selected?Theme::green:Theme::muted,true,
                juce::Justification::centredRight);
        }
        const auto sub=adlibsBounds().toFloat();
        g.setColour(mainAdlibs?Theme::green.withAlpha(.15f):
                   hoveredAdlibs?Theme::panelAlt.brighter(.19f):
                                 Theme::panelAlt.withAlpha(.47f));
        g.fillRoundedRectangle(sub,5.f);
        g.setColour(mainAdlibs?Theme::green.withAlpha(.68f):
                   hoveredAdlibs?Theme::muted.withAlpha(.75f):
                                 Theme::stroke.withAlpha(.45f));
        g.drawRoundedRectangle(sub.reduced(.5f),5.f,.85f);
        g.setColour(Theme::green.withAlpha(mainAdlibs?.85f:.26f));
        g.drawLine(33.f,147.f,33.f,162.f,1.f);
        g.drawLine(33.f,162.f,41.f,162.f,1.f);
        Theme::text(g,"ADLIBS",{46,152,91,19},10.7f,
            mainAdlibs?Theme::white:Theme::muted,mainAdlibs);
        Theme::text(g,juce::String(adlibsCount),{144,153,29,17},
            10.f,mainAdlibs?Theme::green:Theme::muted,true,
            juce::Justification::centredRight);
    } else {
        Theme::text(g,"ARTIST",{19,159,166,17},10.f,Theme::muted,true);
        Theme::text(g,"CATEGORY",{19,264,166,17},10.f,Theme::muted,true);
        Theme::text(g,"CREATE YOUR OWN",{19,335,166,17},10.f,Theme::muted,true);
    }
    // Don't draw clickable library cards beneath Edit Details input fields.
    // Children on that page remain visible and keyboard accessible.
    if(detailsOpen)return;
    // Artist metadata can be assigned to personal presets with the sidebar
    // text field; the actual native preset and host program IDs stay intact.
    for(int i=0;i<6;i++){
        const int itemIndex=i+scroll;
        if(itemIndex>=(int)entries.size())break;
        const int entry=entries[(size_t)itemIndex];
        const bool custom=entry<0;
        const juce::String itemName=custom
            ?userPresets[(size_t)(-entry-1)].getFileNameWithoutExtension()
            :(entry==alchemistIndex?juce::String("Alchemist")
              :(entry==deuceIndex?juce::String("Deuce")
                :juce::String(skellpreset::factoryPresets[(size_t)entry].name)));
        const juce::String userArtist=custom?artistForUser(-entry-1):juce::String{};
        const juce::String userGenre=custom?genreForUser(-entry-1):juce::String{};
        const juce::String itemBank=custom
            ?(userGenre.isNotEmpty()?userGenre.toUpperCase()+
                 (userArtist.isNotEmpty()?"  ·  "+userArtist:juce::String{})
                :(userArtist.isNotEmpty()?"ARTIST: "+userArtist:juce::String("My Presets")))
            :juce::String(skellpreset::factoryPresets[(size_t)entry].bank);
        const int y=110+i*61;
        auto rect=juce::Rectangle<float>(207.f,(float)y,(float)getWidth()-224.f,56.f);
        // Layered smoked-metal row rather than the old flat tile. This
        // stays a single paint pass and does not fetch images from disk.
        const bool rowHover=itemIndex==hoveredCard;
        g.setGradientFill(juce::ColourGradient(
            Theme::panelAlt.brighter(rowHover?.13f:.065f),
            rect.getTopLeft(),
            Theme::panel.darker(rowHover?.04f:.09f),
            rect.getBottomRight(),false));
        g.fillRoundedRectangle(rect,7.f);
        g.setColour(Theme::white.withAlpha(rowHover?.10f:.05f));
        g.drawLine(rect.getX()+84.f,rect.getY()+1.6f,
                   rect.getRight()-13.f,rect.getY()+1.6f,.7f);
        const juce::Image tileArt=custom
            ?userArtwork[(size_t)(-entry-1)]
            :(entry==alchemistIndex?alchemistArtwork:juce::Image{});
        if(tileArt.isValid()) {
            juce::Graphics::ScopedSaveState cardArtwork(g);
            g.reduceClipRegion(rect.toNearestInt());
            g.setOpacity(.13f);
            g.drawImageWithin(tileArt,(int)rect.getX(),(int)rect.getY(),
                (int)rect.getWidth(),(int)rect.getHeight(),
                juce::RectanglePlacement::fillDestination);
        }
        const std::array<juce::Colour,4> accents{{Theme::green,Theme::amber,Theme::pink,Theme::white}};
        const int seed=custom?-entry:entry+1;
        const auto accent=accents[(size_t)artworkPaletteIndexForTest(seed)];
        const auto cover=juce::Rectangle<float>(215.f,(float)y+5.f,67.f,46.f);
        const juce::Image coverArt=custom
            ?userArtwork[(size_t)(-entry-1)]
            :(entry==alchemistIndex?alchemistArtwork:juce::Image{});
        if(coverArt.isValid()) {
            // Full-bleed card artwork; graphics clipping keeps rounded card edges.
            juce::Graphics::ScopedSaveState artworkSave(g);
            g.reduceClipRegion(cover.toNearestInt());
            g.drawImageWithin(coverArt,(int)cover.getX(),(int)cover.getY(),
                              (int)cover.getWidth(),(int)cover.getHeight(),
                              juce::RectanglePlacement::centred);
        } else paintCover(g,cover,seed,accent);
        g.setColour(Theme::stroke.withAlpha(.8f));g.drawRoundedRectangle(cover,5.f,1.f);
        const bool loaded=custom
            ?activeUserPreset==userPresets[(size_t)(-entry-1)]
            :entry==processor.loadedFactoryPresetIndex();
        const bool hovered=itemIndex==hoveredCard;
        const bool focused=itemIndex==keyboardCard;
        g.setColour(loaded?Theme::green:
                    focused?Theme::blue.withAlpha(.92f):
                    hovered?Theme::green.withAlpha(.68f):Theme::stroke);
        g.drawRoundedRectangle(rect.reduced(.5f),7.f,
                               loaded?1.6f:focused?1.6f:1.f);
        if(focused){
            // Stable focus indicator without obscuring the artwork/name.
            g.setColour(Theme::blue.withAlpha(.90f));
            g.fillRoundedRectangle(rect.getX()+2.f,rect.getY()+10.f,2.3f,
                                   rect.getHeight()-20.f,1.f);
        }
        if(loaded) {
            g.setColour(Theme::green.withAlpha(.84f));
            g.fillRoundedRectangle(rect.getX(),rect.getY()+9.f,2.5f,
                                   rect.getHeight()-18.f,1.25f);
        }
        Theme::fittedText(g,itemName,{294,y+7,260,24},15.f,Theme::white,true,
                          juce::Justification::centredLeft);
        const auto bankColour=custom?Theme::green:Theme::blue;
        Theme::fittedText(g,itemBank,{294,y+33,210,16},11.f,bankColour,true,
                          juce::Justification::centredLeft);
        // Independent star target: selecting a favorite must not audition the
        // preset or alter any audio/host automation IDs.
        const bool favorite=isFavorite(entry);
        const float starX=(float)getWidth()-57.f,starY=(float)y+28.f;
        juce::Path star;
        for(int point=0;point<10;++point) {
            const float angle=-juce::MathConstants<float>::halfPi
                +point*juce::MathConstants<float>::pi/5.f;
            const float radius=(point%2==0)?9.2f:4.1f;
            const float x=starX+std::cos(angle)*radius;
            const float yy=starY+std::sin(angle)*radius;
            if(point==0)star.startNewSubPath(x,yy);
            else star.lineTo(x,yy);
        }
        star.closeSubPath();
        if(hoveredStar==itemIndex){
            g.setColour(Theme::green.withAlpha(.16f));
            g.fillEllipse(starX-15.f,starY-15.f,30.f,30.f);
        }
        g.setColour(favorite?Theme::green:
            hoveredStar==itemIndex?Theme::green.withAlpha(.9f):
            Theme::muted.withAlpha(.8f));
        if(favorite)g.fillPath(star);
        else g.strokePath(star,juce::PathStrokeType(1.3f));
        if(loaded){g.setColour(Theme::green);g.fillEllipse((float)getWidth()-29.f,(float)y+25.f,6.f,6.f);}
    }
    if(entries.size()==1 && search.getText().trim().isEmpty()) {
        // A single active factory sound shouldn't produce an unexplained
        // empty browser. Preview the editable cover instead; no fake
        // content, duplicate presets or heavy image loads in paint().
        const int entry=entries[0];
        const bool custom=entry<0;
        const auto art=custom?userArtwork[(size_t)(-entry-1)]
            :(entry==alchemistIndex?alchemistArtwork:juce::Image{});
        const auto frame=juce::Rectangle<float>(229.f,277.f,177.f,167.f);
        g.setColour(Theme::bg);g.fillRoundedRectangle(frame,8.f);
        if(art.isValid()) {
            juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(frame.toNearestInt());
            g.drawImageWithin(art,(int)frame.getX(),(int)frame.getY(),
                (int)frame.getWidth(),(int)frame.getHeight(),
                juce::RectanglePlacement::centred);
        } else paintCover(g,frame,custom?-entry:entry+1,
                          entry==deuceIndex?Theme::blue:Theme::green);
        g.setColour(Theme::green.withAlpha(.40f));
        g.drawRoundedRectangle(frame.reduced(.7f),7.6f,1.15f);
        Theme::text(g,"COVER PREVIEW",{424,282,191,20},10,Theme::green,true);
        Theme::fittedText(g,singlePreviewName(entry),
            {424,308,193,25},16.f,Theme::white,true,
            juce::Justification::centredLeft);
        if(entry==deuceIndex) {
            // Deuce has no editable factory cover. Never suggest that the
            // SET COVER control would modify this factory sound.
            Theme::text(g,"Deuce factory artwork",{424,347,190,18},12,Theme::muted);
            Theme::text(g,"is preserved.",{424,367,190,18},12,Theme::muted);
            Theme::text(g,"Save a personal version",{424,407,196,18},11,Theme::muted);
            Theme::text(g,"to choose new artwork.",{424,424,196,18},11,Theme::muted);
        } else {
            Theme::text(g,"Select the sound, then",{424,347,190,18},12,Theme::muted);
            Theme::text(g,"use SET COVER.",{424,367,190,18},12,Theme::muted);
            Theme::text(g,"Your saved artwork",{424,407,196,18},11,Theme::muted);
            Theme::text(g,"can become the backdrop.",{424,424,196,18},11,Theme::muted);
        }
    }
    if(entries.empty()) {
        const bool searched=search.getText().trim().isNotEmpty();
        const bool filteredGenre=genreFilter.getSelectedId()>1;
        const juce::String title=searched?"NO SEARCH MATCHES":
            filteredGenre?(genreFilter.getText()=="Untagged"
                ?"NO UNTAGGED SOUNDS":
                 "NO "+genreFilter.getText().toUpperCase()+" SOUNDS"):
            bank==3?"NO FAVORITES YET":
            bank==2?"NO ARTIST PRESETS YET":"NO MATCHING PRESETS";
        Theme::fittedText(g,title,{246,222,355,40},
                          15.f,Theme::muted,true);
        if(searched || filteredGenre)
            Theme::text(g,"Clear the search or change the category.",
                {246,255,380,23},11,Theme::muted);
        else if(bank==3)
            Theme::text(g,"Star a sound to save it here.",
                {246,255,385,23},11,Theme::muted);
        else if(bank==2)
            Theme::text(g,"Set an artist in Edit Details to group saved sounds.",
                {246,255,385,23},11,Theme::muted);
    }
    if((int)entries.size()>6){
        const float railTop=110.f,railHeight=361.f;
        const float thumbHeight=juce::jmax(30.f,railHeight*6.f/(float)entries.size());
        const float travel=railHeight-thumbHeight;
        const float thumbY=railTop+travel*(float)scroll/(float)(entries.size()-6);
        g.setColour(Theme::stroke.withAlpha(.6f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)getWidth()-12.f,railTop,3.f,railHeight),1.5f);
        g.setColour(Theme::green.withAlpha(.85f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)getWidth()-12.f,thumbY,3.f,thumbHeight),1.5f);
        Theme::fittedText(g,"SHOWING "+juce::String(scroll+1)+" - "+
            juce::String(std::min(scroll+6,(int)entries.size()))+
            " OF "+juce::String((int)entries.size())+"  /  WHEEL TO SCROLL",
            {260,getHeight()-20,310,16},10.f,Theme::muted);
    }
}
void PresetBrowser::loadFactoryFromBrowser(int index) {
    if(index<0 || index>=(int)skellpreset::factoryPresets.size())return;
    if(onBeforeLoad)onBeforeLoad();
    activeUserPreset=juce::File{};
    const int previousMode=(int)processor.state.getRawParameterValue("mode")->load();
    processor.loadFactoryPreset(index);
    refreshArtwork(); // Cover follows the selected factory sound.
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
    if(onBeforeLoad)onBeforeLoad();
    const auto result=processor.importPreset(file);
    if(result.failed()) {
        if(onLoadFailed)onLoadFailed();
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
    if(!detailsOpen && e.x>=17 && e.x<185){
        if(adlibsBounds().contains(e.getPosition())){
            chooseAdlibs();grabKeyboardFocus();return;
        }
        for(int index=0;index<banks.size();++index)
            if(bankBounds(index).contains(e.getPosition())) {
                chooseBank(index);grabKeyboardFocus();return;
            }
    }
    if(!e.mods.isPopupMenu() &&
       presetCardHitForTest(detailsOpen,e.x,e.y,getWidth())){
        const auto entries=filtered();const int row=(e.y-110)/61;
        if(row>=0&&row<6 && scroll+row<(int)entries.size()){
            keyboardCard=scroll+row;grabKeyboardFocus();
            const int entry=entries[(size_t)(scroll+row)];
            if(e.x>=getWidth()-77 && e.x<=getWidth()-38)
                toggleFavorite(entry);
            else
                auditionEntry(entry);
            // Star clicks never change the loaded preset or DSP settings.
        }
    }
}
void PresetBrowser::mouseMove(const juce::MouseEvent& e){
    int nextBank=-1;
    bool nextAdlibs=false;
    if(!detailsOpen && e.x>=17 && e.x<185){
        nextAdlibs=adlibsBounds().contains(e.getPosition());
        if(!nextAdlibs)
            for(int i=0;i<banks.size();++i)
                if(bankBounds(i).contains(e.getPosition())){nextBank=i;break;}
    }
    const int candidate=presetCardHitForTest(detailsOpen,e.x,e.y,getWidth())
        ? (e.y-110)/61+scroll : -1;
    const bool onStar=e.x>=getWidth()-77 && e.x<=getWidth()-38;
    // Avoid filtering all the cards for every pixel of mouse travel.
    const bool stableCard=candidate>=0 && candidate==hoveredCard &&
        (onStar?hoveredStar==candidate:hoveredStar<0);
    const int next=stableCard?hoveredCard:
        candidate>=0&&candidate<(int)filtered().size()?candidate:-1;
    const int nextStar=onStar?next:-1;
    if(next==hoveredCard && nextStar==hoveredStar &&
       nextBank==hoveredBank && nextAdlibs==hoveredAdlibs)return;
    hoveredCard=next;hoveredStar=nextStar;
    hoveredBank=nextBank;hoveredAdlibs=nextAdlibs;
    repaint();
    setTooltip(nextStar>=0?"Toggle favorite without changing the loaded sound.":
               next>=0?"Load this preset; your current sound can be undone.":
               nextAdlibs?"Browse native and saved Adlibs.":
               nextBank>=0?"Browse this library category.":
               "Click a category, choose a genre or search for a saved sound.");
    setMouseCursor(juce::MouseCursor(next>=0 || nextBank>=0 || nextAdlibs?
        juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor));
}
void PresetBrowser::mouseExit(const juce::MouseEvent&){
    if(hoveredCard>=0 || hoveredStar>=0 || hoveredBank>=0 || hoveredAdlibs){
        hoveredCard=-1;hoveredStar=-1;hoveredBank=-1;hoveredAdlibs=false;
        repaint();
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}
void PresetBrowser::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    if(detailsOpen || e.x<197 || e.y<102 || e.y>486 ||
       !std::isfinite(wheel.deltaY) || wheel.deltaY==0.f)return;
    const int maxScroll=std::max(0,(int)filtered().size()-6);
    // One row per traditional wheel tick, so categories do not jump past
    // results; trackpads use small accumulated deltas on the message thread.
    browserWheelCarry=juce::jlimit(-1.f,1.f,
        (browserWheelCarry*wheel.deltaY<0.f?0.f:browserWheelCarry)+wheel.deltaY);
    if(std::abs(browserWheelCarry)<.18f)return;
    const int direction=browserWheelCarry<0.f?1:-1;
    browserWheelCarry=0.f;
    const int next=juce::jlimit(0,maxScroll,scroll+direction);
    if(scroll!=next){
        scroll=next;hoveredCard=-1;hoveredStar=-1;
        // Mouse scrolling invalidates the old keyboard row, which may now
        // be outside the six displayed cards. Enter then auditions the
        // FIRST VISIBLE result rather than an off-screen preset.
        keyboardCard=-1;
        repaint();
    }
}
bool PresetBrowser::keyPressed(const juce::KeyPress& key){
    if(key==juce::KeyPress::escapeKey){
        if(detailsOpen){showDetails(false);return true;}
        setVisible(false);if(onClose)onClose();return true;
    }
    const bool command=key.getModifiers().isCommandDown();
    const int code=key.getKeyCode();
    if(command && (code=='F' || code=='f')){
        search.grabKeyboardFocus();
        search.selectAll();
        return true;
    }
    if(detailsOpen)return false;
    const auto entries=filtered();
    if(code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey){
        const int next=juce::jlimit(0,banks.size()-1,
            bank+(code==juce::KeyPress::leftKey?-1:1));
        chooseBank(next);
        return true;
    }
    if(code==juce::KeyPress::upKey || code==juce::KeyPress::downKey){
        focusCard(keyboardCard<0?0:keyboardCard+
            (code==juce::KeyPress::upKey?-1:1));
        return true;
    }
    if(code==juce::KeyPress::homeKey){
        focusCard(0);return true;
    }
    if(code==juce::KeyPress::endKey){
        focusCard((int)entries.size()-1);return true;
    }
    if(code==juce::KeyPress::pageUpKey ||
       code==juce::KeyPress::pageDownKey){
        const int next=(keyboardCard<0?0:keyboardCard)+
            (code==juce::KeyPress::pageUpKey?-6:6);
        focusCard(next);
        return true;
    }
    if(code==juce::KeyPress::returnKey){
        if(entries.empty())return true;
        const int index=auditionTargetForTest(keyboardCard,scroll,(int)entries.size());
        if(index>=0)auditionEntry(entries[(size_t)index]);
        return true;
    }
    if(code==juce::KeyPress::spaceKey){
        if(keyboardCard>=0 && keyboardCard<(int)entries.size())
            toggleFavorite(entries[(size_t)keyboardCard]);
        return true;
    }
    return false;
}
