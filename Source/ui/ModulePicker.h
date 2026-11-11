#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"
#include <functional>
#include <vector>
#include <algorithm>

// ShallowHost-inspired native effect finder. Unlike a generic ComboBox, this
// search/filter UI is rendered inside the existing JUCE PluginEditor. It never
// instantiates a plugin or touches the audio graph while typing or scrolling.
// All actual changes still go through the original stable slot-type parameter.
class SkellModulePicker final : public juce::Component {
    // Keyboard arrows move the highlighted result while the search retains
    // focus. This avoids stealing text input on a VST3-hosted plugin.
    class SearchBox final : public juce::TextEditor {
    public:
        std::function<void(int)> onArrow;
        bool keyPressed(const juce::KeyPress& key) override {
            if(key.getKeyCode()==juce::KeyPress::upKey) {
                if(onArrow)onArrow(-1);
                return true;
            }
            if(key.getKeyCode()==juce::KeyPress::downKey) {
                if(onArrow)onArrow(1);
                return true;
            }
            return juce::TextEditor::keyPressed(key);
        }
    };
public:
    explicit SkellModulePicker(SkellPresetAudioProcessor& p):processor(p) {
        setVisible(false);
        setWantsKeyboardFocus(true);
        search.setMultiLine(false);
        search.setReturnKeyStartsNewLine(false);
        search.setTextToShowWhenEmpty("Search effects, categories or descriptions...",
                                      Theme::muted);
        search.onTextChange=[this]{topItem=0;filter();repaint();};
        search.onArrow=[this](int step){
            if(results.empty())return;
            selectedRow=juce::jlimit(0,(int)results.size()-1,selectedRow+step);
            if(selectedRow<topItem)topItem=selectedRow;
            if(selectedRow>=topItem+7)topItem=selectedRow-6;
            repaint();
        };
        search.onReturnKey=[this] {
            if(!results.empty())choose(results[(size_t)juce::jlimit(0,(int)results.size()-1,selectedRow)]);
        };
        search.onEscapeKey=[this]{hide();};
        addAndMakeVisible(search);
        filter();
        applyTheme();
    }

    std::function<void(int)> onSelect;
    std::function<void()> onDismiss;

    void applyTheme() {
        Theme::selectFor(processor.state);
        search.setColour(juce::TextEditor::backgroundColourId,Theme::plot);
        search.setColour(juce::TextEditor::textColourId,Theme::white);
        search.setColour(juce::TextEditor::outlineColourId,Theme::stroke);
        search.setColour(juce::TextEditor::focusedOutlineColourId,Theme::green);
        search.setColour(juce::CaretComponent::caretColourId,Theme::green);
        search.setTextToShowWhenEmpty("Search effects, categories or descriptions...",
                                      Theme::muted);
        repaint();
    }
    void open(int type) {
        currentType=type;
        topItem=0;selectedRow=0;
        search.setText({},juce::dontSendNotification);
        filter();
        // When reopening, highlight the effect currently in the rack.
        for(int i=0;i<(int)results.size();++i)
            if(results[(size_t)i]==currentType){
                selectedRow=i;
                topItem=juce::jlimit(0,std::max(0,(int)results.size()-7),i-2);
                break;
            }
        setVisible(true);
        toFront(true);
        search.grabKeyboardFocus();
        repaint();
    }
    void hide() {
        setVisible(false);
        if(onDismiss)onDismiss();
    }
    bool filterMatchesForTest(juce::String query,int type) const {
        return matches(query,type);
    }
    int resultCountForTest() const noexcept {return (int)results.size();}
    void chooseForTest(int type) {choose(type);}

