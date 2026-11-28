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
// Original, worker-only spectral probe. Learn short bursts of upper-band
// energy relative to the whole vocal, rather than permanently EQing all highs.
struct PresenceLearn {float harshHz=0,sibilantHz=0;};
inline PresenceLearn learnPresence(const Audio& a,Segment phrase,const std::atomic<bool>* cancel=nullptr) noexcept {
    if(!std::isfinite(a.rate)||a.rate<8000||a.rate>192000||phrase.begin>=phrase.end||phrase.end>a.frames())return {};
    // Two cascaded narrow band-pass sections and non-overlapping category
    // centers. A 20 ms burst histogram measures relative energy, not pitch.
    constexpr std::array<float,12> frequencies{{1800,2200,2700,3200,3800,4400,5100,6000,7200,8400,9800,11500}};
    struct Probe {double b=0,a1=0,a2=0;std::array<std::array<double,2>,2> z1{},z2{};double energy=0;std::array<unsigned,81> hist{};unsigned count=0;};
    std::array<Probe,frequencies.size()> probes{};
    for(size_t k=0;k<probes.size();++k){if(frequencies[k]>=a.rate*.44)continue;auto& q=probes[k];const double w=6.283185307179586*frequencies[k]/a.rate,alpha=std::sin(w)/10.;q.b=alpha/(1.+alpha);q.a1=-2.*std::cos(w)/(1.+alpha);q.a2=(1.-alpha)/(1.+alpha);}
    const size_t window=std::max<size_t>(1,(size_t)(a.rate*.020));double total=0;size_t n=0;
    for(size_t i=phrase.begin;i<phrase.end;++i){if((i&2047u)==0&&cancel&&cancel->load(std::memory_order_relaxed))return {};
        for(size_t c=0;c<2;++c){const double x=a.stereo[2*i+c];if(!std::isfinite(x))return {};total+=x*x;
            for(auto& q:probes){if(q.b==0)continue;double v=x;for(size_t section=0;section<2;++section){const double y=q.b*v+q.z1[section][c];q.z1[section][c]=q.z2[section][c]-q.a1*y;q.z2[section][c]=-q.b*v-q.a2*y;v=y;}q.energy+=v*v;}}
        if(++n==window){if(total/(2.*window)>=1.e-5){for(auto& q:probes){if(q.b==0)continue;const double db=10.*std::log10(std::max(1.e-8,q.energy/std::max(total,1.e-12)));++q.hist[(size_t)std::clamp((int)(db+80.),0,80)];++q.count;}}for(auto& q:probes)q.energy=0;total=0;n=0;}
    }
    PresenceLearn result;float bestHarsh=0,bestSib=0;
    std::array<float,frequencies.size()> upper{},scores{};upper.fill(-80.f);
    for(size_t k=0;k<probes.size();++k){const auto& q=probes[k];if(q.count<20)continue;
        auto percentile=[&](unsigned goal){unsigned sum=0;for(size_t b=0;b<q.hist.size();++b){sum+=q.hist[b];if(sum>=goal)return -80.f+(float)b;}return 0.f;};
        const float typical=percentile((q.count+1)/2);upper[k]=percentile((9*q.count+9)/10);
        scores[k]=std::max(0.f,upper[k]+26.f)*std::clamp((upper[k]-typical-4.f)/8.f,0.f,1.f);
    }
    for(size_t k=0;k<probes.size();++k){
        // Require a local spectral maximum; a skirt of a stronger adjacent
        // burst cannot independently trigger the other treatment family.
        if((k>0&&upper[k]<upper[k-1])||(k+1<probes.size()&&upper[k]<=upper[k+1]))continue;
        const float score=scores[k];
        if(k<5&&score>bestHarsh){bestHarsh=score;result.harshHz=frequencies[k];}
        if(k>=5&&score>bestSib){bestSib=score;result.sibilantHz=frequencies[k];}
    }
    return result;
}
// Conservative sub-bass detector for the optional clean-input fallback. Four
// cascaded low-pass poles suppress voiced fundamentals; never sum L/R first.
inline float rumbleDb(const Audio& a,Segment phrase,const std::atomic<bool>* cancel=nullptr) noexcept {
    if(!std::isfinite(a.rate)||a.rate<8000||a.rate>192000||phrase.begin>=phrase.end||phrase.end>a.frames())return 0.f;
    const double pole=1.-std::exp(-6.283185307179586*35./a.rate);
    std::array<std::array<double,4>,2> state{};double sub=0,total=0;
    for(size_t i=phrase.begin;i<phrase.end;++i){if((i&2047u)==0&&cancel&&cancel->load(std::memory_order_relaxed))return 0.f;
        for(size_t ch=0;ch<2;++ch){double x=a.stereo[2*i+ch];if(!std::isfinite(x))return 0.f;total+=x*x;
            for(auto& z:state[ch]){z+=pole*(x-z);x=z;}sub+=x*x;}}
    return (float)(10.*std::log10(std::max(1.e-12,sub/std::max(1.e-20,total))));
}
inline constexpr int autoMixBudget=71;
inline Optimization optimizeAutoMix(const Audio& a,const Profile& dry,MixStyle style,const std::atomic<bool>* cancel=nullptr,std::atomic<int>* progress=nullptr,const MixIntent& intent={},const Profile* beat=nullptr,const Recipe* previous=nullptr) {
    Optimization o;
    const auto canceled=[&]{return cancel&&cancel->load(std::memory_order_relaxed);};
    if(canceled()||!dry.valid()||!validMatchAudio(a,cancel))return o;
    const auto train=selectReferenceSegment(a,0,a.frames()/2),hold=selectReferenceSegment(a,a.frames()/2,a.frames());
    const auto d1=measure(a,train,nullptr,cancel,true),d2=measure(a,hold,nullptr,cancel,true);
    if(!usableSegment(d1)||!usableSegment(d2)||canceled())return o;
    const auto guide=contextualGuide(style,d1,intent,beat);
    const auto t1=styleTarget(d1,guide),t2=styleTarget(d2,guide);
    Recipe neutral;neutral.ratio=1;neutral.threshold=-9;neutral.output=0;
    o.dryScore=styleLoss(d1,d1,t1,neutral,guide);o.dryHoldout=styleLoss(d2,d2,t2,neutral,guide);
    std::vector<float> rendered;
    auto evaluate=[&](Recipe& r){
        if(canceled()||o.tested>=autoMixBudget)return 1.e6f;
        ++o.tested;if(progress)progress->store(o.tested,std::memory_order_relaxed);
        r.output=0;const auto measured=measure(a,train,&r,cancel,true,&rendered);
        if(!usableSegment(measured)||canceled())return 1.e6f;
        // Short captures have less evidence about loud syllables. Reserve
        // extra peak headroom before holdout validation, not after it.
        const float peakCap=a.frames()<a.rate*9.? .65f : .90f;
        r.output=std::clamp(std::min(t1.level-measured.rmsDb,20.f*std::log10(peakCap/std::max(measured.peak,1.e-8f))),-12.f,12.f);
        return styleLoss(measureScaled(rendered,a.rate,r.output,cancel),d1,t1,r,guide);
    };
    auto offer=[&](Recipe r){const auto score=evaluate(r);if(score+.015f<o.final){o.recipe=r;o.final=score;++o.accepted;}};
    Recipe seed=fit(d1);seed.low=std::clamp((t1.low-d1.lowDb)*.55f,-4.f,3.f);seed.high=std::clamp((t1.high-d1.highDb)*.65f,-3.f,4.f);
    seed.ratio=std::clamp(1.4f+(d1.crest-t1.crest)*.4f,1.2f,4.f);seed.deess=d1.highDb>-10.f&&seed.high<=0;
    o.recipe=seed;o.initial=o.final=evaluate(o.recipe);
    const auto starter=o.recipe;
    o.holdoutInitial=styleLoss(measure(a,hold,&starter,cancel,true),d2,t2,starter,guide);
    offer(neutral);auto shaped=seed;shaped.threshold=std::clamp(d1.rmsDb-4.f,-45.f,-9.f);offer(shaped);
    // Predeclare a low-complexity fallback using training only. The original
    // voiced seed is not always the safest choice for a narrow-band signal.
    const auto safeSeed=o.recipe;const float safeTrain=o.final;
    Recipe cleanFallback=neutral;float cleanTrain=1.e6f;
    // A narrow, clean vocal need not lose low-register energy to a fixed
    // high-pass. Fit only the output level of this predeclared alternative.
    if(d1.highDb<-24.f&&rumbleDb(a,train,cancel)<-20.f){cleanFallback.highpassHz=0.f;cleanTrain=evaluate(cleanFallback);}
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
    if(o.holdoutFinal>=o.dryHoldout-.025f){
        const float safeHold=styleLoss(measure(a,hold,&safeSeed,cancel,true),d2,t2,safeSeed,guide);
        o.recipe=safeSeed;o.final=safeTrain;o.holdoutFinal=safeHold;o.accepted=0;
        // Neither fallback is fitted on holdout; this only vetoes unsafe work.
        if(safeHold>=o.dryHoldout-.025f){o.recipe=starter;o.final=o.initial;o.holdoutFinal=o.holdoutInitial;}
        if(o.holdoutFinal>=o.dryHoldout-.025f&&cleanTrain+.025f<o.dryScore&&rumbleDb(a,hold,cancel)<-20.f){
            const float cleanHold=styleLoss(measure(a,hold,&cleanFallback,cancel,true),d2,t2,cleanFallback,guide);
            if(cleanHold+.025f<o.dryHoldout){o.recipe=cleanFallback;o.final=cleanTrain;o.holdoutFinal=cleanHold;}
        }
    }
    // Preserve the original search winner as a predeclared fallback. The
    // timing challenger is selected solely on train; holdout only vetoes it.
    const auto timingBase=o.recipe;const float timingBaseTrain=o.final,timingBaseHold=o.holdoutFinal;
    const int timingBaseAccepted=o.accepted;
    if(timingBase.ratio>1.05f){
        // The fast/slow response families are intentionally broad, not fixed
        // genre presets. The dry dynamics and rendered score decide the winner.
        for(float attack:{5.f,24.f,40.f})for(float release:{65.f,190.f}){
            auto candidate=timingBase;candidate.attackMs=attack;candidate.releaseMs=release;offer(candidate);
        }
        const auto timingWinner=o.recipe;
        for(float release:{90.f,280.f}){auto candidate=timingWinner;candidate.releaseMs=release;offer(candidate);}
        if(canceled())return {};
        const float challengerHold=styleLoss(measure(a,hold,&o.recipe,cancel,true),d2,t2,o.recipe,guide);
        if(o.final+.025f<timingBaseTrain&&challengerHold+.025f<timingBaseHold){o.holdoutFinal=challengerHold;}
        else{o.recipe=timingBase;o.final=timingBaseTrain;o.holdoutFinal=timingBaseHold;o.accepted=timingBaseAccepted;}
    }
    // Learn from the dry training phrase only. Keep the timing winner as a
    // fallback; a new frequency setting must improve both measured phrases.
    const auto learned=learnPresence(a,train,cancel);
    if(learned.harshHz>0||learned.sibilantHz>0){
        const auto incumbent=o.recipe;const float incumbentTrain=o.final,incumbentHold=o.holdoutFinal;const int incumbentAccepted=o.accepted;
        if(learned.harshHz>0){for(float amount:{.5f,1.f,1.5f}){auto candidate=incumbent;candidate.harshness={std::min(.65f,std::max(.08f,tame)*amount),{{std::clamp(std::log(learned.harshHz/1800.f)/std::log(4.2f),0.f,1.f),.32f,.45f,.30f}}};offer(candidate);}}
        if(learned.sibilantHz>0){for(float reduction:{.10f,.25f,.40f}){auto candidate=o.recipe;candidate.deess=true;candidate.deessControls={{std::clamp((learned.sibilantHz*.85f-3500.f)/8000.f,0.f,1.f),.50f,reduction,.25f}};offer(candidate);}}
        if(canceled())return {};
        const float challengerHold=styleLoss(measure(a,hold,&o.recipe,cancel,true),d2,t2,o.recipe,guide);
        if(o.final+.025f<incumbentTrain&&challengerHold+.025f<incumbentHold)o.holdoutFinal=challengerHold;
        else{o.recipe=incumbent;o.final=incumbentTrain;o.holdoutFinal=incumbentHold;o.accepted=incumbentAccepted;}
    }
    // A refinement must beat the currently applied recipe on both phrases,
    // measured under the NEW intent; otherwise the rack remains untouched.
    if(previous){const float priorTrain=styleLoss(measure(a,train,previous,cancel,true),d1,t1,*previous,guide);
        const float priorHold=styleLoss(measure(a,hold,previous,cancel,true),d2,t2,*previous,guide);
        if(o.final+.025f>=priorTrain||o.holdoutFinal+.025f>=priorHold)return o;}
    o.valid=!canceled()&&o.final+.025f<o.dryScore&&o.holdoutFinal+.025f<o.dryHoldout&&o.final<1.e5f&&o.holdoutFinal<1.e5f;
    return o;
}
}
