#pragma once
#include "PresetPackage.h"
#include "VocalPreviewAnalysis.h"

namespace skellpreset::starter {
// Original Skell starter profiles. Not artist matches or factory programs.
// Template parameter maps to the existing, stable processor ModuleType IDs.
inline constexpr const char* names[]{"Warm Upfront","Atmospheric Trap","Airy Melodic","Clean Studio","Punchy Rap",
    "Afro Silk Lead","Amapiano Close","Highlife Harmony","Afro R&B Velvet","Dancehall Rhythm"};
inline constexpr int count=10,variations=64;
inline constexpr const char* moods[]{"Gentle","Forward","Spacious","Intimate"};
template<class P>
package::Document make(int profile,int variant=0) {
    const int index=juce::jlimit(0,count-1,profile);
    constexpr int base[]{0,1,2,3,4,0,3,2,0,4};
    const int k=base[index];
    package::Document doc;
    doc.name=names[index];doc.bank="Vocal Starter Chains";doc.mode=1;
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
    // Original genre-inspired starting points; first five remain exact.
    if(index==5){
        doc.micPreAmount=.48f;doc.slots[3].params[1]=.19f;doc.slots[5].wet=.14f;
        reverb.wet=.17f;echo.params[10]=.12f;echo.params[13]=.44f;
    }else if(index==6){
        doc.micGateThreshold=-58.f;doc.slots[0].wet=.20f;doc.slots[3].params[2]=.17f;
        reverb.wet=.055f;echo.params[10]=.085f;echo.params[13]=.57f;
    }else if(index==7){
        doc.slots[3].params[1]=.15f;reverb.wet=.21f;echo.params[10]=.105f;
        slot(8,P::AdlibDoubler,.10f,{.26f,.35f,.32f,.5f}).parallelReturn=true;
    }else if(index==8){
        doc.micPreAmount=.52f;doc.slots[4].wet=.17f;doc.slots[3].params[2]=.19f;
        reverb.wet=.23f;echo.params[10]=.16f;echo.params[13]=.32f;
    }else if(index==9){
        doc.slots[3].params[1]=.30f;doc.slots[5].wet=.09f;
        reverb.wet=.065f;echo.params[10]=.18f;echo.params[13]=.44f;
        echo.params[2]=.61f;echo.rightFirst=true;
    }
    // Four coordinated intents, sixteen reproducible choices per intent.
    // Only existing controls change; all values remain conservatively bounded.
    if(variant>0 && variant<=variations){
        const int mood=(variant-1)/16;
        const unsigned seed=(unsigned)(index*variations+variant);
        auto adjust=[&](float& value,float delta,unsigned d,float lo,float hi){
            value=std::clamp(value+delta+.025f*audition::variation(seed,d),lo,hi);
        };
        auto& comp=doc.slots[3];auto& drive=doc.slots[4];auto& air=doc.slots[5];
        constexpr float compression[]{.06f,-.08f,-.015f,.035f};
        constexpr float space[]{-.035f,-.02f,.09f,-.07f};
        adjust(comp.params[0],compression[mood],1,.30f,.78f);
        adjust(comp.params[1],-compression[mood],2,.10f,.42f);
        adjust(drive.wet,mood==1?.055f:-.025f,3,.035f,.24f);
        adjust(air.wet,mood==1?.035f:0.f,4,.03f,.20f);
        adjust(reverb.wet,space[mood],5,.025f,.34f);
        adjust(echo.params[10],space[mood]*.75f,6,.025f,.31f);
        adjust(echo.params[1],space[mood]*.5f,7,.12f,.50f);
        adjust(reverb.duckDepth,mood==2?.12f:0.f,8,.40f,.80f);
        adjust(echo.duckDepth,mood==2?.10f:0.f,9,.45f,.85f);
        doc.outputDb=-4.f-.5f*audition::variation(seed,10);
        doc.name=juce::String(names[index])+" / "+moods[mood]+" "+juce::String(variant).paddedLeft('0',2);
    }
    return doc;
}
}
