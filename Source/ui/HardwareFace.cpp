#include "SlotEditor.h"
#include "HardwareLayout.h"
#include "../chain/EffectRegistry.h"

namespace {
void meter(juce::Graphics& g,juce::Rectangle<float> r,float amount,
           const juce::String& name,const juce::String& value,bool vertical=false) {
    Theme::panelBox(g,r,6.f);
    Theme::text(g,name,(r.getHeight()<60.f?r.reduced(12.f,6.f).withWidth(r.getWidth()*.48f):r.reduced(12.f,8.f)).withHeight(19.f).toNearestInt(),r.getHeight()<60.f?9.f:10.5f,Theme::muted,true);
    auto rail=r.reduced(15.f,36.f);
    if(vertical)rail=rail.withWidth(20.f).withCentre({r.getCentreX(),r.getCentreY()});
    else if(r.getHeight()<60.f)rail={r.getX()+15.f,r.getBottom()-12.f,r.getWidth()-30.f,6.f};
    else rail=rail.withHeight(15.f).withY(r.getCentreY()-3.f);
    constexpr int segments=26;
    const float level=juce::jlimit(0.f,1.f,amount);
    for(int i=0;i<segments;++i) {
        const float f=(float)i/(float)segments;
        const auto lit=i>21?Theme::amber:Theme::green;
        g.setColour(f<level?lit.withAlpha(.85f):Theme::stroke.withAlpha(.37f));
        if(vertical) {
            const float h=rail.getHeight()/segments;
            g.fillRoundedRectangle(rail.getX(),rail.getBottom()-(i+1)*h,rail.getWidth(),h-2.f,1.f);
        } else {
            const float w=rail.getWidth()/segments;
            g.fillRoundedRectangle(rail.getX()+i*w,rail.getY(),w-2.f,rail.getHeight(),1.f);
        }
    }
    Theme::fittedText(g,value,{(int)(r.getHeight()<60.f?r.getCentreX():r.getX()+8.f),(int)(r.getHeight()<60.f?r.getY()+3.f:r.getBottom()-28.f),(int)(r.getHeight()<60.f?r.getWidth()*.5f-8.f:r.getWidth()-16.f),22},
                      r.getHeight()<60.f?12.f:15.f,Theme::white,true,juce::Justification::centred);
}
}