    void resized() override {
        search.setBounds(17,49,getWidth()-34,35);
    }
    void paint(juce::Graphics& g) override {
        Theme::selectFor(processor.state);
        const float W=(float)getWidth(),H=(float)getHeight();
        g.setColour(Theme::panel);
        g.fillRoundedRectangle({0.f,0.f,W,H},9.f);
        g.setColour(Theme::stroke);
        g.drawRoundedRectangle({1.f,1.f,W-2.f,H-2.f},9.f,1.2f);
        g.setColour(Theme::green);
        g.fillRoundedRectangle(juce::Rectangle<float>(18.f,14.f,3.f,22.f),1.f);
        Theme::text(g,"EFFECT LIBRARY",{31,9,255,30},17,Theme::white,true);
        Theme::text(g,"X",{getWidth()-34,9,23,26},15,Theme::muted,true,
                    juce::Justification::centred);
        Theme::text(g,"SEARCH / CLICK TO LOAD / SCROLL FOR MORE",
                    {18,86,getWidth()-34,17},10,Theme::muted);

        constexpr int visible=7,rowHeight=37,rowY=108;
        const int end=std::min((int)results.size(),topItem+visible);
        for(int i=topItem;i<end;++i){
            const int type=results[(size_t)i];
            const int y=rowY+(i-topItem)*rowHeight;
            const bool highlighted=(selectedRow==i);
            const bool active=(currentType==type);
            auto rect=juce::Rectangle<float>(15.f,(float)y,W-30.f,34.f);
            g.setColour(active?Theme::green.withAlpha(.18f):
                        highlighted?Theme::panelAlt.brighter(.12f):Theme::panelAlt);
            g.fillRoundedRectangle(rect,5.f);
            if(active || highlighted){
                g.setColour(active?Theme::green:Theme::stroke);
                g.drawRoundedRectangle(rect.reduced(.5f),5.f,1.1f);
            }
            const auto name=processor.moduleName(type);
            Theme::text(g,name,{27,y+4,198,23},13,active?Theme::green:Theme::white,true);
            Theme::text(g,category(type).toUpperCase(),
                        {getWidth()-130,y+7,100,17},9,
                        active?Theme::green:Theme::muted,false,
                        juce::Justification::centredRight);
        }
        const int foot=getHeight()-26;
        if(results.empty())
            Theme::text(g,"No matching effects. Try 'delay', 'vocal', 'modulation'...",
                        {22,rowY+65,getWidth()-44,26},12,Theme::muted,false,
                        juce::Justification::centred);
        g.setColour(Theme::stroke);
        g.drawLine(18.f,(float)(foot-7),W-18.f,(float)(foot-7),1.f);
        Theme::text(g,"NATIVE DSP / HOST-SAFE BROWSING",{18,foot,300,17},
                    9,Theme::muted);
        Theme::text(g,juce::String(results.size())+" EFFECTS",
                    {getWidth()-125,foot,105,17},9,Theme::green,false,
                    juce::Justification::centredRight);
        if(results.size()>visible) {
            const float trackH=(float)(visible*rowHeight);
            const float gripH=trackH*visible/(float)results.size();
            const float progress=topItem/(float)std::max(1,(int)results.size()-visible);
            g.setColour(Theme::stroke);
            g.fillRoundedRectangle(W-11.f,(float)rowY,3.f,trackH,1.5f);
            g.setColour(Theme::green.withAlpha(.75f));
            g.fillRoundedRectangle(W-11.f,(float)rowY+progress*(trackH-gripH),
                                   3.f,gripH,1.5f);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.y<42 && e.x>=getWidth()-43){hide();return;}
        constexpr int rowY=108,rowHeight=37,visible=7;
        if(e.y<rowY||e.y>=rowY+visible*rowHeight)return;
        const int i=topItem+(e.y-rowY)/rowHeight;
        if(i>=0&&i<(int)results.size()){
            selectedRow=i;
            choose(results[(size_t)i]);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override {
        constexpr int rowY=108,rowHeight=37,visible=7;
        if(e.y>=rowY&&e.y<rowY+visible*rowHeight){
            const int i=topItem+(e.y-rowY)/rowHeight;
            if(i<(int)results.size() && i!=selectedRow){
                selectedRow=i;repaint();
            }
        }
    }
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails& wheel) override {
        const int next=juce::jlimit(0,std::max(0,(int)results.size()-7),
            topItem+(wheel.deltaY<0?2:-2));
        if(next!=topItem) {
            topItem=next;selectedRow=next;repaint();
        }
    }

private:
    static juce::String category(int type){
        switch(type){
            case 1:case 5:case 8:case 13:case 14:return "Tone";
            case 2:case 3:case 4:case 11:case 17:case 19:return "Dynamics";
            case 6:case 7:case 9:case 10:case 18:return "Space";
            case 15:case 16:case 20:case 21:case 22:return "Motion";
            case 12:return "External";
            default:return "Utility";
        }
    }
    static juce::String terms(int type){
        switch(type){
            case 1:return "frequency bands filter";
            case 2:case 3:case 17:case 19:return "compression loud voice";
            case 4:return "noise threshold suppression";
            case 5:case 14:return "drive grit distortion lofi";
            case 6:case 18:return "room reverb ambience";
            case 7:case 9:return "echo throw delay adlib";
            case 8:return "harmonics treble";
            case 10:case 16:case 20:return "stereo layers doubler";
            case 11:return "sibilance consonants";
            case 12:return "vst3 third party insert";
            case 13:return "radio telephone lo-fi";
            case 15:case 21:return "sweep wobble flange";
            case 22:return "alien metallic voice modulation";
            default:return {};
        }
    }
    bool matches(const juce::String& query,int type) const {
        const auto text=query.trim();
        if(text.isEmpty())return true;
        const auto searchable=processor.moduleName(type)+" "+
                              category(type)+" "+terms(type);
        auto words=juce::StringArray::fromTokens(text," \t","\"");
        for(const auto& word:words)
            if(!searchable.containsIgnoreCase(word))return false;
        return true;
    }
    void filter(){
        results.clear();
        for(int type=0;type<SkellPresetAudioProcessor::NumberOfModules;++type)
            if(matches(search.getText(),type))results.push_back(type);
        // Stable sort within categories retains each existing module ID
        // and automation mapping, while giving users a predictable catalog.
        std::stable_sort(results.begin(),results.end(),[](int a,int b){
            return category(a).compareIgnoreCase(category(b))<0;
        });
        selectedRow=0;topItem=0;
    }
    void choose(int type){
        if(type>=0 && type<SkellPresetAudioProcessor::NumberOfModules) {
            currentType=type;
            hide();
            if(onSelect)onSelect(type);
        }
    }
    SkellPresetAudioProcessor& processor;
    SearchBox search;
    std::vector<int> results;
    int currentType=0,selectedRow=0,topItem=0;
};
