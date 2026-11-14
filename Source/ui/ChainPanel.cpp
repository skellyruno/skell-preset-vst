#include "ChainPanel.h"
#include "Theme.h"
#include <cmath>
namespace {void drawIcon(juce::Graphics&g,int type,int x,int y) {
    g.setColour(Theme::green.withAlpha(.77f));
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
    } else if(type==SkellPresetAudioProcessor::VoxRackRoom) {
        for(int i=0;i<4;++i){
            const float height=13.f-i*2.6f;
            p.addRoundedRectangle((float)x+2.f+i*6.f,(float)y+15.f-height,
                                  3.f,height,1.f);
        }
    } else if(type==SkellPresetAudioProcessor::VoxRackSmoothComp) {
        p.startNewSubPath((float)x,(float)y+15.f);
        p.lineTo((float)x+8.f,(float)y+6.f);
        p.quadraticTo((float)x+14.f,(float)y+4.f,
                      (float)x+23.f,(float)y+4.f);
    } else if(type==SkellPresetAudioProcessor::VocalChorusFX) {
        for(int k=0;k<3;++k) {
            p.startNewSubPath((float)x,(float)y+5.f+3.f*k);
            for(int i=1;i<23;i+=2)
                p.lineTo((float)x+i,(float)y+5.f+3.f*k+
                         2.f*std::sin(i*.47f+k*1.5f));
        }
    } else if(type==SkellPresetAudioProcessor::VocalFlangerFX) {
        p.startNewSubPath((float)x,(float)y+12.f);
        p.cubicTo((float)x+6.f,(float)y+12.f,
                  (float)x+5.f,(float)y+3.f,
                  (float)x+11.f,(float)y+3.f);
        p.cubicTo((float)x+16.f,(float)y+3.f,
                  (float)x+16.f,(float)y+14.f,
                  (float)x+23.f,(float)y+5.f);
    } else if(type==SkellPresetAudioProcessor::RingModFX) {
        p.addEllipse((float)x+1.f,(float)y+2.f,12.f,12.f);
        p.addEllipse((float)x+10.f,(float)y+2.f,12.f,12.f);
    } else if(type==SkellPresetAudioProcessor::VocalRiderFX) {
        p.startNewSubPath((float)x,(float)y+13.f);
        p.lineTo((float)x+5.f,(float)y+13.f);
        p.lineTo((float)x+8.f,(float)y+5.f);
        p.lineTo((float)x+12.f,(float)y+11.f);
        p.lineTo((float)x+16.f,(float)y+5.f);
        p.lineTo((float)x+22.f,(float)y+5.f);
    } else if(type==SkellPresetAudioProcessor::DuckingPingPongFX) {
        for(int i=0;i<4;++i)
            p.addEllipse((float)x+1.f+i*6.f,(float)y+6.f+(i%2?4.f:0.f),3.f,3.f);
        p.startNewSubPath((float)x+1.f,(float)y+14.f);
        p.lineTo((float)x+22.f,(float)y+14.f);
    } else if(type==SkellPresetAudioProcessor::VocalVibratoFX) {
        p.startNewSubPath((float)x,(float)y+9.f);
        for(int i=1;i<=22;++i)
            p.lineTo((float)x+i,(float)y+9.f-5.f*std::sin((float)i*.68f));
    } else if(type==SkellPresetAudioProcessor::EightBandToneFX) {
        for(int band=0;band<8;++band)
            p.addEllipse((float)x+2.f+band*2.7f,(float)y+5.f+
                         4.f*std::sin((float)band*.9f),2.6f,2.6f);
    } else if(type==SkellPresetAudioProcessor::SerialDynamicsFX) {
        p.startNewSubPath((float)x,(float)y+13.f);
        p.lineTo((float)x+9.f,(float)y+5.f);
        p.quadraticTo((float)x+15.f,(float)y+2.f,(float)x+23.f,(float)y+2.f);
        p.startNewSubPath((float)x,(float)y+15.f);
        p.lineTo((float)x+10.f,(float)y+10.f);
        p.quadraticTo((float)x+16.f,(float)y+8.f,(float)x+23.f,(float)y+8.f);
    } else if(type==SkellPresetAudioProcessor::DynamicHarshnessFX) {
        p.startNewSubPath((float)x,(float)y+5.f);
        p.lineTo((float)x+7.f,(float)y+5.f);
        p.quadraticTo((float)x+12.f,(float)y+15.f,(float)x+17.f,(float)y+5.f);
        p.lineTo((float)x+23.f,(float)y+5.f);
    } else if(type==SkellPresetAudioProcessor::AdaptiveNoiseFX) {
        for(int i=0;i<5;++i)
            p.addRectangle((float)x+1.f+i*5.f,(float)y+3.f+(i%2)*5.f,
                           2.5f,13.f-(i%2)*7.f);
    } else if(type==SkellPresetAudioProcessor::StudioPitchFormantFX) {
        for(int row=0;row<2;++row){
            p.startNewSubPath((float)x,(float)y+6.f+6.f*row);
            for(int i=1;i<=23;++i)
                p.lineTo((float)x+i,(float)y+6.f+6.f*row+
                         3.f*std::sin(i*(row?.76f:.43f)));
        }
    } else if(type!=SkellPresetAudioProcessor::Empty){p.startNewSubPath((float)x,(float)y+13);p.lineTo((float)x+7,(float)y+4);p.lineTo((float)x+14,(float)y+13);p.lineTo((float)x+22,(float)y+4);}
    g.strokePath(p,juce::PathStrokeType(1.8f));
}}
ChainPanel::ChainPanel(SkellPresetAudioProcessor& p):processor(p) {}
float ChainPanel::slotWet(int physicalSlot) const noexcept {
    if(physicalSlot<0 || physicalSlot>=SkellPresetAudioProcessor::slotCount)return 0.f;
    const auto* value=processor.state.getRawParameterValue(
        "s"+juce::String(physicalSlot+1)+"_wet");
    return value?juce::jlimit(0.f,1.f,value->load()):0.f;
}
void ChainPanel::setSlotWet(int physicalSlot,float amount) {
    if(physicalSlot<0 || physicalSlot>=SkellPresetAudioProcessor::slotCount)return;
    const auto name="s"+juce::String(physicalSlot+1)+"_wet";
    if(auto* parameter=processor.state.getParameter(name)) {
        // Retain the original host automation ID and physical 0..1 range.
        const float next=juce::jlimit(0.f,1.f,amount);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(next));
    }
    visualSnapshotValid=false;
    repaint();
}
void ChainPanel::toggleSlotPower(int physicalSlot) {
    if(physicalSlot<0 || physicalSlot>=SkellPresetAudioProcessor::slotCount ||
       processor.slotType(physicalSlot)==SkellPresetAudioProcessor::Empty)return;
    const auto* value=processor.state.getRawParameterValue(
        "s"+juce::String(physicalSlot+1)+"_on");
    processor.setSlotOn(physicalSlot,!(value && value->load()>.5f));
    visualSnapshotValid=false;
    repaint();
}
bool ChainPanel::selectAndReplace(int visualRow) {
    if(visualRow<0 || visualRow>=processor.activeSlotCount())return false;
    const int slot=processor.slotIdAt(visualRow);
    selected=slot;
    if(onSelection)onSelection(slot);
    if(onReplaceRequest)onReplaceRequest(slot);
    repaint();
    return (bool)onReplaceRequest;
}
bool ChainPanel::testRackControlsAndWet() {
    const int id=processor.slotIdAt(0);
    if(processor.slotType(id)==SkellPresetAudioProcessor::Empty)return false;
    const float previous=slotWet(id);
    const auto* enabled=processor.state.getRawParameterValue(
        "s"+juce::String(id+1)+"_on");
    if(!enabled)return false;
    const bool wasOn=enabled->load()>.5f;
    setSlotWet(id,.375f);
    const bool correctWet=std::abs(slotWet(id)-.375f)<.001f;
    toggleSlotPower(id);
    const bool correctToggle=(enabled->load()>.5f)!=wasOn;
    setSlotWet(id,previous);
    processor.setSlotOn(id,wasOn);
    const bool correctRestore=std::abs(slotWet(id)-previous)<.001f &&
        (enabled->load()>.5f)==wasOn;
    return correctWet && correctToggle && correctRestore;
}
namespace {
constexpr int rowStart=47, rowStep=51, rowHeight=45;
constexpr int addTop=472, addHeight=33;
}
void ChainPanel::clampScroll() {
    scrollOffset=juce::jlimit(0,juce::jmax(0,processor.activeSlotCount()-visibleRows),scrollOffset);
}
int ChainPanel::rowAt(int y) const {
    const int relative=y-rowStart;
    if(relative<0||relative>=rowStep*SkellPresetAudioProcessor::slotCount)return -1;
    if(relative%rowStep>=rowHeight || relative/rowStep>=visibleRows)return -1;
    const int idx=scrollOffset+relative/rowStep;
    return idx<processor.activeSlotCount()?idx:-1;
}
void ChainPanel::paint(juce::Graphics& g) {
    Theme::selectFor(processor.state);
    Theme::panelBox(g,getLocalBounds().toFloat());
    // Hardware rack rails are deliberately outside the draggable module faces.
    g.setColour(Theme::stroke.withAlpha(.40f));
    g.drawLine(5.f,42.f,5.f,467.f,.8f);
    g.drawLine((float)getWidth()-5.f,42.f,(float)getWidth()-5.f,467.f,.8f);
    for(float y:{44.f,462.f})for(float x:{5.f,211.f}) {
        g.setColour(Theme::bg);g.fillEllipse(x-2.f,y-2.f,4.f,4.f);
        g.setColour(Theme::muted.withAlpha(.40f));g.drawEllipse(x-2.f,y-2.f,4.f,4.f,.8f);
    }
    clampScroll();
    const int active=processor.activeSlotCount();
    int used=0;
    for(int row=0;row<active;row++) {
        const int physical=processor.slotIdAt(row);
        if(processor.slotType(physical)!=SkellPresetAudioProcessor::Empty)++used;
    }
    Theme::text(g,"EFFECT RACK",{15,8,148,22},15,Theme::white,true);
    Theme::text(g,dockingDrag?"RELEASE TO DOCK":
        (dockedRight?"DRAG LEFT TO DOCK":"DRAG RIGHT TO DOCK"),
        {15,28,130,14},9,dockingDrag?Theme::green:Theme::muted);
    Theme::text(g,juce::String(used)+" FX",{142,13,50,19},10,Theme::muted,true,juce::Justification::centredRight);
    Theme::text(g,juce::String(active)+"/16",{145,29,49,14},9,Theme::green,true,juce::Justification::centredRight);
    for(int visual=0;visual<visibleRows && scrollOffset+visual<active;++visual) {
        const int row=scrollOffset+visual;
        const int slot=processor.slotIdAt(row),top=rowStart+rowStep*visual;
        auto box=juce::Rectangle<float>(10.f,(float)top,196.f,(float)rowHeight);
        auto* on=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_on");
        const int type=processor.slotType(slot);
        const bool enabled=on && on->load()>.5f && type!=0;
        g.setColour(selected==slot?Theme::panelAlt.brighter(.1f):(hoveredRow==row?Theme::green.withAlpha(.12f):Theme::panelAlt.withMultipliedAlpha(row%2?.78f:.96f)));
        g.fillRoundedRectangle(box,6.f);
        if(selected==slot) {g.setColour(Theme::green.withAlpha(.88f));g.drawRoundedRectangle(box.reduced(.5f),6.f,1.5f);}
        if(dragging && dragTo==row) {
            g.setColour(Theme::green);g.fillRoundedRectangle(10.f,(float)top-2.f,196.f,2.5f,1.f);
        }
        g.setColour(Theme::muted.withAlpha(.38f));
        for(int dot=0;dot<6;dot++)g.fillEllipse(18.f+(dot%2)*4.f,(float)top+12.f+(dot/2)*4.f,1.6f,1.6f);
        Theme::text(g,juce::String(row+1).paddedLeft('0',2),{32,top+9,21,18},10,Theme::muted);
        // Thin bone engraving on each module face; no generic FL clone art.
        drawIcon(g,type,54,top+10);
        // Narrow, hardware-like row: effect / FL-style wet trim / power dot.
        // Wet is the existing 0..1 automatable APVTS slot parameter.
        juce::String compact=processor.moduleName(type);
        if(type==SkellPresetAudioProcessor::VoxRackSmoothComp)compact="Vox Smooth";
        else if(type==SkellPresetAudioProcessor::VoxRackRoom)compact="Vox Room";
        else if(type==SkellPresetAudioProcessor::ThreeBandDynamics)compact="3-Band";
        else if(type==SkellPresetAudioProcessor::ExternalVST3)compact="VST3 Insert";
        else if(type==SkellPresetAudioProcessor::DuckingPingPongFX)compact="Duck Echo";
        else if(type==SkellPresetAudioProcessor::VocalVibratoFX)compact="Vibrato";
        else if(type==SkellPresetAudioProcessor::EightBandToneFX)compact="8-Band EQ";
        else if(type==SkellPresetAudioProcessor::SerialDynamicsFX)compact="FET/Opto";
        else if(type==SkellPresetAudioProcessor::DynamicHarshnessFX)compact="Harsh";
        else if(type==SkellPresetAudioProcessor::AdaptiveNoiseFX)compact="Noise";
        else if(type==SkellPresetAudioProcessor::StudioPitchFormantFX)compact="Pitch";
        const float nameSize=compact.length()>10?9.1f:compact.length()>8?9.9f:10.8f;
        Theme::text(g,compact,{78,top+8,69,19},nameSize,
                    type==0?Theme::muted:(enabled?Theme::white:Theme::muted),selected==slot);
        const float cx=165.f, cy=(float)top+19.f;
        const float wet=slotWet(slot);
        // Recessed FL-inspired wet dial, with bone-white marker and true arc.
        g.setColour(Theme::bg.withAlpha(.94f));
        g.fillEllipse(cx-12.f,cy-12.f,24.f,24.f);
        g.setColour(Theme::stroke.brighter(.14f));
        g.drawEllipse(cx-11.1f,cy-11.1f,22.2f,22.2f,1.1f);
        juce::Path baseArc,valueArc;
        constexpr float pi=juce::MathConstants<float>::pi;
        constexpr float a0=1.24f*pi, a1=2.76f*pi;
        baseArc.addCentredArc(cx,cy,8.45f,8.45f,0.f,a0,a1,true);
        valueArc.addCentredArc(cx,cy,8.45f,8.45f,0.f,a0,a0+(a1-a0)*wet,true);
        g.setColour(Theme::muted.withAlpha(.27f));
        g.strokePath(baseArc,juce::PathStrokeType(2.2f));
        if(wet>.001f) {
            g.setColour((enabled?Theme::green:Theme::muted).withAlpha(enabled?.94f:.40f));
            g.strokePath(valueArc,juce::PathStrokeType(2.45f));
        }
        const float angle=a0+(a1-a0)*wet;
        g.setColour(enabled?Theme::white:Theme::muted);
        g.drawLine(cx+std::sin(angle)*3.5f,cy-std::cos(angle)*3.5f,
                   cx+std::sin(angle)*6.8f,cy-std::cos(angle)*6.8f,1.35f);
        // Percentage is a human-readable view of the SAME normalized
        // 0..1 wet automation used in the live DSP path, not a second gain.
        Theme::text(g,juce::String(juce::roundToInt(wet*100.f))+"%",
            {149,top+32,33,12},9.2f,enabled?Theme::green:Theme::muted,
            true,juce::Justification::centred);
        // Hit target is wider than the dot, for usable control at 70% scale.
        const float px=193.f,py=cy;
        if(enabled) {
            g.setColour(Theme::green.withAlpha(.13f));
            g.fillEllipse(px-9.5f,py-9.5f,19.f,19.f);
        }
        g.setColour(Theme::bg);
        g.fillEllipse(px-6.2f,py-6.2f,12.4f,12.4f);
        g.setColour(enabled?Theme::green.withAlpha(.90f):Theme::stroke.brighter(.3f));
        g.drawEllipse(px-6.2f,py-6.2f,12.4f,12.4f,1.25f);
        g.setColour(enabled?Theme::green.brighter(.22f):Theme::muted.withAlpha(.28f));
        g.fillEllipse(px-3.55f,py-3.55f,7.1f,7.1f);
        const auto peak=juce::jlimit(0.f,1.f,processor.getSlotPeak(slot));
        // Bone-rib activity strip is driven by measured audio, not fabricated
        // motion. Separate bright vertebrae with dark joints for visual depth.
        for(int rib=0;rib<9;++rib) {
            const float x=15.f+14.1f*(float)rib;
            const bool lit=enabled && peak>(float)rib/9.f;
            g.setColour(lit?Theme::green.withAlpha(.25f+.55f*peak)
                           :Theme::stroke.withAlpha(.55f));
            g.fillRoundedRectangle(x,(float)top+34.3f,9.2f,2.5f,1.1f);
            if(lit) {
                g.setColour(Theme::white.withAlpha(.13f));
                g.fillRoundedRectangle(x+1.f,(float)top+34.25f,6.f,.65f,.3f);
            }
        }
    }
    if(active>visibleRows) {
        // Fixed, readable scroll indicator across all three themes.
        const float railY=(float)rowStart,railH=(float)visibleRows*rowStep-4.f;
        const float thumbH=railH*(float)visibleRows/(float)active;
        const float y=railY+(railH-thumbH)*(float)scrollOffset/(float)(active-visibleRows);
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
    Theme::text(g,active>visibleRows?"WHEEL TO SCROLL / DRAG WET / DBL-CLICK TO SWAP":"DBL-CLICK FX / WET RING / POWER DOT",
        {13,511,193,18},8.5f,Theme::muted,false,juce::Justification::centred);
}
void ChainPanel::refreshIfChanged() {
    const int currentSlots=processor.activeSlotCount();
    bool changed=!visualSnapshotValid || currentSlots!=lastSlotCount;
    lastSlotCount=currentSlots;
    clampScroll();
    for(int row=0;row<currentSlots;++row) {
        const int slot=processor.slotIdAt(row);
        const auto prefix="s"+juce::String(slot+1)+"_";
        const auto* enabled=processor.state.getRawParameterValue(prefix+"on");
        const int wetPercent=juce::jlimit(0,100,juce::roundToInt(slotWet(slot)*100.f));
        const int value=slot*65536+processor.slotType(slot)*4096
            +(enabled && enabled->load()>.5f?2048:0)
            +wetPercent*16
            +(std::isfinite(processor.getSlotPeak(slot))
                ? juce::jlimit(0,15,juce::roundToInt(processor.getSlotPeak(slot)*15.f)) : 0);
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
    scrollOffset=juce::jmax(0,processor.activeSlotCount()-visibleRows);
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
    if(e.x>=powerControlLeft && e.x<=powerControlRight) {
        toggleSlotPower(slot);return;
    }
    if(e.x>=wetControlLeft && e.x<=wetControlRight) {
        selected=slot;
        if(onSelection)onSelection(slot);
        wetDragSlot=slot;wetDragStart=slotWet(slot);pressPoint=e.getPosition();
        if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+"_wet"))
            p->beginChangeGesture();
        return;
    }
    selected=slot;if(onSelection)onSelection(slot);
    pendingFrom=index;dragFrom=dragTo=-1;dragging=false;pressPoint=e.getPosition();repaint();
}
void ChainPanel::mouseDrag(const juce::MouseEvent& e) {
    if(wetDragSlot>=0) {
        const float delta=(float)(pressPoint.y-e.y)*.008f
                        +(float)(e.x-pressPoint.x)*.003f;
        setSlotWet(wetDragSlot,wetDragStart+delta);
        return;
    }
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
    if(wetDragSlot>=0) {
        if(auto* p=processor.state.getParameter("s"+juce::String(wetDragSlot+1)+"_wet"))
            p->endChangeGesture();
        wetDragSlot=-1;
        repaint();return;
    }
    if(dockingDrag) {
        dockingDrag=false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        if(onDockRequest)onDockRequest(dockCandidateRight);
        repaint();
        return;
    }
    if(pendingFrom<0)return;
    if(dragging && dragTo>=0 && dragTo!=dragFrom)processor.reorderSlot(dragFrom,dragTo);
    pendingFrom=dragFrom=dragTo=-1;dragging=false;repaint();
}