void SlotEditor::paintHardware(juce::Graphics& g,int type) {
    using namespace skellpreset::face;
    const auto panel=juce::Rectangle<float>(19.f,142.f,868.f,378.f);
    g.setGradientFill(juce::ColourGradient(Theme::panelAlt.brighter(.08f),panel.getTopLeft(),
                                          Theme::bg,panel.getBottomRight(),false));
    g.fillRoundedRectangle(panel,9.f);
    g.setColour(Theme::white.withAlpha(.017f));
    for(int y=148;y<518;y+=4)g.drawHorizontalLine(y,25.f,880.f);
    g.setColour(Theme::stroke.withAlpha(.5f));g.drawRoundedRectangle(panel,9.f,1.f);
    for(float x:{28.f,878.f})for(float y:{151.f,510.f}) {
        g.setColour(Theme::bg);g.fillEllipse(x-3.f,y-3.f,6.f,6.f);
        g.setColour(Theme::muted.withAlpha(.4f));g.drawLine(x-1.7f,y+1.f,x+1.7f,y-1.f,.8f);
    }
    auto labels=titlesForHardware(type);
    for(int i=0;i<4;++i) {
        if(!visible(type,i))continue;
        const auto r=control(type,i);
        Theme::fittedText(g,labels[(size_t)i].toUpperCase(),
            {r.getX()-3,r.getY()-25,r.getWidth()+6,20},12.f,Theme::white,true,juce::Justification::centred);
    }
    const float peak=processor.getSlotPeak(slot);
    const float db=skellpreset::dsp::gainToDb(peak);
    const float level=juce::jlimit(0.f,1.f,(db+60.f)/60.f);
    const float reduction=processor.getSlotGainReductionDb(slot);
    const auto output=peak>0.00001f?juce::String(db,1)+" dBFS":juce::String("-inf dBFS");
    const auto gr=juce::String(reduction,1)+" dB";
    if(limiter(type)) {
        meter(g,{754.f,177.f,106.f,310.f},level,"OUTPUT",output,true);
        meter(g,{61.f,471.f,639.f,43.f},reduction/24.f,"REDUCTION",gr);
    } else if(dynamics(type)) {
        // Wide reduction meter above the two timing controls. Atomic telemetry
        // comes from this slot's actual envelope, never a transfer-curve proxy.
        const bool measured=type==2 || type==36 || type==43;
        if(type==2 || type==36){
            livePanel.paintCompressorTransfer(g,{42.f,158.f,270.f,161.f},processor,slot,type);
            livePanel.paintHistory(g,{326.f,158.f,534.f,161.f},true);
        }else livePanel.paintHistory(g,{548.f,169.f,306.f,88.f},measured);
        if(type==36)Theme::text(g,"SOFT KNEE",{430,107,116,20},11.5f,Theme::white,true);
    } else if(type==4 || type==29) {
        livePanel.paintHistory(g,{342.f,170.f,496.f,83.f},false);
        const bool open=peak>.0001f;
        g.setColour(open?Theme::green:Theme::stroke);
        g.fillEllipse(330.f,485.f,6.f,6.f);
        Theme::text(g,open?"PASSING AUDIO":"BELOW FLOOR",{346,477,470,22},11.f,Theme::muted,true);
    } else if(type==11 || type==28) {
        const float hz=type==11?3500.f+8000.f*processor.getSlotParam(slot,0):
            1800.f+6200.f*processor.getSlotParam(slot,0);
        Theme::text(g,"SIBILANCE FOCUS",{346,162,360,19},11.f,Theme::muted,true);
        Theme::text(g,juce::String(hz/1000.f,2)+" kHz",{343,183,390,45},31.f,Theme::green,true);
        meter(g,{624.f,171.f,214.f,81.f},type==11?reduction/24.f:level,
              type==11?"REDUCTION":"OUTPUT",type==11?gr:output);
    } else if(space(type)) {
        if(type==6)livePanel.paintSpace(g,{42.f,158.f,818.f,161.f});
        else {livePanel.paintSpace(g,{344.f,166.f,493.f,109.f});
            meter(g,{66.f,478.f,215.f,37.f},level,"RETURN",output);}
    } else if(type==30) {
        Theme::text(g,"PITCH",{111,140,245,20},11.f,Theme::muted,true);
        Theme::text(g,"FORMANT",{476,140,245,20},11.f,Theme::muted,true);
        g.setColour(Theme::stroke);g.drawVerticalLine(440,176.f,495.f);
    } else if(type==17) {
        for(int band=0;band<3;++band) {
            const int x=45+band*216;
            Theme::text(g,juce::StringArray{"LOW","MID","HIGH"}[band],{x,166,180,28},18.f,Theme::green,true,
                        juce::Justification::centred);
        }
        Theme::text(g,"TIMING",{693,166,170,28},18.f,Theme::muted,true,juce::Justification::centred);
        meter(g,{64.f,476.f,771.f,39.f},level,"OUTPUT",output);
    } else if(type==44) {
        std::array<float,skellpreset::visual::LiveSpectrumCapture::capacity> input{},spectrumOutput{};
        processor.getLiveSpectrumInput(input);processor.getLiveSpectrumOutput(spectrumOutput);
        const auto inLevels=suiteSpectrumAnalyzer.analyze(input,processor.preparedAudioRate());
        const auto outLevels=suiteSpectrumAnalyzer.analyze(spectrumOutput,processor.preparedAudioRate());
        Theme::text(g,"RACK INPUT / OUTPUT",{42,410,180,14},9.f,Theme::muted,true);
        for(int band=0;band<skellpreset::visual::LiveSpectrumAnalyzer::bands;++band) {
            const float x=239.f+band*17.f;
            const float before=juce::jlimit(0.f,1.f,(skellpreset::dsp::gainToDb(inLevels[(size_t)band])+72.f)/72.f);
            const float after=juce::jlimit(0.f,1.f,(skellpreset::dsp::gainToDb(outLevels[(size_t)band])+72.f)/72.f);
            g.setColour(Theme::muted.withAlpha(.55f));g.fillRect(x,447.f-before*32.f,5.f,before*32.f);
            g.setColour(Theme::green.withAlpha(.6f));g.fillRect(x+6.f,447.f-after*32.f,5.f,after*32.f);
        }
        const char* returns[]{"MAGIC","SPACE","ATTACK","ATTITUDE","PRESENCE"};
        for(int i=0;i<5;++i) {
            const float rms=processor.getSuiteReturnRms(slot,i);
            const float fraction=juce::jlimit(0.f,1.f,(skellpreset::dsp::gainToDb(rms)+55.f)/55.f);
            meter(g,{43.f+165.f*i,460.f,156.f,49.f},fraction,returns[i],
                  rms>.00001f?juce::String(skellpreset::dsp::gainToDb(rms),1)+" dBFS":"-inf dBFS");
        }
    } else if(type==31 || type==40 || type==42) {
        meter(g,{65.f,477.f,773.f,38.f},level,"OUTPUT",output);
    } else if(saturator(type)) {
        // Dedicated saturation face. Drive and Character are the only two
        // parameters the DSP reads, so the centre of the panel is a truthful
        // tanh transfer curve computed from the same drive/blend the audio
        // path uses, not a decorative ring.
        const float driveParam=processor.getSlotParam(slot,0);
        const float character=processor.getSlotParam(slot,1);
        const float macroGrit=processor.state.getRawParameterValue("macro4")->load();
        const float drive=std::pow(10.f,(driveParam*24.f+10.f*(macroGrit-.5f))/20.f);
        const float blend=.5f+character;
        const float denom=std::max(.5f,drive*.6f);
        const auto screen=juce::Rectangle<float>(42.f,158.f,270.f,161.f);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xff070d0a),screen.getTopLeft(),
            juce::Colour(0xff020504),screen.getBottomRight(),false));
        g.fillRoundedRectangle(screen,7.f);
        g.setColour(Theme::stroke.withAlpha(.55f));
        g.drawRoundedRectangle(screen,7.f,1.f);
        // Graticule plus the 1:1 reference diagonal.
        const auto plot=juce::Rectangle<float>(54.f,183.f,246.f,124.f);
        g.setColour(Theme::stroke.withAlpha(.22f));
        for(int line=1;line<4;++line) {
            const float f=(float)line/4.f;
            g.drawVerticalLine((int)(plot.getX()+plot.getWidth()*f),plot.getY(),plot.getBottom());
            g.drawHorizontalLine((int)(plot.getY()+plot.getHeight()*f),plot.getX(),plot.getRight());
        }
        g.setColour(Theme::muted.withAlpha(.26f));
        g.drawLine(plot.getX(),plot.getBottom(),plot.getRight(),plot.getY(),.9f);
        // Drawn transfer function matching tanh(x*drive*blend)/denom. The
        // vertical axis is normalized to this setting's own full-scale output
        // so the plot fills the screen while the curvature and the amount of
        // compression remain exactly what the DSP applies.
        const float fullScale=std::tanh(1.1f*drive*blend)/denom;
        const float yScale=fullScale>.0001f?plot.getHeight()*.46f/fullScale:0.f;
        juce::Path curve;
        for(int i=0;i<=96;++i) {
            const float t=(float)i/96.f;
            const float inLevel=(t*2.f-1.f)*1.1f;
            const float outLevel=std::tanh(inLevel*drive*blend)/denom;
            const float x=plot.getX()+plot.getWidth()*t;
            const float y=juce::jlimit(plot.getY(),plot.getBottom(),
                                       plot.getCentreY()-outLevel*yScale);
            if(i==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);
        }
        g.setColour(juce::Colour(0xffc77996).withAlpha(.16f));
        g.strokePath(curve,juce::PathStrokeType(6.f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffc77996).withAlpha(.90f));
        g.strokePath(curve,juce::PathStrokeType(2.f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        // Live operating point: where the current slot output sits on the curve.
        if(livePanel.hasSignal()) {
            const float inLevel=juce::jlimit(0.f,1.1f,livePanel.measuredInputRms());
            const float outLevel=std::tanh(inLevel*drive*blend)/denom;
            const float px=plot.getCentreX()+plot.getWidth()*.5f*(inLevel/1.1f);
            const float py=juce::jlimit(plot.getY(),plot.getBottom(),
                                        plot.getCentreY()-outLevel*(yScale>0.f?yScale:1.f));
            g.setColour(Theme::white.withAlpha(.22f));
            g.fillEllipse(px-7.f,py-7.f,14.f,14.f);
            g.setColour(Theme::white);
            g.fillEllipse(px-2.4f,py-2.4f,4.8f,4.8f);
        }
        Theme::text(g,"TRANSFER / IN -> OUT",{54,163,246,16},10.f,Theme::muted,true);
        livePanel.paintDriveSpectrum(g,{326.f,158.f,534.f,161.f});
    } else {
        // Dedicated central hardware instrument, with a live level ring and
        // a purpose-specific description. No generic frequency/XY graph.
        const float cx=453.f,cy=307.f;
        g.setGradientFill(juce::ColourGradient(Theme::stroke.withAlpha(.30f),cx-90.f,cy-80.f,
                                              Theme::bg,cx+90.f,cy+100.f,false));
        g.fillEllipse(cx-115.f,cy-115.f,230.f,230.f);
        for(int i=0;i<40;++i) {
            const float angle=juce::MathConstants<float>::twoPi*i/40.f;
            g.setColour(i<(int)(level*40.f)?Theme::green.withAlpha(.72f):Theme::stroke);
            g.drawLine(cx+103.f*std::sin(angle),cy-103.f*std::cos(angle),
                       cx+109.f*std::sin(angle),cy-109.f*std::cos(angle),1.7f);
        }
        const juce::String caption=type==10?"DOUBLE":type==13?"RADIO":type==14?"CRUSH":
            type==15?"PULSE":type==16?"WIDTH":type==20?"ENSEMBLE":type==21?"FLANGE":
            type==22?"CARRIER":type==25?"VIBRATO":type==39?"CHOP":type==41?"FILTER":
            type==8?"AIR":type==33?"ENHANCE":"CHARACTER";
        livePanel.paintSignalOrb(g,{cx,cy-10.f},76.f);
        Theme::fittedText(g,caption,{337,192,232,25},15.f,Theme::white,true,juce::Justification::centred);
        Theme::fittedText(g,output,{352,382,202,26},13.f,Theme::green,true,juce::Justification::centred);
        Theme::text(g,"MEASURED M/S FIELD",{353,410,200,15},9.f,Theme::muted,false,juce::Justification::centred);
    }
}
