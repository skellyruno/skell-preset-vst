#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace skellpreset::readout {
// Presentation conversions mirror the actual set() calls in PluginProcessor.
// The underlying normalized values and all historical parameter IDs stay intact.
struct Mapping {
    double offset=0.,span=1.;
    const char* unit="";
    int decimals=1;
    bool logarithmic=false,valid=false;
    double physical(double normalized) const {
        return logarithmic?offset*std::pow(span,normalized):offset+span*normalized;
    }
    juce::String format(double normalized) const {
        const double value=physical(normalized);
        if(juce::String(unit)=="Hz" && value>=1000.)return juce::String(value/1000.,2)+" kHz";
        return juce::String(value,decimals)+" "+unit;
    }
    double parse(const juce::String& text) const {
        const double value=text.getDoubleValue()*(text.containsIgnoreCase("kHz")?1000.:1.);
        return juce::jlimit(0.,1.,logarithmic?std::log(juce::jmax(.000001,value)/offset)/std::log(span):
                                                    (value-offset)/span);
    }
};
inline Mapping linear(double start,double range,const char* unit,int decimals=1) {
    return {start,range,unit,decimals,false,true};
}
inline Mapping exponential(double start,double ratio,const char* unit,int decimals=1) {
    return {start,ratio,unit,decimals,true,true};
}
inline Mapping forParameter(int type,int index,double sampleRate) {
    switch(type) {
        case 6:
            if(index==0)return linear(.15,6.,"s",2);
            if(index==1)return linear(12.,80.,"%",0);
            break;
        case 7:
            if(index==0)return linear(55.,850.,"ms",1);
            if(index==1)return linear(0.,80.,"%",0);
            if(index==2)return linear(500.,17500.,"Hz",0);
            break;
        case 9:
            if(index==0)return linear(160.,480.,"ms",1);
            if(index==1)return linear(20.,68.,"%",0);
            if(index==2)return linear(500.,17500.,"Hz",0);
            break;
        case 13:
            if(index==0)return linear(100.,1400.,"Hz",0);
            if(index==1)return linear(1800.,10000.,"Hz",0);
            break;
        case 14:
            if(index==0)return linear(3.,13.,"bits",1);
            if(index==1)return exponential(100.,juce::jmax(8000.,sampleRate)/100.,"Hz",0);
            break;
        case 15:if(index==0)return linear(.15,14.,"Hz",2);break;
        case 16:
            if(index==0)return linear(0.,200.,"%",0);
            if(index==1)return linear(60.,340.,"Hz",0);
            if(index==2)return linear(-100.,200.,"%",0);
            break;
        case 18:
            if(index==2)return linear(0.,200.,"ms",1);
            if(index==3)return linear(20.,980.,"Hz",0);
            break;
        case 19:
            if(index==0)return linear(-55.,53.,"dBFS",1);
            if(index==1)return linear(1.,11.,":1",1);
            if(index==2)return linear(0.,18.,"dB",1);
            if(index==3)return linear(40.,410.,"ms",0);
            break;
        case 20:
            if(index==0)return linear(.08,3.9,"Hz",2);
            if(index==1)return linear(1.,10.,"ms",1);
            if(index==2)return linear(2.,3.,"voices",1);
            break;
        case 21:
            if(index==0)return linear(.05,4.95,"Hz",2);
            if(index==1)return linear(.3,9.,"ms",2);
            if(index==2)return linear(-82.,164.,"%",0);
            break;
        case 22:if(index==0)return exponential(15.,70.,"Hz",1);break;
    }
    return {};
}
}
