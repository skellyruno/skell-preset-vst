#pragma once
#include "LiveMatchOptimizer.h"
#include <string>
#include <cctype>
namespace skellpreset::liveMatch {
// Local mixing preferences, not a genre classifier or measured reference.
enum class MixStyle { Balanced, Underground, Mainstream, RnB, Pop, Rap, Natural, Rock, Count };
struct StyleGuide {const char* name;float low,high,crest,level,chorus,delay,room;float drive=.10f,ride=.20f,tame=.18f;};
inline constexpr std::array<StyleGuide,8> styleGuides{{
    {"Balanced",-8,-15,12,-20,0,.04f,.10f}, {"Underground",-10,-12,9,-17,.26f,.17f,.12f,.28f,.28f,.20f},
    {"Mainstream",-9,-13,10,-18,.08f,.08f,.13f}, {"R&B",-7,-16,12,-20,.12f,.13f,.21f},
    {"Pop",-9,-12,10,-18,.09f,.07f,.17f}, {"Rap",-10,-14,10,-18,0,.08f,.07f,.18f,.30f,.22f},
    {"Natural",-7,-16,15,-22,0,0,.04f,.0f,.10f,.10f}, {"Rock",-9,-15,11,-19,0,.05f,.13f}
}};
inline const StyleGuide& guideFor(MixStyle style) noexcept {
    const int i=static_cast<int>(style);return styleGuides[(size_t)(i>=0&&i<(int)styleGuides.size()?i:0)];
}
// Bounded local intent controls. No arbitrary model-generated parameters.
struct MixIntent {float aggression=0,brightness=0,space=0,width=0,level=0;bool clarity=false;};
inline bool parseMixIntent(const std::string& text,MixIntent& intent) {
    if(text.empty()||text.size()>240)return false;
    std::string normalized=" ";for(unsigned char ch:text){if(std::isalnum(ch))normalized+=(char)std::tolower(ch);else if(normalized.back()!=' ')normalized+=' ';}if(normalized.back()!=' ')normalized+=' ';
    const auto has=[&](const char* term){return normalized.find(std::string(" ")+term+" ")!=std::string::npos;};
    // Unsupported or potentially destructive requests must not silently map to an effect.
    if(has("pitch")||has("autotune")||has("tune")||has("key")||has("not")||has("never")||has("no")||has("don t")||has("without"))return false;
    MixIntent next=intent;bool recognized=false;
    auto axis=[&](bool up,bool down,float& value){if(up&&down)return false;if(up||down){value=std::clamp(value+(up?.5f:-.5f),-1.f,1.f);recognized=true;}return true;};
    if(!axis(has("aggressive")||has("gritty")||has("more saturation"),has("less aggressive")||has("less saturation")||has("softer"),next.aggression)){
        if(has("less aggressive")&&!has("more aggressive")&&!has("gritty")&&!has("more saturation")){next.aggression=std::max(-1.f,intent.aggression-.5f);recognized=true;}else return false;
    }
    if(!axis(has("brighter")||has("more air"),has("darker")||has("less brightness"),next.brightness))return false;
    if(!axis(has("more reverb")||has("more space")||has("wetter"),has("less reverb")||has("less space")||has("drier"),next.space))return false;
    if(!axis(has("wider"),has("narrower")||has("less width"),next.width))return false;
    if(!axis(has("louder"),has("quieter"),next.level))return false;
    if(has("clear")||has("clearer")||has("less harsh")){next.clarity=true;recognized=true;}
    if(recognized)intent=next;
    return recognized;
}
inline StyleGuide contextualGuide(MixStyle style,const Profile& dry,const MixIntent& intent,const Profile* beat) noexcept {
    auto g=guideFor(style);
    g.crest-=2.f*intent.aggression;g.high+=2.f*intent.brightness;g.drive=std::clamp(g.drive+.22f*intent.aggression,0.f,.45f);
    g.room=std::clamp(g.room+.16f*intent.space,0.f,.45f);g.delay=std::clamp(g.delay+.09f*intent.space,0.f,.30f);
    g.chorus=std::clamp(g.chorus+.18f*intent.width,0.f,.45f);
    if(intent.clarity){g.tame=std::max(g.tame,.32f);g.room=std::min(g.room,.18f);g.delay=std::min(g.delay,.15f);}
    if(beat&&beat->valid()&&beat->detailed){
        // Context only: no aligned masking or musical-key inference.
        g.level=std::clamp(beat->rmsDb+1.5f,g.level-3.f,g.level+3.f);
        const float congestion=std::clamp((beat->bands[3]+18.f)/12.f,0.f,1.f);
        g.high+=.8f*congestion;g.room*=1.f-.35f*congestion;g.delay*=1.f-.25f*congestion;
    }
    g.level+=3.f*intent.level;
    if(dry.correlation<.8f)g.chorus*=.35f;
    if(dry.quietDb>-22.f){g.delay*=.5f;g.room*=.5f;}
    return g;
}
inline Target styleTarget(const Profile& dry,const StyleGuide& guide) noexcept {
    return {std::clamp(guide.low,dry.lowDb-4.f,dry.lowDb+3.f),std::clamp(guide.high,dry.highDb-3.f,dry.highDb+4.f),
            std::clamp(guide.crest,dry.crest-4.f,dry.crest),std::clamp(guide.level,dry.rmsDb-6.f,dry.rmsDb+8.f)};
}
inline bool autoMixSafe(const Profile& p,const Profile& dry) noexcept {
    return usableSegment(p)&&p.peak<=.97f&&p.active>=dry.active*.82f&&p.correlation>=std::min(.25f,dry.correlation-.15f)
        &&bandError(p,dry)<=4.f&&p.activeContrast>=std::max(0.f,dry.activeContrast-6.f)&&p.crest>=std::max(3.f,dry.crest-7.f);
}
inline float styleLoss(const Profile& p,const Profile& dry,const Target& target,const Recipe& recipe,const StyleGuide& guide) noexcept {
    if(!autoMixSafe(p,dry))return 1.e6f;
    const float space=std::abs(recipe.mod.wet-guide.chorus)+std::abs(recipe.delay.wet-guide.delay)+std::abs(recipe.room.wet-guide.room);
    const float desiredRide=guide.ride*std::clamp((dry.activeContrast-2.f)/4.f,0.f,1.f);
    const float desiredTame=guide.tame*std::clamp((dry.highDb+22.f)/12.f,0.f,1.f);
    const float color=std::abs(recipe.saturation.wet-guide.drive)+std::abs(recipe.rider.wet-desiredRide)+std::abs(recipe.harshness.wet-desiredTame);
    const float dynamics=.12f*std::abs(p.activeContrast-std::max(1.f,dry.activeContrast-2.f*desiredRide));
    return loss(p,target)+dynamics+.8f*color+.18f*bandError(p,dry)+1.3f*space+.08f*std::max(0.f,p.quietDb-dry.quietDb-12.f)+.6f*std::max(0.f,dry.correlation-p.correlation-.12f);
}
inline constexpr int autoMixBudget=57;
inline Optimization optimizeAutoMix(const Audio& a,const Profile& dry,MixStyle style,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr,const MixIntent& intent={},const Profile* beat=nullptr,const Recipe* previous=nullptr) {
    Optimization o;
    const auto canceled=[&]{return cancel&&cancel->load(std::memory_order_relaxed);};
    if(canceled()||!dry.valid()||!std::isfinite(a.rate)||a.rate<8000||a.rate>192000||a.stereo.size()%2||a.frames()<a.rate*5.9||a.frames()>a.rate*20.+1)return o;
    const auto train=selectReferenceSegment(a,0,a.frames()/2),hold=selectReferenceSegment(a,a.frames()/2,a.frames());
    const auto d1=measure(a,train,nullptr,cancel,true),d2=measure(a,hold,nullptr,cancel,true);
    if(!usableSegment(d1)||!usableSegment(d2)||canceled())return o;
    const auto guide=contextualGuide(style,d1,intent,beat);
    const auto t1=styleTarget(d1,guide),t2=styleTarget(d2,guide);
    Recipe neutral;neutral.ratio=1;neutral.threshold=-9;neutral.output=0;
    o.dryScore=styleLoss(d1,d1,t1,neutral,guide);o.dryHoldout=styleLoss(d2,d2,t2,neutral,guide);
    auto evaluate=[&](Recipe& r){
        if(canceled()||o.tested>=autoMixBudget)return 1.e6f;
        ++o.tested;if(progress)progress->store(o.tested,std::memory_order_relaxed);
        r.output=0;const auto measured=measure(a,train,&r,cancel,true);
        if(!usableSegment(measured)||canceled())return 1.e6f;
        // Short captures have less evidence about loud syllables. Reserve
        // extra peak headroom before holdout validation, not after it.
        const float peakCap=a.frames()<a.rate*9.? .65f : .90f;
        r.output=std::clamp(std::min(t1.level-measured.rmsDb,20.f*std::log10(peakCap/std::max(measured.peak,1.e-8f))),-12.f,12.f);
        return styleLoss(measure(a,train,&r,cancel,true),d1,t1,r,guide);
    };
    auto offer=[&](Recipe r){const auto score=evaluate(r);if(score+.015f<o.final){o.recipe=r;o.final=score;++o.accepted;}};
    Recipe seed=fit(d1);seed.low=std::clamp((t1.low-d1.lowDb)*.55f,-4.f,3.f);seed.high=std::clamp((t1.high-d1.highDb)*.65f,-3.f,4.f);
    seed.ratio=std::clamp(1.4f+(d1.crest-t1.crest)*.4f,1.2f,4.f);seed.deess=d1.highDb>-10.f&&seed.high<=0;
    o.recipe=seed;o.initial=o.final=evaluate(o.recipe);
    const auto starter=o.recipe;
    o.holdoutInitial=styleLoss(measure(a,hold,&starter,cancel,true),d2,t2,starter,guide);
    offer(neutral);auto shaped=seed;shaped.threshold=std::clamp(d1.rmsDb-4.f,-45.f,-9.f);offer(shaped);
    for(float scale:{1.f,.5f})for(int dim=0;dim<5;++dim)for(float sign:{-1.f,1.f}){
        auto r=o.recipe;switch(dim){case 0:r.low=std::clamp(r.low+sign*2.f*scale,-6.f,5.f);break;
        case 1:r.high=std::clamp(r.high+sign*2.f*scale,-5.f,6.f);break;
        case 2:r.threshold=std::clamp(r.threshold+sign*6.f*scale,-45.f,-9.f);break;
        case 3:r.ratio=std::clamp(r.ratio+sign*scale,1.f,5.f);break;case 4:r.deess=sign>0;break;}offer(r);
    }
    if(previous)offer(*previous);
    const auto core=o.recipe;
    const float ride=guide.ride*std::clamp((d1.activeContrast-2.f)/4.f,0.f,1.f);
    const float tame=guide.tame*std::clamp((d1.highDb+22.f)/12.f,0.f,1.f);
    for(float amount:{.5f,1.f,1.5f}){
        auto r=core;r.rider={std::min(.65f,ride*amount),{{.65f,.50f,.25f,0.f}}};if(r.rider.wet>.015f)offer(r);
        r=core;r.saturation={std::min(.50f,guide.drive*amount),{{.28f,.10f,.5f,.5f}}};if(r.saturation.wet>.015f)offer(r);
        r=core;r.harshness={std::min(.65f,tame*amount),{{d1.bands[4]>d1.bands[5]?.42f:.76f,.32f,.45f,.30f}}};if(r.harshness.wet>.015f)offer(r);
        r=o.recipe;r.rider={ride,{{.65f,.50f,.25f,0.f}}};r.saturation={guide.drive,{{.28f,.10f,.5f,.5f}}};r.harshness={tame,{{.55f,.32f,.45f,.30f}}};offer(r);
    }
    const auto tone=o.recipe;
    for(float amount:{.5f,1.f,1.5f}){
        auto r=tone;r.modulation=guide.chorus>0?1:0;r.mod={guide.chorus*amount,{{.12f,.45f,.5f,1.f}}};offer(r);
        r=tone;r.delay={guide.delay*amount,{{.18f,.24f,.36f,.5f}}};offer(r);
        r=tone;r.room={guide.room*amount,{{.45f,.52f,.08f,.14f}}};offer(r);
        r=tone;r.modulation=guide.chorus>0?1:0;r.mod={guide.chorus*amount,{{.12f,.45f,.5f,1.f}}};r.delay={guide.delay*amount,{{.18f,.24f,.36f,.5f}}};r.room={guide.room*amount,{{.45f,.52f,.08f,.14f}}};offer(r);
    }
    if(canceled())return {};
    o.holdoutFinal=styleLoss(measure(a,hold,&o.recipe,cancel,true),d2,t2,o.recipe,guide);
    if(o.holdoutFinal>=o.dryHoldout-.025f){o.recipe=starter;o.final=o.initial;o.holdoutFinal=o.holdoutInitial;o.accepted=0;}
    // A refinement must beat the currently applied recipe on both phrases,
    // measured under the NEW intent; otherwise the rack remains untouched.
    if(previous){const float priorTrain=styleLoss(measure(a,train,previous,cancel,true),d1,t1,*previous,guide);
        const float priorHold=styleLoss(measure(a,hold,previous,cancel,true),d2,t2,*previous,guide);
        if(o.final+.025f>=priorTrain||o.holdoutFinal+.025f>=priorHold)return o;}
    o.valid=!canceled()&&o.final+.025f<o.dryScore&&o.holdoutFinal+.025f<o.dryHoldout&&o.final<1.e5f&&o.holdoutFinal<1.e5f;
    return o;
}
}
