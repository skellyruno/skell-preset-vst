#include "ChainPanel.h"
#include "Theme.h"
namespace {void drawIcon(juce::Graphics&g,int type,int x,int y) {
    g.setColour(Theme::blue.withAlpha(.86f));
    juce::Path p;
    if(type==SkellPresetAudioProcessor::EQ) {p.startNewSubPath((float)x,(float)y+10);p.cubicTo((float)x+6,(float)y+10,(float)x+7,(float)y-1,(float)x+13,(float)y+2);p.cubicTo((float)x+19,(float)y+3,(float)x+18,(float)y+10,(float)x+23,(float)y+10);}
    else if(type==SkellPresetAudioProcessor::Compressor) {p.startNewSubPath((float)x,(float)y+13);p.lineTo((float)x+7,(float)y+13);p.lineTo((float)x+14,(float)y+4);p.lineTo((float)x+23,(float)y+4);}
    else if(type==SkellPresetAudioProcessor::Limiter) {p.startNewSubPath((float)x,(float)y+12);p.lineTo((float)x+9,(float)y+3);p.lineTo((float)x+23,(float)y+3);}
    else if(type==SkellPresetAudioProcessor::Delay||type==SkellPresetAudioProcessor::Reverb||type==SkellPresetAudioProcessor::AdlibThrow){for(int i=0;i<4;i++)p.addRectangle((float)x+i*6,(float)y+4+(i%2?3:0),3.f,10.f-(i%2?6.f:0.f));}
    else if(type!=SkellPresetAudioProcessor::Empty){p.startNewSubPath((float)x,(float)y+13);p.lineTo((float)x+7,(float)y+4);p.lineTo((float)x+14,(float)y+13);p.lineTo((float)x+22,(float)y+4);}
    g.strokePath(p,juce::PathStrokeType(1.8f));
}}
ChainPanel::ChainPanel(SkellPresetAudioProcessor& p):processor(p) {}
namespace {
constexpr int rowStart=47, rowStep=42, rowHeight=38;
constexpr int addTop=472, addHeight=33;
}
int ChainPanel::rowAt(int y) {
    const int relative=y-rowStart;
    if(relative<0||relative>=rowStep*SkellPresetAudioProcessor::slotCount)return -1;
    return relative%rowStep<rowHeight?relative/rowStep:-1;
}
void ChainPanel::paint(juce::Graphics& g) {
    Theme::selectFor(processor.state);
    Theme::panelBox(g,getLocalBounds().toFloat());
    int used=0;
    for(int row=0;row<SkellPresetAudioProcessor::slotCount;row++) {
        auto* pv=processor.state.getRawParameterValue("s"+juce::String(row+1)+"_type");
        if(pv && (int)pv->load()!=SkellPresetAudioProcessor::Empty)++used;
    }
    Theme::text(g,"SIGNAL CHAIN",{15,11,148,22},14,Theme::white,true);
    Theme::text(g,juce::String(used)+" / 10",{157,13,48,19},10,Theme::muted,true,juce::Justification::centredRight);
    for(int row=0;row<SkellPresetAudioProcessor::slotCount;row++) {
        const int slot=processor.slotIdAt(row),top=rowStart+rowStep*row;
        auto box=juce::Rectangle<float>(10.f,(float)top,196.f,(float)rowHeight);
        auto* pv=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_type");
        auto* on=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_on");
        const int type=pv?(int)pv->load():0;
        const bool enabled=on && on->load()>.5f && type!=0;
        g.setColour(selected==slot?Theme::panelAlt.brighter(.1f):(hoveredRow==row?Theme::green.withAlpha(.12f):Theme::panelAlt.withMultipliedAlpha(row%2?.78f:.96f)));
        g.fillRoundedRectangle(box,6.f);
        if(selected==slot) {g.setColour(Theme::green.withAlpha(.88f));g.drawRoundedRectangle(box.reduced(.5f),6.f,1.5f);}
        if(dragging && dragTo==row) {
            g.setColour(Theme::green);g.fillRoundedRectangle(10.f,(float)top-2.f,196.f,2.5f,1.f);
        }
        g.setColour(Theme::muted.withAlpha(.38f));
        for(int dot=0;dot<6;dot++)g.fillEllipse(18.f+(dot%2)*4.f,(float)top+12.f+(dot/2)*4.f,1.6f,1.6f);
        Theme::text(g,juce::String(row+1).paddedLeft('0',2),{35,top+9,23,18},10,Theme::muted);
        drawIcon(g,type,69,top+9);
        Theme::text(g,processor.moduleName(type),{97,top+8,88,19},12,type==0?Theme::muted:Theme::white,selected==slot);
        g.setColour(enabled?Theme::green:Theme::muted.withAlpha(.37f));
        g.fillEllipse(188.f,(float)top+15.f,8.f,8.f);
        const auto peak=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
        if(peak>0.002f) {
            g.setColour(Theme::green.withAlpha(enabled?.72f:.18f));
            g.fillRoundedRectangle(juce::Rectangle<float>(12.f,(float)top+35.f,191.f*peak,2.f),.8f);
        }
    }
    const auto add=juce::Rectangle<float>(10.f,(float)addTop,196.f,(float)addHeight);
    g.setColour(used==10?Theme::panelAlt.withAlpha(.35f):Theme::green.withAlpha(.09f));g.fillRoundedRectangle(add,6.f);
    g.setColour(used==10?Theme::stroke:Theme::green.withAlpha(.55f));g.drawRoundedRectangle(add,6.f,1.f);
    Theme::text(g,used==10?"RACK FULL  (10 / 10)":"+   ADD MODULE",{24,addTop+5,173,23},12,used==10?Theme::muted:Theme::green,true,juce::Justification::centred);
    Theme::text(g,"DRAG ANYWHERE ON A ROW",{13,511,193,18},9,Theme::muted,false,juce::Justification::centred);
}
void ChainPanel::popupFor(int physicalSlot) {
    juce::PopupMenu menu;
    menu.addSectionHeader("REPLACE MODULE");
    for(int i=0;i<SkellPresetAudioProcessor::NumberOfModules;i++)menu.addItem(i+1,processor.moduleName(i));
    menu.addSeparator();menu.addItem(101,"Clear slot");menu.addItem(102,"Toggle bypass");
    juce::Component::SafePointer<ChainPanel> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,physicalSlot](int id){
        if(!safe)return;
        if(id>=1&&id<=SkellPresetAudioProcessor::NumberOfModules){safe->processor.setSlotType(physicalSlot,id-1);safe->processor.setSlotOn(physicalSlot,id!=1);}
        if(id==101){safe->processor.setSlotType(physicalSlot,0);safe->processor.setSlotOn(physicalSlot,false);}
        if(id==102){auto v=safe->processor.state.getRawParameterValue("s"+juce::String(physicalSlot+1)+"_on");safe->processor.setSlotOn(physicalSlot,!(v&&v->load()>.5f));}
        if(safe->onSelection && id!=0)safe->onSelection(physicalSlot);
        safe->repaint();
    });
}
void ChainPanel::addModule() {
    int firstEmpty=-1,lastUsed=-1;
    for(int row=0;row<SkellPresetAudioProcessor::slotCount;row++) {
        const int slot=processor.slotIdAt(row);
        auto* type=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_type");
        if(type && (int)type->load()!=SkellPresetAudioProcessor::Empty)lastUsed=row;
        else if(firstEmpty<0)firstEmpty=row;
    }
    if(firstEmpty<0)return;
    // Append after the last populated row even if a user left a gap earlier
    // in the chain. This keeps "+" intuitive without creating new APVTS IDs.
    const int destination=juce::jmin(SkellPresetAudioProcessor::slotCount-1,lastUsed+1);
    if(firstEmpty!=destination)processor.reorderSlot(firstEmpty,destination);
    const int slot=processor.slotIdAt(destination);
    selected=slot;if(onSelection)onSelection(slot);
    popupFor(slot);repaint();
}
void ChainPanel::mouseDown(const juce::MouseEvent& e) {
    if(e.x>=10 && e.x<=206 && e.y>=addTop && e.y<addTop+addHeight) {
        if(!e.mods.isPopupMenu())addModule();return;
    }
    const int index=rowAt(e.y);
    if(index<0 || e.x<10 || e.x>206)return;
    const auto slot=processor.slotIdAt(index);
    if(e.mods.isPopupMenu()){popupFor(slot);return;}
    if(e.x>=181 && e.x<=206) {
        auto* v=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_on");
        processor.setSlotOn(slot,!(v&&v->load()>.5f));repaint();return;
    }
    selected=slot;if(onSelection)onSelection(slot);
    pendingFrom=index;dragFrom=dragTo=-1;dragging=false;pressPoint=e.getPosition();repaint();
}
void ChainPanel::mouseDrag(const juce::MouseEvent& e) {
    if(pendingFrom<0)return;
    if(!dragging && e.getPosition().getDistanceFrom(pressPoint)>6) {
        dragging=true;dragFrom=pendingFrom;dragTo=pendingFrom;
    }
    if(dragging) {
        dragTo=juce::jlimit(0,SkellPresetAudioProcessor::slotCount-1,(e.y-rowStart)/rowStep);
        repaint();
    }
}
void ChainPanel::mouseUp(const juce::MouseEvent&) {
    if(pendingFrom<0)return;
    if(dragging && dragTo>=0 && dragTo!=dragFrom)processor.reorderSlot(dragFrom,dragTo);
    const int slot=processor.slotIdAt(dragging && dragTo>=0 ? dragTo : pendingFrom);
    auto* type=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_type");
    const bool emptyClick=!dragging && type && (int)type->load()==SkellPresetAudioProcessor::Empty;
    pendingFrom=dragFrom=dragTo=-1;dragging=false;repaint();
    if(emptyClick)popupFor(slot);
}

void ChainPanel::mouseMove(const juce::MouseEvent& e) {
    const int next=(e.x>=10 && e.x<=206)?rowAt(e.y):-1;
    if(next!=hoveredRow){hoveredRow=next;repaint();}
    const bool add=(e.y>=addTop && e.y<addTop+addHeight && e.x>=10 && e.x<=206);
    if(next>=0 && e.x<181)setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else if(next>=0 || add)setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
void ChainPanel::mouseExit(const juce::MouseEvent&) {
    hoveredRow=-1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}
