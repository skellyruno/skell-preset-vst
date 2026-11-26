#pragma once
#include <JuceHeader.h>

namespace skellpreset::face {
inline bool eq(int type) noexcept {return type==1 || type==26 || type==34;}
inline bool limiter(int type) noexcept {return type==3 || type==35;}
// Classic Saturator exposes exactly two DSP parameters (Drive, Character).
inline bool saturator(int type) noexcept {return type==5;}
inline bool hardware(int type) noexcept {return !eq(type) && type!=0 && type!=12 && type!=32;}
inline bool dynamics(int type) noexcept {
    return type==2 || type==19 || type==27 || type==36 || type==43;
}
inline bool space(int type) noexcept {return type==6 || type==7 || type==9 || type==18 || type==24 || type==38;}
inline juce::Rectangle<int> control(int type,int index) noexcept {
    if(limiter(type))return {42+index*222,203,202,246};
    // Saturator: two large dials flanking a real transfer-curve screen.
    if(saturator(type))return index==0?juce::Rectangle<int>{36,196,204,250}
                                      :juce::Rectangle<int>{666,196,204,250};
    if(type==4 || type==29) {
        if(index==0)return {45,177,260,282};
        return {328+(index-1)*181,302,170,181};
    }
    if(dynamics(type)) {
        // Keep dial centers fixed while reducing oversized primary caps.
        // Shared geometry is used by the real controls and their captions.
        if(index<2)return {62+index*244,201,200,244};
        return {556+(index-2)*171,306,147,168};
    }
    if(type==11 || type==28) {
        if(index==0)return {48,181,257,278};
        return {329+(index-1)*179,299,164,184};
    }
    if(space(type)) {
        if(index==0)return {49,179,250,281};
        return {329+(index-1)*179,301,164,182};
    }
    if(type==30)return index<2?juce::Rectangle<int>{81+index*365,166,270,246}:
        juce::Rectangle<int>{81+(index-2)*365,439,270,58};
    if(type==17)return {37+index*216,225,190,237};
    if(type==44)return {38+index*216,178,188,227};
    if(type==31)return {38+index*216,208,188,256};
    if(type==40 || type==42) {
        const int position=type==40?(index==3?2:index):(index==2?1:index==3?2:index);
        return {85+position*258,183,224,274};
    }
    // Motion and character processors: a central feedback instrument sits
    // between two pairs of large dials, rather than above a bottom control row.
    return {36+(index%2)*654,164+(index/2)*177,180,172};
}
inline bool visible(int type,int index) noexcept {
    return type!=0 && type!=12 && !(limiter(type) && index==3) &&
        // Classic faces retain their historical parameters for preset and host
        // compatibility, but do not expose controls that the DSP does not read.
        !(type==5 && index>1) && !(type==6 && index>1) &&
        !(type==7 && index>2) && !(type==8 && index>0) &&
        !(type==40 && index==2) && !(type==42 && index==1);
}
}
