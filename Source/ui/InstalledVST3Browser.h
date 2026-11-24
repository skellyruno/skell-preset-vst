#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../host/InstalledVST3Index.h"
#include "Theme.h"
#include <functional>
#include <array>
#include <vector>

// Native Skell visual browser. The index reads only directories and bundle
// names; choosing LOAD is the sole path that instantiates a third-party VST3.
// This component is owned by SlotEditor and never touches the audio callback.
class SkellInstalledVST3Browser final : public juce::Component,
                                       private juce::ListBoxModel,
                                       private juce::Timer {
public:
    explicit SkellInstalledVST3Browser(SkellPresetAudioProcessor& p):
        processor(p),list("Installed VST3 bundles",this) {
        setVisible(false);
        search.setMultiLine(false);
        search.setReturnKeyStartsNewLine(false);
        search.setTextToShowWhenEmpty("Search installed VST3 bundles...",Theme::muted);
        search.onTextChange=[this]{refreshResults(selectedFile());};
        search.onReturnKey=[this]{loadSelected();};
        search.onEscapeKey=[this]{hide();};
        list.setRowHeight(42);
        list.setMultipleSelectionEnabled(false);
        rescan.setButtonText("RESCAN");
        chooseFile.setButtonText("BROWSE FILE...");
        cancel.setButtonText("CLOSE");
        load.setButtonText("LOAD");
        rescan.onClick=[this]{beginScan(skellpreset::host::InstalledVST3Index::defaultRoots());};
        chooseFile.onClick=[this]{
            hide();
            if(onBrowseFile)onBrowseFile();
        };
        cancel.onClick=[this]{hide();};
        load.onClick=[this]{loadSelected();};
        for(auto* button:{&rescan,&chooseFile,&cancel,&load})
            addAndMakeVisible(*button);
        addAndMakeVisible(search);
        addAndMakeVisible(list);
        applyTheme();
    }
    ~SkellInstalledVST3Browser() override {stopTimer();}

    std::function<void(const juce::File&)> onLoad;
    std::function<void()> onBrowseFile;
    std::function<void()> onDismiss;

    void open() {
        scanPreferredFile=juce::File{};
        search.setText({},juce::dontSendNotification);
        beginScan(skellpreset::host::InstalledVST3Index::defaultRoots());
        applyTheme();
        setVisible(true);
        toFront(true);
        search.grabKeyboardFocus();
    }
    void hide() {
        stopTimer();
        const bool wasVisible=isVisible();
        setVisible(false);
        if(wasVisible && onDismiss)onDismiss();
    }
    void applyTheme() {
        Theme::selectFor(processor.state);
        search.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
        search.setColour(juce::TextEditor::textColourId,Theme::white);
        search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
        search.setColour(juce::TextEditor::focusedOutlineColourId,Theme::green);
        search.setColour(juce::CaretComponent::caretColourId,Theme::green);
        list.setColour(juce::ListBox::backgroundColourId,Theme::plot);
        list.setColour(juce::ListBox::outlineColourId,Theme::stroke);
        for(auto* button:{&rescan,&chooseFile,&cancel,&load}) {
            button->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
            button->setColour(juce::TextButton::textColourOffId,Theme::white);
        }
        load.setColour(juce::TextButton::textColourOffId,Theme::green);
        repaint();
    }
    // Responsive 4-action footer: wide and 390px layouts share exact
    // rectangles with the hit-target regression instead of overlapping.
    static std::array<juce::Rectangle<int>,4> footerBoundsFor(int w,int h) {
        if(w>=472)
            return {{ {17,h-40,90,27}, {115,h-40,155,27},
                      {w-193,h-40,77,27}, {w-108,h-40,91,27} }};
        return {{ {17,h-40,75,27}, {100,h-40,125,27},
                  {w-157,h-40,64,27}, {w-85,h-40,68,27} }};
    }
    static bool testFooterBoundsForTest() {
        for(int w:{390,422,471,472,600,860}) {
            const int h=240;
            const auto bounds=juce::Rectangle<int>(0,0,w,h);
            const auto footer=footerBoundsFor(w,h);
            for(size_t i=0;i<footer.size();++i){
                if(!bounds.contains(footer[i]))return false;
                for(size_t j=i+1;j<footer.size();++j)
                    if(footer[i].intersects(footer[j]))return false;
            }
        }
        return true;
    }
    bool testLayoutForTest() const {
        const auto bounds=getLocalBounds();
        const juce::Component* controls[]{&search,&list,&rescan,&chooseFile,&cancel,&load};
        if(getWidth()<390 || getHeight()<240)return false;
        for(int i=0;i<6;++i) {
            if(!bounds.contains(controls[i]->getBounds()))return false;
            for(int j=i+1;j<6;++j)
                if(controls[i]->getBounds().intersects(controls[j]->getBounds()))
                    return false;
        }
        const auto expected=footerBoundsFor(getWidth(),getHeight());
        return rescan.getBounds()==expected[0] &&
               chooseFile.getBounds()==expected[1] &&
               cancel.getBounds()==expected[2] &&
               load.getBounds()==expected[3] &&
               testFooterBoundsForTest() && testSelectedFileRowForTest();
    }
    static bool testSelectedFileRowForTest() {
        // Synthetic names only: no VST3 is instantiated or scanned.
        const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto a=root.getChildFile("skell-order-a.vst3");
        const auto b=root.getChildFile("skell-order-b.vst3");
        const std::vector<skellpreset::host::InstalledVST3Bundle> entries{{a,"A"},{b,"B"}};
        const std::vector<int> visibleRows{0,1};
        return rowForFile(b,visibleRows,entries)==1 &&
               rowForFile(a,visibleRows,entries)==0 &&
               rowForFile(root.getChildFile("skell-order-c.vst3"),
                          visibleRows,entries)==-1 &&
               rowForFile(b,std::vector<int>{0},entries)==-1;
    }
    void resized() override {
        const auto w=getWidth(),h=getHeight();
        search.setBounds(17,54,w-34,35);
        list.setBounds(17,112,w-34,h-165);
        const auto footer=footerBoundsFor(w,h);
        rescan.setBounds(footer[0]);
        chooseFile.setBounds(footer[1]);
        cancel.setBounds(footer[2]);
        load.setBounds(footer[3]);
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        const auto area=getLocalBounds().toFloat();
        g.setColour(Theme::panel);
        g.fillRoundedRectangle(area,8.f);
        g.setColour(Theme::stroke);
        g.drawRoundedRectangle(area.reduced(.6f),8.f,1.3f);
        g.setColour(Theme::green);
        g.fillRoundedRectangle({17.f,12.f,3.f,23.f},1.f);
        Theme::text(g,"INSTALLED VST3 EFFECTS",{29,9,getWidth()-46,29},16,Theme::white,true);
        const auto count=(int)index.entries().size();
        juce::String status;
        if(!index.finished())
            status="SEARCHING INSTALLED FOLDERS...  "+juce::String(count)+" FOUND";
        else if(index.truncated())
            status=juce::String(count)+" FOUND  /  INDEX LIMIT REACHED";
        else
            status=juce::String(count)+" BUNDLES FOUND  /  NAMES ONLY, NOT SCANNED";
        Theme::text(g,status,{17,92,getWidth()-35,18},10,Theme::muted);
        if(index.finished() && index.entries().empty())
            Theme::text(g,"No installed VST3 found in standard folders. Use BROWSE FILE...",
                        {36,180,getWidth()-75,40},12,Theme::muted);
        else if(index.finished() && visible.empty())
            Theme::text(g,"No results for this search.",{36,180,getWidth()-75,40},12,Theme::muted);
    }

private:
    SkellPresetAudioProcessor& processor;
    skellpreset::host::InstalledVST3Index index;
    std::vector<int> visible;
    juce::TextEditor search;
    juce::ListBox list;
    juce::TextButton rescan,chooseFile,cancel,load;

    // The index inserts and re-sorts bundles while scanning. Save the file
    // identity BEFORE stepping, otherwise a new alphabetically-earlier VST3
    // can cause the selected row to silently load the wrong plugin.
    juce::File selectedFile() const {
        const int row=list.getSelectedRow();
        if(row<0 || row>=(int)visible.size())return {};
        const int entry=visible[(size_t)row];
        if(entry<0 || entry>=(int)index.entries().size())return {};
        return index.entries()[(size_t)entry].file;
    }
    static int rowForFile(const juce::File& file,const std::vector<int>& rows,
                          const std::vector<skellpreset::host::InstalledVST3Bundle>& entries) {
        if(file==juce::File{})return -1;
        for(int row=0;row<(int)rows.size();++row){
            const int entry=rows[(size_t)row];
            if(entry>=0 && entry<(int)entries.size() &&
               entries[(size_t)entry].file==file)return row;
        }
        return -1;
    }
    void beginScan(const juce::Array<juce::File>& roots) {
        stopTimer();
        const auto before=selectedFile();
        index.start(roots);
        scanPreferredFile=before;
        refreshResults(before);
        if(!index.finished())startTimer(35);
        repaint();
    }
    void timerCallback() override {
        const auto before=selectedFile();
        const bool wasFinished=index.finished();
        const bool changed=index.step(2);
        if(changed){
            const auto preferred=scanPreferredFile!=juce::File{}?scanPreferredFile:before;
            refreshResults(preferred);
            if(selectedFile()==scanPreferredFile)scanPreferredFile=juce::File{};
        }
        if(index.finished()){stopTimer();scanPreferredFile=juce::File{};}
        // Avoid 28 full browser repaints/second while crawling slow disks.
        // Only found items and the terminal scan status require invalidation.
        if(!changed && !wasFinished && index.finished())repaint();
    }
    void refreshResults(const juce::File& preserve={}) {
        visible=index.filter(search.getText());
        list.setVisible(!visible.empty());
        list.updateContent();
        const int selected=rowForFile(preserve,visible,index.entries());
        if(selected>=0)list.selectRow(selected);
        else if(visible.empty())list.deselectAllRows();
        else list.selectRow(0);
        load.setEnabled(!visible.empty());
        list.repaint();
        repaint();
    }
    int getNumRows() override {return (int)visible.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override {
        if(row<0 || row>=(int)visible.size())return;
        Theme::selectFor(processor.state);
        const auto& item=index.entries()[(std::size_t)visible[(std::size_t)row]];
        g.setColour(selected?Theme::green.withAlpha(.20f):Theme::plot);
        g.fillRoundedRectangle({3.f,2.f,(float)width-6.f,(float)height-4.f},4.f);
        if(selected) {
            g.setColour(Theme::green.withAlpha(.65f));
            g.drawRoundedRectangle({3.f,2.f,(float)width-6.f,(float)height-4.f},4.f,1.f);
        }
        Theme::fittedText(g,item.name,{12,2,width-24,23},13.f,
                          selected?Theme::green:Theme::white,true);
        Theme::fittedText(g,item.file.getFullPathName(),{13,24,width-27,16},
                          9.f,Theme::muted);
    }
    void listBoxItemClicked(int,const juce::MouseEvent&) override {
        // An explicit manual choice wins over the pending rescan preference.
        scanPreferredFile=juce::File{};
    }
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent& e) override {
        if(e.mods.isLeftButtonDown() && row>=0 && row<(int)visible.size())loadSelected(row);
    }
    juce::File scanPreferredFile;
    void loadSelected(int requestedRow=-1) {
        const int row=requestedRow>=0?requestedRow:list.getSelectedRow();
        if(row<0 || row>=(int)visible.size())return;
        const auto file=index.entries()[(std::size_t)visible[(std::size_t)row]].file;
        if(!file.exists()){
            beginScan(skellpreset::host::InstalledVST3Index::defaultRoots());
            return; // Removed while the browser was open: refresh stale rows.
        }
        hide();
        if(onLoad)onLoad(file);
    }
};
