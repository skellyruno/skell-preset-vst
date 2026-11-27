#pragma once
#include "LiveMatchOptimizer.h"
namespace skellpreset::liveMatch {
// Local mixing preferences, not a genre classifier or measured reference.
enum class MixStyle { Balanced, Underground, Mainstream, RnB, Pop, Rap, Natural, Rock, Count };
struct StyleGuide {const char* name;float low,high,crest,level,chorus,delay,room;};
inline constexpr std::array<StyleGuide,8> styleGuides{{
    {"Balanced",-8,-15,12,-20,0,.04f,.10f}, {"Underground",-10,-12,9,-17,.26f,.17f,.12f},
    {"Mainstream",-9,-13,10,-18,.08f,.08f,.13f}, {"R&B",-7,-16,12,-20,.12f,.13f,.21f},
    {"Pop",-9,-12,10,-18,.09f,.07f,.17f}, {"Rap",-10,-14,10,-18,0,.08f,.07f},
    {"Natural",-7,-16,15,-22,0,0,.04f}, {"Rock",-9,-15,11,-19,0,.05f,.13f}
}};
inline const StyleGuide& guideFor(MixStyle style) noexcept {
    const int i=static_cast<int>(style);return styleGuides[(size_t)(i>=0&&i<(int)styleGuides.size()?i:0)];
}
inline Target styleTarget(const Profile& dry,const StyleGuide& guide) noexcept {
    return {std::clamp(guide.low,dry.lowDb-4.f,dry.lowDb+3.f),std::clamp(guide.high,dry.highDb-3.f,dry.highDb+4.f),
            std::clamp(guide.crest,dry.crest-4.f,dry.crest),std::clamp(guide.level,dry.rmsDb-6.f,dry.rmsDb+8.f)};
}
inline bool autoMixSafe(const Profile& p,const Profile& dry) noexcept {
    return usableSegment(p)&&p.peak<=.97f&&p.active>=dry.active*.82f&&p.correlation>=std::min(.25f,dry.correlation-.15f)
        &&bandError(p,dry)<=4.f&&p.crest>=std::max(3.f,dry.crest-7.f);
}
inline float styleLoss(const Profile& p,const Profile& dry,const Target& target,const Recipe& recipe,const StyleGuide& guide) noexcept {
    if(!autoMixSafe(p,dry))return 1.e6f;
    const float space=std::abs(recipe.mod.wet-guide.chorus)+std::abs(recipe.delay.wet-guide.delay)+std::abs(recipe.room.wet-guide.room);
    return loss(p,target)+.18f*bandError(p,dry)+1.3f*space+.08f*std::max(0.f,p.quietDb-dry.quietDb-12.f)+.6f*std::max(0.f,dry.correlation-p.correlation-.12f);
}
inline constexpr int autoMixBudget=35;
inline Optimization optimizeAutoMix(const Audio& a,const Profile& dry,MixStyle style,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr) {
    Optimization o;
    const auto canceled=[&]{return cancel&&cancel->load(std::memory_order_relaxed);};
    if(canceled()||!dry.valid()||!std::isfinite(a.rate)||a.rate<8000||a.rate>192000||a.stereo.size()%2||a.frames()<a.rate*5.9||a.frames()>a.rate*20.+1)return o;
    const auto train=selectReferenceSegment(a,0,a.frames()/2),hold=selectReferenceSegment(a,a.frames()/2,a.frames());
    const auto d1=measure(a,train,nullptr,cancel,true),d2=measure(a,hold,nullptr,cancel,true);
    if(!usableSegment(d1)||!usableSegment(d2)||canceled())return o;
    auto guide=guideFor(style);
    if(d1.correlation<.8f)guide.chorus*=.35f;
    if(d1.quietDb>-22.f){guide.delay*=.5f;guide.room*=.5f;}
    const auto t1=styleTarget(d1,guide),t2=styleTarget(d2,guide);
    Recipe neutral;neutral.ratio=1;neutral.threshold=-9;neutral.output=0;
    o.dryScore=styleLoss(d1,d1,t1,neutral,guide);o.dryHoldout=styleLoss(d2,d2,t2,neutral,guide);
    auto evaluate=[&](Recipe& r){
        if(canceled()||o.tested>=autoMixBudget)return 1.e6f;
        ++o.tested;if(progress)progress->store(o.tested,std::memory_order_relaxed);
        r.output=0;const auto measured=measure(a,train,&r,cancel,true);
        if(!usableSegment(measured)||canceled())return 1.e6f;
        r.output=std::clamp(std::min(t1.level-measured.rmsDb,20.f*std::log10(.90f/std::max(measured.peak,1.e-8f))),-12.f,12.f);
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
    o.valid=!canceled()&&o.final+.025f<o.dryScore&&o.holdoutFinal+.025f<o.dryHoldout&&o.final<1.e5f&&o.holdoutFinal<1.e5f;
    return o;
}
}
