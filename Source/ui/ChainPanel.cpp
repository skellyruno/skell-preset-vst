#include "ChainPanel.h"
#include "Theme.h"
#include <cmath>
#include <memory>
namespace {void drawIcon(juce::Graphics&g,int type,int pixelX,int pixelY) {
    const float x=static_cast<float>(pixelX),y=static_cast<float>(pixelY);
    using M=SkellPresetAudioProcessor;
    auto ink=Theme::green;
    switch(type){
        case M::Compressor:case M::Limiter:case M::VoxRackSmoothComp:case M::StudioCompressorFX:case M::StudioLimiterFX:case M::VocalFeelOptoFX:case M::SerialDynamicsFX:case M::ThreeBandDynamics:case M::Gate:case M::VocalRiderFX:ink=Theme::amber;break;
        case M::Delay:case M::StudioDelayFX:case M::Reverb:case M::VoxRackRoom:case M::DuckingPingPongFX:case M::StereoWidth:case M::AdlibThrow:ink=Theme::blue;break;
        case M::Saturator:case M::StudioFastDistFX:case M::AdlibSaturate:case M::RetroCrush:case M::Exciter:ink=Theme::pink;break;
        case M::VocalChorusFX:case M::VocalFlangerFX:case M::Tremolo:case M::VocalVibratoFX:case M::RingModFX:case M::AdlibMotion:case M::AdlibDoubler:case M::PhaseRotationFX:ink=Theme::blue.interpolatedWith(Theme::pink,.5f);break;
        default:break;
    }
    g.setColour(Theme::white.interpolatedWith(ink,.60f).withAlpha(.94f));
    juce::Path p;
    if(type==M::EQ) {p.startNewSubPath(x+1,y+13);p.lineTo(x+1,y+2);p.startNewSubPath(x+1,y+13);p.lineTo(x+23,y+13);p.startNewSubPath(x+3,y+10);p.quadraticTo(x+7,y+10,x+10,y+4);p.quadraticTo(x+14,y+1,x+18,y+9);p.lineTo(x+22,y+9);}
    else if(type==M::Compressor) {p.addRoundedRectangle(x+7,y+5,9,7,1.f);for(float side:{-1.f,1.f}){const float xx=x+11.5f+side*10.f,tip=x+11.5f+side*5.f;p.startNewSubPath(xx,y+8.5f);p.lineTo(tip,y+8.5f);p.lineTo(tip+side*2.5f,y+5.5f);p.startNewSubPath(tip,y+8.5f);p.lineTo(tip+side*2.5f,y+11.5f);}}
    else if(type==SkellPresetAudioProcessor::Limiter) {p.startNewSubPath((float)x,(float)y+12);p.lineTo((float)x+9,(float)y+3);p.lineTo((float)x+23,(float)y+3);}
    else if(type==SkellPresetAudioProcessor::Delay || type==SkellPresetAudioProcessor::StudioDelayFX){for(int i=0;i<4;++i){float h=13.f-i*3.f;p.addRoundedRectangle(x+i*6.f,y+14.f-h,3.f,h,1.f);}}
    else if(type==SkellPresetAudioProcessor::Reverb){for(int i=0;i<3;++i)p.addEllipse(x+11.f-(4.f+i*3.f),y+8.f-(3.f+i*2.f),8.f+i*6.f,6.f+i*4.f);}
    else if(type==SkellPresetAudioProcessor::AdlibThrow){p.startNewSubPath(x,y+13.f);p.quadraticTo(x+6.f,y-5.f,x+19.f,y+3.f);p.lineTo(x+23.f,y+3.f);p.lineTo(x+19.f,y+8.f);}
    else if(type==SkellPresetAudioProcessor::Gate){p.startNewSubPath(x,y+14.f);p.lineTo(x+9.f,y+14.f);p.lineTo(x+9.f,y+2.f);p.lineTo(x+23.f,y+2.f);}
    else if(type==SkellPresetAudioProcessor::Saturator || type==SkellPresetAudioProcessor::StudioFastDistFX || type==SkellPresetAudioProcessor::AdlibSaturate){
        p.startNewSubPath(x+1,y+12.f);p.lineTo(x+5,y+12.f);p.lineTo(x+9,y+3.f);p.lineTo(x+14,y+3.f);p.lineTo(x+18,y+12.f);p.lineTo(x+22,y+12.f);
        if(type!=SkellPresetAudioProcessor::Saturator){p.startNewSubPath(x+2.f,y+11.f);p.lineTo(x+8.f,y+11.f);p.lineTo(x+14.f,y+4.f);p.lineTo(x+21.f,y+4.f);}
    }
    else if(type==SkellPresetAudioProcessor::Exciter){for(int i=0;i<4;++i)p.addRectangle(x+2.f+i*6.f,y+14.f-(i+1)*3.f,2.f,(i+1)*3.f);}
    else if(type==SkellPresetAudioProcessor::DeEsser){p.startNewSubPath(x+2.f,y+4.f);p.cubicTo(x+24.f,y-6.f,x-1.f,y+13.f,x+20.f,y+12.f);p.startNewSubPath(x+14.f,y+1.f);p.lineTo(x+14.f,y+15.f);}
    else if(type==SkellPresetAudioProcessor::AdlibDoubler){p.addRoundedRectangle(x+2.f,y+2.f,13.f,10.f,3.f);p.addRoundedRectangle(x+9.f,y+6.f,13.f,10.f,3.f);}
    else if(type==SkellPresetAudioProcessor::StudioEQFX){p.startNewSubPath(x,y+11.f);p.quadraticTo(x+6.f,y+11.f,x+8.f,y+3.f);p.quadraticTo(x+14.f,y+14.f,x+23.f,y+5.f);}
    else if(type==SkellPresetAudioProcessor::StudioCompressorFX || type==SkellPresetAudioProcessor::VocalFeelOptoFX){p.startNewSubPath(x,y+15.f);p.lineTo(x+8.f,y+7.f);p.quadraticTo(x+12.f,y+3.f,x+23.f,y+4.f);}
    else if(type==SkellPresetAudioProcessor::StudioLimiterFX){p.startNewSubPath(x,y+14.f);p.lineTo(x+10.f,y+3.f);p.lineTo(x+23.f,y+3.f);p.startNewSubPath(x+2.f,y+1.f);p.lineTo(x+23.f,y+1.f);}
    else if(type==SkellPresetAudioProcessor::SkelldizerFX){for(int i=0;i<3;++i){p.addEllipse(x+1.f+i*8.f,y+5.f-(i%2)*3.f,6.f,6.f);p.startNewSubPath(x+4.f+i*8.f,y+11.f-(i%2)*3.f);p.lineTo(x+4.f+i*8.f,y+15.f);}}
    else if(type==SkellPresetAudioProcessor::AdlibChop){for(int i=0;i<3;++i)p.addRectangle(x+2.f+i*8.f,y+3.f,4.f,11.f);}
    else if(type==SkellPresetAudioProcessor::AdlibMotion){p.addCentredArc(x+11.f,y+8.f,10.f,6.f,0.f,0.f,5.4f,true);p.lineTo(x+3.f,y+3.f);}
    else if(type==SkellPresetAudioProcessor::AdlibFilter){p.startNewSubPath(x,y+3.f);p.lineTo(x+10.f,y+3.f);p.quadraticTo(x+14.f,y+4.f,x+17.f,y+13.f);p.lineTo(x+23.f,y+13.f);}
    else if(type==SkellPresetAudioProcessor::VocalFeelSuiteFX){for(int i=0;i<3;++i)p.addRoundedRectangle(x+2.f+i*7.f,y+3.f+i*2.f,5.f,12.f-i*2.f,1.f);}
    else if(type==SkellPresetAudioProcessor::ExternalVST3){p.addRoundedRectangle(x+5.f,y+2.f,13.f,12.f,2.f);p.startNewSubPath(x+8.f,y+1.f);p.lineTo(x+8.f,y-2.f);p.startNewSubPath(x+15.f,y+1.f);p.lineTo(x+15.f,y-2.f);}
    else if(type==SkellPresetAudioProcessor::MicModelFX){p.addRoundedRectangle(x+8.f,y+1.f,7.f,10.f,3.f);p.startNewSubPath(x+4.f,y+7.f);p.quadraticTo(x+11.f,y+20.f,x+19.f,y+7.f);}

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
        p.startNewSubPath(x+1,y+10);p.cubicTo(x+6,y-3,x+9,y-1,x+12,y+9);p.cubicTo(x+15,y+17,x+19,y+13,x+22,y+7);
        p.startNewSubPath(x+2,y+8);p.quadraticTo(x+12,y+5,x+22,y+8);
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
        for(int band=0;band<5;++band){
            const float xx=x+2.5f+band*4.5f,yy=y+4.f+(band%3)*3.6f;
            p.startNewSubPath(xx,y+2);p.lineTo(xx,y+15);p.addRoundedRectangle(xx-1.6f,yy,3.2f,3.f,.6f);
        }
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
    } else if(type==SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
        p.addEllipse(x+4,y+1,15,15);p.addEllipse(x+8,y+5,7,7);
        p.startNewSubPath(x+11.5f,y+1);p.lineTo(x+11.5f,y+5);
        p.startNewSubPath(x+11.5f,y+12);p.lineTo(x+11.5f,y+16);
        p.startNewSubPath(x+1,y+8.5f);p.lineTo(x+8,y+8.5f);p.startNewSubPath(x+15,y+8.5f);p.lineTo(x+22,y+8.5f);
    } else if(type==M::PhaseRotationFX){p.addCentredArc(x+11.5f,y+8.f,8.f,6.f,0.f,.4f,5.5f,true);p.lineTo(x+6,y+1);p.startNewSubPath(x+11.5f,y+8);p.lineTo(x+17,y+4);}
    else if(type!=SkellPresetAudioProcessor::Empty){p.startNewSubPath((float)x,(float)y+13);p.lineTo((float)x+7,(float)y+4);p.lineTo((float)x+14,(float)y+13);p.lineTo((float)x+22,(float)y+4);}
    // Fit the actual glyph bounds into one compact optical cell. Uniform
    // scaling preserves the distinct silhouettes, including tall/narrow icons.
    if(!p.isEmpty()){
        p.applyTransform(p.getTransformToScaleToFit(x,y,15.5f,11.5f,true));
        g.strokePath(p,juce::PathStrokeType(1.18f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
}}
ChainPanel::ChainPanel(SkellPresetAudioProcessor& p):processor(p) {
    setWantsKeyboardFocus(true);
    setTooltip("Wheel on effect: move it. Side rail: scroll rack. Shift + wheel: scroll anywhere.");
}
ChainPanel::~ChainPanel(){finishWetGesture();}
void ChainPanel::finishWetGesture(){
    if(wetDragSlot>=0){
        if(auto* p=processor.state.getParameter("s"+juce::String(wetDragSlot+1)+"_wet"))
            p->endChangeGesture();
        wetDragSlot=-1;
    }
}
void ChainPanel::cancelPointerGestures(){
    finishWetGesture();
    pendingFrom=dragFrom=dragTo=pendingPhysical=-1;
    dragging=false;dockingDrag=false;railDragging=false;
    hoveredWetSlot=-1;hoveredRow=-1;railHovered=false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}
void ChainPanel::visibilityChanged(){
    if(!isVisible()){
        cancelPointerGestures();
        contextOpen=false;contextHover=-1;
        wheelTargetSlot=-1;railWheelCarry=effectWheelCarry=0.f;
    }
}
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
        const float next=std::isfinite(amount)?juce::jlimit(0.f,1.f,amount):slotWet(physicalSlot);
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
    const bool hiddenAtIdle=!wetReadoutVisibleForTest(id);
    const auto* enabled=processor.state.getRawParameterValue(
        "s"+juce::String(id+1)+"_on");
    if(!enabled)return false;
    const bool wasOn=enabled->load()>.5f;
    // The visible integer 0–100% is only a formatter; prove its endpoints
    // and fine values still map to the continuous 0.0–1.0 DSP wet parameter.
    setSlotWet(id,0.f);
    const bool zero=std::abs(slotWet(id))<.001f;
    setSlotWet(id,.01f);
    const bool onePercent=std::abs(slotWet(id)-.01f)<.001f;
    setSlotWet(id,1.f);
    const bool hundred=std::abs(slotWet(id)-1.f)<.001f;
    setSlotWet(id,.375f);
    const bool correctWet=zero && onePercent && hundred &&
        std::abs(slotWet(id)-.375f)<.001f;
    toggleSlotPower(id);
    const bool correctToggle=(enabled->load()>.5f)!=wasOn;
    setSlotWet(id,previous);
    processor.setSlotOn(id,wasOn);
    const bool correctRestore=std::abs(slotWet(id)-previous)<.001f &&
        (enabled->load()>.5f)==wasOn;
    return hiddenAtIdle && correctWet && correctToggle && correctRestore;
}
juce::RangedAudioParameter* ChainPanel::sidechainParameter(int physicalSlot) const {
    if(physicalSlot<0 || physicalSlot>=SkellPresetAudioProcessor::slotCount)return nullptr;
    const int type=processor.slotType(physicalSlot);
    if(!SkellPresetAudioProcessor::supportsEffectSidechain(type))return nullptr;
    return processor.state.getParameter("s"+juce::String(physicalSlot+1)+"_"+
        SkellPresetAudioProcessor::sidechainEnableSuffix(type));
}
bool ChainPanel::sidechainLinked(int physicalSlot) const {
    const auto* p=sidechainParameter(physicalSlot);
    return p && p->getValue()>.5f;
}
bool ChainPanel::toggleSidechainLink(int physicalSlot) {
    auto* p=sidechainParameter(physicalSlot);
    if(!p)return false;
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->getValue()>.5f?0.f:1.f);
    p->endChangeGesture();
    visualSnapshotValid=false;
    repaint();
    return true;
}
namespace {
constexpr int rowStart=47, rowStep=51, rowHeight=45;
constexpr int addTop=472, addHeight=33;
constexpr int dockTop=510;
}
void ChainPanel::openSidechainTargets() {
    cancelPointerGestures();
    if(onRoutingRequest)onRoutingRequest(-1);
}
bool ChainPanel::testSidechainDock() {
    bool found=false;
    for(int row=0;row<processor.activeSlotCount();++row) {
        const int id=processor.slotIdAt(row);
        auto* p=sidechainParameter(id);
        if(!p) {
            if(toggleSidechainLink(id))return false;
            continue;
        }
        found=true;
        const bool old=sidechainLinked(id);
        const float oldWet=slotWet(id);
        const int oldType=processor.slotType(id);
        const auto orderBefore=processor.slotIdAt(row);
        const bool changed=toggleSidechainLink(id) && sidechainLinked(id)!=old;
        const bool restored=toggleSidechainLink(id) && sidechainLinked(id)==old;
        if(!changed || !restored || slotWet(id)!=oldWet ||
           processor.slotType(id)!=oldType || processor.slotIdAt(row)!=orderBefore)return false;
    }
    return found;
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
    Theme::panelBox(g,getLocalBounds().toFloat(),9.f);
    Theme::workspaceFrame(g,getLocalBounds().toFloat());
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
    int links=0,occupied=0;
    for(int row=0;row<active;++row){
        const int id=processor.slotIdAt(row);
        if(processor.slotType(id)!=SkellPresetAudioProcessor::Empty)++occupied;
        if(sidechainLinked(id))++links;
    }
    Theme::fittedText(g,"EFFECT RACK",{12,7,118,20},13.f,Theme::white,true);
    Theme::text(g,dockingDrag?juce::String("RELEASE TO DOCK"):
        juce::String(occupied)+" FX / "+juce::String(active)+" SLOTS",
        {12,27,118,14},9.5f,dockingDrag?Theme::green:Theme::muted);
    for(int visual=0;visual<visibleRows && scrollOffset+visual<active;++visual) {
        const int row=scrollOffset+visual;
        const int slot=processor.slotIdAt(row),top=rowStart+rowStep*visual;
        auto box=juce::Rectangle<float>(10.f,(float)top,196.f,(float)rowHeight);
        auto* on=processor.state.getRawParameterValue("s"+juce::String(slot+1)+"_on");
        const int type=processor.slotType(slot);
        const bool enabled=on && on->load()>.5f && type!=0;
        const bool focused=selected==slot;
        const bool hovering=hoveredRow==row;
        const auto face=Theme::panelAlt;
        g.setGradientFill(juce::ColourGradient(
            face.brighter(focused?.13f:hovering?.085f:.045f),
            box.getTopLeft(),face.darker(row%2?.12f:.18f),
            box.getBottomRight(),false));
        g.fillRoundedRectangle(box,6.f);
        // A single machined edge and inset glint instead of thick neon
        // outlines around every rack row.
        g.setColour(focused?Theme::green.withAlpha(.79f):
                    hovering?Theme::stroke.withAlpha(.72f):
                             Theme::stroke.withAlpha(.31f));
        g.drawRoundedRectangle(box.reduced(.65f),6.f,focused?1.2f:.85f);
        g.setColour(Theme::white.withAlpha(focused?.12f:.045f));
        g.drawLine(box.getX()+13.f,box.getY()+1.4f,
                   box.getRight()-13.f,box.getY()+1.4f,.65f);
        if(focused){
            g.setColour(Theme::green.withAlpha(.79f));
            g.fillRoundedRectangle(box.getX()+2.f,box.getY()+8.f,
                                   2.5f,box.getHeight()-16.f,1.2f);
        }
        if(dragging && dragTo==row) {
            g.setColour(Theme::green);g.fillRoundedRectangle(10.f,(float)top-2.f,196.f,2.5f,1.f);
        }
        g.setColour(Theme::muted.withAlpha(focused?.72f:.44f));
        for(int dot=0;dot<6;dot++)g.fillEllipse(18.f+(dot%2)*4.f,(float)top+12.f+(dot/2)*4.f,1.6f,1.6f);
        Theme::text(g,juce::String(row+1).paddedLeft('0',2),{32,top+9,21,18},11.5f,Theme::muted,true);
        if(type==SkellPresetAudioProcessor::Empty) {
            Theme::fittedText(g,"Add effect",{61,top+8,110,19},
                11.6f,Theme::muted,true);
            const float px=193.f,py=(float)top+19.f;
            g.setColour(Theme::muted.withAlpha(focused?.95f:.62f));
            g.drawEllipse(px-9.f,py-9.f,18.f,18.f,1.f);
            g.drawLine(px-5.f,py,px+5.f,py,1.5f);
            g.drawLine(px,py-5.f,px,py+5.f,1.5f);
            continue;
        }
        // Link state is the real host parameter, not a separate UI-only map.
        const bool canLink=sidechainParameter(slot)!=nullptr;
        const bool linked=canLink && sidechainLinked(slot);
        if(linked) {
            const auto ink=linked?Theme::green:Theme::muted;
            g.setColour(ink.withAlpha(linked?.93f:.53f));
            g.drawLine(5.f,(float)top+37.f,13.f,(float)top+37.f,1.2f);
            g.fillEllipse(15.f,(float)top+34.f,5.f,5.f);
            // Cable endpoint is the indicator; leave room for direct remove.
        }
        // Thin bone engraving on each module face; no generic FL clone art.
        // Smaller artwork only: row, wet trim, power and removal hit
        // targets keep their existing bounds and automation behavior.
        drawIcon(g,type,54,top+13);
        // Always-visible direct removal target, separate from drag, wet and power.
        g.setColour(hovering?Theme::red:Theme::muted.withAlpha(.70f));
        const float crossY=(float)top+34.f;
        g.drawLine(35.f,crossY-3.5f,43.f,crossY+3.5f,1.35f);
        g.drawLine(43.f,crossY-3.5f,35.f,crossY+3.5f,1.35f);
        // Narrow, hardware-like row: effect / FL-style wet trim / power dot.
        // Wet is the existing 0..1 automatable APVTS slot parameter.
        juce::String compact=processor.moduleDisplayName(type);
        if(type==SkellPresetAudioProcessor::ExternalVST3) {
            compact=processor.hostedVST3Name(slot);
            if(compact.isEmpty())compact="VST3 INSERT";
        }
        const auto labelColour=type==0?Theme::muted.brighter(.20f):
            (enabled?Theme::white:Theme::muted);
        // Two short fitted lines keep long native and third-party product
        // names legible in the hardware rack without colliding with wet.
        const int wrapAt=compact.substring(0,13).lastIndexOfChar(' ');
        if(compact.length()>12 && wrapAt>2 && wrapAt<compact.length()-2) {
            Theme::fittedText(g,compact.substring(0,wrapAt),
                              {77,top+3,74,16},12.3f,labelColour,selected==slot);
            Theme::fittedText(g,compact.substring(wrapAt+1),
                              {77,top+19,74,17},12.1f,labelColour,false);
        } else {
            const float nameSize=compact.length()>10?11.5f:12.4f;
            Theme::fittedText(g,compact,{77,top+7,74,23},nameSize,
                              labelColour,selected==slot);
        }
        const float cx=165.f, cy=(float)top+19.f;
        const float wet=slotWet(slot);
        // Empty physical slots have no audible wet blend. Don't show a
        // meaningless 100% or a functional-looking dial on an empty row.
        if(type!=SkellPresetAudioProcessor::Empty) {
        // Recessed FL-inspired wet dial, with bone-white marker and true arc.
        g.setColour(Theme::bg.withAlpha(.94f));
        g.fillEllipse(cx-12.f,cy-12.f,24.f,24.f);
        g.setColour(Theme::stroke.withAlpha(.81f));
        g.drawEllipse(cx-11.1f,cy-11.1f,22.2f,22.2f,.9f);
        juce::Path baseArc,valueArc;
        constexpr float pi=juce::MathConstants<float>::pi;
        constexpr float a0=1.24f*pi, a1=2.76f*pi;
        baseArc.addCentredArc(cx,cy,8.45f,8.45f,0.f,a0,a1,true);
        valueArc.addCentredArc(cx,cy,8.45f,8.45f,0.f,a0,a0+(a1-a0)*wet,true);
        g.setColour(Theme::muted.withAlpha(.27f));
        g.strokePath(baseArc,juce::PathStrokeType(2.2f));
        if(wet>.001f) {
            g.setColour((enabled?Theme::green:Theme::muted).withAlpha(enabled?.84f:.36f));
            g.strokePath(valueArc,juce::PathStrokeType(2.45f));
        }
        const float angle=a0+(a1-a0)*wet;
        g.setColour(enabled?Theme::white:Theme::muted);
        g.drawLine(cx+std::sin(angle)*3.5f,cy-std::cos(angle)*3.5f,
                   cx+std::sin(angle)*6.8f,cy-std::cos(angle)*6.8f,1.35f);
        // The 0-100 readout appears ONLY while hovering/dragging this
        // particular wet dial; it is the same host-automated normalized wet.
        if(wetReadoutVisibleForTest(slot))
            Theme::text(g,juce::String(juce::roundToInt(wet*100.f))+"%",
                {148,top+31,35,13},9.6f,enabled?Theme::green:Theme::muted,
                true,juce::Justification::centred);
        }
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
        // Rack face intentionally has no per-effect audio-level dashes.
        // Audio meters remain available in the dedicated main/scope views.
    }
    if(active>visibleRows) {
        // Fixed, readable scroll indicator across all three themes.
        const float railY=(float)rowStart,railH=(float)visibleRows*rowStep-4.f;
        const float thumbH=railH*(float)visibleRows/(float)active;
        const float y=railY+(railH-thumbH)*(float)scrollOffset/(float)(active-visibleRows);
        g.setColour(Theme::panelAlt.brighter(.08f));
        g.fillRoundedRectangle(208.f,railY,4.f,railH,2.f);
        g.setColour((railHovered || railDragging ? Theme::green : Theme::stroke)
            .withAlpha(.62f));
        g.drawRoundedRectangle(208.f,railY,4.f,railH,2.f,.65f);
        g.setColour(Theme::muted.withAlpha(railDragging?.85f:railHovered?.60f:.30f));
        g.fillRoundedRectangle(208.f,y,4.f,thumbH,2.f);
    }
    const auto add=juce::Rectangle<float>(10.f,(float)addTop,196.f,(float)addHeight);
    g.setColour(Theme::panelAlt.withAlpha(.78f));g.fillRoundedRectangle(add,6.f);
    g.setColour(Theme::stroke.withAlpha(.90f));g.drawRoundedRectangle(add,6.f,1.f);
    Theme::fittedText(g,active==16?juce::String("16/16 SLOTS ALLOCATED"):"+ ADD SLOT  ("+juce::String(active)+"/16)",
        {18,addTop+5,180,23},11.6f,active==16?Theme::muted:Theme::green,true,juce::Justification::centred);
    // The dock fits the unused footer without shrinking the eight-row viewport.
    // Cable spine terminates in the real vocal-source link box; rack-side
    // scrolling and left/right docking retain the same local geometry.
    if(links>0) {
        g.setColour(Theme::green.withAlpha(.57f));
        int firstLinked=-1;
        for(int v=0;v<visibleRows && scrollOffset+v<active;++v)
            if(sidechainLinked(processor.slotIdAt(scrollOffset+v))){
                firstLinked=rowStart+rowStep*v+37;break;
            }
        if(firstLinked>=0) {
            g.drawLine(5.f,(float)firstLinked,5.f,(float)dockTop+13.f,1.25f);
            g.drawLine(5.f,(float)dockTop+13.f,10.f,(float)dockTop+13.f,1.25f);
        }
    }
    const auto dock=juce::Rectangle<float>(10.f,(float)dockTop,196.f,45.f);
    g.setColour(Theme::panelAlt);g.fillRoundedRectangle(dock,5.f);
    g.setColour(Theme::stroke.withAlpha(.7f));
    g.drawRoundedRectangle(dock,5.f,.85f);
    Theme::text(g,"SIDECHAIN",{18,dockTop+3,116,17},11.f,Theme::white,true);
    const auto linkButton=juce::Rectangle<float>(137.f,(float)dockTop+3.f,62.f,19.f);
    g.setColour(Theme::stroke.withAlpha(.58f));g.fillRoundedRectangle(linkButton,4.f);
    Theme::text(g,"LINKS",{137,dockTop+3,62,19},10.f,Theme::green,true,juce::Justification::centred);
    Theme::fittedText(g,juce::String(links)+(links==1?" LINKED EFFECT":" LINKED EFFECTS"),{18,dockTop+23,181,18},10.f,links?Theme::green:Theme::muted,true);
    // Reordering remains available through gestures and the context menu.
    // The visible FX movement/status strip is intentionally removed.
    if(contextOpen) {
        const auto bounds=juce::Rectangle<float>(7.f,(float)contextTop,202.f,244.f);
        g.setColour(Theme::bg.withAlpha(.98f));g.fillRoundedRectangle(bounds,8.f);
        Theme::panelBox(g,bounds,8.f,true);
        const auto effectName=processor.slotType(contextSlot)==
            SkellPresetAudioProcessor::ExternalVST3
            ?processor.hostedVST3Name(contextSlot)
            :processor.moduleDisplayName(processor.slotType(contextSlot));
        Theme::fittedText(g,effectName.isEmpty()?"RACK ACTIONS":effectName,
                          {17,contextTop+4,182,24},12,Theme::green,true);
        const juce::StringArray choices{
            "EDIT EFFECT","REPLACE EFFECT","MOVE UP","MOVE DOWN",
            "BYPASS / ENABLE","CLEAR EFFECT","REMOVE FROM RACK","ADD TO SIDECHAIN"};
        for(int i=0;i<choices.size();++i) {
            const int y=contextTop+30+i*26;
            const bool enabled=contextActionEnabled(i);
            const auto accent=(i==5||i==6)?Theme::red:Theme::green;
            g.setColour(i==contextHover&&enabled?accent.withAlpha(.19f):
                        (i%2?Theme::plot.withAlpha(.55f):Theme::panelAlt.withAlpha(.65f)));
            g.fillRoundedRectangle(13.f,(float)y,190.f,24.f,4.f);
            g.setColour((i==contextHover&&enabled)?accent:
                        (enabled?((i==5||i==6)?Theme::red:Theme::white):Theme::muted.withAlpha(.35f)));
            g.setFont(Theme::font(11.f,true));
            g.drawFittedText(choices[i],{22,y+2,171,20},
                             juce::Justification::centredLeft,1,.82f);
        }
    }
}
void ChainPanel::refreshIfChanged() {
    const int currentSlots=processor.activeSlotCount();
    bool changed=!visualSnapshotValid || currentSlots!=lastSlotCount;
    lastSlotCount=currentSlots;
    clampScroll();
    if(contextOpen && contextPosition()<0){
        contextOpen=false;contextHover=-1;changed=true;
    }
    if(wetDragSlot>=0 && processor.slotType(wetDragSlot)==SkellPresetAudioProcessor::Empty){
        finishWetGesture();changed=true;
    }
    for(int row=0;row<currentSlots;++row) {
        const int slot=processor.slotIdAt(row);
        const auto prefix="s"+juce::String(slot+1)+"_";
        const auto* enabled=processor.state.getRawParameterValue(prefix+"on");
        const int wetPercent=juce::jlimit(0,100,juce::roundToInt(slotWet(slot)*100.f));
        // The previous 65536 stride collided when module IDs exceeded 15.
        // Use disjoint fields for physical ID, 45 types, on/off and wet.
        const int value=(slot<<20)|(processor.slotType(slot)<<12)|
            (enabled && enabled->load()>.5f?2048:0)|(wetPercent<<4)|(sidechainLinked(slot)?1:0); // no 30-Hz per-row peak-driven repaints
        if(value!=lastVisual[(size_t)row])changed=true;
        lastVisual[(size_t)row]=value;
    }
    visualSnapshotValid=true;
    if(changed)repaint();
}
int ChainPanel::contextPosition() const noexcept {
    if(contextSlot<0)return -1;
    for(int row=0;row<processor.activeSlotCount();++row)
        if(processor.slotIdAt(row)==contextSlot)return row;
    return -1;
}
int ChainPanel::contextActionAt(int x,int y,int top) noexcept {
    const int relative=y-top-30;
    return x>=13 && x<203 && relative>=0 && relative<8*26 &&
           relative%26<24?relative/26:-1;
}
bool ChainPanel::contextActionEnabled(int index) const noexcept {
    const int pos=contextPosition();
    if(index<0 || index>7 || pos<0)return false;
    if(index==7)return sidechainParameter(contextSlot)!=nullptr;
    const bool nonEmpty=processor.slotType(contextSlot)!=SkellPresetAudioProcessor::Empty;
    if(index==2)return pos>0 && nonEmpty;
    if(index==3)return pos+1<processor.activeSlotCount() && nonEmpty;
    if(index==4 || index==5)return nonEmpty;
    return true;
}
void ChainPanel::popupFor(int physicalSlot,juce::Point<int> screenPoint) {
    (void)screenPoint;
    if(physicalSlot<0 || physicalSlot>=SkellPresetAudioProcessor::slotCount)return;
    contextSlot=physicalSlot;
    const int position=contextPosition();
    if(position<0)return;
    // Theme-native right click overlay: no stock OS / DAW popup and no
    // focus-loss surprises while automating another control.
    contextTop=juce::jlimit(44,juce::jmax(44,getHeight()-248),
                           rowStart+(position-scrollOffset)*rowStep);
    cancelPointerGestures();
    contextHover=-1;
    contextOpen=true;
    if(isShowing())grabKeyboardFocus();
    repaint();
}
void ChainPanel::selectContextAction(int index) {
    if(!contextOpen || !contextActionEnabled(index))return;
    const int physical=contextSlot,position=contextPosition();
    contextOpen=false;contextHover=-1;
    if(position<0)return;
    if(index==2 && position>0)movePhysicalSlot(physical,position-1);
    if(index==3 && position+1<processor.activeSlotCount())
        movePhysicalSlot(physical,position+1);
    if(index==4)toggleSlotPower(physical);
    if(index==5) {
        processor.setSlotOn(physical,false);
        processor.setSlotType(physical,SkellPresetAudioProcessor::Empty);
    }
    if(index==6){removeEffectAt(position);return;}
    if(index==7){if(onRoutingRequest)onRoutingRequest(physical);return;}
    selected=physical;
    if(onSelection)onSelection(physical);
    if(index==1 && onReplaceRequest)onReplaceRequest(physical);
    visualSnapshotValid=false;
    repaint();
}
void ChainPanel::removeEffectAt(int position) {
    if(position<0 || position>=processor.activeSlotCount())return;
    const int physical=processor.slotIdAt(position);
    if(processor.slotType(physical)==SkellPresetAudioProcessor::Empty)return;
    if(onBeforeRemove)onBeforeRemove();
    cancelPointerGestures();
    if(processor.activeSlotCount()<=SkellPresetAudioProcessor::defaultSlotCount) {
        processor.setSlotOn(physical,false);
        processor.setSlotType(physical,SkellPresetAudioProcessor::Empty);
        processor.reorderSlot(position,processor.activeSlotCount()-1);
    } else processor.removeRackSlot(position);
    selected=-1;clampScroll();visualSnapshotValid=false;
    if(onAfterRemove)onAfterRemove();
    if(onClearSelection)onClearSelection();
    repaint();
}
void ChainPanel::addModule() {
    // A new slot is real: a preallocated DSP engine plus stable s11..s16
    // automation parameters. The UI scrolls to reveal it.
    const int slot=processor.appendRackSlot();
    if(slot<0)return;
    scrollOffset=juce::jmax(0,processor.activeSlotCount()-visibleRows);
    selected=slot;
    if(onSelection)onSelection(slot);
    // The plus button must open the searchable, movable library directly,
    // not the new right-click action menu for an empty slot.
    if(onReplaceRequest)onReplaceRequest(slot);
    repaint();
}
void ChainPanel::mouseDown(const juce::MouseEvent& e) {
    if(contextOpen) {
        if(!e.mods.isLeftButtonDown() || e.mods.isPopupMenu())return;
        const int line=contextActionAt(e.x,e.y,contextTop);
        if(line>=0)selectContextAction(line);
        else {contextOpen=false;contextHover=-1;repaint();}
        return;
    }
    if(!e.mods.isLeftButtonDown() && !e.mods.isPopupMenu())return;
    if(e.x>=10 && e.x<=206 && e.y>=dockTop && e.y<dockTop+45) {
        if(e.mods.isPopupMenu() || e.getNumberOfClicks()!=1)return;
        openSidechainTargets();return;
    }
    // Rail clicks/drag move only the viewport; no audio order changes.
    if(!e.mods.isPopupMenu() && processor.activeSlotCount()>visibleRows &&
       e.x>=205 && e.x<getWidth() &&
       e.y>=rowStart && e.y<rowStart+visibleRows*rowStep-4) {
        const float railTop=(float)rowStart;
        const float railHeight=(float)(visibleRows*rowStep-4);
        const float thumbHeight=railHeight*(float)visibleRows/
            (float)processor.activeSlotCount();
        const float thumbY=railTop+
            (railHeight-thumbHeight)*(float)scrollOffset/
            (float)(processor.activeSlotCount()-visibleRows);
        railGrabOffset=(float)e.y>=thumbY && (float)e.y<=thumbY+thumbHeight
            ?(float)e.y-thumbY:thumbHeight*.5f;
        railDragging=true;railHovered=true;
        moveNotice={};
        grabKeyboardFocus();
        moveRailTo(e.y);return;
    }
    if(e.x>=174 && e.x<=208 && e.y>=5 && e.y<43) {
        if(!e.mods.isPopupMenu() && onMicRequest)onMicRequest();
        return;
    }
    if(!e.mods.isPopupMenu() && e.x>=8 && e.x<=132 && e.y>=5 && e.y<43) {
        dockingDrag=true;
        dockCandidateRight=dockedRight;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }
    if(e.x>=10 && e.x<=206 && e.y>=addTop && e.y<addTop+addHeight) {
        if(!e.mods.isPopupMenu())
            addModule();
        return;
    }
    const int index=rowAt(e.y);
    if(index<0 || e.x<10 || e.x>206) {
        // Dead rack space is for scrolling/dragging the rack, not a hidden
        // return-to-home shortcut. Only a deliberate blank gap or the
        // VISUAL control should leave the current effect editor.
        return;
    }
    const auto slot=processor.slotIdAt(index);
    if(e.mods.isPopupMenu()){popupFor(slot,e.getScreenPosition());return;}
    if(e.x>=28 && e.x<50 && (e.y-rowStart)%rowStep>=26 && e.getNumberOfClicks()==1){removeEffectAt(index);return;}
    if(e.x>=powerControlLeft && e.x<powerControlRight) {
        if(e.getNumberOfClicks()==1) {
            if(processor.slotType(slot)==SkellPresetAudioProcessor::Empty) {
                selected=slot;
                if(onSelection)onSelection(slot);
                if(onReplaceRequest)onReplaceRequest(slot);
                repaint();
            } else toggleSlotPower(slot);
        }
        return;
    }
    if(e.x>=wetControlLeft && e.x<=wetControlRight) {
        if(processor.slotType(slot)==SkellPresetAudioProcessor::Empty)return;
        selected=slot;
        if(onSelection)onSelection(slot);
        finishWetGesture();
        wetDragSlot=slot;hoveredWetSlot=slot;
        wetDragStart=slotWet(slot);pressPoint=e.getPosition();repaint();
        if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+"_wet"))
            p->beginChangeGesture();
        return;
    }
    selected=slot;if(onSelection)onSelection(slot);
    grabKeyboardFocus();
    pendingFrom=index;pendingPhysical=slot;
    dragFrom=dragTo=-1;dragging=false;pressPoint=e.getPosition();repaint();
}
void ChainPanel::mouseDrag(const juce::MouseEvent& e) {
    if(railDragging) {moveRailTo(e.y);return;}
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
    if(pendingFrom<0 || pendingPhysical<0)return;
    if(!dragging && e.getPosition().getDistanceFrom(pressPoint)>6) {
        dragging=true;dragFrom=pendingFrom;dragTo=pendingFrom;
    }
    if(dragging) {
        dragTo=juce::jlimit(0,processor.activeSlotCount()-1,scrollOffset+(e.y-rowStart)/rowStep);
        repaint();
    }
}
void ChainPanel::mouseUp(const juce::MouseEvent& e) {
    if(railDragging){
        railDragging=false;
        railHovered=e.x>=205 && e.x<getWidth();repaint();return;
    }
    if(wetDragSlot>=0) {
        finishWetGesture();
        const int hoverRow=(e.x>=wetControlLeft && e.x<=wetControlRight)?
            rowAt(e.y):-1;
        hoveredWetSlot=hoverRow>=0?processor.slotIdAt(hoverRow):-1;
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
    if(dragging && dragTo>=0 && pendingPhysical>=0)
        movePhysicalSlot(pendingPhysical,dragTo);
    pendingFrom=dragFrom=dragTo=pendingPhysical=-1;dragging=false;repaint();
}

void ChainPanel::mouseDoubleClick(const juce::MouseEvent& e) {
    if(e.mods.isPopupMenu() || !e.mods.isLeftButtonDown() || contextOpen)return;
    const int row=rowAt(e.y);
    if(row<0 || e.x<10 || e.x>powerControlRight)return;
    const int slot=processor.slotIdAt(row);
    if(e.x>=28 && e.x<50 && (e.y-rowStart)%rowStep>=26)return;
    if(e.x>=powerControlLeft)return; // power is a single-click switch
    if(e.x>=wetControlLeft) {
        if(processor.slotType(slot)==SkellPresetAudioProcessor::Empty)return;
        finishWetGesture();
        if(auto* p=processor.state.getParameter("s"+juce::String(slot+1)+"_wet")){
            p->beginChangeGesture();
            setSlotWet(slot,1.f);
            p->endChangeGesture();
        }
        return; // FL-style double-click resets wet
    }
    selectAndReplace(row);
}
void ChainPanel::mouseMove(const juce::MouseEvent& e) {
    const bool nextRail=processor.activeSlotCount()>visibleRows &&
        e.x>=205 && e.x<getWidth() &&
        e.y>=rowStart && e.y<rowStart+visibleRows*rowStep-4;
    if(railHovered!=nextRail){railHovered=nextRail;repaint();}
    if(nextRail && !contextOpen){
        setTooltip("Rack scrollbar: wheel to scroll, click track to jump, drag thumb to navigate. Effect order stays unchanged.");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        return;
    }
    if(contextOpen) {
        const int target=contextActionAt(e.x,e.y,contextTop);
        const int hovered=contextActionEnabled(target)?target:-1;
        if(hovered!=contextHover){contextHover=hovered;repaint();}
        setMouseCursor(hovered>=0?juce::MouseCursor::PointingHandCursor:
                                juce::MouseCursor::NormalCursor);
        return;
    }
    if(e.x>=10 && e.x<=206 && e.y>=dockTop && e.y<dockTop+45) {
        setTooltip("Open sidechain routing: choose a source and target, then Connect");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }
    const int next=(e.x>=10 && e.x<=206)?rowAt(e.y):-1;
    const int hoveredPhysical=next>=0?processor.slotIdAt(next):-1;
    if(wheelTargetSlot>=0 &&
       (std::abs(e.x-lastWheelX)>12 || std::abs(e.y-lastWheelY)>10)) {
        wheelTargetSlot=-1;
        effectWheelCarry=0.f;
    } // pointer movement, not a reordered row, ends the wheel latch
    const int nextWet=(hoveredPhysical>=0 &&
                       processor.slotType(hoveredPhysical)!=SkellPresetAudioProcessor::Empty &&
                       e.x>=wetControlLeft && e.x<=wetControlRight)?hoveredPhysical:-1;
    if(next!=hoveredRow || nextWet!=hoveredWetSlot) {
        hoveredRow=next;hoveredWetSlot=nextWet;repaint();
    }
    const bool add=(e.y>=addTop && e.y<addTop+addHeight && e.x>=10 && e.x<=206);
    if(e.y>=5 && e.y<43 && e.x>=174 && e.x<=208) {
        setTooltip("Open movable microphone modeling widget");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else if(e.y>=5 && e.y<43 && e.x>=8 && e.x<=132)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else if(next>=0 && e.x>=28 && e.x<50 && (e.y-rowStart)%rowStep>=26 && processor.slotType(hoveredPhysical)!=SkellPresetAudioProcessor::Empty){
        setTooltip("Remove effect (undo available for native rack)");setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    else if(next>=0 && e.x>=wetControlLeft && e.x<wetControlRight) {
        const int physical=processor.slotIdAt(next);
        if(processor.slotType(physical)==SkellPresetAudioProcessor::Empty) {
            setTooltip("Empty rack slot: double-click its name to add an effect");
            setMouseCursor(juce::MouseCursor::NormalCursor);
            return;
        }
        setTooltip("WET "+juce::String(juce::roundToInt(slotWet(physical)*100.f))+
                   "%  /  drag to blend  /  double-click to reset");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    } else if(next>=0 && e.x>=powerControlLeft) {
        setTooltip("Click green dot to enable / bypass this effect");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else if(next>=0) {
        const int physical=processor.slotIdAt(next);
        const auto hosted=processor.slotType(physical)==SkellPresetAudioProcessor::ExternalVST3
            ?processor.hostedVST3Name(physical):juce::String{};
        setTooltip((hosted.isNotEmpty()?hosted+" / ":"")+
            "Click to edit / wheel: move effect / Shift+wheel: scroll rack / drag: reorder");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else if(add)setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
void ChainPanel::mouseExit(const juce::MouseEvent&) {
    hoveredRow=-1;hoveredWetSlot=-1;railHovered=false;wheelTargetSlot=-1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

// Move the chosen PHYSICAL slot ID, never rename/rewrite its APVTS values.
bool ChainPanel::movePhysicalSlot(int physical,int destination){
    if(physical<0 || physical>=SkellPresetAudioProcessor::slotCount ||
       processor.slotType(physical)==SkellPresetAudioProcessor::Empty)return false;
    int from=-1;
    for(int i=0;i<processor.activeSlotCount();++i)
        if(processor.slotIdAt(i)==physical){from=i;break;}
    if(from<0 || destination<0 || destination>=processor.activeSlotCount() ||
       from==destination)return false;
    processor.reorderSlot(from,destination);
    selectPhysical(physical);
    moveNotice=juce::String("FX ")+juce::String(from+1)+"  "+
        (destination<from?"UP":"DOWN")+"  "+juce::String(destination+1);
    visualSnapshotValid=false;
    repaint();
    return true;
}
void ChainPanel::selectPhysical(int physical){
    int position=-1;
    for(int i=0;i<processor.activeSlotCount();++i)
        if(processor.slotIdAt(i)==physical){position=i;break;}
    if(position<0)return;
    // Keep the moved/keyboard-selected effect in the visible eight-row view.
    if(position<scrollOffset)scrollOffset=position;
    if(position>=scrollOffset+visibleRows)
        scrollOffset=position-visibleRows+1;
    clampScroll();
    selected=physical;
    if(onSelection)onSelection(physical);
    hoveredWetSlot=-1;
    visualSnapshotValid=false;
    repaint();
}
void ChainPanel::moveRailTo(int y){
    const int count=processor.activeSlotCount();
    if(count<=visibleRows)return;
    const float height=(float)(visibleRows*rowStep-4);
    const float thumbHeight=height*(float)visibleRows/(float)count;
    const float travel=height-thumbHeight;
    if(travel<=0.f)return;
    const float position=((float)y-(float)rowStart-railGrabOffset)/travel;
    const int next=juce::jlimit(0,count-visibleRows,
        juce::roundToInt(position*(float)(count-visibleRows)));
    if(scrollOffset!=next){
        scrollOffset=next;hoveredRow=-1;hoveredWetSlot=-1;
        visualSnapshotValid=false;
        repaint();
    }
}
// 0 = scroll viewport, 1 = reorder FX, 2 = automate wet.
// Track/side/power/gutter = scroll; center body = reorder; wet knob = wet.
int ChainPanel::wheelRegionAt(int x,int y,bool shift) const{
    if(shift)return 0;
    const int row=rowAt(y);
    if(row<0)return 0;
    if(x>=wetControlLeft && x<=wetControlRight)return 2;
    if(x>=10 && x<wetControlLeft)return 1;
    return 0;
}
void ChainPanel::applyEffectWheel(int physical,float deltaY){
    if(!std::isfinite(deltaY) || deltaY==0.f)return;
    const double now=juce::Time::getMillisecondCounterHiRes();
    // Latch the PHYSICAL effect throughout a single trackpad/wheel gesture.
    // Otherwise moving the face out from under the cursor accidentally
    // starts moving its replacement row in the opposite direction.
    if(wheelTargetSlot!=physical || now-lastWheelEventMs>360.){
        wheelTargetSlot=physical;effectWheelCarry=0.f;
    }
    lastWheelEventMs=now;
    if(effectWheelCarry*deltaY<0.f)effectWheelCarry=0.f;
    effectWheelCarry=juce::jlimit(-1.f,1.f,effectWheelCarry+deltaY);
    // Half a conventional mouse-wheel step is enough, but tiny trackpad
    // deltas must accumulate instead of unintentionally shuffling the rack.
    if(std::abs(effectWheelCarry)<.45f)return;
    effectWheelCarry=0.f;
    int position=-1;
    for(int i=0;i<processor.activeSlotCount();++i)
        if(processor.slotIdAt(i)==physical){position=i;break;}
    if(position<0)return;
    const int next=position+(deltaY<0.f?1:-1);
    movePhysicalSlot(physical,next);
}
bool ChainPanel::keyPressed(const juce::KeyPress& key){
    if(key.getKeyCode()==juce::KeyPress::escapeKey){
        if(contextOpen){
            contextOpen=false;contextHover=-1;repaint();return true;
        }
        return false;
    }
    if(contextOpen){
        const int code=key.getKeyCode();
        if(code==juce::KeyPress::upKey||code==juce::KeyPress::downKey||
           code==juce::KeyPress::homeKey||code==juce::KeyPress::endKey){
            const int delta=code==juce::KeyPress::upKey||code==juce::KeyPress::endKey?-1:1;
            int next=code==juce::KeyPress::homeKey?0:code==juce::KeyPress::endKey?7:
                contextHover<0?(delta>0?0:7):juce::jlimit(0,7,contextHover+delta);
            while(next>=0 && next<8 && !contextActionEnabled(next))next+=delta;
            if(next>=0 && next<8)contextHover=next;
            repaint();return true;
        }
        if(code==juce::KeyPress::returnKey||code==juce::KeyPress::spaceKey)
            selectContextAction(contextHover>=0?contextHover:0);
        // Do not let rack reordering shortcuts pass through an open menu.
        return true;
    }
    const bool command=key.getModifiers().isCommandDown();
    const bool shift=key.getModifiers().isShiftDown();
    int position=-1;
    for(int i=0;i<processor.activeSlotCount();++i)
        if(processor.slotIdAt(i)==selected){position=i;break;}
    if(position<0 && processor.activeSlotCount()>0)position=0;
    if(position<0)return false;
    const int code=key.getKeyCode();
    if(code==juce::KeyPress::upKey || code==juce::KeyPress::downKey){
        const int delta=code==juce::KeyPress::upKey?-1:1;
        if(command){
            movePhysicalSlot(processor.slotIdAt(position),position+delta);
        } else if(shift){
            applyWheelDelta(delta<0?.5f:-.5f);
        } else {
            const int next=juce::jlimit(0,processor.activeSlotCount()-1,
                position+delta);
            selectPhysical(processor.slotIdAt(next));
        }
        return true;
    }
    if(code==juce::KeyPress::homeKey || code==juce::KeyPress::endKey){
        const int target=code==juce::KeyPress::homeKey
            ?0:processor.activeSlotCount()-1;
        if(command && shift)
            movePhysicalSlot(processor.slotIdAt(position),target);
        else if(!command)
            selectPhysical(processor.slotIdAt(target));
        return true;
    }
    if(code==juce::KeyPress::pageUpKey ||
       code==juce::KeyPress::pageDownKey){
        const int delta=code==juce::KeyPress::pageUpKey
            ?-(visibleRows-1):visibleRows-1;
        scrollOffset=juce::jlimit(0,
            juce::jmax(0,processor.activeSlotCount()-visibleRows),
            scrollOffset+delta);
        moveNotice={};hoveredRow=-1;visualSnapshotValid=false;repaint();
        return true;
    }
    if((code==juce::KeyPress::deleteKey || code==juce::KeyPress::backspaceKey) && selected>=0){removeEffectAt(position);return true;}
    if(code==juce::KeyPress::returnKey){
        selectPhysical(processor.slotIdAt(position));
        return true;
    }
    if(command && (code=='R' || code=='r')){
        selectAndReplace(position);return true;
    }
    if(command && (code=='B' || code=='b')){
        toggleSlotPower(processor.slotIdAt(position));return true;
    }
    if(command && code=='0'){
        const int physical=processor.slotIdAt(position);
        const auto name="s"+juce::String(physical+1)+"_wet";
        if(processor.slotType(physical)!=SkellPresetAudioProcessor::Empty)
            if(auto* parameter=processor.state.getParameter(name)){
                parameter->beginChangeGesture();
                setSlotWet(physical,1.f);
                parameter->endChangeGesture();
            }
        return true;
    }
    return false;
}
void ChainPanel::applyWheelDelta(float deltaY) {
    if(!std::isfinite(deltaY) || deltaY==0.f)return;
    if(railWheelCarry*deltaY<0.f)railWheelCarry=0.f;
    railWheelCarry=juce::jlimit(-1.f,1.f,railWheelCarry+deltaY);
    // Trackpad micro-deltas are not separate commands; one row per detent.
    if(std::abs(railWheelCarry)<.12f)return;
    const int direction=railWheelCarry<0.f?1:-1;
    railWheelCarry=0.f;
    const int next=juce::jlimit(0,
        juce::jmax(0,processor.activeSlotCount()-visibleRows),scrollOffset+direction);
    if(next!=scrollOffset){scrollOffset=next;hoveredRow=-1;hoveredWetSlot=-1;
        moveNotice={};
        visualSnapshotValid=false;repaint();}
}
void ChainPanel::applyWetWheel(int physical,float deltaY) {
    if(physical<0 || physical>=SkellPresetAudioProcessor::slotCount ||
       processor.slotType(physical)==SkellPresetAudioProcessor::Empty ||
       !std::isfinite(deltaY) || deltaY==0.f)return;
    auto* p=processor.state.getParameter("s"+
        juce::String(physical+1)+"_wet");
    if(!p)return;
    // Smooth trackpads produce tiny deltas; old fixed 2.5%/event jumps
    // were overly sensitive. Cap each gesture to 5% and preserve the
    // physical slot ID even after rack reordering.
    const float delta=juce::jlimit(-.05f,.05f,deltaY*.09f);
    p->beginChangeGesture();
    setSlotWet(physical,slotWet(physical)+delta);
    p->endChangeGesture();
    hoveredWetSlot=physical;repaint();
}
void ChainPanel::mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) {
    if(contextOpen){contextOpen=false;contextHover=-1;repaint();}
    if(wetDragSlot>=0 || dragging || dockingDrag || railDragging)return;
    const int region=wheelRegionAt(event.x,event.y,event.mods.isShiftDown());
    const int row=rowAt(event.y);
    if(region==2 && row>=0){
        wheelTargetSlot=-1;
        applyWetWheel(processor.slotIdAt(row),wheel.deltaY);
        return;
    }
    if(region==1 && row>=0){
        // Hold the moving effect by its permanent ID until the trackpad
        // gesture ends, despite the next row appearing under the pointer.
        const double now=juce::Time::getMillisecondCounterHiRes();
        const bool samePointer=std::abs(event.x-lastWheelX)<=12 &&
                               std::abs(event.y-lastWheelY)<=10;
        const int physical=wheelTargetSlot>=0 &&
            now-lastWheelEventMs<360. && samePointer
            ?wheelTargetSlot:processor.slotIdAt(row);
        lastWheelX=event.x;lastWheelY=event.y;
        applyEffectWheel(physical,wheel.deltaY);
        return;
    }
    wheelTargetSlot=-1;effectWheelCarry=0.f;
    moveNotice={};
    applyWheelDelta(wheel.deltaY);
}
bool ChainPanel::testWheelRegions(){
    // The three controls must remain independent at the first rack row.
    const int y=rowStart+rowHeight/2;
    return wheelRegionAt(100,y,false)==1 &&
           wheelRegionAt(165,y,false)==2 &&
           wheelRegionAt(193,y,false)==0 &&
           wheelRegionAt(209,y,false)==0 &&
           wheelRegionAt(100,y,true)==0 &&
           wheelRegionAt(165,y,true)==0 &&
           wheelRegionAt(100,5,false)==0 &&
           contextActionAt(13,80,50)==0 &&
           contextActionAt(202,103,50)==0 &&
           contextActionAt(202,104,50)==-1 &&
           contextActionAt(203,80,50)==-1 &&
           contextActionAt(13,80+6*26+23,50)==6 &&
           contextActionAt(13,80+6*26+24,50)==-1 &&
           contextActionAt(13,80+7*26+23,50)==7 &&
           contextActionAt(13,80+7*26+24,50)==-1;
}
bool ChainPanel::testProgrammaticSelectionReveal(){
    if(processor.activeSlotCount()<=visibleRows)return false;
    const int oldSelection=selected,oldScroll=scrollOffset;
    const int last=processor.activeSlotCount()-1;
    const int target=processor.slotIdAt(last);
    scrollOffset=0;
    setSelectedSlot(target);
    const bool revealed=selected==target &&
        last>=scrollOffset && last<scrollOffset+visibleRows;
    scrollOffset=oldScroll;selected=oldSelection;
    visualSnapshotValid=false;repaint();
    return revealed;
}
bool ChainPanel::testRackRailNavigation(){
    const int count=processor.activeSlotCount();
    if(count<=visibleRows)return false;
    const int previousScroll=scrollOffset;
    const float previousGrab=railGrabOffset;
    std::array<int,SkellPresetAudioProcessor::slotCount> order{};
    for(int i=0;i<count;++i)order[(size_t)i]=processor.slotIdAt(i);
    scrollOffset=0;railGrabOffset=0.f;
    moveRailTo(rowStart+visibleRows*rowStep-4);
    const bool atBottom=scrollOffset==count-visibleRows;
    moveRailTo(rowStart);
    bool valid=atBottom && scrollOffset==0;
    for(int i=0;i<count;++i)
        valid=valid && processor.slotIdAt(i)==order[(size_t)i];
    scrollOffset=previousScroll;railGrabOffset=previousGrab;
    visualSnapshotValid=false;repaint();
    return valid;
}
bool ChainPanel::testWheelReordersPhysicalEffect(){
    const int count=processor.activeSlotCount();
    if(count<3)return false;
    const int first=processor.slotIdAt(0),second=processor.slotIdAt(1);
    if(processor.slotType(first)==SkellPresetAudioProcessor::Empty)return false;
    std::array<int,SkellPresetAudioProcessor::slotCount> before{};
    for(int i=0;i<count;++i)before[(size_t)i]=processor.slotIdAt(i);
    const int oldSelected=selected,oldScroll=scrollOffset;
    const auto previousNotice=moveNotice;
    const float priorWet=slotWet(first);
    wheelTargetSlot=-1;effectWheelCarry=0.f;lastWheelEventMs=0.;
    applyEffectWheel(first,-.5f);
    const bool down=processor.slotIdAt(1)==first &&
        processor.slotIdAt(0)==second;
    applyEffectWheel(first,-.5f); // same physical ID, not the replacement row
    const bool downTwice=processor.slotIdAt(2)==first;
    applyEffectWheel(first,.5f);
    applyEffectWheel(first,.5f);
    bool restored=true;
    for(int i=0;i<count;++i)
        restored=restored && processor.slotIdAt(i)==before[(size_t)i];
    if(!restored){
        int position=-1;
        for(int i=0;i<count;++i)
            if(processor.slotIdAt(i)==first)position=i;
        if(position>=0)processor.reorderSlot(position,0);
    }
    bool finalOrder=true;
    for(int i=0;i<count;++i)
        finalOrder=finalOrder && processor.slotIdAt(i)==before[(size_t)i];
    selected=oldSelected;scrollOffset=oldScroll;clampScroll();
    moveNotice=previousNotice;
    wheelTargetSlot=-1;effectWheelCarry=0.f;lastWheelEventMs=0.;
    visualSnapshotValid=false;repaint();
    return down && downTwice && restored && finalOrder &&
           std::abs(slotWet(first)-priorWet)<.00001f;
}
bool ChainPanel::testRackKeyboardNavigation(){
    const int count=processor.activeSlotCount();
    if(count<=visibleRows || count<2)return false;
    const int first=processor.slotIdAt(0),second=processor.slotIdAt(1);
    if(processor.slotType(first)==SkellPresetAudioProcessor::Empty)return false;
    const int oldSelected=selected,oldScroll=scrollOffset;
    const auto oldNotice=moveNotice;
    selectPhysical(first);
    const auto plain=juce::ModifierKeys();
    const auto command=juce::ModifierKeys(juce::ModifierKeys::commandModifier);
    keyPressed(juce::KeyPress(juce::KeyPress::downKey,plain,0));
    const bool arrowDown=selected==second;
    keyPressed(juce::KeyPress(juce::KeyPress::upKey,plain,0));
    const bool arrowUp=selected==first;
    keyPressed(juce::KeyPress(juce::KeyPress::downKey,command,0));
    const bool moveDown=processor.slotIdAt(1)==first;
    keyPressed(juce::KeyPress(juce::KeyPress::upKey,command,0));
    const bool moveUp=processor.slotIdAt(0)==first;
    keyPressed(juce::KeyPress(juce::KeyPress::endKey,plain,0));
    const bool end=selected==processor.slotIdAt(count-1) &&
        scrollOffset==count-visibleRows;
    keyPressed(juce::KeyPress(juce::KeyPress::homeKey,plain,0));
    const bool home=selected==first && scrollOffset==0;
    keyPressed(juce::KeyPress(juce::KeyPress::pageDownKey,plain,0));
    const bool pageDown=scrollOffset==count-visibleRows;
    keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey,plain,0));
    const bool pageUp=scrollOffset==0;
    selected=oldSelected;scrollOffset=oldScroll;clampScroll();
    moveNotice=oldNotice;visualSnapshotValid=false;repaint();
    return arrowDown && arrowUp && moveDown && moveUp &&
           end && home && pageDown && pageUp &&
           processor.slotIdAt(0)==first && processor.slotIdAt(1)==second;
}
bool ChainPanel::testWheelScrollDoesNotReorder() {
    if(processor.activeSlotCount()<=visibleRows)return false;
    const int oldScroll=scrollOffset;
    std::array<int,SkellPresetAudioProcessor::slotCount> before{};
    for(int i=0;i<processor.activeSlotCount();++i)
        before[(size_t)i]=processor.slotIdAt(i);
    scrollOffset=0;
    railWheelCarry=0.f;
    applyWheelDelta(-.55f);
    bool valid=scrollOffset==1;
    for(int i=0;i<processor.activeSlotCount();++i)
        valid=valid && before[(size_t)i]==processor.slotIdAt(i);
    scrollOffset=oldScroll;
    railWheelCarry=0.f;
    visualSnapshotValid=false;
    repaint();
    return valid;
}
bool ChainPanel::testWheelWetAndRack() {
    if(processor.activeSlotCount()<=visibleRows)return false;
    const int physical=processor.slotIdAt(0);
    if(processor.slotType(physical)==SkellPresetAudioProcessor::Empty)return false;
    const float previous=slotWet(physical);
    const int previousScroll=scrollOffset;
    setSlotWet(physical,.50f);
    applyWetWheel(physical,.5f);
    const bool wetIncreased=std::abs(slotWet(physical)-.545f)<.002f;
    const int initialScroll=scrollOffset;
    applyWetWheel(physical,-.5f);
    const bool wetRestored=std::abs(slotWet(physical)-.50f)<.002f &&
                           scrollOffset==initialScroll;
    setSlotWet(physical,previous);
    scrollOffset=0;railWheelCarry=0.f;
    applyWheelDelta(-.2f);
    const bool scrolled=scrollOffset==1;
    scrollOffset=previousScroll;
    railWheelCarry=0.f;
    visualSnapshotValid=false;repaint();
    return wetIncreased && wetRestored && scrolled;
}
bool ChainPanel::testRemoveExtraSlot() {
    const int before=processor.activeSlotCount();
    if(before>=SkellPresetAudioProcessor::slotCount)return false;
    const int added=processor.appendRackSlot();
    if(added<0)return false;
    const bool removed=processor.removeRackSlot(before);
    const bool stable=processor.activeSlotCount()==before &&
        processor.slotIdAt(before)==added;
    visualSnapshotValid=false;repaint();
    return removed&&stable;
}
