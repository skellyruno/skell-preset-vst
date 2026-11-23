#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>
class PresetBrowser final:public juce::Component,public juce::SettableTooltipClient {
public:
    explicit PresetBrowser(SkellPresetAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void open();
    void applyTheme();
    // Cached on the message thread; no image file I/O from paint/audio.
    juce::Image selectedCover() const;
    int visibleFactoryPresetsForTest() const;
    bool canSetCoverForTest() const {return canEditCover();}
    bool testMainVocalsPresetFilterForTest(); // primary whitelist and independent artists/favorites
    bool testBrowserLayoutForTest(); // visible, nonoverlapping controls on both pages
    bool testBrowserKeyboardForTest(); // roving result focus without triggering audio loads
    bool testBrowserSortingForTest(); // no data mutation during sorting
    bool testCategoryCountsForTest(); // cached library counts track user metadata
    // The highlighted keyboard card must be the one Enter auditions, even
    // after Home/End/PageDown scrolling. With no highlight use the first
    // visible card; never index a filtered list out of bounds.
    static constexpr int auditionTargetForTest(int focus,int scroll,int count) noexcept {
        if(count<=0)return -1;
        return focus>=0 && focus<count?focus:
               (scroll<0?0:scroll>=count?count-1:scroll);
    }
    static constexpr int artworkPaletteIndexForTest(int seed) noexcept {
        // This exact helper is used by the real paint path. The previous
        // seed%5 indexed a four-colour palette out of bounds for 4/9/44...
        return (seed<0?0:seed)%4;
    }
    static bool presetCardHitForTest(bool editing,int x,int y,int width) noexcept {
        // Each visible card is 56px tall, 61px apart. The five-pixel gaps
        // are never click-to-load regions, nor is the Edit Details overlay.
        const int relative=y-110;
        return !editing && x>=207 && x<width-17 &&
               relative>=0 && relative<6*61 && relative%61<56;
    }
    void loadFactoryForHistoryTest(int index) {loadFactoryFromBrowser(index);}
    juce::File historySelection() const {return activeUserPreset;}
    void restoreHistorySelection(const juce::File& selection) {
        activeUserPreset=selection;
        refreshArtwork();
    }
    std::function<void()> onBeforeLoad;
    std::function<void()> onLoadFailed;
    std::function<void()> onClose;
    std::function<void()> onLoaded;
    std::function<void()> onCoverChanged;
private:
    SkellPresetAudioProcessor& processor;
    juce::TextEditor search,artistEditor,categoryEditor;
    juce::ComboBox genrePicker,genreFilter,sortPicker;
    juce::TextButton importButton{"IMPORT"},exportButton{"SAVE PRESET"},
        artworkButton{"SET COVER"},artistButton{"SET ARTIST"},
        genreButton{"TAG"},detailsButton{"EDIT DETAILS"},
        addCategoryButton{"ADD CATEGORY"},clearSearchButton{"CLEAR"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool chooserActive=false;
    void setChooserActive(bool active);
    void chooseImport();
    void chooseExport();
    void chooseArtwork();
    void refreshArtwork();
    int bank=0,scroll=0,hoveredCard=-1,hoveredStar=-1,keyboardCard=-1,
        hoveredBank=-1;
    bool hoveredAdlibs=false;
    float browserWheelCarry=0.f;
    std::array<int,4> categoryCounts{};
    bool categoryCountsDirty=true;
    void refreshCategoryCounts();
    bool detailsOpen=false;
    bool mainAdlibs=false; // subcategory nested under Main Vocals, not a bank/dropdown
    int adlibsCount=0; // cached on message thread with other category counts
    void showDetails(bool open);
    void chooseBank(int index);
    void chooseAdlibs();
    void focusCard(int index);
    juce::StringArray favoriteKeys; // sidecar only; not in VST3 state or preset ZIP
    juce::StringArray artistKeys,artistNames; // user library sidecar, never audio state
    juce::StringArray genreKeys,genreNames; // manually assigned; never auto-detected
    juce::StringArray customCategories; // user-owned local sidecar, not a preset
    std::vector<juce::File> userPresets;
    std::vector<juce::Image> userArtwork; // UI-thread thumbnail cache, no filesystem in paint()
    juce::Image alchemistArtwork; // editable factory cover, stored separately
    juce::Image selectedUserBackdrop; // single full-res cached artwork
    juce::File activeUserPreset;
    std::vector<int> filtered() const; // >=0 factory ID; <0 user library index (-i-1)
    juce::String favoriteKey(int entry) const;
    bool isFavorite(int entry) const;
    void toggleFavorite(int entry);
    void loadFavorites();
    bool saveFavorites() const;
    juce::File favoritesFile() const;
    juce::File artistsFile() const;
    juce::String artistForUser(int index) const;
    void loadArtists();
    bool saveArtists() const;
    bool setArtistForUser(int index,const juce::String& name);
    juce::File genresFile() const;
    juce::String genreForUser(int index) const;
    void loadGenres();
    bool saveGenres() const;
    bool setGenreForUser(int index,const juce::String& label);
    juce::File categoriesFile() const;
    void loadCustomCategories();
    bool saveCustomCategories() const;
    void refreshGenreChoices();
    bool isKnownCategory(const juce::String& label) const;
    juce::File libraryDirectory() const;
    juce::File activeArtworkDestination() const;
    bool canEditCover() const;
    void reloadAlchemistArtwork();
    void scanLibrary();
    bool copyToLibrary(const juce::File& file);
    void loadFactoryFromBrowser(int index);
    void loadUserFromBrowser(int index);
    void auditionEntry(int entry);
};
