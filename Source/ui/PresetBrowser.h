#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>
class PresetBrowser final:public juce::Component {
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
    juce::ComboBox genrePicker,genreFilter;
    juce::TextButton importButton{"IMPORT"},exportButton{"SAVE PRESET"},
        artworkButton{"SET COVER"},artistButton{"SET ARTIST"},
        genreButton{"TAG"},detailsButton{"EDIT DETAILS"},
        addCategoryButton{"ADD CATEGORY"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool chooserActive=false;
    void setChooserActive(bool active);
    void chooseImport();
    void chooseExport();
    void chooseArtwork();
    void refreshArtwork();
    int bank=0,scroll=0,hoveredCard=-1,keyboardCard=-1;
    float browserWheelCarry=0.f;
    bool detailsOpen=false;
    void showDetails(bool open);
    void chooseBank(int index);
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
