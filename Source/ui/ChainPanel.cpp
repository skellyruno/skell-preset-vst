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
int ChainPanel::rowAt(int y) {const int relative=y-49;return relative>=0&&relative<470?relative/47:-1;}
void ChainPanel::paint(juce::Graphics&g) {
    Theme::panelBox(g,getLocalBounds().toFloat());
    Theme::text(g,"Signal chain",{15,13,155,22},15,Theme::white,true);
    Theme::text(g,"10 SLOTS",{153,13,48,22},10,Theme::muted,false,juce::Justification::centredRight);
    for(int row=0;row<10;row++) {
        const int slot=processor.slotIdAt(row),top=49+47*row;
        auto box=juce::Rectangle<float>(10.f,(float)top,196.f,43.f);
        auto* pv=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_type");
        auto* on=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_on");
        const int type=pv?(int)pv->load():0;const bool enabled=on&&on->load()>.5f;
        g.setColour(row%2?juce::Colour(0xff132137):juce::Colour(0xff15253a));g.fillRoundedRectangle(box,5.f);
        if(selected==slot) {g.setColour(Theme::blue);g.drawRoundedRectangle(box,6.f,1.6f);}
        if(dragFrom>=0 && dragTo==row) {g.setColour(Theme::green);g.fillRect(10,top-1,196,2);}
        g.setColour(Theme::muted.withAlpha(.5f));
        for(int dot=0;dot<6;dot++)g.fillEllipse(19.f+(dot%2)*4.f,(float)top+18.f+(dot/2)*4.f,1.5f,1.5f);
        Theme::text(g,juce::String(row+1).paddedLeft('0',2),{37,top+10,22,18},10,Theme::muted);
        drawIcon(g,type,71,top+13);
        Theme::text(g,processor.moduleName(type),{99,top+9,90,21},12,type==0?Theme::muted:Theme::white,selected==slot);
        g.setColour(enabled?Theme::green:Theme::muted.withAlpha(.35f));g.fillEllipse(189.f,(float)top+17.f,7,7);
        const auto peak=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
        g.setColour(Theme::green.withAlpha(enabled?.67f:.16f));g.fillRoundedRectangle(juce::Rectangle<float>(11.f,(float)top+40.f,194.f*peak,1.5f),.5f);
    }
    Theme::text(g,"SKELL ORIGINALS",{15,518,150,14},9,Theme::muted);
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
        safe->repaint();
    });
}
void ChainPanel::mouseDown(const juce::MouseEvent& e) {
    const int index=rowAt(e.y);if(index<0)return;
    const auto slot=processor.slotIdAt(index);
    if(e.mods.isPopupMenu()){popupFor(slot);return;}
    if(e.x>=181){auto*v=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_on");processor.setSlotOn(slot,!(v&&v->load()>.5f));repaint();return;}
    selected=slot; if(onSelection)onSelection(slot);
    if(e.x<34){dragFrom=index;dragTo=index;}
    repaint();
}
void ChainPanel::mouseDrag(const juce::MouseEvent& e) {
    if(dragFrom>=0){dragTo=juce::jlimit(0,9,(e.y-49)/47);repaint();}
}
void ChainPanel::mouseUp(const juce::MouseEvent&) {
    if(dragFrom>=0&&dragTo>=0&&dragFrom!=dragTo)processor.reorderSlot(dragFrom,dragTo);
    dragFrom=dragTo=-1;repaint();
}
