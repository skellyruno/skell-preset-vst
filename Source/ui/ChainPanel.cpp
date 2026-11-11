#include "ChainPanel.h"
#include "Theme.h"
#include <cmath>
namespace {void drawIcon(juce::Graphics&g,int type,int x,int y) {
    g.setColour(Theme::blue.withAlpha(.86f));
    juce::Path p;
    if(type==SkellPresetAudioProcessor::EQ) {p.startNewSubPath((float)x,(float)y+10);p.cubicTo((float)x+6,(float)y+10,(float)x+7,(float)y-1,(float)x+13,(float)y+2);p.cubicTo((float)x+19,(float)y+3,(float)x+18,(float)y+10,(float)x+23,(float)y+10);}
    else if(type==SkellPresetAudioProcessor::Compressor) {p.startNewSubPath((float)x,(float)y+13);p.lineTo((float)x+7,(float)y+13);p.lineTo((float)x+14,(float)y+4);p.lineTo((float)x+23,(float)y+4);}
    else if(type==SkellPresetAudioProcessor::Limiter) {p.startNewSubPath((float)x,(float)y+12);p.lineTo((float)x+9,(float)y+3);p.lineTo((float)x+23,(float)y+3);}
    else if(type==SkellPresetAudioProcessor::Delay||type==SkellPresetAudioProcessor::Reverb||type==SkellPresetAudioProcessor::AdlibThrow){for(int i=0;i<4;i++)p.addRectangle((float)x+i*6,(float)y+4+(i%2?3:0),3.f,10.f-(i%2?6.f:0.f));}
    else if(type==SkellPresetAudioProcessor::Radio) {
        p.startNewSubPath((float)x,(float)y+4);p.lineTo((float)x+4,(float)y+4);
        p.lineTo((float)x+8,(float)y+13);p.lineTo((float)x+15,(float)y+13);
        p.lineTo((float)x+19,(float)y+4);p.lineTo((float)x+23,(float)y+4);
    } else if(type==SkellPresetAudioProcessor::RetroCrush) {
        p.startNewSubPath((float)x,(float)y+14);p.lineTo((float)x+5,(float)y+14);
        p.lineTo((float)x+5,(float)y+6);p.lineTo((float)x+13,(float)y+6);
        p.lineTo((float)x+13,(float)y+1);p.lineTo((float)x+20,(float)y+1);
        p.lineTo((float)x+20,(float)y+10);p.lineTo((float)x+23,(float)y+10);
    } else if(type==SkellPresetAudioProcessor::Tremolo) {
        p.startNewSubPath((float)x,(float)y+11);
        for(int i=1;i<=23;++i)
            p.lineTo((float)x+i,(float)y+9.f-6.f*std::sin((float)i*.46f));
    } else if(type==SkellPresetAudioProcessor::StereoWidth) {
        p.startNewSubPath((float)x,(float)y+8);p.lineTo((float)x+23,(float)y+8);
        p.startNewSubPath((float)x+11,(float)y+8);
        p.lineTo((float)x+22,(float)y+1);p.startNewSubPath((float)x+11,(float)y+8);
        p.lineTo((float)x+22,(float)y+15);
    } else if(type==SkellPresetAudioProcessor::ThreeBandDynamics) {
        for(int i=0;i<3;++i)
            p.addRoundedRectangle((float)x+2.f+8.f*i,(float)y+3.f+3.f*(i%2),
                                  5.f,12.f-3.f*(i%2),1.f);
    } else if(type!=SkellPresetAudioProcessor::Empty){p.startNewSubPath((float)x,(float)y+13);p.lineTo((float)x+7,(float)y+4);p.lineTo((float)x+14,(float)y+13);p.lineTo((float)x+22,(float)y+4);}
    g.strokePath(p,juce::PathStrokeType(1.8f));
}}
ChainPanel::ChainPanel(SkellPresetAudioProcessor& p):processor(p) {}
namespace {
constexpr int rowStart=47, rowStep=42, rowHeight=38;
constexpr int addTop=472, addHeight=33;
}
void ChainPanel::clampScroll() {
    scrollOffset=juce::jlimit(0,juce::jmax(0,processor.activeSlotCount()-10),scrollOffset);
}
int ChainPanel::rowAt(int y) const {
    const int relative=y-rowStart;
    if(relative<0||relative>=rowStep*SkellPresetAudioProcessor::slotCount)return -1;
    if(relative%rowStep>=rowHeight || relative/rowStep>=10)return -1;
    const int idx=scrollOffset+relative/rowStep;
    return idx<processor.activeSlotCount()?idx:-1;
}
void ChainPanel::paint(juce::Graphics& g) {
    Theme::selectFor(processor.state);
    Theme::panelBox(g,getLocalBounds().toFloat());
    clampScroll();
    const int active=processor.activeSlotCount();
    int used=0;
    for(int row=0;row<active;row++) {
        const int physical=processor.slotIdAt(row);
        auto* pv=processor.state.getRawParameterValue("s"+juce::String(physical+1)+"_type");
        if(pv && (int)pv->load()!=SkellPresetAudioProcessor::Empty)++used;
    }
    Theme::text(g,"SIGNAL CHAIN",{15,8,148,22},14,Theme::white,true);
    Theme::text(g,dockingDrag?"RELEASE TO DOCK":
        (dockedRight?"DRAG HEADER LEFT":"DRAG HEADER RIGHT"),
        {15,28,130,14},9,dockingDrag?Theme::green:Theme::muted);
    Theme::text(g,juce::String(used)+" FX",{142,13,50,19},10,Theme::muted,true,juce::Justification::centredRight);
    Theme::text(g,juce::String(active)+"/16",{145,29,49,14},9,Theme::green,true,juce::Justification::centredRight);
    for(int visual=0;visual<10 && scrollOffset+visual<active;++visual) {
        const int row=scrollOffset+visual;
        const int slot=processor.slotIdAt(row),top=rowStart+rowStep*visual;
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
    if(active>10) {
        // Fixed, readable scroll indicator across all three themes.
        const float railY=(float)rowStart,railH=10.f*rowStep-4.f;
        const float thumbH=railH*10.f/(float)active;
        const float y=railY+(railH-thumbH)*(float)scrollOffset/(float)(active-10);
        g.setColour(Theme::stroke.withAlpha(.8f));
        g.fillRoundedRectangle(209.f,railY,3.f,railH,1.5f);
        g.setColour(Theme::green.withAlpha(.80f));
        g.fillRoundedRectangle(209.f,y,3.f,thumbH,1.5f);
    }
    const auto add=juce::Rectangle<float>(10.f,(float)addTop,196.f,(float)addHeight);
    g.setColour(active==16?Theme::panelAlt.withAlpha(.35f):Theme::green.withAlpha(.09f));g.fillRoundedRectangle(add,6.f);
    g.setColour(active==16?Theme::stroke:Theme::green.withAlpha(.55f));g.drawRoundedRectangle(add,6.f,1.f);
    Theme::text(g,active==16?"RACK FULL  (16 / 16)":"+   ADD SLOT  "+juce::String(active+1)+"/16",
        {24,addTop+5,173,23},12,active==16?Theme::muted:Theme::green,true,juce::Justification::centred);
    Theme::text(g,active>10?"SCROLL TO SEE MORE  /  DRAG TO ORDER":"DRAG ANYWHERE ON A ROW",
        {13,511,193,18},9,Theme::muted,false,juce::Justification::centred);
}
void ChainPanel::refreshIfChanged() {
    const int currentSlots=processor.activeSlotCount();
    bool changed=!visualSnapshotValid || currentSlots!=lastSlotCount;
    lastSlotCount=currentSlots;
    clampScroll();
    for(int row=0;row<currentSlots;++row) {
        const int slot=processor.slotIdAt(row);
        const auto prefix="s"+juce::String(slot+1)+"_";
        const auto* type=processor.state.getRawParameterValue(prefix+"type");
        const auto* enabled=processor.state.getRawParameterValue(prefix+"on");
        const int value=slot*4096+(type?(int)type->load():0)*128
            +(enabled && enabled->load()>.5f?64:0)
            +(std::isfinite(processor.getSlotPeak(slot))
                ? juce::jlimit(0,63,(int)(processor.getSlotPeak(slot)*64.f)) : 0);
        if(value!=lastVisual[(size_t)row])changed=true;
        lastVisual[(size_t)row]=value;
    }
    visualSnapshotValid=true;
    if(changed)repaint();
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
    // A new slot is real: a preallocated DSP engine plus stable s11..s16
    // automation parameters. The UI scrolls to reveal it.
    const int slot=processor.appendRackSlot();
    if(slot<0)return;
    scrollOffset=juce::jmax(0,processor.activeSlotCount()-10);
    selected=slot;
    if(onSelection)onSelection(slot);
    popupFor(slot);
    repaint();
}
void ChainPanel::mouseDown(const juce::MouseEvent& e) {
    if(!e.mods.isPopupMenu() && e.x>=8 && e.x<=142 && e.y>=5 && e.y<43) {
        dockingDrag=true;
        dockCandidateRight=dockedRight;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }
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
    if(dockingDrag) {
        if(auto* parent=getParentComponent()) {
            const int parentX=e.getEventRelativeTo(parent).getPosition().x;
            dockCandidateRight=parentX>=parent->getWidth()/2;
            repaint();
        }
        return;
    }
    if(pendingFrom<0)return;
    if(!dragging && e.getPosition().getDistanceFrom(pressPoint)>6) {
        dragging=true;dragFrom=pendingFrom;dragTo=pendingFrom;
    }
    if(dragging) {
        dragTo=juce::jlimit(0,processor.activeSlotCount()-1,scrollOffset+(e.y-rowStart)/rowStep);
        repaint();
    }
}
void ChainPanel::mouseUp(const juce::MouseEvent&) {
    if(dockingDrag) {
        dockingDrag=false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        if(onDockRequest)onDockRequest(dockCandidateRight);
        repaint();
        return;
    }
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
    if(e.y>=5 && e.y<43 && e.x>=8 && e.x<=142)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else if(next>=0 && e.x<181)setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else if(next>=0 || add)setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
void ChainPanel::mouseExit(const juce::MouseEvent&) {
    hoveredRow=-1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void ChainPanel::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel) {
    const int change=wheel.deltaY<0?2:-2;
    const int next=juce::jlimit(0,juce::jmax(0,processor.activeSlotCount()-10),scrollOffset+change);
    if(next!=scrollOffset){scrollOffset=next;hoveredRow=-1;visualSnapshotValid=false;repaint();}
}
