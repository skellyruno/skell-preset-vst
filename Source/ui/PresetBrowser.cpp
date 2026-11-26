#include "PresetBrowser.h"
#include "../chain/VocalStarterChains.h"
#include "Theme.h"
#include "PresetCoverArt.h"
using skellpreset::art::paintCover;
#include "../chain/AlchemistNativePreset.h"
#include "../chain/NativeSoundMatchTrials.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <mutex>
#include <utility>

namespace {
// Serialize library copy/name selection across instances in this process.
// Workers may wait; the message thread never takes this mutex.
std::mutex libraryWriteMutex;
constexpr int maxUserLibraryPresets=1000;
// Direct Main Vocals > Adlibs submenu: no fifth top-level library bank.
 // Historical factory IDs and personal genre tags remain unchanged.
const juce::StringArray banks{"Main Presets","My Presets","Artists","Favorites","Genres"};
const juce::StringArray standardGenres{"Rage","Indie","R&B","Trap","Pluggnb","Hyperpop","Pop","Drill","Alternative","Experimental","Ad-libs"};
constexpr int alchemistIndex=skellpreset::alchemist::factoryIndex;
constexpr int deuceIndex=skellpreset::deuce_v25::factoryIndex;
// A browser curation rule only. Never remove/reorder the host program table.
bool visibleBoneAdlib(int index) {
    return index>=0 && index<alchemistIndex &&
        juce::String(skellpreset::factoryPresets[(size_t)index].bank)=="Adlibs" &&
        juce::String(skellpreset::factoryPresets[(size_t)index].name)=="Digital Bones";
}
// One shared geometry model for painting, click hit-testing and UI regression.
// The three genres have a visible MANUAL heading and never overlap controls.
juce::Rectangle<int> bankBounds(int index) noexcept {
    // Visual order: Main, Adlib, Genres, Artists, My Presets, Favorites.
    constexpr int visualRows[]{0,4,3,5,2};
    return {17,109+38*visualRows[index],168,33};
}
juce::Rectangle<int> adlibsBounds() noexcept {return {17,147,168,33};}
juce::String singlePreviewName(int entry) {
    if(entry==alchemistIndex)return "ALCHEMIST";
    if(entry==deuceIndex)return "DEUCE";
    return entry<0?"YOUR PRESET":"FACTORY PRESET";
}
}
PresetBrowser::PresetBrowser(SkellPresetAudioProcessor& p):processor(p) {
    loadUIPreferences();
    addAndMakeVisible(thumbnailView);addAndMakeVisible(listView);
    thumbnailView.setTooltip("Browse artwork cards while keeping the current search and filters");
    listView.setTooltip("Browse compact rows while keeping the current search and filters");
    thumbnailView.onClick=[this]{setListView(false);};
    listView.onClick=[this]{setListView(true);};
    search.setMultiLine(false);
    search.setTextToShowWhenEmpty("Search sounds / artist:name / genre:name",Theme::muted);
    artistEditor.setMultiLine(false);
    artistEditor.setTextToShowWhenEmpty("Artist for selected preset",Theme::muted);
    artistEditor.setTooltip("Type an artist name for your selected personal preset, then press SET ARTIST");
    genrePicker.setTooltip("Assign a category to the selected saved preset, never automatically.");
    genreFilter.setTooltip("Filter your saved presets by genre/category.");
    categoryEditor.setTextToShowWhenEmpty("New category name",Theme::muted);
    categoryEditor.setTooltip("Create your own library category; presets and automation stay unchanged.");
    for(auto* picker:{&artistTierFilter,&artistFilter,&typeFilter,&tierEditor,&typeEditor})addAndMakeVisible(*picker);
    addAndMakeVisible(pairEditor);addAndMakeVisible(metadataButton);addAndMakeVisible(relatedButton);
    addAndMakeVisible(directoryBack);
    const char* tabNames[]{"ALL","MAIN PRESETS","ADLIB PRESETS"};
    for(int i=0;i<3;++i){
        auto& tab=vocalTabs[(size_t)i];tab.setButtonText(tabNames[i]);addAndMakeVisible(tab);
        tab.onClick=[this,i]{typeFilter.setSelectedId(i+1,juce::dontSendNotification);resetResults();};
    }
    directoryBack.setTooltip("Return to the previous artist or genre directory without loading a preset.");
    directoryBack.onClick=[this]{goBackDirectory();grabKeyboardFocus();};
    artistTierFilter.addItem("All artists",1);artistTierFilter.addItem("Underground",2);
    artistTierFilter.addItem("Mainstream",3);artistTierFilter.addItem("Unassigned tier",4);
    artistTierFilter.setSelectedId(1,juce::dontSendNotification);
    typeFilter.addItem("All vocal types",1);typeFilter.addItem("Main presets",2);typeFilter.addItem("Adlib presets",3);
    typeFilter.setSelectedId(1,juce::dontSendNotification);
    tierEditor.addItem("Unassigned tier",1);tierEditor.addItem("Underground",2);tierEditor.addItem("Mainstream",3);
    typeEditor.addItem("Unassigned type",1);typeEditor.addItem("Main preset",2);typeEditor.addItem("Adlib preset",3);
    pairEditor.setMultiLine(false);
    pairEditor.setTextToShowWhenEmpty("Optional shared pairing name",Theme::muted);
    metadataButton.setTooltip("Save manual vocal type, artist tier and pairing without modifying the preset file.");
    relatedButton.setTooltip("Find all saved presets with this pairing name.");
    artistTierFilter.onChange=[this]{refreshArtistChoices();artistFilter.setSelectedId(1,juce::dontSendNotification);resetResults();};
    artistFilter.onChange=[this]{resetResults();};
    typeFilter.onChange=[this]{resetResults();};
    metadataButton.onClick=[this]{
        const int index=activeUserIndex();if(index<0)return;
        const auto pair=pairEditor.getText().trim();
        if(pair.length()>80 || pair.containsAnyOf("\r\n\t"))return;
        const auto key=favoriteKey(-index-1),artist=artistForUser(index);
        const auto beforeKeys=organizationKeys,beforeValues=organizationValues;
        setOrganizationValue("type:"+key,typeEditor.getSelectedId()==2?juce::String("main"):
            typeEditor.getSelectedId()==3?juce::String("adlib"):juce::String("unassigned"));
        if(artist.isNotEmpty())setOrganizationValue("tier:"+artist.toLowerCase(),
            tierEditor.getSelectedId()==2?juce::String("Underground"):
            tierEditor.getSelectedId()==3?juce::String("Mainstream"):juce::String{});
        setOrganizationValue("pair:"+key,pair);
        if(!saveOrganization()){
            organizationKeys=beforeKeys;organizationValues=beforeValues;
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Organization not saved","Check your preset library permissions.");
        }
        refreshArtistChoices();refreshOrganizationEditor();resetResults();
    };
    relatedButton.onClick=[this]{
        const int index=activeUserIndex();if(index<0)return;
        const auto pair=pairForUser(index);if(pair.isEmpty())return;
        chooseBank(1);showDetails(false);search.setText("pair:"+pair);
    };
    applyTheme();
    search.onTextChange=[this]{
        scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;browserWheelCarry=0.f;
        clearSearchButton.setEnabled(search.getText().isNotEmpty());
        resized();repaint();
    };
    search.onEscapeKey=[this]{
        if(detailsOpen)showDetails(false);
        else if(!goBackDirectory()){setVisible(false);if(onClose)onClose();}
    };
    // Enter auditions the first filtered result without dismissing the browser.
    search.onReturnKey=[this]{
        if(detailsOpen || chooserActive)return; // never audition while editing metadata
        if(directoryMode()!=DirectoryMode::none){
            activateDirectoryRow(auditionTargetForTest(keyboardCard,scroll,resultCount()));return;
        }
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
    sortPicker.addItem("Recently used",4);
    sortPicker.setSelectedId(1,juce::dontSendNotification);
    sortPicker.setTooltip("Change display order, never rename presets or modify audio.");
    sortPicker.onChange=[this]{
        scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;browserWheelCarry=0.f;repaint();
    };
    addAndMakeVisible(categoryEditor);
    for(auto* button:{&importButton,&exportButton,&saveAsButton,&artworkButton,
                      &artistButton,&genreButton,&detailsButton,&addCategoryButton,
                      &clearSearchButton,&starterButton})
        addAndMakeVisible(*button);
    detailsButton.setTooltip("Manage import, save, artist, genre, pairing and cover artwork.");
    importButton.setTooltip("Open a validated native .skellpreset ZIP archive");
    exportButton.setTooltip("Overwrite the selected personal preset with the active chain. A factory sound saves a new library copy instead.");
    saveAsButton.setTooltip("Save the active chain, slot order, macros and global controls as a new personal preset");
    artworkButton.setTooltip("Choose a cover for Alchemist or any selected saved preset");
    artworkButton.setEnabled(false);
    artistButton.setEnabled(false);
    artistButton.setTooltip("Save an artist label to the selected personal preset. Does not modify its .skellpreset file.");
    clearSearchButton.setTooltip("Clear the search phrase without changing the selected category.");
    clearSearchButton.setEnabled(false);
    clearSearchButton.onClick=[this] {
        search.clear();
        search.grabKeyboardFocus();
        resetResults();
    };
    genreButton.setButtonText("TAG +");
    genreButton.setTooltip("Add this genre to the selected preset. Multiple tags are allowed. Choose Uncategorized to clear all genre assignments.");
    addCategoryButton.setTooltip("Add a new category to the user library; no audio or preset ZIP changes.");
    genreButton.setEnabled(false);
    requestLibraryScan();
    loadFavorites();
    loadArtists();
    loadCustomCategories();
    refreshGenreChoices();
    loadGenres();loadOrganization();refreshArtistChoices();
    refreshArtwork();
    starterButton.setTooltip("Build an original native vocal chain; save a copy to keep it. Does not overwrite factory or personal presets.");
    starterButton.onClick=[this]{showVocalStarters();};
    detailsButton.onClick=[this]{showDetails(!detailsOpen);};
    importButton.onClick=[this] {chooseImport();};
    exportButton.onClick=[this] {saveActivePreset(false);};
    saveAsButton.onClick=[this] {saveActivePreset(true);};
    artworkButton.onClick=[this] {chooseArtwork();};
    artistButton.onClick=[this] {
        if(!activeUserPreset.existsAsFile())return;
        for(int i=0;i<(int)userPresets.size();++i)
            if(userPresets[(size_t)i]==activeUserPreset) {
                if(setArtistForUser(i,artistEditor.getText())) {
                    refreshArtistChoices();refreshOrganizationEditor();resetResults();
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
                if(setGenreForUser(i,label)){refreshOrganizationEditor();resetResults();}
                return;
            }
    };
    genreFilter.onChange=[this]{resetResults();};
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
    for(auto* picker:{&artistTierFilter,&artistFilter,&typeFilter,&tierEditor,&typeEditor}){
        picker->setColour(juce::ComboBox::backgroundColourId,Theme::plot);
        picker->setColour(juce::ComboBox::textColourId,Theme::white);
        picker->setColour(juce::ComboBox::outlineColourId,Theme::stroke);
        picker->setColour(juce::ComboBox::arrowColourId,Theme::green);
    }
    pairEditor.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    pairEditor.setColour(juce::TextEditor::textColourId,Theme::white);
    pairEditor.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    for(auto* button:{&metadataButton,&relatedButton,&directoryBack}){
        button->setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.12f));
        button->setColour(juce::TextButton::textColourOffId,Theme::white);
    }
    categoryEditor.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
    categoryEditor.setColour(juce::TextEditor::textColourId,Theme::white);
    categoryEditor.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
    for(auto* button:{&importButton,&exportButton,&saveAsButton,&artworkButton,
                       &artistButton,&genreButton,&detailsButton,&addCategoryButton,
                       &clearSearchButton,&starterButton}) {
        button->setColour(juce::TextButton::buttonColourId,Theme::panel);
        button->setColour(juce::TextButton::textColourOffId,Theme::white);
    }
    exportButton.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.22f));
    detailsButton.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.12f));
    addCategoryButton.setColour(juce::TextButton::buttonColourId,Theme::green.withAlpha(.12f));
    syncVocalTabs();repaint();
}
juce::File PresetBrowser::libraryDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Skell Audio").getChildFile("Skell-Preset")
        .getChildFile("Presets");
}
PresetBrowser::LibrarySnapshot PresetBrowser::collectLibrary(
    const juce::File& folder,const CoverCache& previous) {
    LibrarySnapshot result;
    if(folder.createDirectory().failed())return result;
    juce::Array<juce::File> files;
    folder.findChildFiles(files,juce::File::findFiles,false,"*.skellpreset");
    for(const auto& item:files)
        if(item.getSize()>0 && item.getSize()<=4*1024*1024)
            result.files.push_back(item);
    std::sort(result.files.begin(),result.files.end(),
        [](const juce::File& a,const juce::File& b) {
            return a.getFileName().compareIgnoreCase(b.getFileName())<0;
        });
    if(result.files.size()>maxUserLibraryPresets)result.files.resize(maxUserLibraryPresets);
    result.thumbnails.resize(result.files.size());
    const auto factoryCover=folder.getChildFile("Covers").getChildFile("Alchemist.png");
    if(factoryCover.existsAsFile() && factoryCover.getSize()>0 &&
       factoryCover.getSize()<=8*1024*1024) {
        auto image=juce::ImageFileFormat::loadFrom(factoryCover);
        if(image.isValid() && image.getWidth()<=4096 && image.getHeight()<=4096)
            result.alchemist=image.rescaled(1200,900,juce::Graphics::highResamplingQuality);
    }
    for(size_t i=0;i<result.files.size();++i) {
        auto cover=result.files[i].withFileExtension(".png");
        if(!cover.existsAsFile())cover=result.files[i].withFileExtension(".jpg");
        if(!cover.existsAsFile() || cover.getSize()<=0 ||
           cover.getSize()>8*1024*1024)continue;
        const auto key=cover.getFullPathName();
        const auto modified=cover.getLastModificationTime().toMilliseconds();
        const auto size=cover.getSize();
        const auto found=previous.find(key);
        if(found!=previous.end() && found->second.modified==modified &&
           found->second.size==size && found->second.thumbnail.isValid()) {
            result.thumbnails[i]=found->second.thumbnail;
            result.covers.emplace(key,found->second);
            continue;
        }
        auto image=juce::ImageFileFormat::loadFrom(cover);
        if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096)continue;
        result.thumbnails[i]=image.rescaled(128,96,juce::Graphics::mediumResamplingQuality);
        result.covers.emplace(key,CoverCacheEntry{modified,size,result.thumbnails[i]});
    }
    result.ok=true;
    return result;
}
void PresetBrowser::applyLibrarySnapshot(LibrarySnapshot snapshot) {
    if(!snapshot.ok)return; // keep the prior view if enumeration cannot start
    categoryCountsDirty=true;
    if(snapshot.bankScanned)embeddedArtistMetadata=std::move(snapshot.bankMetadata);
    userPresets=std::move(snapshot.files);
    userArtwork=std::move(snapshot.thumbnails);
    coverCache=std::move(snapshot.covers);
    alchemistArtwork=std::move(snapshot.alchemist);
}
void PresetBrowser::scanLibrary() {
    applyLibrarySnapshot(collectLibrary(libraryDirectory(),coverCache));
}
void PresetBrowser::requestLibraryScan() {
    if(preferencesReadOnly)return;
    if(saveInFlight || loadInFlight || coverWriteInFlight || libraryScanInFlight) {
        libraryScanQueued=true;
        return;
    }
    libraryScanQueued=false;
    libraryScanInFlight=true;
    const auto revision=libraryRevision;
    const auto folder=libraryDirectory();
    const auto previous=coverCache;
    juce::Component::SafePointer<PresetBrowser> safe(this);
    juce::Thread::launch([safe,revision,folder,previous] {
        // Worker only: populate the private bundled artist presets before the
        // ordinary snapshot. Existing personal metadata remains authoritative.
        skellpreset::artistbank::SyncResult bank;
        {
            std::lock_guard<std::mutex> lock(libraryWriteMutex);
            bank=skellpreset::artistbank::syncEmbedded(folder.getParentDirectory());
        }
        auto snapshot=collectLibrary(folder,previous);
        snapshot.bankScanned=bank.error.isEmpty();
        if(bank.error.isNotEmpty())juce::Logger::writeToLog("Skell artist bank: "+bank.error);
        snapshot.bankMetadata=std::move(bank.metadata);
        juce::MessageManager::callAsync([safe,revision,snapshot=std::move(snapshot)]() mutable {
            if(!safe)return;
            safe->libraryScanInFlight=false;
            if(!safe->preferencesReadOnly && revision==safe->libraryRevision) {
                safe->applyLibrarySnapshot(std::move(snapshot));
                safe->refreshArtistChoices();
                safe->refreshArtwork(false);
                safe->resetResults();
                if(safe->onCoverChanged)safe->onCoverChanged();
            }
            if(safe->libraryScanQueued)safe->requestLibraryScan();
        });
    });
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

juce::File PresetBrowser::copyToLibraryFile(const juce::File& source,const juce::File& folder) {
    if(folder.createDirectory().failed())return {};
    if(source.getParentDirectory()==folder)return source;
    const auto dest=folder.getNonexistentChildFile(
        juce::File::createLegalFileName(source.getFileNameWithoutExtension()),
        ".skellpreset",true);
    if(!source.copyFileTo(dest))return {};
    auto originalCover=source.withFileExtension(".png");
    if(!originalCover.existsAsFile())originalCover=source.withFileExtension(".jpg");
    if(originalCover.existsAsFile() && originalCover.getSize()>0 &&
       originalCover.getSize()<=8*1024*1024)
        originalCover.copyFileTo(dest.withFileExtension(originalCover.getFileExtension()));
    return dest;
}
void PresetBrowser::open() {
    // The local renderer's synthetic fixtures are intentionally synchronous.
    // Real editor opening keeps its previous snapshot until the worker returns.
    if(preferencesReadOnly)scanLibrary();else requestLibraryScan();
    loadFavorites();loadArtists();
    loadCustomCategories();refreshGenreChoices();loadGenres();loadOrganization();
    artistTierFilter.setSelectedId(1,juce::dontSendNotification);
    typeFilter.setSelectedId(1,juce::dontSendNotification);refreshArtistChoices();
    artistFilter.setSelectedId(1,juce::dontSendNotification);refreshArtwork();
    bank=0;mainAdlibs=false;scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;
    hoveredBank=-1;hoveredAdlibs=false;browserWheelCarry=0.f;
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    // Preserve the user's in-session display sort without saving DAW state.
    showDetails(false);
    search.setText({},juce::dontSendNotification);
    clearSearchButton.setEnabled(false);
    categoryCountsDirty=true;
    resized();setVisible(true);toFront(true);search.grabKeyboardFocus();repaint();
}
bool PresetBrowser::configureDirectoryForScreenshot(int page,int width,int height) {
    // Called only by the local renderer. Fixture labels are explicit; never
    // write fake artists, presets or pairing metadata into the user's library.
    preferencesReadOnly=true;open();
    // The local screenshot harness uses an isolated application-data root.
    // Production opening uses only the background scan above.
    const auto fixtureRoot=libraryDirectory().getParentDirectory();
    if(juce::SystemStats::getEnvironmentVariable("SKELL_ARTIST_BANK_QA",{})=="1" &&
       fixtureRoot.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory))){
        auto fixtureBank=skellpreset::artistbank::syncEmbedded(fixtureRoot);
        if(fixtureBank.error.isEmpty()){
            embeddedArtistMetadata=std::move(fixtureBank.metadata);
            scanLibrary();refreshArtistChoices();
        }
    }
    if(page>=9){
        if(embeddedArtistMetadata.empty())return false;
        const auto artist=embeddedArtistMetadata.begin()->second.artist;
        userPlaced=true;setBounds(constrainWindow({usableBounds.getX()+8,usableBounds.getY()+4,width,height}));
        chooseBank(2);
        const auto tier=artistTier(artist);
        if(!activateDirectoryRow(tier=="Underground"?0:tier=="Mainstream"?1:2))return false;
        if(page>9){
            const auto cards=directoryCards();int row=-1;
            for(int n=0;n<(int)cards.size();++n)if(cards[(size_t)n].label.equalsIgnoreCase(artist))row=n;
            if(!activateDirectoryRow(row))return false;
            vocalTabs[(size_t)(page==11?1:page==12?2:0)].onClick();
        }
        resetResults();return true;
    }
    if(page>=1){
        const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("Skell UI fixture "+juce::Uuid().toString());
        for(int i=0;i<4;++i){
            const juce::String artist=i<2?"QA Underground":"QA Mainstream";
            const juce::String type=i%2==0?"main":"adlib";
            const int index=(int)userPresets.size();
            userPresets.push_back(root.getChildFile(artist+" "+type+".skellpreset"));
            userArtwork.emplace_back();
            const auto key=favoriteKey(-index-1);
            artistKeys.add(key);artistNames.add(artist);
            genreKeys.add(key);genreNames.add("Rage");
            setOrganizationValue("type:"+key,type);
            setOrganizationValue("tier:"+artist.toLowerCase(),i<2?"Underground":"Mainstream");
            setOrganizationValue("pair:"+key,artist+" QA pair");
        }
        refreshArtistChoices();
    }
    userPlaced=true;setBounds(constrainWindow({usableBounds.getX()+8,usableBounds.getY()+4,width,height}));
    if(page==1 || page==2){
        chooseBank(4);
        if(page==2){
            const auto cards=directoryCards();
            for(int n=0;n<(int)cards.size();++n)if(cards[(size_t)n].label=="Rage"){activateDirectoryRow(n);break;}
        }
    }else if(page>=3){
        chooseBank(2);
        if(page>=4){
            if(!activateDirectoryRow(page==5?1:0))return false;
            if(page>=6){
                const auto cards=directoryCards();
                int row=-1;
                for(int n=0;n<(int)cards.size();++n)if(cards[(size_t)n].label=="QA Underground")row=n;
                if(!activateDirectoryRow(row))return false;
                vocalTabs[(size_t)(page==7?1:page==8?2:0)].onClick();
            }
        }
    }
    resetResults();return true;
}
void PresetBrowser::showDetails(bool openDetails) {
    detailsOpen=openDetails;
    hoveredCard=-1;hoveredStar=-1;hoveredBank=-1;hoveredAdlibs=false;
    browserWheelCarry=0.f;
    detailsButton.setButtonText(detailsOpen?"BACK TO LIBRARY":"MANAGE LIBRARY");
    if(!detailsOpen)grabKeyboardFocus();
    resized();repaint();
}
void PresetBrowser::chooseBank(int index){
    if(index<0 || index>=banks.size())return;
    bank=index;mainAdlibs=false;
    scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;browserWheelCarry=0.f;
    // Factory Main Vocals are not hidden by a previous user genre filter.
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    artistTierFilter.setSelectedId(1,juce::dontSendNotification);
    typeFilter.setSelectedId(1,juce::dontSendNotification);
    refreshArtistChoices();artistFilter.setSelectedId(1,juce::dontSendNotification);
    resized();repaint();
}
void PresetBrowser::chooseAdlibs(){
    bank=0;mainAdlibs=true;
    scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;browserWheelCarry=0.f;
    genreFilter.setSelectedId(1,juce::dontSendNotification);
    resized();repaint();
}
void PresetBrowser::focusCard(int index){
    const int count=resultCount();
    if(count<=0){keyboardCard=-1;scroll=0;hoveredCard=-1;hoveredStar=-1;repaint();return;}
    keyboardCard=juce::jlimit(0,count-1,index);
    if(keyboardCard<scroll)scroll=keyboardCard;
    if(keyboardCard>=scroll+pageCapacity())scroll=keyboardCard-pageCapacity()+1;
    hoveredCard=-1;hoveredStar=-1;repaint();
}
bool PresetBrowser::showVocalTabs() const {
    return !detailsOpen && ((bank==2 && artistFilter.getSelectedId()>1) ||
                            (bank==4 && genreFilter.getSelectedId()>1));
}
void PresetBrowser::syncVocalTabs(){
    for(int i=0;i<3;++i){
        auto& tab=vocalTabs[(size_t)i];const bool selected=typeFilter.getSelectedId()==i+1;
        tab.setVisible(showVocalTabs());
        tab.setColour(juce::TextButton::buttonColourId,selected?Theme::green.withAlpha(.20f):Theme::panelAlt);
        tab.setColour(juce::TextButton::textColourOffId,selected?Theme::green:Theme::white);
    }
}
juce::Rectangle<int> PresetBrowser::constrainWindow(juce::Rectangle<int> r) const {
    if(usableBounds.isEmpty())return r;
    const int w=juce::jlimit(juce::jmin(620,usableBounds.getWidth()),juce::jmin(1000,usableBounds.getWidth()),r.getWidth());
    const int h=juce::jlimit(juce::jmin(530,usableBounds.getHeight()),juce::jmin(610,usableBounds.getHeight()),r.getHeight());
    return {juce::jlimit(usableBounds.getX(),usableBounds.getRight()-w,r.getX()),
        juce::jlimit(usableBounds.getY(),usableBounds.getBottom()-h,r.getY()),w,h};
}
void PresetBrowser::placeWithin(juce::Rectangle<int> usable,juce::Rectangle<int> preferred){
    usableBounds=usable;
    const auto bounds=constrainWindow(userPlaced?getBounds():preferred);
    if(bounds!=getBounds())setBounds(bounds);
}
void PresetBrowser::resized(){
    search.setBounds(19,17,getWidth()-111,42);
    search.setFont(juce::Font(juce::FontOptions(15.f)));
    search.setEnabled(!detailsOpen && !chooserActive);
    clearSearchButton.setEnabled(!detailsOpen && !chooserActive && search.getText().isNotEmpty());
    clearSearchButton.setBounds(getWidth()-86,22,66,31);
    sortPicker.setVisible(!detailsOpen && directoryMode()==DirectoryMode::none);
    sortPicker.setBounds(18,getHeight()-77,166,29);
    thumbnailView.setBounds(getWidth()-205,69,117,29);
    listView.setBounds(getWidth()-82,69,63,29);
    for(auto* b:{&thumbnailView,&listView})b->setVisible(!detailsOpen);
    thumbnailView.setColour(juce::TextButton::buttonColourId,!listMode?Theme::green.withAlpha(.18f):Theme::panelAlt);
    listView.setColour(juce::TextButton::buttonColourId,listMode?Theme::green.withAlpha(.18f):Theme::panelAlt);
    thumbnailView.setColour(juce::TextButton::textColourOffId,!listMode?Theme::green:Theme::muted);
    listView.setColour(juce::TextButton::textColourOffId,listMode?Theme::green:Theme::muted);
    directoryBack.setVisible(!detailsOpen && hasDirectoryParent());
    directoryBack.setBounds(207,69,64,29);
    detailsButton.setBounds(18,detailsOpen?112:getHeight()-38,166,28);
    starterButton.setVisible(detailsOpen);
    starterButton.setBounds(207,412,std::max(0,std::min(310,getWidth()-234)),32);
    importButton.setVisible(detailsOpen);exportButton.setVisible(detailsOpen);
    saveAsButton.setVisible(detailsOpen);
    for(auto* control:{static_cast<juce::Component*>(&artistEditor),
                       static_cast<juce::Component*>(&artistButton),
                       static_cast<juce::Component*>(&genrePicker),
                       static_cast<juce::Component*>(&genreButton),
                       static_cast<juce::Component*>(&categoryEditor),
                       static_cast<juce::Component*>(&addCategoryButton),
                       static_cast<juce::Component*>(&artworkButton)})
        control->setVisible(detailsOpen);
    genreFilter.setVisible(false);artistTierFilter.setVisible(false);
    artistFilter.setVisible(false);typeFilter.setVisible(false);
    syncVocalTabs();
    const int tabWidth=juce::jmax(1,(getWidth()-229)/3);
    for(int i=0;i<3;++i)vocalTabs[(size_t)i].setBounds(207+i*tabWidth,getHeight()-45,juce::jmax(1,tabWidth-5),30);
    for(auto* control:{static_cast<juce::Component*>(&tierEditor),static_cast<juce::Component*>(&typeEditor),
        static_cast<juce::Component*>(&pairEditor),static_cast<juce::Component*>(&metadataButton),
        static_cast<juce::Component*>(&relatedButton)})control->setVisible(detailsOpen);
    if(detailsOpen){
        const int detailWidth=std::max(0,std::min(310,getWidth()-234));
        tierEditor.setBounds(207,148,detailWidth,30);
        typeEditor.setBounds(207,215,detailWidth,30);
        pairEditor.setBounds(207,282,detailWidth,32);
        metadataButton.setBounds(207,330,detailWidth,32);
        relatedButton.setBounds(207,371,detailWidth,32);
        artistEditor.setBounds(18,184,166,29);
        artistButton.setBounds(18,221,166,29);
        genrePicker.setBounds(18,289,107,29);
        genreButton.setBounds(132,289,52,29);
        categoryEditor.setBounds(18,359,166,29);
        addCategoryButton.setBounds(18,393,166,29);
        artworkButton.setBounds(18,429,166,29);
        importButton.setBounds(18,464,166,28);
        exportButton.setBounds(18,496,80,28);
        saveAsButton.setBounds(104,496,80,28);
    }else{
        genreFilter.setBounds(18,337,166,27);
        artistTierFilter.setBounds(18,337,166,27);
        artistFilter.setBounds(18,368,166,27);
        typeFilter.setBounds(18,399,166,27);
        importButton.setBounds(18,getHeight()-70,166,28);
        exportButton.setBounds(18,getHeight()-36,166,28);
    }
    if(!detailsOpen){const int count=resultCount();scroll=juce::jlimit(0,std::max(0,count-pageCapacity()),scroll);}
}
void PresetBrowser::setChooserActive(bool active) {
    chooserActive=active;
    refreshSaveControls();
    artworkButton.setEnabled(!active && !saveInFlight && canEditCover());
    artistButton.setEnabled(!active && activeUserPreset.existsAsFile());
    artistEditor.setEnabled(!active && activeUserPreset.existsAsFile());
    genreButton.setEnabled(!active && activeUserPreset.existsAsFile());
    genrePicker.setEnabled(!active && activeUserPreset.existsAsFile());
    addCategoryButton.setEnabled(!active);
    categoryEditor.setEnabled(!active);
    refreshOrganizationEditor();
}
void PresetBrowser::chooseImport() {
    if(chooserActive || saveInFlight || loadInFlight || coverWriteInFlight)return;
    setChooserActive(true);
    fileChooser=std::make_unique<juce::FileChooser>("Import native Skell-Preset", juce::File{}, "*.skellpreset");
    juce::Component::SafePointer<PresetBrowser> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if(!safe)return; // editor/plugin may have closed while native dialog was open
            safe->setChooserActive(false);
            const auto file=chooser.getResult();
            if(file==juce::File{})return;
            safe->startPresetRead(file,true);
        });
}
void PresetBrowser::startPresetRead(const juce::File& file,bool external) {
    if(loadInFlight || coverWriteInFlight || (external && saveInFlight))return;
    loadInFlight=true;
    const auto selection=++selectionRevision;
    const auto program=processor.getCurrentProgram();
    if(external)++libraryRevision;
    refreshSaveControls();
    setSaveStatus("LOADING...");
    const auto folder=libraryDirectory();
    const auto previous=coverCache;
    juce::Component::SafePointer<PresetBrowser> safe(this);
    juce::Thread::launch([safe,file,external,selection,program,folder,previous] {
        skellpreset::package::Document doc;
        auto result=skellpreset::package::read(file,doc);
        juce::File copy=file;
        LibrarySnapshot snapshot;
        BackdropSnapshot backdrop;
        if(result.wasOk()) {
            if(external) {
                std::lock_guard<std::mutex> lock(libraryWriteMutex);
                skellpreset::artistbank::LibraryMutationLock mutation(folder.getParentDirectory());
                if(!mutation.acquired())result=juce::Result::fail("Artist library is busy; try again.");
                else {copy=copyToLibraryFile(file,folder);snapshot=collectLibrary(folder,previous);}
            }
            backdrop=collectBackdrop(copy);
        }
        juce::MessageManager::callAsync([safe,external,selection,program,copy,result,doc=std::move(doc),
                snapshot=std::move(snapshot),backdrop=std::move(backdrop)]() mutable {
            if(!safe)return;
            safe->loadInFlight=false;
            if(external && snapshot.ok) {
                safe->applyLibrarySnapshot(std::move(snapshot));
                safe->refreshArtistChoices();
                safe->resetResults();
            }
            if(selection==safe->selectionRevision && program==safe->processor.getCurrentProgram()) {
                if(result.failed()) {
                    safe->setSaveStatus("LOAD FAILED");
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                        "Cannot load preset",result.getErrorMessage());
                }else {
                    const int previousMode=(int)safe->processor.state.getRawParameterValue("mode")->load();
                    if(safe->onBeforeLoad)safe->onBeforeLoad();
                    const auto applied=safe->processor.applyValidatedPresetDocument(doc);
                    if(applied.failed()) {
                        if(safe->onLoadFailed)safe->onLoadFailed();
                        safe->setSaveStatus("LOAD FAILED");
                    }else {
                        if(!external && (previousMode==0 || previousMode==3))
                            if(auto* mode=safe->processor.state.getParameter("mode"))
                                mode->setValueNotifyingHost(mode->convertTo0to1((float)previousMode));
                        safe->activeUserPreset=copy;
                        ++safe->backdropRevision;
                        safe->backdropPreset=copy;
                        safe->applyBackdrop(std::move(backdrop));
                        const int index=safe->activeUserIndex();
                        if(index>=0)safe->rememberRecent(-index-1);
                        safe->setSaveStatus(copy==juce::File{}?"LOADED - NO LIBRARY COPY":"LOADED");
                        safe->refreshArtwork(false);
                        if(safe->onLoaded)safe->onLoaded();
                        if(safe->onCoverChanged)safe->onCoverChanged();
                    }
                }
            }else safe->setSaveStatus({});
            safe->refreshSaveControls();
            if(safe->libraryScanQueued)safe->requestLibraryScan();
            safe->repaint();
        });
    });
}
void PresetBrowser::saveActivePreset(bool chooseName) {
    if(chooserActive || saveInFlight || loadInFlight || coverWriteInFlight)return;
    // Quick save rewrites the selected personal preset. Factory sounds and
    // never-saved chains must choose a name so no factory program is touched.
    if(!chooseName && activeUserPreset.existsAsFile()) {
        startPresetWrite(activeUserPreset);
        return;
    }
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
            safe->startPresetWrite(file);
        });
}
void PresetBrowser::startPresetWrite(const juce::File& destination) {
    if(saveInFlight)return;
    const auto target=destination.hasFileExtension(".skellpreset")
        ?destination:destination.withFileExtension(".skellpreset");
    // Snapshot APVTS and metadata on the message thread. The worker owns
    // only value copies: ZIP, library copy, cover decode and enumeration.
    skellpreset::package::Document doc;
    if(const auto status=processor.makePresetDocument(doc);status.failed()) {
        completePresetWrite({},false,status.getErrorMessage(),{},{});
        return;
    }
    pendingSave=SaveMeta{};
    pendingSave.previousUser=activeUserPreset;
    pendingSave.selection=selectionRevision;
    pendingSave.program=processor.getCurrentProgram();
    for(int i=0;i<(int)userPresets.size();++i)
        if(userPresets[(size_t)i]==activeUserPreset) {
            pendingSave.artist=artistForUser(i);
            pendingSave.genres=genresForUser(i);
            pendingSave.type=vocalType(i);pendingSave.pair=pairForUser(i);
            break;
        }
    auto oldCover=activeArtworkDestination();
    if(!oldCover.existsAsFile() && activeUserPreset.existsAsFile())
        oldCover=activeUserPreset.withFileExtension(".jpg");
    pendingSave.oldCover=oldCover;
    saveInFlight=true;
    ++libraryRevision;
    setSaveStatus("SAVING...");
    refreshSaveControls();
    const auto folder=libraryDirectory();
    const auto oldCoverFile=pendingSave.oldCover;
    const auto previousCovers=coverCache;
    juce::Component::SafePointer<PresetBrowser> safe(this);
    juce::Thread::launch([safe,target,doc,folder,oldCoverFile,previousCovers] {
        juce::File libraryCopy;
        LibrarySnapshot snapshot;
        BackdropSnapshot backdrop;
        const auto result=[&] {
            std::lock_guard<std::mutex> lock(libraryWriteMutex);
            skellpreset::artistbank::LibraryMutationLock mutation(folder.getParentDirectory());
            if(!mutation.acquired())return juce::Result::fail("Artist library is busy; try again.");
            const auto written=skellpreset::package::write(target,doc);
            if(written.failed())return written;
            libraryCopy=copyToLibraryFile(target,folder);
            if(libraryCopy!=juce::File{}) {
                if(oldCoverFile.existsAsFile()) {
                    const auto newCover=libraryCopy.withFileExtension(oldCoverFile.getFileExtension());
                    if(oldCoverFile!=newCover)oldCoverFile.copyFileTo(newCover);
                }
                backdrop=collectBackdrop(libraryCopy);
            }
            snapshot=collectLibrary(folder,previousCovers);
            return written;
        }();
        juce::MessageManager::callAsync([safe,libraryCopy,result,
                                       snapshot=std::move(snapshot),backdrop=std::move(backdrop)]() mutable {
            if(!safe)return;
            safe->completePresetWrite(libraryCopy,result.wasOk(),result.getErrorMessage(),
                                      std::move(snapshot),std::move(backdrop));
        });
    });
}
void PresetBrowser::completePresetWrite(const juce::File& libraryCopy,bool ok,
                                        const juce::String& error,LibrarySnapshot snapshot,
                                        BackdropSnapshot backdrop) {
    if(!ok) {
        saveInFlight=false;
        setSaveStatus("SAVE FAILED");
        refreshSaveControls();
        if(libraryScanQueued)requestLibraryScan();
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Cannot save preset",error);
        return;
    }
    const bool copied=libraryCopy!=juce::File{};
    const bool indexed=snapshot.ok;
    applyLibrarySnapshot(std::move(snapshot));
    // The user can keep auditioning while saving. Never replace a newer
    // selection with the file belonging to an earlier snapshot.
    if(copied && pendingSave.selection==selectionRevision &&
       pendingSave.program==processor.getCurrentProgram() &&
       pendingSave.previousUser==activeUserPreset) {
        activeUserPreset=libraryCopy;
        ++backdropRevision;
        backdropPreset=libraryCopy;
        applyBackdrop(std::move(backdrop));
    }
    // Metadata follows the saved file, not whichever preset is now selected.
    if(copied && libraryCopy!=pendingSave.previousUser) {
        for(int i=0;i<(int)userPresets.size();++i)
            if(userPresets[(size_t)i]==libraryCopy) {
                if(pendingSave.artist.isNotEmpty())
                    setArtistForUser(i,pendingSave.artist);
                for(const auto& genre:pendingSave.genres)
                    setGenreForUser(i,genre);
                if(pendingSave.type.isNotEmpty() || pendingSave.pair.isNotEmpty()) {
                    const auto beforeKeys=organizationKeys,beforeValues=organizationValues;
                    const auto key=favoriteKey(-i-1);
                    setOrganizationValue("type:"+key,pendingSave.type);
                    setOrganizationValue("pair:"+key,pendingSave.pair);
                    if(!saveOrganization()) {
                        organizationKeys=beforeKeys;organizationValues=beforeValues;
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Preset organization not copied",
                            "The sound was saved, but the organization labels could not be copied.");
                    }
                }
                break;
            }
    }
    saveInFlight=false;
    if(libraryScanQueued)requestLibraryScan();
    refreshArtistChoices();
    refreshArtwork(false); // already decoded in the worker, or unchanged selection
    resetResults();
    if(onCoverChanged)onCoverChanged();
    setSaveStatus(!copied?"EXPORTED - NO LIBRARY COPY":
                  !indexed?"SAVED - REFRESH LIBRARY":"SAVED");
    refreshSaveControls();
    repaint();
}
void PresetBrowser::setSaveStatus(const juce::String& text) {
    saveStatus=text;
    repaint();
}
void PresetBrowser::refreshSaveControls() {
    const bool idle=!chooserActive && !saveInFlight && !loadInFlight && !coverWriteInFlight;
    importButton.setEnabled(idle);
    starterButton.setEnabled(idle);
    exportButton.setEnabled(idle);
    saveAsButton.setEnabled(idle);
    artworkButton.setEnabled(idle && canEditCover());
    exportButton.setButtonText(activeUserPreset.existsAsFile()?"SAVE":"SAVE COPY");
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
    return standardGenres.contains(label,true) ||
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
    for(const auto& label:standardGenres){
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
juce::StringArray PresetBrowser::genresForUser(int index) const {
    juce::StringArray result;if(index<0 || index>=(int)userPresets.size())return result;
    const auto key=favoriteKey(-index-1),extension=organizationValue("genres:"+key);
    if(extension.isNotEmpty()) {
        const auto decoded=juce::JSON::parse(extension);
        if(const auto* values=decoded.getArray()) {
            for(const auto& v:*values)if(v.isString() && isKnownCategory(v.toString()))result.addIfNotAlreadyThere(v.toString(),true);
            return result;
        }
    }
    const int pos=genreKeys.indexOf(key,true);
    if(pos>=0 && pos<genreNames.size() && genreNames[pos].isNotEmpty())result.add(genreNames[pos]);
    return result;
}
juce::String PresetBrowser::genreForUser(int index) const {return genresForUser(index).joinIntoString(", ");}
void PresetBrowser::loadGenres() {
    genreKeys.clear();genreNames.clear();
    const auto file=genresFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>1024*1024)return;
    for(const auto& line:juce::StringArray::fromLines(file.loadFileAsString())) {
        const int tab=line.indexOfChar('\t');
        if(tab<6)continue;
        const auto key=line.substring(0,tab).trim().toLowerCase();
        const auto label=line.substring(tab+1).trim();
        if(!key.startsWith("user:") || key.length()>260 ||
           key.containsAnyOf("\r\n\t") || genreKeys.contains(key,true) ||
           !isKnownCategory(label))continue;
        genreKeys.add(key);genreNames.add(label);
        if(genreKeys.size()>=maxUserLibraryPresets)break;
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
    if(index<0 || index>=(int)userPresets.size() || (label.isNotEmpty() && !isKnownCategory(label)))return false;
    auto labels=genresForUser(index);
    if(label.isEmpty())labels.clear();else labels.addIfNotAlreadyThere(label,true);
    if(labels.size()>32)return false;
    const auto oldKeys=organizationKeys,oldValues=organizationValues;
    juce::Array<juce::var> values;for(const auto& item:labels)values.add(item);
    setOrganizationValue("genres:"+favoriteKey(-index-1),juce::JSON::toString(juce::var(values),true));
    if(!saveOrganization()) {
        organizationKeys=oldKeys;organizationValues=oldValues;
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Categories not saved",
            "Check the preset library folder permissions.");return false;
    }
    categoryCountsDirty=true;return true;
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
    if(found>=0 && found<artistNames.size())return artistNames[found];
    const auto builtIn=embeddedArtistMetadata.find(key);
    return builtIn==embeddedArtistMetadata.end()?juce::String{}:builtIn->second.artist;
}
juce::File PresetBrowser::organizationFile() const {
    return libraryDirectory().getParentDirectory().getChildFile("LibraryOrganization.txt");
}
juce::String PresetBrowser::organizationValue(const juce::String& key) const {
    const int pos=organizationKeys.indexOf(key,true);
    if(pos>=0 && pos<organizationValues.size())return organizationValues[pos];
    const auto separator=key.indexOfChar(':');
    if(separator<0)return {};
    const auto builtIn=embeddedArtistMetadata.find(key.substring(separator+1));
    if(builtIn==embeddedArtistMetadata.end())return {};
    const auto field=key.substring(0,separator);
    if(field=="title")return builtIn->second.title;
    if(field=="type")return builtIn->second.type;
    if(field=="pair")return builtIn->second.pair;
    return {};
}
void PresetBrowser::setOrganizationValue(const juce::String& key,const juce::String& value){
    const int pos=organizationKeys.indexOf(key,true);
    if(pos>=0){
        if(value.isEmpty()){organizationKeys.remove(pos);organizationValues.remove(pos);}
        else organizationValues.set(pos,value);
    }else if(value.isNotEmpty()){organizationKeys.add(key);organizationValues.add(value);}
}
void PresetBrowser::loadOrganization(){
    organizationKeys.clear();organizationValues.clear();
    const auto file=organizationFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>1024*1024)return;
    for(const auto& row:juce::StringArray::fromLines(file.loadFileAsString())){
        const int tab=row.indexOfChar('\t');if(tab<=0)continue;
        const auto key=row.substring(0,tab).trim().toLowerCase();
        const auto value=row.substring(tab+1).trim();
        if(key.length()>270 || value.length()>(key.startsWith("genres:user:")?2048:80) || value.isEmpty() ||
           value.containsAnyOf("\r\n\t") || organizationKeys.contains(key,true))continue;
        const bool type=key.startsWith("type:user:") &&
            (value=="main" || value=="adlib" || value=="unassigned");
        const bool tier=key.startsWith("tier:") && key.length()>5 &&
            (value=="Underground" || value=="Mainstream");
        bool genres=false;
        if(key.startsWith("genres:user:")) {
            const auto decoded=juce::JSON::parse(value);
            if(const auto* labels=decoded.getArray()) {
                genres=labels->size()<=32;
                for(const auto& item:*labels)genres=genres && item.isString() && isKnownCategory(item.toString());
            }
        }
        if(type || tier || genres || key.startsWith("pair:user:") || key.startsWith("title:user:")){
            organizationKeys.add(key);organizationValues.add(value);
        }
        if(organizationKeys.size()>=4096)break;
    }
}
bool PresetBrowser::saveOrganization() const {
    const auto file=organizationFile();
    if(organizationKeys.size()>4096 || file.getSize()>1024*1024 || file.getParentDirectory().createDirectory().failed())return false;
    juce::StringArray rows;
    for(int i=0;i<organizationKeys.size();++i)
        rows.add(organizationKeys[i]+"\t"+organizationValues[i]);
    juce::TemporaryFile pending(file);
    if(!pending.getFile().replaceWithText(rows.joinIntoString("\n")+"\n"))return false;
    return pending.overwriteTargetFileWithTemporary();
}
juce::String PresetBrowser::userDisplayName(int index) const {
    if(index<0 || index>=(int)userPresets.size())return {};
    const auto title=organizationValue("title:"+favoriteKey(-index-1));
    return title.isNotEmpty()?title:userPresets[(size_t)index].getFileNameWithoutExtension();
}
juce::String PresetBrowser::vocalType(int index) const {
    if(index<0 || index>=(int)userPresets.size())return {};
    const auto value=organizationValue("type:"+favoriteKey(-index-1));
    if(value.isNotEmpty())return value;
    return genresForUser(index).contains("Ad-libs",true)?juce::String("adlib"):juce::String("unassigned");
}
juce::String PresetBrowser::artistTier(const juce::String& artist) const {
    return organizationValue("tier:"+artist.toLowerCase());
}
juce::String PresetBrowser::pairForUser(int index) const {
    return index>=0 && index<(int)userPresets.size()?
        organizationValue("pair:"+favoriteKey(-index-1)):juce::String{};
}
int PresetBrowser::activeUserIndex() const {
    if(!activeUserPreset.existsAsFile())return -1;
    for(int i=0;i<(int)userPresets.size();++i)
        if(userPresets[(size_t)i]==activeUserPreset)return i;
    return -1;
}
void PresetBrowser::refreshOrganizationEditor(){
    const int index=activeUserIndex();
    const bool enabled=index>=0 && !chooserActive;
    const auto artist=artistForUser(index),tier=artistTier(artist),type=vocalType(index);
    tierEditor.setSelectedId(tier=="Underground"?2:tier=="Mainstream"?3:1,juce::dontSendNotification);
    typeEditor.setSelectedId(type=="main"?2:type=="adlib"?3:1,juce::dontSendNotification);
    pairEditor.setText(pairForUser(index),juce::dontSendNotification);
    tierEditor.setEnabled(enabled && artist.isNotEmpty());
    typeEditor.setEnabled(enabled);pairEditor.setEnabled(enabled);metadataButton.setEnabled(enabled);
    relatedButton.setEnabled(enabled && pairForUser(index).isNotEmpty());
}
void PresetBrowser::refreshArtistChoices(){
    const auto previous=artistFilter.getText();
    artistFilter.clear(juce::dontSendNotification);artistFilter.addItem("All matching artists",1);
    juce::StringArray names;
    const int tierId=artistTierFilter.getSelectedId();
    for(int i=0;i<(int)userPresets.size();++i){
        const auto artist=artistForUser(i),tier=artistTier(artist);
        if(artist.isEmpty() || (tierId==2 && tier!="Underground") ||
           (tierId==3 && tier!="Mainstream") || (tierId==4 && tier.isNotEmpty()))continue;
        if(!names.contains(artist,true))names.add(artist);
    }
    names.sort(true);
    int id=1,selected=1;
    for(const auto& name:names){
        ++id;artistFilter.addItem(name,id);
        if(name.equalsIgnoreCase(previous))selected=id;
    }
    artistFilter.setSelectedId(selected,juce::dontSendNotification);
}
void PresetBrowser::resetResults(){
    scroll=0;hoveredCard=-1;hoveredStar=-1;keyboardCard=-1;browserWheelCarry=0.f;
    categoryCountsDirty=true;resized();repaint();
}
PresetBrowser::DirectoryMode PresetBrowser::directoryMode() const {
    // Search remains a direct preset lookup, even from a category root.
    if(detailsOpen || search.getText().trim().isNotEmpty())return DirectoryMode::none;
    if(bank==4 && genreFilter.getSelectedId()<=1)return DirectoryMode::genres;
    if(bank==2 && artistFilter.getSelectedId()<=1)
        return artistTierFilter.getSelectedId()<=1?DirectoryMode::tiers:DirectoryMode::artists;
    return DirectoryMode::none;
}
bool PresetBrowser::hasDirectoryParent() const {
    return (bank==4 && genreFilter.getSelectedId()>1) ||
           (bank==2 && (artistTierFilter.getSelectedId()>1 || artistFilter.getSelectedId()>1));
}
std::vector<PresetBrowser::DirectoryCard> PresetBrowser::directoryCards() const {
    const auto mode=directoryMode();
    std::vector<DirectoryCard> cards;
    auto add=[&cards](const juce::String& label,int id){
        DirectoryCard card;card.label=label;card.id=id;cards.push_back(card);
    };
    if(mode==DirectoryMode::genres){
        for(int n=0;n<genreFilter.getNumItems();++n)
            if(genreFilter.getItemId(n)>2)add(genreFilter.getItemText(n),genreFilter.getItemId(n));
        add("Untagged",2);
    }else if(mode==DirectoryMode::tiers){
        add("Underground",2);add("Mainstream",3);add("Unassigned tier",4);
    }else if(mode==DirectoryMode::artists){
        for(int n=0;n<artistFilter.getNumItems();++n)
            if(artistFilter.getItemId(n)>1)add(artistFilter.getItemText(n),artistFilter.getItemId(n));
    }else return cards;
    const int typeId=typeFilter.getSelectedId();
    for(int i=0;i<(int)userPresets.size();++i){
        const auto genre=genreForUser(i),artist=artistForUser(i),type=vocalType(i);
        const auto tier=artistTier(artist);
        if((typeId==2 && type!="main") || (typeId==3 && type!="adlib"))continue;
        for(auto& card:cards){
            bool matches=false;
            if(mode==DirectoryMode::genres)matches=card.id==2?genre.isEmpty():genresForUser(i).contains(card.label,true);
            else if(mode==DirectoryMode::tiers)matches=artist.isNotEmpty() &&
                (card.id==4?tier.isEmpty():tier==card.label);
            else matches=artist.isNotEmpty() && artist.equalsIgnoreCase(card.label);
            if(!matches)continue;
            if(type=="main")++card.main;else if(type=="adlib")++card.adlib;else ++card.other;
            // Reuse cached user cover art only. No generated artist portraits or disk I/O.
            if(card.artIndex<0 && i<(int)userArtwork.size() && userArtwork[(size_t)i].isValid())
                card.artIndex=i;
        }
    }
    return cards;
}
int PresetBrowser::resultCount() const {
    return directoryMode()==DirectoryMode::none?(int)filtered().size():(int)directoryCards().size();
}
int PresetBrowser::directoryColumns() const {
    return listMode?1:directoryMode()==DirectoryMode::tiers?2:getWidth()>=850?3:2;
}
int PresetBrowser::directoryRows() const {return listMode?visibleRows():directoryMode()==DirectoryMode::tiers?2:3;}
int PresetBrowser::pageCapacity() const {
    return directoryMode()==DirectoryMode::none && listMode?visibleRows():directoryColumns()*directoryRows();
}
juce::Rectangle<int> PresetBrowser::directoryCardBounds(int position) const {
    if(listMode)return {207,110+position*61,getWidth()-224,56};
    const int cols=directoryColumns(),gap=10;
    const int width=(getWidth()-224-(cols-1)*gap)/cols;
    const int rows=directoryRows();
    const int height=(getHeight()-151-(rows-1)*gap)/rows;
    return {207+(position%cols)*(width+gap),110+(position/cols)*(height+gap),width,height};
}
int PresetBrowser::resultAt(juce::Point<int> p) const {
    if(detailsOpen)return -1;
    if(directoryMode()==DirectoryMode::none && listMode)
        return presetCardHitForTest(false,p.x,p.y,getWidth(),visibleRows())?scroll+(p.y-110)/61:-1;
    for(int i=0;i<pageCapacity();++i)
        if(directoryCardBounds(i).contains(p))return scroll+i;
    return -1;
}
bool PresetBrowser::activateDirectoryRow(int index){
    const auto mode=directoryMode();
    const auto cards=directoryCards();
    if(index<0 || index>=(int)cards.size())return false;
    const int id=cards[(size_t)index].id;
    if(mode==DirectoryMode::genres)genreFilter.setSelectedId(id,juce::dontSendNotification);
    else if(mode==DirectoryMode::tiers){
        artistTierFilter.setSelectedId(id,juce::dontSendNotification);
        refreshArtistChoices();artistFilter.setSelectedId(1,juce::dontSendNotification);
    }else if(mode==DirectoryMode::artists)artistFilter.setSelectedId(id,juce::dontSendNotification);
    else return false;
    typeFilter.setSelectedId(1,juce::dontSendNotification);resetResults();return true;
}
bool PresetBrowser::goBackDirectory(){
    if(bank==4 && genreFilter.getSelectedId()>1)
        genreFilter.setSelectedId(1,juce::dontSendNotification);
    else if(bank==2 && artistFilter.getSelectedId()>1)
        artistFilter.setSelectedId(1,juce::dontSendNotification);
    else if(bank==2 && artistTierFilter.getSelectedId()>1){
        artistTierFilter.setSelectedId(1,juce::dontSendNotification);
        refreshArtistChoices();artistFilter.setSelectedId(1,juce::dontSendNotification);
    }else return false;
    search.setText({},juce::dontSendNotification);clearSearchButton.setEnabled(false);
    typeFilter.setSelectedId(1,juce::dontSendNotification);resetResults();return true;
}
void PresetBrowser::paintDirectory(juce::Graphics& g,const std::vector<DirectoryCard>& cards){
    const int count=(int)cards.size(),capacity=pageCapacity();
    const int start=juce::jlimit(0,std::max(0,count-capacity),scroll);
    const int right=getWidth()-17;
    if(cards.empty()){
        Theme::fittedText(g,"NO ARTISTS IN THIS GROUP",{222,210,std::max(1,right-239),32},15.f,Theme::white,true);
        Theme::fittedText(g,"Assign an artist and tier in Manage Library.",{222,250,std::max(1,right-239),26},12.f,Theme::muted);
        return;
    }
    const std::array<juce::Colour,4> accents{{Theme::green,Theme::amber,Theme::pink,Theme::blue}};
    for(int n=0;n<capacity && start+n<count;++n){
        const int index=start+n;const auto& card=cards[(size_t)index];
        const auto bounds=directoryCardBounds(n).toFloat();
        const bool hover=index==hoveredCard,focused=index==keyboardCard;
        const auto accent=accents[(size_t)artworkPaletteIndexForTest(card.id)];
        g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(hover?.20f:.08f),
            bounds.getTopLeft(),Theme::plot,bounds.getBottomRight(),false));
        g.fillRoundedRectangle(bounds,8.f);
        const auto art=listMode?juce::Rectangle<float>(bounds.getX()+6.f,bounds.getY()+6.f,44.f,44.f):
            bounds.reduced(6.f).removeFromTop(bounds.getHeight()-49.f);
        if(card.artIndex>=0 && card.artIndex<(int)userArtwork.size() && userArtwork[(size_t)card.artIndex].isValid()){
            juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(art.toNearestInt());
            g.drawImageWithin(userArtwork[(size_t)card.artIndex],(int)art.getX(),(int)art.getY(),
                (int)art.getWidth(),(int)art.getHeight(),juce::RectanglePlacement::fillDestination);
        }else paintCover(g,art,card.id,accent);
        g.setGradientFill(juce::ColourGradient(Theme::plot.withAlpha(.06f),art.getCentreX(),art.getY(),
            Theme::plot.withAlpha(.75f),art.getCentreX(),art.getBottom(),false));
        g.fillRoundedRectangle(art,5.f);
        const int total=card.main+card.adlib+card.other;
        const auto badge=listMode?juce::Rectangle<float>(bounds.getRight()-56.f,bounds.getY()+17.f,44.f,21.f):
            juce::Rectangle<float>(art.getRight()-50.f,art.getY()+6.f,44.f,21.f);
        g.setColour(Theme::bg.withAlpha(.92f));g.fillRoundedRectangle(badge,5.f);
        Theme::fittedText(g,juce::String(total),badge.toNearestInt(),12.f,Theme::white,true,juce::Justification::centred);
        const auto x=(int)bounds.getX()+(listMode?64:10),y=(int)bounds.getBottom()-47,w=(int)bounds.getWidth()-(listMode?130:20);
        Theme::fittedText(g,card.label,{x,y,w,22},16.f,Theme::white,true,juce::Justification::centredLeft);
        juce::String summary=juce::String(card.main)+" MAIN  ·  "+juce::String(card.adlib)+" ADLIB";
        if(card.other>0)summary+="  ·  "+juce::String(card.other)+" OTHER";
        Theme::fittedText(g,summary,{x,y+23,w,15},10.5f,Theme::muted,false,juce::Justification::centredLeft);
        g.setColour(focused||hover?Theme::green.withAlpha(.88f):Theme::stroke);
        g.drawRoundedRectangle(bounds.reduced(.6f),8.f,focused?2.f:1.f);
    }
    if(count>capacity){
        const float rail=(float)(getHeight()-151),thumb=juce::jmax(30.f,rail*capacity/(float)count);
        g.setColour(Theme::stroke);g.fillRoundedRectangle((float)getWidth()-12.f,110.f,3.f,rail,1.5f);
        g.setColour(Theme::green);g.fillRoundedRectangle((float)getWidth()-12.f,
            110.f+(rail-thumb)*(float)start/(float)(count-capacity),3.f,thumb,1.5f);
    }
    Theme::fittedText(g,"SELECT A COLLECTION  /  "+juce::String(count)+" GROUPS",
        {207,getHeight()-27,std::max(1,getWidth()-227),18},11.f,Theme::muted);
}
void PresetBrowser::loadArtists() {
    artistKeys.clear();artistNames.clear();
    const auto file=artistsFile();
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>1024*1024)return;
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
        if(artistKeys.size()>=maxUserLibraryPresets)break;
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
    } else if(artist.isNotEmpty()) {
        if(artistKeys.size()>=maxUserLibraryPresets)return false;
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
    if(entry>=0)return "factory:"+juce::String(skellpreset::recipeForFactoryIndex(entry));
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
    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>1024*1024)return;
    for(auto key:juce::StringArray::fromLines(file.loadFileAsString())) {
        key=key.trim().toLowerCase();
        const bool safeFactory=key=="factory:43" || key=="factory:46" || key=="factory:27";
        if(key.length()>0 && key.length()<260 &&
           (key.startsWith("user:") || safeFactory) &&
           !key.containsChar('\r') && !favoriteKeys.contains(key,true))
            favoriteKeys.add(key);
        if(favoriteKeys.size()>=maxUserLibraryPresets+64)break;
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
    scroll=juce::jlimit(0,std::max(0,resultCount()-pageCapacity()),scroll);
    hoveredCard=-1;hoveredStar=-1;
    // Starred-first sorting and fav: search can also reorder row identities.
    keyboardCard=-1;
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
    juce::ScopedValueSetter<bool> listTest(listMode,true);
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
    const int rows=visibleRows();
    const bool end=keyboardCard==11 && scroll==12-rows &&
        auditionTargetForTest(keyboardCard,scroll,12)==11;
    keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey,unmodified,0));
    const bool pageUp=keyboardCard==11-rows && scroll==11-rows &&
        auditionTargetForTest(-1,scroll,12)==11-rows &&
        auditionTargetForTest(-1,999,12)==11 &&
        auditionTargetForTest(-1,0,0)==-1;
    keyPressed(juce::KeyPress(juce::KeyPress::homeKey,unmodified,0));
    const bool home=keyboardCard==0 && scroll==0;
    keyPressed(juce::KeyPress(juce::KeyPress::leftKey,unmodified,0));
    const bool left=bank==2;
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
bool PresetBrowser::testLibrarySnapshotForTest() {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skell-browser-snapshot","",true);
    struct Cleanup {juce::File path;~Cleanup(){path.deleteRecursively();}} cleanup{root};
    if(root.createDirectory().failed())return false;
    const auto folder=root.getChildFile("Library");
    const auto source=root.getChildFile("B.skellpreset");
    if(!source.replaceWithText("fixture") || folder.createDirectory().failed())return false;
    const auto first=folder.getChildFile("A.skellpreset");
    if(!first.replaceWithText("fixture"))return false;
    const auto cover=source.withFileExtension(".png");
    {
        juce::Image image(juce::Image::RGB,32,24,true);
        juce::PNGImageFormat png;
        juce::FileOutputStream out(cover);
        if(!out.openedOk() || !png.writeImageToStream(image,out))return false;
        out.flush();
    }
    const auto copied=copyToLibraryFile(source,folder);
    if(copied==juce::File{} || !copied.withFileExtension(".png").existsAsFile())return false;
    const auto initial=collectLibrary(folder,{});
    if(!initial.ok || initial.files.size()!=2 || initial.files[0]!=first ||
       initial.files[1]!=copied || initial.thumbnails.size()!=2 ||
       initial.thumbnails[0].isValid() || !initial.thumbnails[1].isValid() ||
       initial.covers.size()!=1)return false;
    const auto backdrop=collectBackdrop(copied);
    if(!backdrop.image.isValid() || backdrop.image.getWidth()!=32 ||
       backdrop.image.getHeight()!=24)return false;
    const auto cached=collectLibrary(folder,initial.covers);
    if(!cached.ok || cached.covers.size()!=1 || !cached.thumbnails[1].isValid())return false;
    if(!copied.withFileExtension(".png").deleteFile())return false;
    const auto removed=collectLibrary(folder,cached.covers);
    if(!removed.ok || removed.files.size()!=2 || !removed.covers.empty() ||
       removed.thumbnails[1].isValid())return false;
    // Large library verifies the expanded cap and bounded cache.
    for(int i=0;i<1020;++i)
        if(!folder.getChildFile("Z"+juce::String(i).paddedLeft('0',4)+".skellpreset")
                .replaceWithText("fixture"))return false;
    const auto large=collectLibrary(folder,{});
    return large.ok && large.files.size()==maxUserLibraryPresets && large.thumbnails.size()==maxUserLibraryPresets &&
           large.files.front()==first && large.files.back().getFileName()=="Z0997.skellpreset";
}
bool PresetBrowser::testBrowserLayoutForTest() {
    juce::ScopedValueSetter<bool> readOnly(preferencesReadOnly,true);
    if(!testViewSwitchForTest())return false;
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
           sortPicker.isVisible()!=(!expanded && directoryMode()==DirectoryMode::none) ||
           !bounds.contains(directoryBack.getBounds()) ||
           directoryBack.getBounds().intersects(sortPicker.getBounds()) ||
           directoryBack.isVisible()!=(!expanded && hasDirectoryParent()))return false;
        if(search.isEnabled()==expanded || importButton.isVisible()!=expanded ||
           exportButton.isVisible()!=expanded || saveAsButton.isVisible()!=expanded ||
           artistEditor.isVisible()!=expanded ||
           artistButton.isVisible()!=expanded ||
           genrePicker.isVisible()!=expanded ||
           genreButton.isVisible()!=expanded ||
           artworkButton.isVisible()!=expanded ||
           categoryEditor.isVisible()!=expanded ||
           addCategoryButton.isVisible()!=expanded ||
           genreFilter.isVisible() || artistTierFilter.isVisible() || artistFilter.isVisible() || typeFilter.isVisible())return false;
        for(const auto& tab:vocalTabs)if(tab.isVisible()!=showVocalTabs() || (tab.isVisible() && !bounds.contains(tab.getBounds())))return false;
        const std::array<const juce::Component*,22> widgets{{
            &detailsButton,&genrePicker,&genreButton,&artistEditor,
            &artistButton,&importButton,&exportButton,&saveAsButton,&artworkButton,
            &categoryEditor,&addCategoryButton,&genreFilter,&artistTierFilter,&artistFilter,&typeFilter,
             &tierEditor,&typeEditor,&pairEditor,&metadataButton,&relatedButton,&directoryBack,&starterButton}};
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
    const int oldBank=bank;bool library=true;
    const auto oldBounds=getBounds();
    for(const auto size:{juce::Point<int>{620,530},juce::Point<int>{1000,604}}){
        setSize(size.x,size.y);
        for(int b=0;b<banks.size();++b){bank=b;library=validate(false)&&library;}
        library=validate(true)&&library;
    }
    setBounds(oldBounds);bank=oldBank;
    const bool details=validate(true);showDetails(oldDetails);
    return library && details && singlePreviewName(deuceIndex)=="DEUCE" &&
           singlePreviewName(alchemistIndex)=="ALCHEMIST";
}
bool PresetBrowser::testMainVocalsPresetFilterForTest() {
    const int oldBank=bank,oldScroll=scroll;
    const bool oldMainAdlibs=mainAdlibs,oldDetails=detailsOpen;
    detailsOpen=false;mainAdlibs=false;
    const auto oldQuery=search.getText();
    const auto oldFilter=genreFilter.getText();
    const auto oldArtistKeys=artistKeys,oldArtists=artistNames;
    const auto oldGenreKeys=genreKeys,oldGenres=genreNames;
    const auto oldFavorites=favoriteKeys,oldCustom=customCategories;
    const auto oldOrganizationKeys=organizationKeys,oldOrganizationValues=organizationValues;
    const int oldTier=artistTierFilter.getSelectedId(),oldType=typeFilter.getSelectedId();
    const auto oldArtistSelection=artistFilter.getText();
    artistTierFilter.setSelectedId(1,juce::dontSendNotification);
    typeFilter.setSelectedId(1,juce::dontSendNotification);
    refreshArtistChoices();artistFilter.setSelectedId(1,juce::dontSendNotification);
    search.setText({},juce::dontSendNotification);
    genreFilter.setSelectedId(1,juce::dontSendNotification);

    bank=0;
    const auto main=filtered();
    const bool approvedOnly=main.size()>=2 && main[0]==alchemistIndex && main[1]==deuceIndex &&
        std::all_of(main.begin()+2,main.end(),[this](int e){return e<0 && vocalType(-e-1)=="main";});
    mainAdlibs=true;
    const auto adlibs=filtered();
    const bool factorySubmenu=!adlibs.empty() && visibleBoneAdlib(adlibs[0]) &&
        std::all_of(adlibs.begin()+1,adlibs.end(),[this](int e){return e<0 && vocalType(-e-1)=="adlib";});
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
                visibleBoneAdlib(entry);
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
    // The only factory Adlib remains Digital Bones; personal Adlibs are also visible.
    const bool taggedVisible=std::find(tagged.begin(),tagged.end(),entry)!=tagged.end();
    setOrganizationValue("tier:__skell_artist_probe__","Underground");
    setOrganizationValue("type:"+key,"adlib");
    setOrganizationValue("pair:"+key,"__skell_pair_probe__");
    bank=2;mainAdlibs=false;
    search.setText("artist:__skell_artist_probe__",juce::dontSendNotification);
    artistTierFilter.setSelectedId(2,juce::dontSendNotification);refreshArtistChoices();
    typeFilter.setSelectedId(3,juce::dontSendNotification);
    const auto underground=filtered();
    const bool tierMatch=underground.size()==1 && underground[0]==entry;
    artistTierFilter.setSelectedId(3,juce::dontSendNotification);refreshArtistChoices();
    const bool tierExcluded=filtered().empty();
    artistTierFilter.setSelectedId(1,juce::dontSendNotification);refreshArtistChoices();
    artistFilter.setSelectedId(1,juce::dontSendNotification);
    typeFilter.setSelectedId(2,juce::dontSendNotification);
    const bool typeExcluded=filtered().empty();
    typeFilter.setSelectedId(1,juce::dontSendNotification);
    bank=1;search.setText("pair:__skell_pair_probe__",juce::dontSendNotification);
    const int mainIndex=(int)userPresets.size();
    userPresets.push_back(juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("__skell_browser_main_pair_probe__.skellpreset"));
    const int mainEntry=-mainIndex-1;const auto mainKey=favoriteKey(mainEntry);
    artistKeys.add(mainKey);artistNames.add("__skell_artist_probe__");
    setOrganizationValue("type:"+mainKey,"main");
    setOrganizationValue("pair:"+mainKey,"__skell_pair_probe__");
    const auto paired=filtered();
    const bool pairMatch=paired.size()==2 &&
        std::find(paired.begin(),paired.end(),entry)!=paired.end() &&
        std::find(paired.begin(),paired.end(),mainEntry)!=paired.end();
    bank=2;artistTierFilter.setSelectedId(2,juce::dontSendNotification);refreshArtistChoices();
    for(int i=0;i<artistFilter.getNumItems();++i)
        if(artistFilter.getItemText(i)=="__skell_artist_probe__")
            artistFilter.setSelectedId(artistFilter.getItemId(i),juce::dontSendNotification);
    typeFilter.setSelectedId(2,juce::dontSendNotification);
    const auto mainPair=filtered();const bool artistMain=mainPair.size()==1 && mainPair[0]==mainEntry;
    typeFilter.setSelectedId(3,juce::dontSendNotification);
    const auto adlibPair=filtered();const bool artistAdlib=adlibPair.size()==1 && adlibPair[0]==entry;
    // Actual directory navigation is separate from audition and uses the same data as cards.
    search.setText({},juce::dontSendNotification);typeFilter.setSelectedId(1,juce::dontSendNotification);
    artistFilter.setSelectedId(1,juce::dontSendNotification);artistTierFilter.setSelectedId(1,juce::dontSendNotification);
    refreshArtistChoices();
    scroll=0;keyboardCard=-1;
    const auto tiers=directoryCards();
    const bool tierDirectory=directoryMode()==DirectoryMode::tiers && tiers.size()==3 && tiers[0].main>=1 && tiers[0].adlib>=1;
    const bool gridHit=resultAt(directoryCardBounds(0).getCentre())==0 &&
        resultAt({directoryCardBounds(0).getRight()+2,directoryCardBounds(0).getY()+4})==-1;
    focusCard(0);
    keyPressed(juce::KeyPress(juce::KeyPress::rightKey,juce::ModifierKeys(),0));
    const bool gridKeyboard=keyboardCard==1 && directoryMode()==DirectoryMode::tiers;
    const bool enteredTier=activateDirectoryRow(0) && directoryMode()==DirectoryMode::artists;
    const auto artistCards=directoryCards();
    int row=-1;
    for(int n=0;n<(int)artistCards.size();++n)
        if(artistCards[(size_t)n].label=="__skell_artist_probe__")row=n;
    const bool artistDirectory=row>=0 && artistCards[(size_t)row].main==1 && artistCards[(size_t)row].adlib==1;
    const bool enteredArtist=activateDirectoryRow(row) && directoryMode()==DirectoryMode::none && filtered().size()==2;
    vocalTabs[1].onClick();const auto tabMain=filtered();
    vocalTabs[2].onClick();const auto tabAdlib=filtered();
    vocalTabs[0].onClick();
    const bool tabsWork=tabMain.size()==1 && tabMain[0]==mainEntry && tabAdlib.size()==1 && tabAdlib[0]==entry && filtered().size()==2;
    const bool returnedArtist=goBackDirectory() && directoryMode()==DirectoryMode::artists;
    const bool returnedTier=goBackDirectory() && directoryMode()==DirectoryMode::tiers;
    userPresets.pop_back();artistKeys.remove(artistKeys.size()-1);artistNames.remove(artistNames.size()-1);
    artistTierFilter.setSelectedId(1,juce::dontSendNotification);refreshArtistChoices();
    artistFilter.setSelectedId(1,juce::dontSendNotification);
    typeFilter.setSelectedId(1,juce::dontSendNotification);
    search.setText({},juce::dontSendNotification);
    mainAdlibs=false;bank=4;
    // The editable personal tag remains in Edit Details, but Adlibs
    // is no longer a genre-filter dropdown duplicate.
    bool tagEditable=false,filterDuplicate=false;
    for(int i=0;i<genrePicker.getNumItems();++i)
        tagEditable=tagEditable ||
            genrePicker.getItemText(i).equalsIgnoreCase("Ad-libs");
    for(int i=0;i<genreFilter.getNumItems();++i)
        filterDuplicate=filterDuplicate ||
            genreFilter.getItemText(i).equalsIgnoreCase("Ad-libs");

    genreFilter.setSelectedId(1,juce::dontSendNotification);
    const auto genreCards=directoryCards();
    int rageRow=-1;
    for(int n=0;n<(int)genreCards.size();++n)if(genreCards[(size_t)n].label=="Rage")rageRow=n;
    const bool genreDirectory=directoryMode()==DirectoryMode::genres && rageRow>=0 &&
        activateDirectoryRow(rageRow) && genreFilter.getText()=="Rage" &&
        directoryMode()==DirectoryMode::none && goBackDirectory() && directoryMode()==DirectoryMode::genres;
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
    organizationKeys=oldOrganizationKeys;organizationValues=oldOrganizationValues;
    artistTierFilter.setSelectedId(oldTier,juce::dontSendNotification);
    typeFilter.setSelectedId(oldType,juce::dontSendNotification);refreshArtistChoices();
    for(int n=0;n<artistFilter.getNumItems();++n)
        if(artistFilter.getItemText(n)==oldArtistSelection)
            artistFilter.setSelectedId(artistFilter.getItemId(n),juce::dontSendNotification);
    refreshGenreChoices();
    for(int i=0;i<genreFilter.getNumItems();++i)
        if(genreFilter.getItemText(i)==oldFilter)
            genreFilter.setSelectedId(genreFilter.getItemId(i),juce::dontSendNotification);
    bank=oldBank;mainAdlibs=oldMainAdlibs;scroll=oldScroll;
    search.setText(oldQuery,juce::dontSendNotification);
    categoryCountsDirty=true;showDetails(oldDetails);
    return banks.size()==5 && approvedOnly && factorySubmenu && tierMatch && tierExcluded && typeExcluded && pairMatch && artistMain && artistAdlib && tierDirectory && enteredTier && artistDirectory &&
        enteredArtist && gridHit && gridKeyboard && tabsWork && returnedArtist && returnedTier && genreDirectory &&
        personalOnly && artistsOnly && favoritesOnly && personalPreserved &&
        artistPreserved && favoritePreserved && taggedVisible &&
        tagEditable && !filterDuplicate &&
        knownGenres && customPreserved && untaggedPreserved;
}
std::vector<int> PresetBrowser::filtered() const {
    std::vector<int> list;
    const auto q=search.getText().toLowerCase().trim();
    const bool favoritesOnly=bank==3 || q.startsWith("fav:");
    const bool genreActive=genreFilter.getSelectedId()>1;
    const auto selectedGenre=genreFilter.getText();

    if(((bank==0 && !mainAdlibs) || bank==3) && (!genreActive || bank==0)) {
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
    // The separate ADLIB PRESETS section uses the same curated factory program. Factory
    // program indices remain exactly as released; no preset file changes.
    // Only Digital Bones is published in the curated Adlibs and Favorites
    // lists. Hidden historical factory stars remain on disk for compatibility.
    if((bank==0 && mainAdlibs) || (bank==3 && !genreActive)){
        for(int i=0;i<alchemistIndex;++i){
            const auto& item=skellpreset::factoryPresets[(size_t)i];
            if(!visibleBoneAdlib(i))continue;
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
    // Personal sounds remain available via the personal library and its tags.
    {
        for(int i=0;i<(int)userPresets.size();++i) {
            const auto label=userDisplayName(i);
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
            if(q.startsWith("pair:"))found=pairForUser(i).isNotEmpty() &&
                pairForUser(i).equalsIgnoreCase(q.fromFirstOccurrenceOf(":",false,false).trim());
            const auto type=vocalType(i);
            if(bank==0 && type!=(mainAdlibs?"adlib":"main"))found=false;
            if(bank==4 && genreFilter.getSelectedId()<=1 &&
                (genre.isEmpty() || genre.equalsIgnoreCase("Ad-libs")))found=false;
            if(bank==2){
                const auto tier=artistTier(artist);const int tierId=artistTierFilter.getSelectedId();
                if(artist.isEmpty() || (tierId==2 && tier!="Underground") ||
                    (tierId==3 && tier!="Mainstream") || (tierId==4 && tier.isNotEmpty()) ||
                    (artistFilter.getSelectedId()>1 && !artist.equalsIgnoreCase(artistFilter.getText())))found=false;
            }
            if(bank==2 || bank==4){
                const int typeId=typeFilter.getSelectedId();
                if((typeId==2 && type!="main") || (typeId==3 && type!="adlib"))found=false;
            }
            if(bank!=0 && bank!=2 && genreActive &&
               (genreFilter.getSelectedId()==2
                    ?genre.isNotEmpty():!genresForUser(i).contains(selectedGenre,true)))
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
    if(bank==1 || bank==2 || bank==3 || bank==4 || sortPicker.getSelectedId()==4){
        const int mode=sortPicker.getSelectedId();
        const auto label=[this](int entry){
            return entry>=0
                ?juce::String(skellpreset::factoryPresets[(size_t)entry].name)
                :userDisplayName(-entry-1);
        };
        std::stable_sort(list.begin(),list.end(),
            [this,mode,&label](int a,int b){
                if(mode==4) {
                    const int ia=recentKeys.indexOf(favoriteKey(a)),ib=recentKeys.indexOf(favoriteKey(b));
                    const int ra=ia<0?999:ia,rb=ib<0?999:ib;if(ra!=rb)return ra<rb;
                }
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
void PresetBrowser::refreshArtwork(bool decodeBackdrop) {
    artworkButton.setEnabled(!chooserActive && !saveInFlight && canEditCover());
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
    refreshOrganizationEditor();
    refreshSaveControls();
    if(decodeBackdrop)requestBackdrop();
    repaint();
}
void PresetBrowser::requestBackdrop() {
    if(backdropPreset!=activeUserPreset) {
        backdropPreset=activeUserPreset;
        selectedUserBackdrop={}; // never show the previous preset's full cover
    }
    const auto revision=++backdropRevision;
    if(backdropPreset==juce::File{}) {
        backdropQueued=false;
        applyBackdrop({});
        return;
    }
    if(preferencesReadOnly) {
        applyBackdrop(collectBackdrop(backdropPreset));
        return;
    }
    if(backdropInFlight){backdropQueued=true;return;}
    backdropInFlight=true;
    backdropQueued=false;
    const auto preset=backdropPreset;
    juce::Component::SafePointer<PresetBrowser> safe(this);
    juce::Thread::launch([safe,preset,revision] {
        auto snapshot=collectBackdrop(preset);
        juce::MessageManager::callAsync([safe,preset,revision,snapshot=std::move(snapshot)]() mutable {
            if(!safe)return;
            safe->backdropInFlight=false;
            if(revision==safe->backdropRevision && preset==safe->activeUserPreset) {
                safe->applyBackdrop(std::move(snapshot));
                if(safe->onCoverChanged)safe->onCoverChanged();
                safe->repaint();
            }
            if(safe->backdropQueued)safe->requestBackdrop();
        });
    });
}
PresetBrowser::BackdropSnapshot PresetBrowser::collectBackdrop(const juce::File& preset) {
    BackdropSnapshot result;
    if(!preset.existsAsFile())return result;
    auto candidate=preset.withFileExtension(".png");
    if(!candidate.existsAsFile())candidate=preset.withFileExtension(".jpg");
    if(!candidate.existsAsFile() || candidate.getSize()<=0 ||
       candidate.getSize()>8*1024*1024)return result;
    result.source=candidate;
    result.modified=candidate.getLastModificationTime().toMilliseconds();
    auto img=juce::ImageFileFormat::loadFrom(candidate);
    if(img.isValid() && img.getWidth()<=4096 && img.getHeight()<=4096) {
        const float scale=std::min({1.f,1600.f/(float)img.getWidth(),
                                        1000.f/(float)img.getHeight()});
        result.image=img.rescaled(
            juce::jmax(1,juce::roundToInt(img.getWidth()*scale)),
            juce::jmax(1,juce::roundToInt(img.getHeight()*scale)),
            juce::Graphics::highResamplingQuality);
    }
    return result;
}
void PresetBrowser::applyBackdrop(BackdropSnapshot snapshot) {
    backdropSource=std::move(snapshot.source);
    backdropModified=snapshot.modified;
    backdropLoaded=true;
    selectedUserBackdrop=std::move(snapshot.image);
}
void PresetBrowser::chooseArtwork() {
    if(chooserActive || saveInFlight || loadInFlight || coverWriteInFlight || !canEditCover())return;
    const auto destination=activeArtworkDestination(); // fix target before opening dialog
    if(destination==juce::File{})return;
    setChooserActive(true);
    fileChooser=std::make_unique<juce::FileChooser>(
        "Choose artwork for "+(activeUserPreset.existsAsFile()?activeUserPreset.getFileNameWithoutExtension():juce::String("Alchemist")),
        juce::File{}, "*.png;*.jpg;*.jpeg");
    juce::Component::SafePointer<PresetBrowser> safe(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [safe,destination](const juce::FileChooser& chooser) {
            if(!safe)return;
            safe->setChooserActive(false);
            const auto file=chooser.getResult();
            if(file==juce::File{})return;
            safe->coverWriteInFlight=true;
            ++safe->libraryRevision;
            safe->refreshSaveControls();
            safe->setSaveStatus("SAVING COVER...");
            juce::Thread::launch([safe,file,destination] {
                const auto result=[&]() -> juce::Result {
                    if(!file.existsAsFile() || file.getSize()<=0 || file.getSize()>8*1024*1024)
                        return juce::Result::fail("Use a PNG/JPG image smaller than 8 MB.");
                    auto image=juce::ImageFileFormat::loadFrom(file);
                    if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096)
                        return juce::Result::fail("Choose a valid image up to 4096 x 4096.");
                    std::lock_guard<std::mutex> lock(libraryWriteMutex);
                    if(destination.getParentDirectory().createDirectory().failed())
                        return juce::Result::fail("Cannot create artwork directory.");
                    juce::TemporaryFile staging(destination);
                    juce::PNGImageFormat png;
                    const float scale=std::min({1.f,1600.f/(float)image.getWidth(),
                                                    1000.f/(float)image.getHeight()});
                    auto resized=image.rescaled(
                        juce::jmax(1,juce::roundToInt(image.getWidth()*scale)),
                        juce::jmax(1,juce::roundToInt(image.getHeight()*scale)),
                        juce::Graphics::highResamplingQuality);
                    bool written=false;
                    {
                        juce::FileOutputStream out(staging.getFile());
                        if(out.openedOk()) {
                            written=png.writeImageToStream(resized,out);
                            out.flush();
                        }
                    }
                    if(!written || !staging.overwriteTargetFileWithTemporary())
                        return juce::Result::fail("Your previous cover is unchanged. Check library permissions.");
                    return juce::Result::ok();
                }();
                juce::MessageManager::callAsync([safe,result] {
                    if(!safe)return;
                    safe->coverWriteInFlight=false;
                    safe->setSaveStatus(result.wasOk()?"COVER SAVED":"COVER FAILED");
                    safe->refreshSaveControls();
                    if(result.failed())
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Cannot save artwork",result.getErrorMessage());
                    safe->requestLibraryScan();
                    safe->requestBackdrop();
                });
            });
        });
}
void PresetBrowser::refreshCategoryCounts(){
    int artists=0,favorites=0,mainCount=2,genres=0;
    adlibsCount=0;
    if(isFavorite(alchemistIndex))++favorites;
    if(isFavorite(deuceIndex))++favorites;
    for(int i=0;i<alchemistIndex;++i){
        if(!visibleBoneAdlib(i))continue;
        ++adlibsCount;
        if(isFavorite(i))++favorites;
    }
    for(int i=0;i<(int)userPresets.size();++i){
        if(artistForUser(i).isNotEmpty())++artists;
        if(isFavorite(-i-1))++favorites;
        if(vocalType(i)=="main")++mainCount;
        if(vocalType(i)=="adlib")++adlibsCount;
        const auto genre=genreForUser(i);
        if(genre.isNotEmpty() && !genre.equalsIgnoreCase("Ad-libs"))++genres;
    }
    categoryCounts={{mainCount,(int)userPresets.size(),artists,favorites,genres}};
    categoryCountsDirty=false;
}
bool PresetBrowser::testCategoryCountsForTest(){
    refreshCategoryCounts();
    const int size=(int)userPresets.size();
    int factoryAdlibs=0;
    for(int i=0;i<alchemistIndex;++i)
        if(visibleBoneAdlib(i))++factoryAdlibs;
    return !categoryCountsDirty && categoryCounts[0]>=2 && categoryCounts[0]<=size+2 &&
        categoryCounts[1]==size &&
        categoryCounts[2]>=0 && categoryCounts[2]<=size &&
        categoryCounts[3]>=0 && categoryCounts[3]<=size+2+factoryAdlibs &&
        factoryAdlibs==1 && adlibsCount>=1 && adlibsCount<=size+1 && categoryCounts[4]<=size;
}
void PresetBrowser::paint(juce::Graphics&g) {
    Theme::selectFor(processor.state);
    g.setGradientFill(juce::ColourGradient(
        Theme::panel,0.f,0.f,
        Theme::bg,0.f,(float)getHeight(),false));
    g.fillRoundedRectangle(getLocalBounds().toFloat(),10.f);
    g.setColour(Theme::stroke);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),10.f,1.2f);
    g.setColour(Theme::green.withAlpha(.82f));g.fillRoundedRectangle(juce::Rectangle<float>(18.f,67.f,3.f,23.f),1.5f);
    g.setColour(Theme::stroke.withAlpha(.7f));
    g.drawVerticalLine(196,105.f,(float)getHeight()-15.f);
    Theme::text(g,detailsOpen?"MANAGE LIBRARY":"LIBRARY",
                {30,73,154,20},13.5f,Theme::white,true);
    const auto directory=directoryMode();
    const auto entries=directory==DirectoryMode::none?filtered():std::vector<int>{};
    const auto cards=directory==DirectoryMode::none?std::vector<DirectoryCard>{}:directoryCards();
    // Cache library counts on the UI thread: no O(N × sidecar tags)
    // recount on every hover repaint or every mouse movement.
    if(categoryCountsDirty)refreshCategoryCounts();
    juce::String location=mainAdlibs && bank==0?juce::String("Adlib Presets"):banks[bank];
    if(bank==2 && artistTierFilter.getSelectedId()>1)location+=" / "+artistTierFilter.getText();
    if(bank==2 && artistFilter.getSelectedId()>1)location+=" / "+artistFilter.getText();
    if(bank==4 && genreFilter.getSelectedId()>1)location+=" / "+genreFilter.getText();
    const int visibleCount=directory==DirectoryMode::none?(int)entries.size():(int)cards.size();
    Theme::fittedText(g,location+"  /  " +juce::String(visibleCount),
        {hasDirectoryParent()?280:207,73,std::max(0,getWidth()-(hasDirectoryParent()?493:420)),20},13.f,Theme::white,true,juce::Justification::centredLeft);
    if(detailsOpen)
        Theme::text(g,"ESC TO RETURN",{getWidth()-175,75,151,20},
            10,Theme::muted,false,juce::Justification::centredRight);
    if(!detailsOpen) {
        for(int i=0;i<banks.size();++i){
            const auto b=bankBounds(i).toFloat();
            const bool selected=i==bank && (i!=0 || !mainAdlibs);
            const bool hovered=i==hoveredBank;
            g.setColour(selected?Theme::green.withAlpha(.14f):
                        hovered?Theme::panelAlt.brighter(.075f):Theme::panel);
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
                   hoveredAdlibs?Theme::panelAlt.brighter(.075f):
                                 Theme::panel);
        g.fillRoundedRectangle(sub,5.f);
        g.setColour(mainAdlibs?Theme::green.withAlpha(.68f):
                   hoveredAdlibs?Theme::muted.withAlpha(.75f):
                                 Theme::stroke.withAlpha(.45f));
        g.drawRoundedRectangle(sub.reduced(.5f),5.f,.85f);
        Theme::text(g,"Adlib Presets",{29,152,112,19},12.f,
            mainAdlibs?Theme::white:Theme::muted,mainAdlibs);
        Theme::text(g,juce::String(adlibsCount),{144,153,29,17},
            10.f,mainAdlibs?Theme::green:Theme::muted,true,
            juce::Justification::centredRight);
    } else {
        Theme::text(g,"ARTIST",{19,159,166,17},10.f,Theme::muted,true);
        Theme::text(g,"CATEGORY",{19,264,166,17},10.f,Theme::muted,true);
        Theme::text(g,"CREATE YOUR OWN",{19,335,166,17},10.f,Theme::muted,true);
        Theme::text(g,"ARTIST TIER",{207,123,310,20},11.f,Theme::muted,true);
        Theme::text(g,"VOCAL TYPE",{207,190,310,20},11.f,Theme::muted,true);
        Theme::text(g,"MAIN / ADLIB PAIRING",{207,257,310,20},11.f,Theme::muted,true);
        Theme::fittedText(g,"Save a preset first. Assign its artist on the left.",
            {207,416,getWidth()-230,24},11.f,Theme::muted);
        Theme::fittedText(g,"Use the same pairing name on complementary sounds.",
            {207,442,getWidth()-230,24},11.f,Theme::muted);
        if(saveStatus.isNotEmpty())
            Theme::fittedText(g,saveStatus,{207,474,getWidth()-230,20},
                10.5f,saveStatus.startsWith("SAVE FAILED")?Theme::red:Theme::green,
                true,juce::Justification::centredLeft);
    }
    // Don't draw clickable library cards beneath Edit Details input fields.
    // Children on that page remain visible and keyboard accessible.
    g.setColour(Theme::muted.withAlpha(.65f));
    for(int i=0;i<3;++i)g.drawLine((float)getWidth()-5.f-i*4.f,(float)getHeight()-3.f,
        (float)getWidth()-3.f,(float)getHeight()-5.f-i*4.f,1.f);
    if(detailsOpen)return;
    if(directory!=DirectoryMode::none){paintDirectory(g,cards);return;}
    if(!detailsOpen && !listMode){paintPresetGrid(g,entries);return;}
    // Preset cards only appear at a leaf or in search. Artist metadata can be assigned with the sidebar
    // text field; the actual native preset and host program IDs stay intact.
    const int rows=visibleRows();
    for(int i=0;i<rows;i++){
        const int itemIndex=i+scroll;
        if(itemIndex>=(int)entries.size())break;
        const int entry=entries[(size_t)itemIndex];
        const bool custom=entry<0;
        const juce::String itemName=custom
            ?userDisplayName(-entry-1)
            :(entry==alchemistIndex?juce::String("Alchemist")
              :(entry==deuceIndex?juce::String("Deuce")
                :juce::String(skellpreset::factoryPresets[(size_t)entry].name)));
        const juce::String userArtist=custom?artistForUser(-entry-1):juce::String{};
        const juce::String userGenre=custom?genreForUser(-entry-1):juce::String{};
        juce::String itemBank=custom
            ?(userGenre.isNotEmpty()?userGenre.toUpperCase()+
                 (userArtist.isNotEmpty()?"  ·  "+userArtist:juce::String{})
                :(userArtist.isNotEmpty()?"ARTIST: "+userArtist:juce::String("My Presets")))
            :juce::String(skellpreset::factoryPresets[(size_t)entry].bank);
        if(custom){
            const auto type=vocalType(-entry-1);
            if(type=="main")itemBank="MAIN  /  "+itemBank;
            if(type=="adlib")itemBank="ADLIB  /  "+itemBank;
        }
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
            ?((size_t)(-entry-1)<userArtwork.size()?userArtwork[(size_t)(-entry-1)]:juce::Image{})
            :(entry==alchemistIndex?alchemistArtwork:juce::Image{});
        // Preserve text contrast: artwork lives in its own thumbnail, not behind the title.
        const std::array<juce::Colour,4> accents{{Theme::green,Theme::amber,Theme::pink,Theme::white}};
        const int seed=custom?-entry:entry+1;
        const auto accent=accents[(size_t)artworkPaletteIndexForTest(seed)];
        const auto cover=juce::Rectangle<float>(rect.getX()+8.f,(float)y+5.f,65.f,46.f);
        const auto& coverArt=tileArt;
        if(coverArt.isValid()) {
            // Full-bleed card artwork; graphics clipping keeps rounded card edges.
            juce::Graphics::ScopedSaveState artworkSave(g);
            g.reduceClipRegion(cover.toNearestInt());
            g.drawImageWithin(coverArt,(int)cover.getX(),(int)cover.getY(),
                              (int)cover.getWidth(),(int)cover.getHeight(),
                              juce::RectanglePlacement::fillDestination);
        } else paintCover(g,cover,seed,accent);
        g.setColour(Theme::stroke.withAlpha(.8f));
        g.drawRoundedRectangle(cover.reduced(.5f),5.f,1.f);
        g.setColour(Theme::bg.withAlpha(.84f));
        g.fillRoundedRectangle(rect.getRight()-76.f,rect.getY()+8.f,52.f,40.f,5.f);
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
        Theme::fittedText(g,itemName,{292,y+6,juce::jmax(1,getWidth()-376),25},17.f,Theme::white,true,
                          juce::Justification::centredLeft);
        const auto bankColour=custom?Theme::green:Theme::muted;
        Theme::fittedText(g,itemBank,{292,y+33,juce::jmax(1,getWidth()-376),17},11.5f,bankColour,true,
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
    if(entries.size()==1 && search.getText().trim().isEmpty() && !mainAdlibs) {
        // A single active factory sound shouldn't produce an unexplained
        // empty browser. Preview the editable cover instead; no fake
        // content, duplicate presets or heavy image loads in paint().
        const int entry=entries[0];
        const bool custom=entry<0;
        const auto art=custom?((size_t)(-entry-1)<userArtwork.size()?userArtwork[(size_t)(-entry-1)]:juce::Image{})
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
        Theme::text(g,"COVER PREVIEW",{424,282,getWidth()-446,20},10,Theme::green,true);
        Theme::fittedText(g,singlePreviewName(entry),
            {424,308,getWidth()-446,25},16.f,Theme::white,true,
            juce::Justification::centredLeft);
        if(entry==deuceIndex) {
            // Deuce has no editable factory cover. Never suggest that the
            // SET COVER control would modify this factory sound.
            Theme::text(g,"Deuce factory artwork",{424,347,getWidth()-446,18},12,Theme::muted);
            Theme::text(g,"is preserved.",{424,367,getWidth()-446,18},12,Theme::muted);
            Theme::text(g,"Save a personal version",{424,407,getWidth()-446,18},11,Theme::muted);
            Theme::text(g,"to choose new artwork.",{424,424,getWidth()-446,18},11,Theme::muted);
        } else {
            Theme::text(g,"Select the sound, then",{424,347,getWidth()-446,18},12,Theme::muted);
            Theme::text(g,"open Manage Library.",{424,367,getWidth()-446,18},12,Theme::muted);
            Theme::text(g,"Your saved artwork",{424,407,getWidth()-446,18},11,Theme::muted);
            Theme::text(g,"can become the backdrop.",{424,424,getWidth()-446,18},11,Theme::muted);
        }
    }
    if(mainAdlibs && entries.size()==1 && visibleBoneAdlib(entries[0]) && search.getText().trim().isEmpty()){
        // An intentionally small bank: no irrelevant SET COVER instructions
        // for the noneditable Digital Bones factory.
        Theme::fittedText(g,"DIGITAL BONES",{230,205,getWidth()-270,38},
            24.f,Theme::white,true);
        Theme::fittedText(g,"Bit-crushed vocal / rhythmic echoes",
            {230,251,getWidth()-270,25},13.f,Theme::muted);
        Theme::fittedText(g,"One factory Adlib. Save a personal copy to edit artwork.",
            {230,290,getWidth()-270,24},11.f,Theme::muted);
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
        Theme::fittedText(g,title,{222,222,getWidth()-244,32},
                          15.f,Theme::muted,true);
        if(searched || filteredGenre)
            Theme::text(g,"Clear the search or change the category.",
                {222,259,getWidth()-244,26},11,Theme::muted);
        else if(bank==3)
            Theme::text(g,"Star a sound to save it here.",
                {222,259,getWidth()-244,26},11,Theme::muted);
        else if(bank==2)
            Theme::text(g,"Assign an artist in Manage Library to group saved sounds.",
                {222,259,getWidth()-244,26},11,Theme::muted);
    }
    if((int)entries.size()>rows){
        const float railTop=110.f,railHeight=(float)(rows*61-5);
        const float thumbHeight=juce::jmax(30.f,railHeight*rows/(float)entries.size());
        const float travel=railHeight-thumbHeight;
        const float thumbY=railTop+travel*(float)scroll/(float)(entries.size()-rows);
        g.setColour(Theme::stroke.withAlpha(.6f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)getWidth()-12.f,railTop,3.f,railHeight),1.5f);
        g.setColour(Theme::green.withAlpha(.85f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)getWidth()-12.f,thumbY,3.f,thumbHeight),1.5f);
        if(!showVocalTabs())Theme::fittedText(g,"SHOWING "+juce::String(scroll+1)+" - "+
            juce::String(std::min(scroll+rows,(int)entries.size()))+
            " OF "+juce::String((int)entries.size())+"  /  WHEEL TO SCROLL",
            {207,getHeight()-24,getWidth()-230,16},10.f,Theme::muted);
    }
}
void PresetBrowser::showVocalStarters() {
    if(chooserActive || saveInFlight || loadInFlight || coverWriteInFlight)return;
    juce::PopupMenu menu;menu.addSectionHeader("SKELLY SOUND LAB / CHOOSE A SOUND");
    for(int group=0;group<3;++group){
        juce::PopupMenu collection;
        for(int i=0;i<skellpreset::starter::count;++i){
        if(skellpreset::starter::groups[i]!=group)continue;
        juce::PopupMenu profile;profile.addItem(1+i*65,"Original chain");
        for(int m=0;m<4;++m){
            juce::PopupMenu variants;
            for(int k=1;k<=16;++k){const int variant=m*16+k;
                variants.addItem(1+i*65+variant,"Variation "+juce::String(variant).paddedLeft('0',2));}
            profile.addSubMenu(skellpreset::starter::moods[m],variants);
        }
        collection.addSubMenu(juce::String(skellpreset::starter::names[i])+" / "+skellpreset::starter::genres[i],profile);
        }
        menu.addSubMenu(skellpreset::starter::collections[group],collection);
    }
    juce::Component::SafePointer<PresetBrowser> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&starterButton),[safe](int id){
        if(!safe || id<1 || id>skellpreset::starter::count*65 || safe->chooserActive || safe->saveInFlight || safe->loadInFlight || safe->coverWriteInFlight)return;
        if(safe->onBeforeLoad)safe->onBeforeLoad();
        const auto result=safe->processor.loadVocalStarter((id-1)/65,(id-1)%65);
        if(result.failed()){if(safe->onLoadFailed)safe->onLoadFailed();safe->setSaveStatus("STARTER FAILED");return;}
        ++safe->selectionRevision;safe->activeUserPreset=juce::File{};
        safe->refreshArtwork();
        safe->setSaveStatus("STARTER LOADED / SAVE COPY TO KEEP");
        if(safe->onLoaded)safe->onLoaded();
        safe->repaint();
    });
}
void PresetBrowser::loadFactoryFromBrowser(int index) {
    if(index<0 || index>=(int)skellpreset::factoryPresets.size())return;
    if(onBeforeLoad)onBeforeLoad();
    ++selectionRevision;
    activeUserPreset=juce::File{};
    const int previousMode=(int)processor.state.getRawParameterValue("mode")->load();
    processor.loadFactoryPreset(index);
    rememberRecent(index);
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
    startPresetRead(userPresets[(size_t)index],false);
}
void PresetBrowser::auditionEntry(int entry) {
    if(entry>=0)loadFactoryFromBrowser(entry);
    else loadUserFromBrowser(-entry-1);
}
void PresetBrowser::mouseDown(const juce::MouseEvent&e) {
    if(!e.mods.isLeftButtonDown())return;
    if(!usableBounds.isEmpty() && getParentComponent()){
        const bool right=e.x>=getWidth()-8,bottom=e.y>=getHeight()-8;
        resizeLeft=e.x<8;resizeTop=e.y<8;
        const bool edge=right || bottom || resizeLeft || resizeTop;
        const bool header=e.y>=63 && e.y<103;
        if(edge || header){
            resizingWindow=edge;draggingWindow=!edge;userPlaced=true;
            resizeWidth=right;resizeHeight=bottom;
            gestureStartBounds=getBounds();windowGestureStart=e.getEventRelativeTo(getParentComponent()).getPosition();
            return;
        }
    }
    if(!detailsOpen && e.x>=17 && e.x<185){
        if(adlibsBounds().contains(e.getPosition())){
            chooseAdlibs();grabKeyboardFocus();return;
        }
        for(int index=0;index<banks.size();++index)
            if(bankBounds(index).contains(e.getPosition())) {
                chooseBank(index);grabKeyboardFocus();return;
            }
    }
    const int hit=resultAt(e.getPosition());
    if(!e.mods.isPopupMenu() && hit>=0){
        if(directoryMode()!=DirectoryMode::none){
            grabKeyboardFocus();activateDirectoryRow(hit);return;
        }
        const auto entries=filtered();
        if(hit<(int)entries.size()){
            keyboardCard=hit;grabKeyboardFocus();
            const int entry=entries[(size_t)hit];
            if(listMode?(e.x>=getWidth()-77 && e.x<=getWidth()-38):
               directoryCardBounds(hit-scroll).removeFromRight(35).removeFromTop(35).contains(e.getPosition()))
                toggleFavorite(entry);
            else
                auditionEntry(entry);
            // Star clicks never change the loaded preset or DSP settings.
        }
    }
}
void PresetBrowser::mouseDrag(const juce::MouseEvent& e){
    if((!draggingWindow && !resizingWindow) || !getParentComponent())return;
    const auto delta=e.getEventRelativeTo(getParentComponent()).getPosition()-windowGestureStart;
    auto bounds=gestureStartBounds;
    if(draggingWindow)bounds=bounds.translated(delta.x,delta.y);
    else {
        const int minW=juce::jmin(620,usableBounds.getWidth()),minH=juce::jmin(530,usableBounds.getHeight());
        if(resizeLeft)bounds.setLeft(juce::jlimit(juce::jmax(usableBounds.getX(),gestureStartBounds.getRight()-1000),
            gestureStartBounds.getRight()-minW,gestureStartBounds.getX()+delta.x));
        if(resizeTop)bounds.setTop(juce::jlimit(juce::jmax(usableBounds.getY(),gestureStartBounds.getBottom()-610),
            gestureStartBounds.getBottom()-minH,gestureStartBounds.getY()+delta.y));
        if(resizeWidth)bounds.setRight(juce::jlimit(bounds.getX()+minW,juce::jmin(usableBounds.getRight(),bounds.getX()+1000),
            gestureStartBounds.getRight()+delta.x));
        if(resizeHeight)bounds.setBottom(juce::jlimit(bounds.getY()+minH,juce::jmin(usableBounds.getBottom(),bounds.getY()+610),
            gestureStartBounds.getBottom()+delta.y));
    }
    setBounds(constrainWindow(bounds));
}
void PresetBrowser::mouseUp(const juce::MouseEvent&){
    if(draggingWindow || resizingWindow)saveUIPreferences();
    draggingWindow=resizingWindow=resizeWidth=resizeHeight=resizeLeft=resizeTop=false;
}
void PresetBrowser::mouseMove(const juce::MouseEvent& e){
    const bool right=e.x>=getWidth()-8 || e.x<8,bottom=e.y>=getHeight()-8 || e.y<8;
    if(right || bottom){setMouseCursor(right && bottom?
        ((e.x<8)==(e.y<8)?juce::MouseCursor::TopLeftCornerResizeCursor:juce::MouseCursor::TopRightCornerResizeCursor):
        right?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::UpDownResizeCursor);return;}
    if(e.y>=63 && e.y<103 && e.x<195){setMouseCursor(juce::MouseCursor::DraggingHandCursor);return;}
    int nextBank=-1;
    bool nextAdlibs=false;
    if(!detailsOpen && e.x>=17 && e.x<185){
        nextAdlibs=adlibsBounds().contains(e.getPosition());
        if(!nextAdlibs)
            for(int i=0;i<banks.size();++i)
                if(bankBounds(i).contains(e.getPosition())){nextBank=i;break;}
    }
    const int candidate=resultAt(e.getPosition());
    const bool directory=directoryMode()!=DirectoryMode::none;
    const bool onStar=!directory && candidate>=scroll && (listMode?
        e.x>=getWidth()-77 && e.x<=getWidth()-38:
        directoryCardBounds(candidate-scroll).removeFromRight(35).removeFromTop(35).contains(e.getPosition()));
    // Avoid filtering all the cards for every pixel of mouse travel.
    const bool stableCard=candidate>=0 && candidate==hoveredCard &&
        (onStar?hoveredStar==candidate:hoveredStar<0);
    const int next=stableCard?hoveredCard:
        candidate>=0&&candidate<resultCount()?candidate:-1;
    const int nextStar=onStar?next:-1;
    // Restore the cursor immediately when leaving a resize/drag edge, even
    // if no hovered row changed.
    setMouseCursor(juce::MouseCursor(next>=0 || nextBank>=0 || nextAdlibs?
        juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor));
    if(next==hoveredCard && nextStar==hoveredStar &&
       nextBank==hoveredBank && nextAdlibs==hoveredAdlibs)return;
    hoveredCard=next;hoveredStar=nextStar;
    hoveredBank=nextBank;hoveredAdlibs=nextAdlibs;
    repaint();
    setTooltip(nextStar>=0?"Toggle favorite without changing the loaded sound.":
               next>=0?(directory?"Open this group without changing the loaded sound.":"Load this preset; your current sound can be undone."):
               nextAdlibs?"Browse factory and manually classified Adlib presets.":
               nextBank>=0?"Browse this library category.":
               "Click a category, choose a genre or search for a saved sound.");
}
void PresetBrowser::mouseExit(const juce::MouseEvent&){
    if(hoveredCard>=0 || hoveredStar>=0 || hoveredBank>=0 || hoveredAdlibs){
        hoveredCard=-1;hoveredStar=-1;hoveredBank=-1;hoveredAdlibs=false;
        repaint();
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}
void PresetBrowser::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    if(detailsOpen || e.x<197 || e.y<102 || e.y>getHeight()-48 ||
       !std::isfinite(wheel.deltaY) || wheel.deltaY==0.f)return;
    const int maxScroll=std::max(0,resultCount()-pageCapacity());
    // One row per traditional wheel tick, so categories do not jump past
    // results; trackpads use small accumulated deltas on the message thread.
    browserWheelCarry=juce::jlimit(-1.f,1.f,
        (browserWheelCarry*wheel.deltaY<0.f?0.f:browserWheelCarry)+wheel.deltaY);
    if(std::abs(browserWheelCarry)<.18f)return;
    const int direction=(browserWheelCarry<0.f?1:-1)*(directoryMode()==DirectoryMode::none && listMode?1:directoryColumns());
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
        if(goBackDirectory())return true;
        setVisible(false);if(onClose)onClose();return true;
    }
    const bool command=key.getModifiers().isCommandDown();
    const int code=key.getKeyCode();
    if(!detailsOpen && !chooserActive && command && (code=='F' || code=='f')){
        search.grabKeyboardFocus();
        search.selectAll();
        return true;
    }
    if(detailsOpen)return false;
    const bool directory=directoryMode()!=DirectoryMode::none;
    const auto entries=directory?std::vector<int>{}:filtered();
    if(code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey){
        if((directory || !listMode) && keyboardCard>=0 && !key.getModifiers().isAltDown()){
            focusCard(keyboardCard+(code==juce::KeyPress::leftKey?-1:1));return true;
        }
        constexpr std::array<int,6> navigation{{0,-1,4,2,1,3}};
        const int current=bank==0 && mainAdlibs?-1:bank;
        const int position=(int)std::distance(navigation.begin(),
            std::find(navigation.begin(),navigation.end(),current));
        const int next=navigation[(size_t)juce::jlimit(0,5,
            position+(code==juce::KeyPress::leftKey?-1:1))];
        if(next<0)chooseAdlibs();else chooseBank(next);
        return true;
    }
    if(code==juce::KeyPress::upKey || code==juce::KeyPress::downKey){
        focusCard(keyboardCard<0?scroll:keyboardCard+
            (code==juce::KeyPress::upKey?-1:1)*(directory || !listMode?directoryColumns():1));
        return true;
    }
    if(code==juce::KeyPress::homeKey){
        focusCard(0);return true;
    }
    if(code==juce::KeyPress::endKey){
        focusCard(resultCount()-1);return true;
    }
    if(code==juce::KeyPress::pageUpKey ||
       code==juce::KeyPress::pageDownKey){
        const int next=(keyboardCard<0?scroll:keyboardCard)+
            (code==juce::KeyPress::pageUpKey?-pageCapacity():pageCapacity());
        focusCard(next);
        return true;
    }
    if(code==juce::KeyPress::returnKey){
        if(directory){activateDirectoryRow(auditionTargetForTest(keyboardCard,scroll,resultCount()));return true;}
        if(entries.empty())return true;
        const int index=auditionTargetForTest(keyboardCard,scroll,(int)entries.size());
        if(index>=0)auditionEntry(entries[(size_t)index]);
        return true;
    }
    if(code==juce::KeyPress::spaceKey){
        if(directory){activateDirectoryRow(auditionTargetForTest(keyboardCard,scroll,resultCount()));return true;}
        if(keyboardCard>=0 && keyboardCard<(int)entries.size())
            toggleFavorite(entries[(size_t)keyboardCard]);
        return true;
    }
    return false;
}