void ChainPanel::mouseDoubleClick(const juce::MouseEvent& e) {
    if(e.mods.isPopupMenu())return;
    const int row=rowAt(e.y);
    if(row<0 || e.x<10 || e.x>powerControlRight)return;
    const int slot=processor.slotIdAt(row);
    if(e.x>=powerControlLeft)return; // power is a single-click switch
    if(e.x>=wetControlLeft) {
        if(wetDragSlot==slot) {
            if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+"_wet"))
                p->endChangeGesture();
            wetDragSlot=-1;
        }
        setSlotWet(slot,1.f);return; // FL-style double-click resets wet
    }
    selectAndReplace(row);
}
void ChainPanel::mouseMove(const juce::MouseEvent& e) {
    const int next=(e.x>=10 && e.x<=206)?rowAt(e.y):-1;
    if(next!=hoveredRow){hoveredRow=next;repaint();}
    const bool add=(e.y>=addTop && e.y<addTop+addHeight && e.x>=10 && e.x<=206);
    if(e.y>=5 && e.y<43 && e.x>=8 && e.x<=142)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else if(next>=0 && e.x>=wetControlLeft && e.x<wetControlRight) {
        const int physical=processor.slotIdAt(next);
        setTooltip("WET "+juce::String(juce::roundToInt(slotWet(physical)*100.f))+
                   "%  /  drag to blend  /  double-click to reset");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    } else if(next>=0 && e.x>=powerControlLeft) {
        setTooltip("Click green dot to enable / bypass this effect");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else if(next>=0) {
        setTooltip("Click to select / double-click to change effect / drag to reorder");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else if(add)setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
void ChainPanel::mouseExit(const juce::MouseEvent&) {
    hoveredRow=-1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void ChainPanel::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel) {
    if(wheel.deltaY==0.f)return;
    const int change=wheel.deltaY<0.f?1:-1;
    const int next=juce::jlimit(0,juce::jmax(0,processor.activeSlotCount()-visibleRows),scrollOffset+change);
    if(next!=scrollOffset){scrollOffset=next;hoveredRow=-1;visualSnapshotValid=false;repaint();}
}
