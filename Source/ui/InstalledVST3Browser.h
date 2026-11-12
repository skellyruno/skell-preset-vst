#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../host/InstalledVST3Index.h"
#include "Theme.h"
#include <functional>
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
        search.onTextChange=[this]{refreshResults();};
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

    void open() {
        search.setText({},juce::dontSendNotification);
        beginScan(skellpreset::host::InstalledVST3Index::defaultRoots());
        applyTheme();
        setVisible(true);
        toFront(true);
        search.grabKeyboardFocus();
    }
    void hide() {
        stopTimer();
        setVisible(false);
    }
    void applyTheme() {
        Theme::selectFor(processor.state);
        search.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
        search.setColour(juce::TextEditor::textColourId,Theme::white);
        search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
        search.setColour(juce::TextEditor::focusedOutlineColourId,Theme::green);
        list.setColour(juce::ListBox::backgroundColourId,Theme::plot);
        list.setColour(juce::ListBox::outlineColourId,Theme::stroke);
        for(auto* button:{&rescan,&chooseFile,&cancel,&load}) {
            button->setColour(juce::TextButton::buttonColourId,Theme::panelAlt);
            button->setColour(juce::TextButton::textColourOffId,Theme::white);
        }
        load.setColour(juce::TextButton::textColourOffId,Theme::green);
        repaint();
    }
    void resized() override {
        const auto w=getWidth(),h=getHeight();
        search.setBounds(17,54,w-34,35);
        list.setBounds(17,112,w-34,h-165);
        rescan.setBounds(17,h-40,90,27);
        chooseFile.setBounds(115,h-40,155,27);
        cancel.setBounds(w-193,h-40,77,27);
        load.setBounds(w-108,h-40,91,27);
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

    void beginScan(const juce::Array<juce::File>& roots) {
        stopTimer();
        index.start(roots);
        refreshResults();
        if(!index.finished())startTimer(35);
        repaint();
    }
    void timerCallback() override {
        const bool changed=index.step(2);
        if(changed)refreshResults();
        if(index.finished())stopTimer();
        repaint();
    }
    void refreshResults() {
        visible=index.filter(search.getText());
        list.updateContent();
        if(visible.empty())list.deselectAllRows();
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
        Theme::text(g,item.name,{12,2,width-24,23},13,selected?Theme::green:Theme::white,true);
        Theme::text(g,item.file.getFullPathName(),{13,24,width-27,16},9,Theme::muted);
    }
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override {
        if(row>=0 && row<(int)visible.size())loadSelected(row);
    }
    void loadSelected(int requestedRow=-1) {
        const int row=requestedRow>=0?requestedRow:list.getSelectedRow();
        if(row<0 || row>=(int)visible.size())return;
        const auto file=index.entries()[(std::size_t)visible[(std::size_t)row]].file;
        hide();
        if(onLoad)onLoad(file);
    }
};
