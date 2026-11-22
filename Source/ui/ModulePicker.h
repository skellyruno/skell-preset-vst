#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"
#include "../chain/EffectRegistry.h"
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
            if(selectedRow>=topItem+visibleRows())
                topItem=selectedRow-visibleRows()+1;
            topItem=juce::jlimit(0,std::max(0,(int)results.size()-visibleRows()),topItem);
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
    // External VST3 belongs to a dedicated footer action, never the DSP list.
    std::function<void()> onExternal;
    std::function<void()> onDismiss;
    // The double-click effect library floats within the editor. Reopening
    // preserves its last dragged position; editor resize clamps it onscreen.
    bool wasMoved() const noexcept {return movedByUser;}
    int visibleRows() const noexcept {
        return juce::jlimit(4,10,(getHeight()-173)/37);
    }
    void positionWithin(juce::Rectangle<int> initial) {
        if(!movedByUser) {setBounds(initial);return;}
        if(auto* parent=getParentComponent()) {
            const int w=juce::jlimit(340,juce::jmin(800,parent->getWidth()-12),getWidth());
            const int h=juce::jlimit(345,juce::jmin(510,parent->getHeight()-12),getHeight());
            const int x=juce::jlimit(0,juce::jmax(0,parent->getWidth()-w),getX());
            const int y=juce::jlimit(0,juce::jmax(0,parent->getHeight()-h),getY());
            setBounds(x,y,w,h);
        }
    }
    bool testResizeKeepsSizeForTest() {
        auto* parent=getParentComponent();
        if(!parent)return false;
        const auto oldBounds=getBounds();
        const bool oldMoved=movedByUser;
        movedByUser=true;
        setSize(juce::jmin(800,oldBounds.getWidth()+16),
                juce::jmin(510,oldBounds.getHeight()+16));
        positionWithin({405,66,470,400});
        const bool correct=getWidth()==oldBounds.getWidth()+16 &&
             getHeight()==oldBounds.getHeight()+16 &&
             parent->getLocalBounds().contains(getBounds());
        movedByUser=oldMoved;
        setBounds(oldBounds);
        return correct;
    }

    bool testLayoutForTest() const {
        if(getWidth()<340 || getHeight()<345)return false;
        const auto bounds=getLocalBounds();
        const auto heading=juce::Rectangle<int>(31,9,150,30);
        const auto dragLabel=juce::Rectangle<int>(200,15,getWidth()-243,16);
        const auto resultText=juce::Rectangle<int>(27,112,juce::jmax(110,getWidth()-165),23);
        const auto categoryText=juce::Rectangle<int>(getWidth()-130,115,100,17);
        const auto firstRow=juce::Rectangle<int>(15,108,getWidth()-30,34);
        const auto lastRow=firstRow.translated(0,(visibleRows()-1)*37);
        const auto footer=juce::Rectangle<int>(17,getHeight()-55,getWidth()-34,26);
        return bounds.contains(search.getBounds()) && bounds.contains(firstRow) &&
            bounds.contains(lastRow) && bounds.contains(footer) &&
            !search.getBounds().intersects(firstRow) &&
            !heading.intersects(dragLabel) &&
            !resultText.intersects(categoryText) &&
            !lastRow.intersects(footer);
    }
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
                topItem=juce::jlimit(0,std::max(0,(int)results.size()-visibleRows()),i-2);
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
    bool testVisibleKeyboardRow() {
        const int count=visibleRows();
        if(static_cast<int>(results.size())<=count)return true;
        const int oldSelected=selectedRow,oldTop=topItem;
        topItem=0;selectedRow=0;
        for(int i=0;i<count;++i)if(search.onArrow)search.onArrow(1);
        const bool correct=selectedRow==count && topItem==1 &&
                           selectedRow>=topItem && selectedRow<topItem+visibleRows();
        selectedRow=oldSelected;topItem=oldTop;repaint();
        return correct;
    }
    void chooseForTest(int type) {choose(type);}

    void resized() override {
        search.setBounds(17,49,getWidth()-34,35);
        topItem=juce::jlimit(0,std::max(0,(int)results.size()-visibleRows()),topItem);
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
        Theme::fittedText(g,"EFFECT LIBRARY",{31,9,150,30},16,Theme::white,true);
        Theme::text(g,"DRAG / RESIZE",{200,15,getWidth()-243,16},9,Theme::muted,
                    false,juce::Justification::centredRight);
        Theme::text(g,"X",{getWidth()-34,9,23,26},15,Theme::muted,true,
                    juce::Justification::centred);
        Theme::text(g,"SEARCH  •  CLICK TO LOAD  •  SCROLL EFFECTS",
                    {18,86,getWidth()-34,17},11.4f,Theme::muted.brighter(.18f));

        const int visible=visibleRows();
        constexpr int rowHeight=37,rowY=108;
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
            // Reserve the category chip's right-side area at 340px minimum
            // width; the old fixed 203px name field overlapped it by 20px.
            Theme::fittedText(g,name,{27,y+4,juce::jmax(110,getWidth()-165),23},14.0f,
                              active?Theme::green:Theme::white,true);
            Theme::text(g,category(type).toUpperCase(),
                        {getWidth()-130,y+7,100,17},10.6f,
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
        // Dedicated external-insert action, separated from native DSP results.
        const auto externalRow=juce::Rectangle<float>(17.f,H-55.f,W-34.f,26.f);
        g.setColour(Theme::green.withAlpha(.12f));
        g.fillRoundedRectangle(externalRow,5.f);
        g.setColour(Theme::green.withAlpha(.72f));
        g.drawRoundedRectangle(externalRow.reduced(.6f),5.f,1.f);
        Theme::text(g,"+ LOAD EXTERNAL VST3 PLUGIN",
                    {28,getHeight()-52,getWidth()-58,21},12,Theme::green,true);
        Theme::text(g,"NATIVE FX  /  VST3 BELOW",{18,foot,getWidth()-160,17},
                    9,Theme::muted);
        g.setColour(Theme::green.withAlpha(.76f));
        const float right=W-5.f,bottom=H-5.f;
        for(int k=0;k<3;++k)
            g.drawLine(right-9.f+4.f*k,bottom,right,bottom-9.f+4.f*k,1.1f);
        Theme::text(g,juce::String(results.size())+" EFFECTS",
                    {getWidth()-125,foot,105,17},9,Theme::green,false,
                    juce::Justification::centredRight);
        if(static_cast<int>(results.size())>visible) {
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
        if(e.x>=getWidth()-23 && e.y>=getHeight()-23) {
            resizing=true;
            resizeStart=getBounds();
            toFront(true);
            setMouseCursor(juce::MouseCursor::BottomRightCornerResizeCursor);
            return;
        }
        if(e.y<42 && e.x>=getWidth()-43){hide();return;}
        if(e.y>=0 && e.y<42 && e.x>=0 && e.x<getWidth()-45) {
            dragHeader=true;
            dragOrigin=getPosition();
            toFront(true);
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            return;
        }
        if(e.y>=getHeight()-55 && e.y<getHeight()-29) {
            hide();
            if(onExternal)onExternal();
            return;
        }
        constexpr int rowY=108,rowHeight=37;
        const int visible=visibleRows();
        if(e.y<rowY||e.y>=rowY+visible*rowHeight)return;
        const int i=topItem+(e.y-rowY)/rowHeight;
        if(i>=0&&i<(int)results.size()){
            selectedRow=i;
            choose(results[(size_t)i]);
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(resizing) {
            if(auto* parent=getParentComponent()) {
                const int w=juce::jlimit(340,juce::jmin(800,parent->getWidth()-12),
                    resizeStart.getWidth()+e.getDistanceFromDragStartX());
                const int h=juce::jlimit(345,juce::jmin(510,parent->getHeight()-12),
                    resizeStart.getHeight()+e.getDistanceFromDragStartY());
                const int x=juce::jlimit(0,parent->getWidth()-w,resizeStart.getX());
                const int y=juce::jlimit(0,parent->getHeight()-h,resizeStart.getY());
                movedByUser=true;
                setBounds(x,y,w,h);
            }
            return;
        }
        if(!dragHeader)return;
        if(auto* parent=getParentComponent()) {
            const int x=juce::jlimit(0,juce::jmax(0,parent->getWidth()-getWidth()),
                                    dragOrigin.x+e.getDistanceFromDragStartX());
            const int y=juce::jlimit(0,juce::jmax(0,parent->getHeight()-getHeight()),
                                    dragOrigin.y+e.getDistanceFromDragStartY());
            if(x!=getX() || y!=getY()) {
                movedByUser=true;
                setTopLeftPosition(x,y);
            }
        }
    }
    void mouseUp(const juce::MouseEvent&) override {
        dragHeader=false;
        resizing=false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    void mouseMove(const juce::MouseEvent& e) override {
        if(e.x>=getWidth()-23 && e.y>=getHeight()-23)
            setMouseCursor(juce::MouseCursor::BottomRightCornerResizeCursor);
        else if(e.y>=0 && e.y<42 && e.x<getWidth()-45)
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        else setMouseCursor(juce::MouseCursor::NormalCursor);
        constexpr int rowY=108,rowHeight=37;
        const int visible=visibleRows();
        if(e.y>=rowY&&e.y<rowY+visible*rowHeight){
            const int i=topItem+(e.y-rowY)/rowHeight;
            if(i<(int)results.size() && i!=selectedRow){
                selectedRow=i;repaint();
            }
        }
    }
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails& wheel) override {
        if(wheel.deltaY==0.f)return;
        if(wheelCarry*wheel.deltaY<0.f)wheelCarry=0.f;
        wheelCarry=juce::jlimit(-1.f,1.f,wheelCarry+wheel.deltaY);
        if(std::abs(wheelCarry)<.38f)return;
        const int step=wheelCarry<0.f?1:-1;
        wheelCarry=0.f;
        const int next=juce::jlimit(0,std::max(0,(int)results.size()-visibleRows()),topItem+step);
        if(next!=topItem){
            topItem=next;
            selectedRow=juce::jlimit(topItem,
                juce::jmin((int)results.size()-1,topItem+visibleRows()-1),selectedRow);
            repaint();
        }
    }

private:
    static juce::String category(int type){
        return skellpreset::registry::at(type).category;
    }
    static juce::String terms(int type){
        return skellpreset::registry::at(type).terms;
    }
    // Conservative fuzzy matching, inspired by ShallowHost's catalog search.
    // Only the message-thread picker uses this, never the audio callback.
    static bool nearWord(juce::String sought,juce::String candidate){
        sought=sought.toLowerCase();
        candidate=candidate.toLowerCase();
        const int a=sought.length(),b=candidate.length();
        const int limit=a<3?0:a<=5?1:a<=8?2:3;
        if(a>24||b>24||std::abs(a-b)>limit)return false;
        std::array<int,26> previous{},current{};
        for(int j=0;j<=b;++j)previous[(size_t)j]=j;
        for(int i=1;i<=a;++i){
            current[0]=i;
            int rowMin=i;
            for(int j=1;j<=b;++j){
                current[(size_t)j]=std::min({
                    previous[(size_t)j]+1,
                    current[(size_t)j-1]+1,
                    previous[(size_t)j-1]+(sought[i-1]==candidate[j-1]?0:1)
                });
                rowMin=std::min(rowMin,current[(size_t)j]);
            }
            if(rowMin>limit)return false;
            previous=current;
        }
        return previous[(size_t)b]<=limit;
    }
    bool matches(const juce::String& query,int type) const {
        const auto text=query.trim();
        if(text.isEmpty())return true;
        const auto searchable=processor.moduleName(type)+" "+
                              category(type)+" "+terms(type);
        auto words=juce::StringArray::fromTokens(text," \t","\"");
        const auto catalogWords=juce::StringArray::fromTokens(
            searchable," -_/+,.:","\"");
        for(const auto& word:words){
            if(searchable.containsIgnoreCase(word))continue;
            bool near=false;
            for(const auto& candidate:catalogWords)
                if(nearWord(word,candidate)) {near=true;break;}
            if(!near)return false;
        }
        return true;
    }
    void filter(){
        results.clear();
        for(int type=0;type<SkellPresetAudioProcessor::NumberOfModules;++type)
            // New mic modeling is a dedicated pre-rack input panel.
            // The old slot type 32 still loads and processes saved projects,
            // but new racks cannot accidentally insert a second mic model.
            if(type!=SkellPresetAudioProcessor::MicModelFX
               && type!=SkellPresetAudioProcessor::ExternalVST3
               // Keep legacy EQ IDs loadable for saved sessions, but offer
               // just one modern Studio EQ to new users.
               && type!=SkellPresetAudioProcessor::EQ
               && type!=SkellPresetAudioProcessor::EightBandToneFX
               && matches(search.getText(),type))results.push_back(type);
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
    bool movedByUser=false,dragHeader=false,resizing=false;
    juce::Rectangle<int> resizeStart{};
    float wheelCarry=0.f;
    juce::Point<int> dragOrigin{};
};
