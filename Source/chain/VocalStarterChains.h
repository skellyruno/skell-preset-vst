#pragma once
#include "PresetPackage.h"

namespace skellpreset::starter {
// Original Skell starter profiles. Not artist matches or factory programs.
// Template parameter maps to the existing, stable processor ModuleType IDs.
inline constexpr const char* names[]{"Warm Upfront","Atmospheric Trap","Airy Melodic","Clean Studio","Punchy Rap"};
inline constexpr int count=5;
template<class P>
package::Document make(int profile) {
    const int k=juce::jlimit(0,count-1,profile);
    package::Document doc;
    doc.name=names[k];doc.bank="Vocal Starter Chains";doc.mode=1;
    doc.micGateOn=true;doc.micGateThreshold=-52.f;doc.micGateDepth=.85f;
    doc.micPreOn=true;doc.micPreInput=0;doc.micPreTarget=k==0?1:k==2?3:k==4?2:0;
    doc.micPreAmount=k==2?.55f:.35f;
    auto slot=[&](int i,int type,float wet,std::initializer_list<float> values)->package::Slot&{
        auto& s=doc.slots[(size_t)i];s.type=type;s.enabled=true;s.wet=wet;
        size_t n=0;for(float v:values)if(n<s.params.size())s.params[n++]=v;
        return s;
    };
    // A gentle native cleanup stage, not neural noise removal.
    slot(0,P::AdaptiveNoiseFX,.35f,{.43f,.40f,.11f,.60f});
    // 300 Hz broad cut plus high-pass at approximately 80 Hz.
    slot(1,P::EQ,1.f,{.353f,.40f,.13f,.24f});
    // Frequency, threshold, ratio and release use the legacy native map.
    slot(2,P::DeEsser,1.f,{.40f,.69f,.20f,.32f});
    const float threshold[]={.59f,.48f,.57f,.57f,.46f};
    const float ratio[]={.22f,.31f,.24f,.22f,.36f};
    slot(3,P::Compressor,1.f,{threshold[k],ratio[k],k==4?.04f:.12f,.20f,.50f});
    const float warmth[]={.20f,.35f,.18f,.12f,.25f};
    slot(4,P::Saturator,k==1?.19f:.12f,{warmth[k],.50f,.50f,.50f});
    slot(5,P::Exciter,k==2?.17f:.08f,{.20f,.44f,.50f,.50f});
    const float room[]={.12f,.25f,.19f,.10f,.075f};
    auto& reverb=slot(6,P::VoxRackRoom,room[k],{k==1?.76f:.32f,.42f,.08f,k==1?.65f:.35f});
    reverb.parallelReturn=true;reverb.duckOn=true;reverb.duckDepth=.55f;
    const float delay[]={.10f,.23f,.15f,.075f,.11f};
    auto& echo=slot(7,P::StudioDelayFX,1.f,{.3f,k==1?.44f:.25f,k==1?.62f:.35f,.12f,.12f,.55f,0.f,0.f,.1f,0.f,delay[k],1.f,1.f,k==1?.32f:.44f,.26f,0.f});
    // Studio Delay is dry + wet internally. Keep serial and duck only its echo.
    echo.duckOn=true;echo.duckDepth=.65f;echo.duckRelease=260.f;
    // No pitch shifter or lookahead limiter in these tracking-oriented chains.
    doc.outputDb=-3.f;
    return doc;
}
}
