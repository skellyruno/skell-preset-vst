#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../../SkellCore/include/SkellCore/VisualMath.h"

class SkellLiveEffectPanel final:public juce::Component {
    using P=SkellPresetAudioProcessor;
    using Analyzer=skellpreset::visual::LiveSpectrumAnalyzer;
    static constexpr int historySize=90;
public:
    bool update(P& p,int slot) {
        const int type=p.slotType(slot);
        const bool selectionChanged=slot!=lastSlot || type!=lastType;
        if(selectionChanged){lastSlot=slot;lastType=type;reset();lastRevision=0;}
        const auto oldTransfer=transfer;
        const auto oldComp=compressorCurve;
        const float oldDeess=deessHz,oldGate=gateDb,oldClose=gateCloseDb;
        deessHz=3500.f+p.getSlotParam(slot,0)*8000.f;
        if(type==P::Saturator){
            const auto* grit=p.state.getRawParameterValue("macro4");
            const float drive=std::pow(10.f,(p.getSlotParam(slot,0)*24.f+10.f*((grit?grit->load():.5f)-.5f))/20.f);
            for(size_t n=0;n<transfer.size();++n){const float x=2.f*(float)n/(transfer.size()-1)-1.f;
                transfer[n]=skellpreset::visual::saturatorCore(x,drive,.5f+p.getSlotParam(slot,1),std::max(.5f,drive*.6f));}
        }else if(type==P::StudioFastDistFX){
            curveDist.set(p.getSlotParam(slot,0),p.getSlotParam(slot,1),p.getSlotParam(slot,2),p.getSlotParam(slot,3),p.getSlotParam(slot,4));
            for(size_t n=0;n<transfer.size();++n)transfer[n]=curveDist.sample(2.f*(float)n/(transfer.size()-1)-1.f);
        }
        gateDb=-75.f+p.getSlotParam(slot,0)*70.f;
        gateCloseDb=skellcore::visual::gateThresholds(gateDb,p.getSlotParam(slot,3)*8.f).closeDb;
        if(type==P::Compressor){
            const auto* punch=p.state.getRawParameterValue("macro2");
            const float threshold=-48.f+p.getSlotParam(slot,0)*42.f-12.f*((punch?punch->load():.5f)-.5f);
            skellcore::visual::buildCompressorCurve(compressorCurve.data(),compressorCurve.size(),-60.f,0.f,
                threshold,1.f+p.getSlotParam(slot,1)*11.f,(p.getSlotParam(slot,4)-.5f)*24.f,0.f);
        }
        const bool controlsChanged=oldTransfer!=transfer || oldComp!=compressorCurve ||
            oldDeess!=deessHz || oldGate!=gateDb || oldClose!=gateCloseDb;
        const auto now=juce::Time::getMillisecondCounter();
        skellpreset::visual::SlotVisualCapture::Frame frame;
        const bool available=p.getSlotVisual(slot,frame);
        if(available && frame.revision!=lastRevision){
            const float elapsed=lastSerial && frame.serial>lastSerial?std::min(.25f,(float)((frame.serial-lastSerial)/p.preparedAudioRate())):1.f/30.f;
            spectrumElapsed+=elapsed;spectrumClock+=elapsed;
            lastRevision=frame.revision;lastSerial=frame.serial;lastFrame=now;fresh=true;
            wetAvailable=frame.returnAvailable;tapMs=frame.tapMs;wetRms=frame.returnRms;
            if(wetAvailable && type==P::VoxRackRoom)tail.update(frame.inputRms,std::sqrt(.5f*(wetRms[0]*wetRms[0]+wetRms[1]*wetRms[1])),elapsed);
            if(type==P::StudioPitchFormantFX && frame.pitchReady && (ticks&1u)==0){
                pitchIn=skellpreset::visual::estimatePitch(frame.pitchInput,frame.pitchRate);
                pitchOut=skellpreset::visual::estimatePitch(frame.pitchOutput,frame.pitchRate);
                shift(pitchInHistory,pitchIn.hz>0.f?pitchIn.midi:-1.f);
                shift(pitchOutHistory,pitchOut.hz>0.f?pitchOut.midi:-1.f);
            }
            inputRms=frame.inputRms;outputRms=frame.outputRms;sideRms=frame.sideRms;
            gr=std::max(p.getSlotGainReductionDb(slot),p.getSlotDeessReductionDb(slot));
            duck=p.getSlotDuckReductionDb(slot);
            correlation=frame.correlation;leftRms=frame.leftRms;rightRms=frame.rightRms;
            sharedValid=frame.sharedValid;
            if(sharedValid){
                const auto& a=frame.sharedInput;const auto& b=frame.sharedOutput;
                inputRms=std::sqrt(.5f*(a.rmsL*a.rmsL+a.rmsR*a.rmsR));
                outputRms=std::sqrt(.5f*(b.rmsL*b.rmsL+b.rmsR*b.rmsR));
                leftRms=b.rmsL;rightRms=b.rmsR;correlation=b.correlation;
                sideRms=std::sqrt(std::max(0.f,b.sideMs));
                grValid=(b.flags&skellcore::telemetry::kFlagGrValid)!=0;
                gateMean=b.gateMean;
                if(grValid)gr=b.grDbMax;
                if(gateMean>=0.f)gr=std::max(0.f,-skellcore::telemetry::toDb(gateMean));
            }
            // Retain a fading actual prior capture instead of averaging unrelated samples.
            previousMid=mid;previousSide=side;mid=frame.mid;side=frame.side;
            shift(inputHistory,level(inputRms));shift(outputHistory,level(outputRms));
            if(sharedValid && grValid && frame.slowCount>0 && (type==P::Compressor || type==P::StudioCompressorFX)){
                // Input/output peak and GR share the same DSP-clock 20ms buckets.
                inputHistory.fill(0.f);outputHistory.fill(0.f);grHistory.fill(0.f);
                compressorHistoryCount=std::min(frame.slowCount,(size_t)historySize);
                for(size_t k=0;k<frame.slowCount;++k){
                    const auto& b=frame.slow[k];const size_t index=historySize-frame.slowCount+k;
                    inputHistory[index]=level(std::max(std::abs(b.inMin),std::abs(b.inMax)));
                    outputHistory[index]=level(std::max(std::abs(b.outMin),std::abs(b.outMax)));
                    grHistory[index]=std::clamp(b.grDbMax/24.f,0.f,1.f);
                }
            }else {compressorHistoryCount=0;shift(grHistory,std::clamp(gr/24.f,0.f,1.f));}
            shift(duckHistory,std::clamp(duck/24.f,0.f,1.f));
            // Bounded analysis clocked by captured audio, not paint rate.
            ++ticks;
            const bool detailed=type==P::EQ || type==P::StudioEQFX || type==P::Saturator || type==P::StudioFastDistFX || type==P::Reverb || type==P::VoxRackRoom;
            const float period=detailed?1.f/30.f:1.f/15.f;
            if(spectrumClock>=period-1.e-5f || ticks==1){
                const float dt=std::clamp(spectrumElapsed,1.f/120.f,.25f);spectrumElapsed=0.f;
                // Preserve fractional cadence; discarding the remainder can
                // halve a 30Hz analyzer at a 31Hz UI/audio update rate.
                spectrumClock=ticks==1?0.f:std::fmod(spectrumClock,period);
                const auto input=analyzer.analyze(frame.input,p.preparedAudioRate());
                const auto output=analyzer.analyze(frame.output,p.preparedAudioRate());
                for(size_t i=0;i<input.size();++i){
                    inBands[i]=skellpreset::visual::followVisual(inBands[i],level(input[i]),dt);
                    outBands[i]=skellpreset::visual::followVisual(outBands[i],level(output[i]),dt);
                    deltaBands[i]=skellpreset::visual::followVisual(deltaBands[i],skellpreset::visual::spectralDeltaDb(input[i],output[i])/24.f,dt,.06f,.12f);
                }
                const bool isolated=frame.returnSpectrumReady;
                if(isolated!=isolatedHistory){historyHead=historyCount=0;isolatedHistory=isolated;}
                auto& row=spectrumHistory[historyHead];
                if(isolated){
                    const auto l=analyzer.analyze(frame.returnLeft,p.preparedAudioRate());
                    const auto r=analyzer.analyze(frame.returnRight,p.preparedAudioRate());
                    for(size_t i=0;i<row.size();++i)row[i]=level(std::sqrt(.5f*(l[i]*l[i]+r[i]*r[i])));
                }else row=outBands;
                historyHead=(historyHead+1)%spectrumHistory.size();
                historyCount=std::min(historyCount+1,spectrumHistory.size());
            }
            for(size_t i=0;i<wave.size();++i){
                wave[i]=frame.output[frame.output.size()-wave.size()+i];
                inputWave[i]=frame.input[frame.input.size()-inputWave.size()+i];
            }
            if(isDrive(type)){
                auto& trace=driveTraces[driveHead];trace.input=inputWave;trace.output=wave;
                driveHead=(driveHead+1)%driveTraces.size();driveCount=std::min(driveCount+1,driveTraces.size());
            }
            shift(leftHistory,level(wetAvailable?wetRms[0]:leftRms));shift(rightHistory,level(wetAvailable?wetRms[1]:rightRms));
            shift(wetHistory,wetAvailable?level(std::sqrt(.5f*(wetRms[0]*wetRms[0]+wetRms[1]*wetRms[1]))):0.f);
        }else if((unsigned)(now-lastFrame)>=400u){
            if(fresh){
                // Clear stale graphics, not the last consumed capture identity.
                // Otherwise the same stopped frame would reappear every 400 ms.
                reset();return true;
            }
            return controlsChanged || selectionChanged;
        }else return controlsChanged || selectionChanged;
        return true;
    }
    bool hasSignal() const noexcept {return fresh && inputRms>1.e-6f;}
    float measuredInputRms() const noexcept {return fresh?inputRms:0.f;}
    // Round the display-bin contour without overshooting measured values.
    template<class Points> static juce::Path smoothContour(const Points& points) {
        juce::Path line;
        if(points.empty())return line;
        line.startNewSubPath(points.front());
        for(size_t i=1;i+1<points.size();++i)line.quadraticTo(points[i],points[i]+(points[i+1]-points[i])*.5f);
        if(points.size()>1)line.lineTo(points.back());
        return line;
    }
    void paintEqSpectrum(juce::Graphics& g,juce::Rectangle<float> plot) const {
        if(!fresh)return;
        juce::Graphics::ScopedSaveState clipped(g);
        g.reduceClipRegion(plot.toNearestInt());
        const auto draw=[&](const auto& values,juce::Colour colour,bool filled){
            std::array<juce::Point<float>,Analyzer::bands> points{};
            for(size_t i=0;i<values.size();++i){
                const float x=plot.getX()+plot.getWidth()*std::log(Analyzer::frequency(static_cast<int>(i))/20.f)/std::log(1000.f);
                points[i]={x,plot.getBottom()-values[i]*plot.getHeight()};
            }
            auto line=smoothContour(points);
            if(filled){auto area=line;const float left=plot.getX()+plot.getWidth()*std::log(80.f/20.f)/std::log(1000.f);
                const float right=plot.getX()+plot.getWidth()*std::log(9000.f/20.f)/std::log(1000.f);
                area.lineTo(right,plot.getBottom());area.lineTo(left,plot.getBottom());area.closeSubPath();
                g.setGradientFill(juce::ColourGradient(colour.withAlpha(.29f),plot.getCentreX(),plot.getY(),colour.withAlpha(.06f),plot.getCentreX(),plot.getBottom(),false));
                g.fillPath(area);}
            g.setColour(colour.withAlpha(.10f));g.strokePath(line,juce::PathStrokeType(4.f));
            g.setColour(colour.withAlpha(.86f));g.strokePath(line,juce::PathStrokeType(1.4f));
        };
        draw(inBands,Theme::muted,true);draw(outBands,Theme::blue,true);
        Theme::fittedText(g,"IN / OUT   -72..0 dBFS",{juce::roundToInt(plot.getX()+12.f),juce::roundToInt(plot.getY()+6.f),164,13},9.f,Theme::muted,true);
        g.setColour(Theme::blue);g.drawLine(plot.getX()+172.f,plot.getY()+12.f,plot.getX()+188.f,plot.getY()+12.f,1.2f);
    }
    void paintCompressorTransfer(juce::Graphics& g,juce::Rectangle<float> area,P& p,int slot,int type) const {
        Theme::panelBox(g,area,6.f);
        auto plot=area.reduced(10.f,8.f);const auto title=plot.removeFromTop(18.f);
        Theme::fittedText(g,"DETECTOR TRANSFER / dB",title.toNearestInt(),10.f,Theme::muted,true);
        const auto bottom=plot.removeFromBottom(12.f);
        plot.removeFromLeft(16.f);plot.removeFromRight(8.f);
        for(int i=0;i<=3;++i){const int x=juce::roundToInt(plot.getX()+plot.getWidth()*i/3.f);
            Theme::fittedText(g,juce::String(-60+20*i),{x-12,juce::roundToInt(bottom.getY()),24,12},8.f,Theme::muted,false,juce::Justification::centred);}

        for(int i=0;i<=3;++i)Theme::fittedText(g,juce::String(-20*i),juce::Rectangle<int>((int)plot.getX()-20,(int)(plot.getY()+plot.getHeight()*i/3.f)-5,18,10),8.f,Theme::muted,false,juce::Justification::centredRight);
        const float a=p.getSlotParam(slot,0),b=p.getSlotParam(slot,1),k=p.getSlotParam(slot,4);
        const bool studio=type==P::StudioCompressorFX;
        const auto* punch=p.state.getRawParameterValue("macro2");
        const float threshold=studio?20.f*std::log10(juce::jmax(.001f,skellpreset::studio::detail::level(juce::roundToInt(a*1000.f),7.))):-48.f+a*42.f-12.f*((punch?punch->load():.5f)-.5f);
        const float ratio=studio?1.f+.38f*(std::pow(51.f,b)-1.f):1.f+b*11.f;
        const float knee=(std::pow(201.f,k)-1.f)*.005f;
        const float kneeNorm=knee>1.e-6f?1.f/std::log1p(knee):0.f;
        for(int i=1;i<3;++i){const float f=(float)i/3.f;g.setColour(Theme::stroke.withAlpha(.45f));g.drawLine(plot.getX()+plot.getWidth()*f,plot.getY(),plot.getX()+plot.getWidth()*f,plot.getBottom(),.6f);g.drawLine(plot.getX(),plot.getY()+plot.getHeight()*f,plot.getRight(),plot.getY()+plot.getHeight()*f,.6f);}
        g.setColour(Theme::muted.withAlpha(.45f));g.drawLine(plot.getX(),plot.getBottom(),plot.getRight(),plot.getY(),.8f);
        juce::Path curve;
        for(int i=0;i<=96;++i){const float f=(float)i/96.f,in=-60.f+60.f*f,over=std::max(0.f,in-threshold);
            const float slope=1.f/ratio-1.f;
            const float gain=studio && knee>1.e-5f && over>0.f?((std::log1p((over+1.f)*knee)*kneeNorm-1.f)*(slope+1.f)-over):over*slope;
            const float y=plot.getBottom()-plot.getHeight()*juce::jlimit(0.f,1.f,(in+gain+60.f)/60.f);
            const float x=plot.getX()+plot.getWidth()*f;if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);
        }
        g.setColour(Theme::blue.withAlpha(.14f));g.strokePath(curve,juce::PathStrokeType(5.f));
        g.setColour(Theme::white);g.strokePath(curve,juce::PathStrokeType(1.7f));
        const float x=plot.getX()+plot.getWidth()*juce::jlimit(0.f,1.f,(threshold+60.f)/60.f);
        g.setColour(Theme::amber.withAlpha(.7f));
        for(float y=plot.getY();y<plot.getBottom()-2.f;y+=5.f)g.drawLine(x,y,x,y+2.5f,.9f);
        if(fresh && inputRms>1.e-6f && outputRms>1.e-6f){
            // Actual slot RMS point, independent of the model curve.
            const auto norm=[](float rms){return juce::jlimit(0.f,1.f,(20.f*std::log10(rms)+60.f)/60.f);};
            const float px=plot.getX()+plot.getWidth()*norm(inputRms),py=plot.getBottom()-plot.getHeight()*norm(outputRms);
            g.setColour(Theme::blue.withAlpha(.22f));g.fillEllipse(px-5.f,py-5.f,10.f,10.f);
            g.setColour(Theme::blue);g.fillEllipse(px-2.f,py-2.f,4.f,4.f);
            g.setColour(Theme::white.withAlpha(.8f));g.drawEllipse(px-2.f,py-2.f,4.f,4.f,.7f);
        }
    }
    void paintHistory(juce::Graphics& g,juce::Rectangle<float> area,bool reduction) const {
        Theme::panelBox(g,area,6.f,true);
        auto plot=area.reduced(10.f,7.f);
        auto title=plot.removeFromTop(15.f);
        // ASCII range labels are portable across the Linux and Windows fonts.
        // Distinguish the measured gain-reduction and sidechain traces.
        const bool measuredCompressor=lastType==P::Compressor || lastType==P::StudioCompressorFX;
        const auto titleText=reduction?juce::String(measuredCompressor?"GAIN REDUCTION / 1.8 s / 0-24 dB":"REDUCTION / 0-24 dB"):juce::String("INPUT / OUTPUT / -72 TO 0 dBFS");
        Theme::fittedText(g,titleText,title.toNearestInt(),10.f,Theme::muted,true);
        const auto legend=plot.removeFromBottom(13.f);
        const auto left=legend.withWidth(legend.getWidth()*.5f);
        const auto right=legend.withTrimmedLeft(legend.getWidth()*.5f);
        if(reduction) {
            Theme::fittedText(g,"GR "+juce::String(grHistory.back()*24.f,1)+" dB / OUTPUT",left.toNearestInt(),9.5f,Theme::red,true);
            Theme::fittedText(g,"OUT "+db(outputRms),right.toNearestInt(),9.5f,Theme::blue,true,juce::Justification::centredRight);
        } else {
            Theme::fittedText(g,"INPUT",left.toNearestInt(),9.5f,Theme::muted,true);
            Theme::fittedText(g,"OUTPUT",right.toNearestInt(),9.5f,Theme::green,true,juce::Justification::centredRight);
        }
        plot.removeFromTop(3.f);
        g.setColour(Theme::stroke.withAlpha(.45f));
        for(int i=0;i<=3;++i)g.drawHorizontalLine(static_cast<int>(plot.getY()+i*plot.getHeight()/3.f),plot.getX(),plot.getRight());
        for(int i=1;i<8;++i)g.drawVerticalLine((int)(plot.getX()+i*plot.getWidth()/8.f),plot.getY(),plot.getBottom());
        auto draw=[&](const auto& values,juce::Colour colour,bool down){
            const size_t first=measuredCompressor && compressorHistoryCount>0?values.size()-compressorHistoryCount:0;
            juce::Path line;for(size_t i=first;i<values.size();++i){
                const float x=plot.getX()+plot.getWidth()*static_cast<float>(i)/static_cast<float>(values.size()-1);
                const float y=down?plot.getY()+values[i]*plot.getHeight():plot.getBottom()-values[i]*plot.getHeight();
                if(i==first)line.startNewSubPath(x,y);else line.lineTo(x,y);}
            auto fill=line;const float base=down?plot.getY():plot.getBottom();
            fill.lineTo(plot.getRight(),base);fill.lineTo(plot.getX()+plot.getWidth()*(float)first/(float)(values.size()-1),base);fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(down?.40f:.25f),plot.getCentreX(),plot.getY(),colour.withAlpha(.035f),plot.getCentreX(),plot.getBottom(),false));g.fillPath(fill);
            g.setColour(colour.withAlpha(.10f));g.strokePath(line,juce::PathStrokeType(3.7f));
            g.setColour(colour);g.strokePath(line,juce::PathStrokeType(1.35f));
        };
        if(reduction && fresh){draw(inputHistory,Theme::muted.withAlpha(.45f),false);draw(outputHistory,Theme::blue,false);draw(grHistory,Theme::red,true);
            // Sidechain history has a different clock from DSP compressor buckets.
            if(lastType!=P::Compressor && lastType!=P::StudioCompressorFX && duck>0.f)draw(duckHistory,Theme::amber,true);}
        else if(reduction)Theme::fittedText(g,"WAITING FOR AUDIO",plot.toNearestInt(),11.f,Theme::muted,true,juce::Justification::centred);
        else if(fresh){draw(inputHistory,Theme::muted,false);draw(outputHistory,Theme::green,false);}
    }
    void paintSpace(juce::Graphics& g,juce::Rectangle<float> area) const {
        Theme::panelBox(g,area,6.f,true);
        auto plot=area.reduced(10.f,7.f);
        auto title=plot.removeFromTop(15.f);
        Theme::fittedText(g,isolatedHistory?"WET RETURN WATERFALL  /  80 Hz - 9 kHz":"OUTPUT WATERFALL  /  80 Hz - 9 kHz",title.toNearestInt(),10.f,Theme::muted,true);
        const auto legend=plot.removeFromBottom(13.f);
        plot.removeFromRight(32.f);
        constexpr float tickHz[]{80.f,200.f,500.f,1000.f,3000.f,9000.f};
        constexpr const char* ticksHz[]{"80 Hz","200","500","1k","3k","9k"};
        for(size_t i=0;i<6;++i){const float f=std::log(tickHz[i]/80.f)/std::log(112.5f);
            const int x=juce::roundToInt(plot.getX()+plot.getWidth()*(.04f+.86f*f));
            Theme::fittedText(g,ticksHz[i],{x-20,juce::roundToInt(legend.getY()),40,13},8.5f,Theme::muted,false,juce::Justification::centred);}

        const float h=plot.getHeight(),w=plot.getWidth();
        // Labels are outside the mesh clip, in the reserved right gutter.
        for(int age=0;age<=4;++age){
            const float d=(float)age/4.f,base=plot.getY()+h*(.30f+.64f*d);
            Theme::fittedText(g,juce::String((1.f-d)*1.033f,2)+"s",
                {(int)plot.getRight()+3,(int)(base-h*.10f)-5,29,10},8.f,Theme::muted);
        }
        juce::Graphics::ScopedSaveState clipped(g);g.reduceClipRegion(plot.toNearestInt());
        for(int age=0;age<=4;++age){
            const float d=(float)age/4.f,base=plot.getY()+h*(.30f+.64f*d),left=plot.getX()+w*(.32f-.28f*d),span=w*(.60f+.26f*d);
            g.setColour(Theme::stroke.withAlpha(.55f));g.drawLine(left,base,left+span,base-h*.10f,.65f);
        }
        for(int grid=0;grid<5;++grid){
            const float f=(float)grid/4.f;
            g.setColour(Theme::stroke.withAlpha(.50f));
            g.drawLine(plot.getX()+w*(.18f+.64f*f),plot.getY()+h*.30f,plot.getX()+w*f,plot.getBottom(),.65f);
        }
        for(size_t row=0;row<historyCount;++row){
            const size_t index=(historyHead+spectrumHistory.size()-historyCount+row)%spectrumHistory.size();
            const float depth=(float)(spectrumHistory.size()-historyCount+row+1)/(float)spectrumHistory.size();
            const float width=w*(.60f+.26f*depth),left=plot.getX()+w*(.32f-.28f*depth);
            const float base=plot.getY()+h*(.30f+.64f*depth);
            const auto& values=spectrumHistory[index];
            std::array<juce::Point<float>,Analyzer::bands> points{};
            for(size_t band=0;band<values.size();++band){
                const float f=(float)band/(float)(values.size()-1);
                points[band]={left+width*f,base-values[band]*h*(.22f+.40f*depth)-h*.10f*f};
            }
            juce::Path ridge;
            if(row+3>=historyCount)ridge=smoothContour(points);
            else{ridge.startNewSubPath(points.front());for(size_t i=1;i<points.size();++i)ridge.lineTo(points[i]);}
            auto fill=ridge;fill.lineTo(left+width,base-h*.10f);fill.lineTo(left,base);fill.closeSubPath();
            const auto ink=Theme::blue.interpolatedWith(Theme::green,.18f*depth);
            g.setGradientFill(juce::ColourGradient(Theme::bg.interpolatedWith(ink,.13f+.23f*depth).withAlpha(.81f),left,base-h*.55f,Theme::bg.withAlpha(.68f),left,base,false));g.fillPath(fill);
            g.setColour(ink.withAlpha(.20f+.65f*depth));g.strokePath(ridge,juce::PathStrokeType(row+1==historyCount?1.7f:.75f));
        }
        if(!fresh)Theme::fittedText(g,"WAITING FOR AUDIO",plot.toNearestInt(),11.f,Theme::muted,true,juce::Justification::centred);
    }
    // Paired slot input/output samples, not the theoretical core curve. A fixed
    // full-scale axis avoids misleading amplitude changes when drive is adjusted.
    void paintDriveTransferTrace(juce::Graphics& g,juce::Rectangle<float> plot) const {
        if(!fresh || driveCount==0)return;
        juce::Graphics::ScopedSaveState clipped(g);g.reduceClipRegion(plot.toNearestInt());
        const auto rose=juce::Colour(0xffc77996);
        for(size_t age=0;age<driveCount;++age){
            const auto& trace=driveTraces[(driveHead+driveTraces.size()-driveCount+age)%driveTraces.size()];
            juce::Path path;
            for(size_t i=0;i<trace.input.size();++i){
                const float x=plot.getCentreX()+plot.getWidth()*.5f*std::clamp(trace.input[i],-1.f,1.f);
                const float y=plot.getCentreY()-plot.getHeight()*.5f*std::clamp(trace.output[i],-1.f,1.f);
                if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
            }
            const float alpha=.10f+.42f*(float)(age+1)/(float)driveCount;
            g.setColour(rose.withAlpha(alpha));g.strokePath(path,juce::PathStrokeType(age+1==driveCount?1.15f:.65f));
        }
        const auto& latest=driveTraces[(driveHead+driveTraces.size()-1)%driveTraces.size()];
        const float x=plot.getCentreX()+plot.getWidth()*.5f*std::clamp(latest.input.back(),-1.f,1.f);
        const float y=plot.getCentreY()-plot.getHeight()*.5f*std::clamp(latest.output.back(),-1.f,1.f);
        g.setColour(Theme::white.withAlpha(.90f));g.fillEllipse(x-1.8f,y-1.8f,3.6f,3.6f);
    }
    void paintDriveSpectrum(juce::Graphics& g,juce::Rectangle<float> area) const {
        Theme::panelBox(g,area,6.f);
        auto inner=area.reduced(10.f,7.f);
        Theme::fittedText(g,"INPUT / OUTPUT SPECTRUM",inner.removeFromTop(15.f).toNearestInt(),10.f,Theme::muted,true);
        auto upper=inner.removeFromTop(inner.getHeight()*.58f);
        inner.removeFromTop(5.f);
        Theme::fittedText(g,"SPECTRAL CHANGE / +/-24 dB",inner.removeFromTop(13.f).toNearestInt(),9.f,Theme::muted,true);
        const auto rose=juce::Colour(0xffc77996);
        for(auto r:{upper,inner})for(int n=1;n<8;++n){const float x=r.getX()+r.getWidth()*n/8.f;g.setColour(Theme::stroke.withAlpha(.4f));g.drawLine(x,r.getY(),x,r.getBottom(),.6f);}
        g.setColour(Theme::stroke);g.drawLine(inner.getX(),inner.getCentreY(),inner.getRight(),inner.getCentreY(),.6f);
        if(!fresh){Theme::fittedText(g,"WAITING FOR AUDIO",upper.toNearestInt(),11.f,Theme::muted,true,juce::Justification::centred);return;}
        auto draw=[&](const auto& values,juce::Rectangle<float> r,juce::Colour tint,bool centred){
            std::array<juce::Point<float>,Analyzer::bands> points{};
            for(size_t i=0;i<values.size();++i){const float x=r.getX()+r.getWidth()*(float)i/(float)(values.size()-1);const float y=centred?r.getCentreY()-values[i]*r.getHeight()*.5f:r.getBottom()-values[i]*r.getHeight();points[i]={x,y};}
            auto line=smoothContour(points);
            auto fill=line;const float base=centred?r.getCentreY():r.getBottom();fill.lineTo(r.getRight(),base);fill.lineTo(r.getX(),base);fill.closeSubPath();g.setColour(tint.withAlpha(.24f));g.fillPath(fill);g.setColour(tint.withAlpha(.88f));g.strokePath(line,juce::PathStrokeType(1.25f));
        };
        draw(inBands,upper,Theme::muted,false);draw(outBands,upper,rose,false);
        draw(deltaBands,inner,rose,true);
    }
    void paintHarmonicSpectrum(juce::Graphics& g,juce::Rectangle<float> area) const {
        if(!fresh)return;
        const float dx=area.getWidth()/static_cast<float>(inBands.size());
        for(size_t i=0;i<inBands.size();++i){
            const float before=inBands[i]*area.getHeight(),after=outBands[i]*area.getHeight();
            const float x=area.getX()+dx*i;
            g.setColour(Theme::muted.withAlpha(.42f));g.fillRect(x,area.getBottom()-before,dx*.40f,before);
            g.setColour(Theme::green.withAlpha(.86f));g.fillRect(x+dx*.46f,area.getBottom()-after,dx*.40f,after);
        }
    }
    void paintSignalOrb(juce::Graphics& g,juce::Point<float> centre,float radius) const {
        const auto tint=Theme::green;
        g.setGradientFill(juce::ColourGradient(tint.withAlpha(.08f),centre.x,centre.y,Theme::bg.withAlpha(0.f),centre.x+radius,centre.y,true));
        g.fillEllipse(centre.x-radius,centre.y-radius,2.f*radius,2.f*radius);
        g.setColour(tint.withAlpha(.23f));g.drawEllipse(centre.x-radius,centre.y-radius,2.f*radius,2.f*radius,1.f);
        if(!fresh)return;
        // Previous actual samples form a short persistence trail.
        for(size_t i=0;i<previousMid.size();++i){
            const float x=centre.x+std::clamp(previousSide[i],-1.f,1.f)*radius;
            const float y=centre.y-std::clamp(previousMid[i],-1.f,1.f)*radius;
            const float e=std::min(1.f,std::hypot(previousMid[i],previousSide[i]));
            if(e>.00001f){g.setColour(tint.withAlpha(.08f+.12f*e));g.fillEllipse(x-2.f,y-2.f,4.f,4.f);}
        }
        for(size_t i=0;i<mid.size();++i){
            const float x=centre.x+std::clamp(side[i],-1.f,1.f)*radius;
            const float y=centre.y-std::clamp(mid[i],-1.f,1.f)*radius;
            const float energy=std::min(1.f,std::hypot(mid[i],side[i]));
            if(energy<.00001f)continue;
            g.setColour(tint.withAlpha(.12f+.26f*energy));g.fillEllipse(x-3.f,y-3.f,6.f,6.f);
            g.setColour(Theme::white.withAlpha(.38f+.55f*energy));g.fillEllipse(x-1.f,y-1.f,2.f,2.f);
        }
    }
    bool testLayoutForTest() const noexcept {return getWidth()>=600 && getHeight()>=190;}
    void paint(juce::Graphics& g) override {
        const auto bounds=getLocalBounds().toFloat();
        Theme::panelBox(g,bounds,8.f,true);
        if(bounds.getWidth()<300 || bounds.getHeight()<140)return;
        const bool stereo=isStereo(lastType),drive=isDrive(lastType);
        const bool pitch=lastType==P::StudioPitchFormantFX;
        const bool transferView=lastType==P::Saturator || lastType==P::StudioFastDistFX;
        const bool taps=lastType==P::Delay || lastType==P::StudioDelayFX;
        const bool room=lastType==P::VoxRackRoom;
        const bool deess=lastType==P::DeEsser,gate=lastType==P::Gate;
        const bool dynamics=isDynamics(lastType) && !deess && !gate;
        const bool compCurve=lastType==P::Compressor;
        const bool delay=lastType==P::Delay || lastType==P::StudioDelayFX ||
            lastType==P::DuckingPingPongFX || lastType==P::AdlibThrow;
        const bool space=P::supportsWetSidechain(lastType) && !stereo;
        const auto accent=dynamics || gate || deess?Theme::amber:space || stereo?Theme::blue:Theme::green;
        auto inner=bounds.reduced(14.f,9.f);
        auto top=inner.removeFromTop(24.f);
        const auto heading=pitch?"PITCH / DETECTED NOTES":transferView?"DRIVE / CORE TRANSFER":taps?"DELAY / LIVE WET TAPS":room?"ROOM / MEASURED WET TAIL":stereo?"STEREO / VECTOR FIELD":drive?"DRIVE / WAVE SHAPE":deess?"DE-ESS / SIBILANCE":gate?"GATE / LEVEL + THRESHOLD":dynamics?"DYNAMICS / REDUCTION":delay?"DELAY / LEFT + RIGHT HISTORY":space?"SPACE / ENERGY HISTORY":"SPECTRUM / BEFORE + AFTER";
        Theme::fittedText(g,heading,
            top.withTrimmedRight(110.f).toNearestInt(),13.f,Theme::white,true);
        Theme::fittedText(g,fresh?"LIVE":"WAITING FOR AUDIO",top.removeFromRight(110.f).toNearestInt(),10.f,
            fresh?Theme::green:Theme::muted,true,juce::Justification::centredRight);
        auto footer=inner.removeFromBottom(23.f);
        auto right=inner.removeFromRight(146.f).reduced(4.f,0.f);
        auto graph=inner.reduced(1.f,5.f);
        g.setGradientFill(juce::ColourGradient(Theme::bg.darker(.35f),graph.getTopLeft(),
            Theme::bg.interpolatedWith(accent,.065f),graph.getBottomRight(),false));
        g.fillRoundedRectangle(graph,6.f);
        g.setColour(accent.withAlpha(.28f));g.drawRoundedRectangle(graph,6.f,1.f);
        for(int i=0;i<=4;++i){
            const float y=graph.getY()+graph.getHeight()*i/4.f;
            g.setColour(Theme::stroke.withAlpha(.42f));g.drawLine(graph.getX(),y,graph.getRight(),y,.7f);
        }
        for(int i=1;i<8;++i){
            const float x=graph.getX()+graph.getWidth()*i/8.f;
            g.drawLine(x,graph.getY(),x,graph.getBottom(),.65f);
        }
        auto plot=[&](const auto& values,juce::Colour colour,bool fill,bool inverted=false){
            juce::Path line;
            for(size_t i=0;i<values.size();++i){
                const float x=graph.getX()+graph.getWidth()*(float)i/(float)(values.size()-1);
                const float v=std::clamp(values[i],0.f,1.f);
                const float y=graph.getY()+graph.getHeight()*(inverted?v:1.f-v);
                if(i==0)line.startNewSubPath(x,y);else line.lineTo(x,y);
            }
            if(fill){
                auto area=line;
                const float baseline=inverted?graph.getY():graph.getBottom();
                area.lineTo(graph.getRight(),baseline);area.lineTo(graph.getX(),baseline);area.closeSubPath();
                g.setGradientFill(juce::ColourGradient(colour.withAlpha(.21f),graph.getCentreX(),graph.getY(),
                    colour.withAlpha(.035f),graph.getCentreX(),graph.getBottom(),false));g.fillPath(area);
            }
            g.setColour(colour.withAlpha(.13f));g.strokePath(line,juce::PathStrokeType(4.f));
            g.setColour(colour.withAlpha(.92f));g.strokePath(line,juce::PathStrokeType(1.6f));
        };
        {
            juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(graph.toNearestInt());
            if(pitch){
                auto noteTrace=[&](const auto& history,juce::Colour tint){
                    juce::Path path;bool started=false;
                    for(size_t n=0;n<history.size();++n){
                        if(history[n]<0.f){started=false;continue;}
                        const float x=graph.getX()+graph.getWidth()*(float)n/(history.size()-1);
                        const float y=graph.getBottom()-std::clamp((history[n]-36.f)/48.f,0.f,1.f)*graph.getHeight();
                        if(!started){path.startNewSubPath(x,y);started=true;}else path.lineTo(x,y);
                        g.setColour(tint.withAlpha(.75f));g.fillEllipse(x-1.2f,y-1.2f,2.4f,2.4f);
                    }
                    g.setColour(tint);g.strokePath(path,juce::PathStrokeType(1.5f));
                };
                noteTrace(pitchInHistory,Theme::muted);noteTrace(pitchOutHistory,accent);
            }else if(transferView){
                const auto area=graph.reduced(10.f,6.f);
                float fullScale=1.f;
                for(float value:transfer)if(std::isfinite(value))fullScale=std::max(fullScale,std::abs(value));
                fullScale=std::ceil(fullScale);
                const float unity=area.getHeight()*.5f/fullScale;
                g.setColour(Theme::muted.withAlpha(.5f));g.drawLine(area.getX(),area.getCentreY()+unity,area.getRight(),area.getCentreY()-unity,.9f);
                Theme::fittedText(g,"Y: +/-"+juce::String(fullScale,0)+" FS",area.withHeight(14.f).toNearestInt(),9.f,Theme::muted,false,juce::Justification::centredRight);
                juce::Path path;
                for(size_t n=0;n<transfer.size();++n){
                    const float x=area.getX()+area.getWidth()*(float)n/(transfer.size()-1);
                    const float y=area.getCentreY()-std::clamp(transfer[n]/fullScale,-1.f,1.f)*area.getHeight()*.5f;
                    if(n==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
                }
                g.setColour(accent.withAlpha(.18f));g.strokePath(path,juce::PathStrokeType(5.f));
                g.setColour(accent);g.strokePath(path,juce::PathStrokeType(1.8f));
            }else if(taps){
                auto history=graph.withTrimmedTop(47.f);
                for(int ch=0;ch<2;++ch){
                    const float y=graph.getY()+12.f+ch*20.f;
                    const float x=graph.getX()+16.f+std::clamp(tapMs[(size_t)ch]/6200.f,0.f,1.f)*(graph.getWidth()-32.f);
                    const auto tint=ch==0?Theme::blue:Theme::green;
                    g.setColour(tint.withAlpha(.24f));g.drawLine(graph.getX()+12.f,y,graph.getRight()-12.f,y,1.f);
                    if(wetAvailable){
                        const float radius=3.f+7.f*level(wetRms[(size_t)ch]);
                        g.setColour(tint.withAlpha(.25f));g.fillEllipse(x-radius,y-radius,2.f*radius,2.f*radius);
                        g.setColour(tint);g.fillEllipse(x-2.f,y-2.f,4.f,4.f);
                    }
                    Theme::fittedText(g,juce::String(ch==0?"L ":"R ")+juce::String(tapMs[(size_t)ch],1)+" ms",
                        juce::Rectangle<int>((int)graph.getRight()-90,(int)y-8,80,16),9.f,tint,true);
                }
                // Live return history, independent of the direct signal.
                auto small=[&](const auto& values,juce::Colour tint){
                    juce::Path path;
                    for(size_t n=0;n<values.size();++n){const float x=history.getX()+history.getWidth()*(float)n/(values.size()-1);
                        const float y=history.getBottom()-values[n]*history.getHeight();
                        if(n==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
                    g.setColour(tint);g.strokePath(path,juce::PathStrokeType(1.5f));
                };
                small(leftHistory,Theme::blue);small(rightHistory,Theme::green);
            }else if(room){
                plot(inputHistory,Theme::muted,false);plot(wetHistory,accent,true);
            }else if(stereo){
                const float radius=graph.getHeight()*.44f;
                const auto centre=graph.getCentre();
                g.setColour(Theme::bg);g.fillEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius);
                g.setColour(accent.withAlpha(.35f));g.drawEllipse(centre.x-radius,centre.y-radius,2*radius,2*radius,1.f);
                g.drawLine(centre.x-radius,centre.y,centre.x+radius,centre.y,.8f);
                g.drawLine(centre.x,centre.y-radius,centre.x,centre.y+radius,.8f);
                // Actual M/S points from the output block, not a simulated stereo image.
                for(size_t i=0;i<mid.size();++i){
                    const float x=centre.x+std::clamp(side[i],-1.f,1.f)*radius;
                    const float y=centre.y-std::clamp(mid[i],-1.f,1.f)*radius;
                    if(fresh){g.setColour(accent.withAlpha(.16f));g.fillEllipse(x-2.f,y-2.f,4.f,4.f);
                        g.setColour(accent.withAlpha(.82f));g.fillEllipse(x-.8f,y-.8f,1.6f,1.6f);}
                }
                Theme::text(g,"MID",juce::Rectangle<int>((int)centre.x+8,(int)graph.getY()+4,40,15),9.f,Theme::muted);
                Theme::text(g,"SIDE",juce::Rectangle<int>((int)centre.x+(int)radius+7,(int)centre.y-8,45,16),9.f,Theme::muted);
                const auto meter=juce::Rectangle<float>(graph.getX()+12.f,graph.getBottom()-22.f,graph.getWidth()*.23f,6.f);
                g.setColour(Theme::stroke);g.fillRoundedRectangle(meter,3.f);
                const float marker=meter.getX()+meter.getWidth()*(correlation+1.f)*.5f;
                if(fresh && outputRms>1.e-6f){g.setColour(accent);g.fillEllipse(marker-3.f,meter.getCentreY()-3.f,6.f,6.f);}
                Theme::text(g,"-1     CORRELATION     +1",meter.expanded(0.f,10.f).translated(0.f,-10.f).toNearestInt(),9.f,Theme::muted);
            }else if(drive){
                auto drawWave=[&](const auto& samples,juce::Colour colour){
                    juce::Path path;
                    for(size_t n=0;n<samples.size();++n){
                        const float x=graph.getX()+graph.getWidth()*(float)n/(samples.size()-1);
                        const float y=graph.getCentreY()-std::clamp(samples[n],-1.f,1.f)*graph.getHeight()*.46f;
                        if(n==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
                    }
                    g.setColour(colour.withAlpha(.15f));g.strokePath(path,juce::PathStrokeType(4.f));
                    g.setColour(colour);g.strokePath(path,juce::PathStrokeType(1.5f));
                };
                drawWave(inputWave,Theme::muted);drawWave(wave,accent);
            }else if(gate){
                plot(inputHistory,Theme::muted,false);plot(outputHistory,accent,true);
                const float y=graph.getBottom()-skellcore::visual::dbToNorm(gateDb,-72.f,0.f)*graph.getHeight();
                const float closeY=graph.getBottom()-skellcore::visual::dbToNorm(gateCloseDb,-72.f,0.f)*graph.getHeight();
                g.setColour(accent.withAlpha(.13f));g.fillRect(graph.getX(),y,graph.getWidth(),std::max(0.f,closeY-y));
                g.setColour(accent.withAlpha(.8f));g.drawLine(graph.getX(),closeY,graph.getRight(),closeY,.8f);
                g.setColour(Theme::white.withAlpha(.8f));g.drawLine(graph.getX(),y,graph.getRight(),y,1.f);
                Theme::fittedText(g,"OPEN "+juce::String(gateDb,1)+" / CLOSE "+juce::String(gateCloseDb,1)+" dB",juce::Rectangle<int>((int)graph.getX()+4,(int)std::clamp(y-16.f,graph.getY(),graph.getBottom()-16.f),260,16),10.f,Theme::white,true);
            }else if(compCurve){
                const auto original=graph;
                graph=original.withTrimmedRight(original.getWidth()*.46f+8.f);
                plot(grHistory,accent,true,true);
                const auto curveArea=original.withTrimmedLeft(original.getWidth()*.54f+8.f).reduced(6.f,4.f);
                const auto area=curveArea.withTrimmedTop(16.f);
                g.setColour(Theme::muted.withAlpha(.45f));g.drawLine(area.getX(),area.getBottom(),area.getRight(),area.getY(),.8f);
                juce::Path curve;
                for(size_t n=0;n<compressorCurve.size();++n){
                    const float x=area.getX()+area.getWidth()*(float)n/(compressorCurve.size()-1);
                    const float y=area.getBottom()-skellcore::visual::dbToNorm(compressorCurve[n],-60.f,0.f)*area.getHeight();
                    if(n==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);
                }
                g.setColour(accent);g.strokePath(curve,juce::PathStrokeType(1.8f));
                Theme::fittedText(g,"STATIC HARD-KNEE LAW",curveArea.withHeight(15.f).toNearestInt(),9.f,Theme::muted,true);
                graph=original;
            }else if(dynamics){plot(grHistory,accent,true,true);plot(duckHistory,Theme::blue,false,true);}
            else if(delay){
                plot(leftHistory,accent,true);plot(rightHistory,Theme::green,false);
            }else if(space){plot(inputHistory,Theme::muted,false);plot(outputHistory,accent,true);}
            else{
                if(deess){
                    const float x=graph.getX()+graph.getWidth()*std::clamp(std::log(deessHz/80.f)/std::log(9000.f/80.f),0.f,1.f);
                    g.setColour(accent.withAlpha(.10f));g.fillRect(graph.withLeft(x));
                    if(deessHz<=9000.f){g.setColour(accent.withAlpha(.8f));g.drawLine(x,graph.getY(),x,graph.getBottom(),1.f);}
                }
                plot(inBands,Theme::muted,false);plot(outBands,accent,true);
            }
        }
        // Explicit units distinguish sampled meter histories from frequency plots.
        if(dynamics){
            Theme::text(g,"0",graph.getSmallestIntegerContainer().withHeight(13).translated(3,0),9,Theme::muted);
            Theme::text(g,"-24 dB",graph.getSmallestIntegerContainer().withTop((int)graph.getBottom()-14).translated(3,0),9,Theme::muted);
        }else if(!space && !stereo && !drive && !gate && !pitch){
            Theme::text(g,"80 Hz",footer.removeFromLeft(68.f).toNearestInt(),9,Theme::muted);
            Theme::fittedText(g,"9 kHz",footer.removeFromRight(68.f).toNearestInt(),9,Theme::muted,false,juce::Justification::centredRight);
        }
        auto metric=[&](const juce::String& label,const juce::String& value,juce::Colour colour){
            auto row=right.removeFromTop(30.f);
            Theme::text(g,label,row.removeFromTop(12.f).toNearestInt(),9.f,Theme::muted,true);
            Theme::text(g,value,row.toNearestInt(),13.f,colour,true);
        };
        if(pitch){
            metric("INPUT NOTE",note(pitchIn),Theme::white);
            metric("OUTPUT NOTE",note(pitchOut),accent);
        }else{
            metric("INPUT RMS",db(inputRms),Theme::white);
            metric("OUTPUT RMS",db(outputRms),accent);
        }
        if(pitch)metric("OUTPUT CONF.",pitchOut.hz>0.f?juce::String(juce::roundToInt(pitchOut.confidence*100.f))+"%":juce::String("--"),accent);
        else if(room)metric("TAIL DROP",tail.active?juce::String(tail.dropDb,1)+" dB":juce::String("--"),accent);
        else if(gate)metric("MEAN GATE GAIN",sharedValid && gateMean>=0.f?juce::String(gateMean*100.f,1)+"%":juce::String("--"),accent);
        else if(compCurve)metric("CORE REDUCTION",sharedValid && grValid?juce::String(gr,1)+" dB":juce::String("--"),accent);
        else metric(stereo?"CORRELATION":deess || dynamics?"REDUCTION":space?"DUCK":"SIDE RMS",
               stereo?(fresh && outputRms>1.e-6f?juce::String(correlation,2):juce::String("--")):
               deess || gate || dynamics?juce::String(gr,1)+" dB":space?juce::String(duck,1)+" dB":db(sideRms),accent);
        // The small footer trace is actual full-rate output, not a generated wave.
        if(right.getHeight()>12.f){
            juce::Path trace;const auto a=right.reduced(1.f,2.f);
            for(size_t i=0;i<wave.size();++i){
                const float x=a.getX()+a.getWidth()*(float)i/(wave.size()-1);
                const float y=a.getCentreY()-std::clamp(wave[i],-1.f,1.f)*a.getHeight()*.48f;
                if(i==0)trace.startNewSubPath(x,y);else trace.lineTo(x,y);
            }
            g.setColour(accent.withAlpha(.8f));g.strokePath(trace,juce::PathStrokeType(1.2f));
        }
        const juce::String legend=pitch?juce::String("DETECTED MONOPHONIC INPUT / OUTPUT / C2-C6"):
            transferView?juce::String("STATIC CORE RESPONSE / INPUT +/-1 FS / BEFORE RACK MIX"):
            taps?juce::String("BASE TAP TIME / MEASURED WET ENERGY / NOT DRY SIGNAL"):
            room?(tail.active?"OBSERVED TAIL "+juce::String(tail.elapsed,2)+" s / RELATIVE DROP":juce::String("WET ENERGY / WAITING FOR INPUT TO FALL SILENT")):
            stereo?juce::String("ACTUAL OUTPUT / MID-SIDE"):
            drive?juce::String("INPUT / OUTPUT WAVEFORM / 128 SAMPLES"):
            deess?"DE-ESS CENTER "+juce::String(deessHz/1000.f,2)+" kHz / BEFORE + AFTER":
            gate?juce::String("RMS HISTORY / DETECTOR OPEN-CLOSE THRESHOLDS"):
            compCurve?juce::String("20 ms GR BUCKETS / STATIC DETECTOR ENVELOPE LAW"):
            dynamics?juce::String("GAIN REDUCTION / DUCKING / 24 dB RANGE"):
            delay?juce::String("LEFT (BLUE) / RIGHT (GREEN) / OUTPUT ENERGY"):
            space?juce::String("INPUT / OUTPUT / -72 TO 0 dBFS"):juce::String("INPUT / OUTPUT / -72 TO 0 dBFS");
        Theme::fittedText(g,legend,
            footer.toNearestInt(),10.f,Theme::muted,false,juce::Justification::centred);
    }
private:
    static juce::String note(skellpreset::visual::PitchReading p){
        if(p.hz<=0.f)return "--";
        constexpr const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        const int n=juce::roundToInt(p.midi);
        return juce::String(names[n%12])+juce::String(n/12-1)+"  "+juce::String(p.hz,0)+" Hz";
    }
    static bool isStereo(int t) noexcept {
        return t==P::AdlibDoubler || t==P::StereoWidth || t==P::VocalChorusFX || t==P::VocalFlangerFX || t==P::VocalVibratoFX || t==P::AdlibMotion;
    }
    static bool isDrive(int t) noexcept {
        return t==P::Saturator || t==P::StudioFastDistFX || t==P::AdlibSaturate || t==P::RetroCrush || t==P::Exciter;
    }
    static bool isDynamics(int t) noexcept {
        return t==P::Compressor || t==P::Limiter || t==P::Gate || t==P::DeEsser ||
            t==P::ThreeBandDynamics || t==P::VoxRackSmoothComp || t==P::VocalRiderFX ||
            t==P::SerialDynamicsFX || t==P::DynamicHarshnessFX || t==P::AdaptiveNoiseFX ||
            t==P::StudioLimiterFX || t==P::StudioCompressorFX || t==P::VocalFeelOptoFX;
    }
    static float level(float x) noexcept {
        return std::clamp((20.f*std::log10(std::max(0.00025119f,x))+72.f)/72.f,0.f,1.f);
    }
    static juce::String db(float x) {return x<.00025119f?juce::String("-INF"):juce::String(20.f*std::log10(x),1)+" dBFS";}
    template<size_t N>static void shift(std::array<float,N>& values,float x) noexcept {
        std::move(values.begin()+1,values.end(),values.begin());values.back()=x;
    }
    void reset() noexcept {
        inBands.fill(0);outBands.fill(0);deltaBands.fill(0);spectrumElapsed=spectrumClock=0.f;inputHistory.fill(0);outputHistory.fill(0);
        grHistory.fill(0);duckHistory.fill(0);wave.fill(0);inputWave.fill(0);driveHead=driveCount=compressorHistoryCount=0;
        leftHistory.fill(0);rightHistory.fill(0);mid.fill(0);side.fill(0);previousMid.fill(0);previousSide.fill(0);
        correlation=leftRms=rightRms=0.f;
        pitchIn={};pitchOut={};pitchInHistory.fill(-1.f);pitchOutHistory.fill(-1.f);
        sharedValid=false;grValid=false;gateMean=-1.f;
        wetHistory.fill(0.f);wetAvailable=false;wetRms={};tapMs={};tail={};
        historyHead=historyCount=0;isolatedHistory=false;for(auto& row:spectrumHistory)row.fill(0.f);
        inputRms=outputRms=sideRms=gr=duck=0.f;lastSerial=0;lastFrame=0;fresh=false;ticks=0;
    }
    Analyzer analyzer;
    skellpreset::visual::PitchReading pitchIn{},pitchOut{};
    skellpreset::visual::TailReading tail;
    skellpreset::studio::FastDist curveDist;
    std::array<float,129> transfer{},compressorCurve{};
    std::array<float,historySize> pitchInHistory{},pitchOutHistory{},wetHistory{};
    std::array<float,2> wetRms{},tapMs{};
    bool wetAvailable=false,sharedValid=false,grValid=false;
    float gateMean=-1.f;
    std::array<float,Analyzer::bands> inBands{},outBands{},deltaBands{};
    float spectrumElapsed=0.f,spectrumClock=0.f;
    std::array<std::array<float,Analyzer::bands>,32> spectrumHistory{};
    size_t historyHead=0,historyCount=0;
    bool isolatedHistory=false;
    std::array<float,historySize> inputHistory{},outputHistory{},grHistory{},duckHistory{};
    struct DriveTrace{std::array<float,128> input{},output{};};
    std::array<DriveTrace,5> driveTraces{};
    size_t driveHead=0,driveCount=0,compressorHistoryCount=0;
    std::array<float,128> wave{},inputWave{},mid{},side{},previousMid{},previousSide{};
    std::array<float,historySize> leftHistory{},rightHistory{};
    float correlation=0.f,leftRms=0.f,rightRms=0.f,deessHz=7500.f,gateDb=-40.f,gateCloseDb=-44.f;
    int lastSlot=-1,lastType=-1;
    std::uint64_t lastSerial=0;
    unsigned lastRevision=0;
    unsigned lastFrame=0,ticks=0;
    bool fresh=false;
    float inputRms=0,outputRms=0,sideRms=0,gr=0,duck=0;
};
