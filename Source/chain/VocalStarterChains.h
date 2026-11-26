#pragma once
#include "PresetPackage.h"
#include "VocalPreviewAnalysis.h"
#include <iterator>

namespace skellpreset::starter {
// Original Skell starter profiles. Not artist matches or factory programs.
// Template parameter maps to the existing, stable processor ModuleType IDs.
// Profile indices 0..9 and variant seeds remain stable. Existing serialized
// user presets retain their own names; these are not factory program IDs.
inline constexpr const char* names[]{
    "Skelly Velvet","Skelly Eclipse","Skelly Halo","Skelly Platinum","Skelly Pressure",
    "Skelly Lagos","Skelly Log Drum","Skelly Highlife","Skelly Sable","Skelly Riddim",
    "Skelly Stardust","Skelly Moonwalk","Skelly After Hours","Skelly Timber",
    "Skelly Concrete","Skelly Black Ice","Skelly Overload","Skelly Candy Code",
    "Skelly Phantom","Skelly Glitch Angel"};
inline constexpr const char* genres[]{
    "Warm R&B","Atmospheric Trap","Melodic Rap","Radio / Clean","Mainstream Rap",
    "Afrobeats","Amapiano","Highlife","Afro R&B","Dancehall",
    "Pop","Melodic Trap","Neo Soul","Indie / Acoustic",
    "Boom Bap","Drill","Rage","Pluggnb","Cloud Rap","Hyperpop"};
inline constexpr const char* collections[]{"Mainstream","Underground","Global Rhythms"};
inline constexpr int groups[]{0,0,0,0,0,2,2,2,2,2,0,0,0,0,1,1,1,1,1,1};
inline constexpr int count=20,variations=64;
static_assert(std::size(names)==count && std::size(genres)==count && std::size(groups)==count);
inline constexpr const char* moods[]{"Gentle","Forward","Spacious","Intimate"};
template<class P>
package::Document make(int profile,int variant=0) {
    const int index=juce::jlimit(0,count-1,profile);
    constexpr int base[]{0,1,2,3,4,0,3,2,0,4,3,1,0,3,4,4,1,2,1,2};
    const int k=base[index];
    package::Document doc;
    doc.name=names[index];doc.bank="Skelly Sound Lab";doc.mode=1;
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
    }else if(index==10){ // Pop: controlled dynamics, clean top, short wide space
        doc.slots[2].params[1]=.62f;doc.slots[3].params[0]=.53f;doc.slots[3].params[1]=.24f;
        doc.slots[4].wet=.055f;doc.slots[5].wet=.16f;reverb.wet=.13f;
        echo.params[10]=.09f;echo.params[13]=.44f;
        slot(8,P::VocalChorusFX,.055f,{.10f,.26f,.32f,.24f});
    }else if(index==11){ // Melodic trap: controlled ping-pong and longer tails
        doc.slots[3].params[0]=.49f;doc.slots[4].wet=.16f;doc.slots[5].wet=.15f;
        reverb.wet=.28f;reverb.params[0]=.83f;reverb.duckDepth=.70f;
        echo.params[2]=.64f;echo.params[10]=.21f;echo.params[13]=.32f;echo.rightFirst=true;
    }else if(index==12){ // Neo soul: slower, softer, close and warm
        doc.micPreAmount=.62f;doc.micGateThreshold=-62.f;doc.slots[0].wet=.15f;
        doc.slots[3].params[0]=.67f;doc.slots[3].params[1]=.14f;doc.slots[3].params[2]=.24f;
        doc.slots[4].wet=.15f;doc.slots[5].wet=.045f;reverb.wet=.115f;echo.params[10]=.05f;
    }else if(index==13){ // Acoustic / indie: preserve dynamics and breath
        doc.micPreOn=false;doc.micGateThreshold=-64.f;doc.micGateDepth=.45f;doc.slots[0].wet=.10f;
        doc.slots[1].params[3]=.16f;doc.slots[3].params[0]=.72f;doc.slots[3].params[1]=.11f;
        doc.slots[3].params[2]=.30f;doc.slots[4].wet=.035f;doc.slots[5].wet=.055f;
        reverb.wet=.095f;echo.params[10]=.025f;
    }else if(index==14){ // Boom bap: narrow ambience, warm mid-forward edge
        doc.micPreTarget=1;doc.slots[1].params[1]=.46f;doc.slots[3].params[2]=.18f;
        doc.slots[4].params[0]=.38f;doc.slots[4].wet=.20f;doc.slots[5].wet=.055f;
        reverb.wet=.06f;echo.params[10]=.065f;echo.params[13]=.69f;echo.params[14]=.56f;
    }else if(index==15){ // Drill: dry, close, hard consonants, short dark throw
        doc.slots[3].params[0]=.40f;doc.slots[3].params[1]=.40f;doc.slots[3].params[2]=.035f;
        doc.slots[1].params[3]=.34f;doc.slots[4].wet=.20f;reverb.wet=.045f;
        echo.params[10]=.075f;echo.params[13]=.57f;echo.params[14]=.54f;
    }else if(index==16){ // Rage: gritty and dense but bounded; no new gain law
        doc.slots[3].params[0]=.39f;doc.slots[3].params[1]=.41f;
        doc.slots[4].params[0]=.58f;doc.slots[4].wet=.24f;doc.slots[5].wet=.10f;
        reverb.wet=.18f;echo.params[10]=.17f;doc.outputDb=-6.f;
        slot(8,P::Radio,.08f,{.06f,.28f,.20f,.45f});
    }else if(index==17){ // Pluggnb: soft shimmering doubling and gentle echoes
        doc.slots[3].params[0]=.63f;doc.slots[3].params[1]=.16f;doc.slots[4].wet=.065f;
        doc.slots[5].wet=.16f;reverb.wet=.24f;echo.params[10]=.17f;
        slot(8,P::VocalChorusFX,.14f,{.12f,.33f,.42f,.60f});
    }else if(index==18){ // Cloud rap: diffuse wet field with protected dry lead
        doc.slots[3].params[0]=.62f;doc.slots[4].wet=.075f;doc.slots[5].wet=.065f;
        reverb.wet=.33f;reverb.params[0]=.91f;reverb.duckDepth=.70f;
        echo.params[10]=.27f;echo.params[1]=.48f;echo.params[15]=.35f;echo.params[14]=.44f;
        slot(8,P::AdlibDoubler,.075f,{.31f,.39f,.38f,.50f}).parallelReturn=true;
    }else if(index==19){ // Hyperpop: animated texture, not an imported tuner
        doc.slots[3].params[0]=.45f;doc.slots[4].params[0]=.45f;doc.slots[4].wet=.23f;
        doc.slots[5].wet=.18f;reverb.wet=.15f;echo.params[10]=.14f;doc.outputDb=-5.f;
        slot(8,P::RetroCrush,.08f,{.52f,.75f,.24f,.40f});
        slot(9,P::VocalChorusFX,.09f,{.27f,.30f,.43f,.55f});
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
