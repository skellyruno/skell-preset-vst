#include "../Source/chain/LiveVocalMatch.h"
#include "../Source/chain/AutoVocalMix.h"
#include "../Source/chain/LiveMatchOptimizer.h"
#include <iostream>
#include <limits>
#include <vector>
using namespace skellpreset::liveMatch;
int main(){
    for(double rate:{44100.,48000.,96000.,192000.}){
        Capture c;Profile p;bool done=false;c.start();
        std::vector<float> b(257,0.f);
        for(int n=0;n<rate*20/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid())return 1;
        c.cancel();if(c.read(p,done))return 2;
        c.start();if(c.read(p,done))return 3;
        int frame=0;
        for(int n=0;n<rate*7/257+1;++n){for(auto& x:b)x=.12f*std::sin(6.283185307179586*220.*frame++/rate);c.push(b.data(),nullptr,257,rate);}
        if(!c.read(p,done)||!done||!p.valid()||p.lowDb<-4.f)return 4;
        const auto r=fit(p);if(r.low>=0||r.ratio<1.5f||r.ratio>4.f)return 5;
        Audio sample;if(!c.copyCompleted(sample)||sample.frames()<rate*5.99||sample.rate!=rate)return 10;
        if(comparable(p,p))return 6;
        Profile ref=p;ref.highDb+=8;if(!comparable(p,ref)||fit(p,&ref).high<=0)return 7;
        c.cancel();c.start();std::fill(b.begin(),b.end(),1.f);
        for(int n=0;n<rate*7/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid())return 8;
        c.cancel();c.start();std::fill(b.begin(),b.end(),std::numeric_limits<float>::quiet_NaN());
        for(int n=0;n<rate*20/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid()||p.invalid<=0)return 9;
        // A single invalid sample must not be hidden by a long active take.
        c.cancel();c.start();std::fill(b.begin(),b.end(),.12f);b[0]=std::numeric_limits<float>::infinity();
        c.push(b.data(),nullptr,257,rate);b[0]=.12f;
        for(int n=0;n<rate*7/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid()||p.invalid!=1.f)return 15;
        c.cancel();c.start();
        for(int n=0;n<rate*7/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||!p.valid()||p.invalid!=0.f||p.correlation<.999f||p.sideDb>-60.f)return 16;
    }
    Audio audio;audio.rate=48000;Analyzer analyzer;analyzer.prepare(audio.rate);
    for(int i=0;i<288000;++i){const double time=i/audio.rate;const float x=(float)((.09+.025*std::sin(8.*time))*std::sin(1382.3*time)+.014*std::sin(25132.7*time));audio.stereo.push_back(x);audio.stereo.push_back(x);analyzer.push(&x,&x,1);}
    auto dry=analyzer.result();auto result=optimize(audio,dry);
    std::cout<<"Candidates "<<result.tested<<" accepted "<<result.accepted<<" train "<<result.dryScore<<" -> "<<result.final<<" holdout "<<result.dryHoldout<<" -> "<<result.holdoutFinal<<"\n";
    if(!result.valid||result.tested!=25||result.final>result.initial||result.holdoutFinal>result.holdoutInitial+.025f)return 11;
    // Sparse first-half vocal: keep the held-out split, extend the
    // training phrase rather than lowering the minimum active duration.
    Audio sparse;sparse.rate=48000.;sparse.stereo.resize(288000u*2);
    for(size_t i=0;i<sparse.frames();++i){
        const double time=i/sparse.rate;
        const bool voiced=(time>=.10&&time<.54)||(time>=1.85&&time<2.29)||time>=3.;
        const float x=voiced?.12f*(float)std::sin(1382.3*time):0.f;
        sparse.stereo[2*i]=sparse.stereo[2*i+1]=x;
    }
    const auto first=selectSegment(sparse,0,sparse.frames()/2),second=selectSegment(sparse,sparse.frames()/2,sparse.frames());
    if(first.end>second.begin||first.end-first.begin<=sparse.rate*1.5||!usableSegment(measure(sparse,first,nullptr)))return 13;
    Profile loud=dry;loud.rmsDb=-1.f;if(!std::isfinite(targetFor(loud,nullptr).level))return 14;
    // Loudness-only references are legitimate. A dual-mono rack must never
    // be promoted as an antiphase/stereo reconstruction just because tone fits.
    Profile level=dry;level.rmsDb-=4.f;if(!comparable(dry,level))return 17;
    Analyzer stereo;stereo.prepare(audio.rate);
    for(size_t i=0;i<audio.frames();++i){const float l=audio.stereo[i*2],r=-l;stereo.push(&l,&r,1);}
    const auto anti=stereo.result();
    if(!anti.valid()||anti.correlation>-.999f||anti.sideDb<20.f||!comparable(dry,anti))return 18;
    const auto rejected=optimize(audio,dry,&anti);
    if(rejected.valid||!rejected.referenceRejected)return 19;
    const auto held=measure(audio,second,&result.recipe);
    auto wrongLevel=held;wrongLevel.elapsed=6;wrongLevel.active=4;wrongLevel.rmsDb+=3.f;
    if(referencePass(held,wrongLevel))return 20;
    auto badAudio=audio;badAudio.stereo[200]=std::numeric_limits<float>::quiet_NaN();
    Analyzer badAnalyzer;badAnalyzer.prepare(audio.rate);
    for(size_t i=0;i<badAudio.frames();++i)badAnalyzer.push(&badAudio.stereo[i*2],&badAudio.stereo[i*2+1],1);
    if(badAnalyzer.result().valid()||optimize(badAudio,badAnalyzer.result()).valid)return 21;
    // Known native effect fixtures with changing voiced phrases and pauses.
    Audio phrase;phrase.rate=44100.;
    for(int i=0;i<8*phrase.rate;++i){const double t=i/phrase.rate,phase=std::fmod(t,1.);const double f=140+20*std::sin(3*t);
        const float e=phase<.72?(float)(.15*(.4+.6*std::sin(3.14159*phase/.72))):0.f;
        const float x=e*(float)(std::sin(2*3.14159*f*t)+.2*std::sin(2*3.14159*3*f*t)+.08*std::sin(2*3.14159*9*f*t));phrase.stereo.push_back(x);phrase.stereo.push_back(x);}
    auto profile=[](const Audio& a){Analyzer z;z.prepare(a.rate);for(size_t i=0;i<a.frames()&&!z.finished();++i)z.push(&a.stereo[2*i],&a.stereo[2*i+1],1);return z.result();};
    const auto pd=profile(phrase);
    for(int mode=0;mode<4;++mode){auto recipe=fit(pd);recipe.output=0.f;
        if(mode<2){recipe.modulation=mode+1;recipe.mod={.65f,{{.12f,.65f,.5f,1.f}}};}
        if(mode==2)recipe.delay={.42f,{{.22f,.35f,.45f,.5f}}};
        if(mode==3)recipe.room={.48f,{{.60f,.45f,.10f,.16f}}};
        Audio ref;ref.rate=phrase.rate;NativeCandidate chain(phrase.rate,recipe);
        for(size_t i=0;i<phrase.frames();++i){auto y=chain.process(phrase.stereo[2*i],phrase.stereo[2*i+1]);ref.stereo.insert(ref.stereo.end(),y.begin(),y.end());}
        const auto pr=profile(ref);const auto found=optimize(phrase,pd,&pr,nullptr,nullptr,&ref);
        std::cout<<"FX "<<mode<<" train "<<found.dryScore<<" -> "<<found.final<<" holdout "<<found.dryHoldout<<" -> "<<found.holdoutFinal<<" candidates "<<found.tested<<"\n";
        if(!found.valid||found.tested>referenceBudget||found.holdoutFinal>=found.dryHoldout)return 22+mode;
        if(mode<2&&(found.recipe.modulation!=mode+1||found.recipe.mod.wet<=0||found.recipe.delay.wet>0||found.recipe.room.wet>0))return 26+mode;
        if(mode==2&&(found.recipe.delay.wet<=0||found.recipe.mod.wet>0||found.recipe.room.wet>0))return 28;
        if(mode==3&&(found.recipe.room.wet<=0||found.recipe.mod.wet>0||found.recipe.delay.wet>0))return 29;
    }
    int stylePasses=0;
    for(auto style:{MixStyle::Balanced,MixStyle::Underground,MixStyle::Mainstream,MixStyle::RnB,MixStyle::Pop,MixStyle::Rap,MixStyle::Natural,MixStyle::Rock}){
        const auto a=optimizeAutoMix(phrase,pd,style);
        std::cout<<guideFor(style).name<<" auto mix "<<a.dryHoldout<<" -> "<<a.holdoutFinal<<" train "<<a.dryScore<<" -> "<<a.final<<" starter "<<a.holdoutInitial<<" accepted "<<a.valid<<" candidates "<<a.tested<<"\n";
        if(a.tested>autoMixBudget)return 30;
        if(a.valid){++stylePasses;if(a.holdoutFinal>=a.dryHoldout||a.final>=a.dryScore||a.recipe.output < -12.f||a.recipe.output >12.f)return 31;
            const auto observed=measure(phrase,selectReferenceSegment(phrase,phrase.frames()/2,phrase.frames()),&a.recipe,nullptr,true);
            const auto heldDry=measure(phrase,selectReferenceSegment(phrase,phrase.frames()/2,phrase.frames()),nullptr,nullptr,true);
            if(!autoMixSafe(observed,heldDry))return 32;}
    }
    if(stylePasses<3)return 33;
    const auto balanced=optimizeAutoMix(audio,dry,MixStyle::Balanced);
    std::cout<<"Renderer fixture balanced "<<balanced.valid<<" "<<balanced.dryHoldout<<" -> "<<balanced.holdoutFinal<<"\n";
    if(!balanced.valid)return 34;
    // Local command parsing is bounded, cumulative and transactional.
    MixIntent intent;
    if(!parseMixIntent("Make my vocal more aggressive but keep it clear",intent)||intent.aggression!=.5f||!intent.clarity)return 36;
    if(parseMixIntent("Do not make it brighter",intent)||intent.brightness!=0.f)return 37;
    if(parseMixIntent("brighter and darker",intent)||intent.brightness!=0.f)return 38;
    if(parseMixIntent("correct my pitch",intent))return 39;
    if(!parseMixIntent("less reverb and wider",intent)||intent.space!=-.5f||intent.width!=.5f)return 40;
    if(!parseMixIntent("more aggressive",intent)||intent.aggression!=1.f||!intent.clarity)return 41;
    const auto dryDetailed=measure(audio,{0,audio.frames()/2},nullptr,nullptr,true);
    Profile beat=dry;beat.detailed=true;beat.rmsDb=-12.f;beat.bands.fill(-30.f);beat.bands[3]=-7.f;
    const auto g1=contextualGuide(MixStyle::Balanced,dryDetailed,{},nullptr),g2=contextualGuide(MixStyle::Balanced,dryDetailed,{},&beat);
    if(g2.level<=g1.level||g2.room>=g1.room||g2.high<=g1.high)return 42;
    beat.invalid=1.f;const auto invalidBeat=contextualGuide(MixStyle::Balanced,dryDetailed,{},&beat);
    if(invalidBeat.level!=g1.level||invalidBeat.room!=g1.room||invalidBeat.high!=g1.high)return 43;
    // Each candidate stage must change the actual render, not just its settings.
    Recipe flat;flat.ratio=1.f;flat.output=0.f;
    for(int fx=0;fx<3;++fx){auto r=flat;
        if(fx==0)r.rider={.8f,{{.8f,.6f,.4f,0.f}}};
        if(fx==1)r.saturation={.5f,{{.55f,.35f,.5f,.5f}}};
        if(fx==2)r.harshness={.8f,{{.55f,.15f,.75f,.3f}}};
        NativeCandidate base(48000.,flat),changed(48000.,r);double diff=0.,power=0.;
        for(int i=0;i<48000;++i){const double t=i/48000.;const float x=(i<24000?.04f:.24f)*(float)std::sin(6.283185307179586*300.*t)+.08f*(float)std::sin(6.283185307179586*4000.*t);const auto a=base.process(x,x),b=changed.process(x,x);if(!std::isfinite(b[0])||b[0]!=b[1])return 44;diff+=(a[0]-b[0])*(a[0]-b[0]);power+=a[0]*a[0];}
        if(diff<std::max(1.e-6,1.e-5*power))return 45+fx;
    }
    const auto refined=optimizeAutoMix(audio,dry,MixStyle::Balanced,nullptr,nullptr,intent,nullptr,&balanced.recipe);
    if(refined.valid){const auto h=selectReferenceSegment(audio,audio.frames()/2,audio.frames());const auto raw=measure(audio,h,nullptr,nullptr,true);const auto guide=contextualGuide(MixStyle::Balanced,dryDetailed,intent,nullptr);const auto target=styleTarget(raw,guide);const auto prev=styleLoss(measure(audio,h,&balanced.recipe,nullptr,true),raw,target,balanced.recipe,guide);if(refined.holdoutFinal+.025f>=prev)return 48;}
    // Default timing matches the existing rack mapping; new timings are bounded.
    Recipe timing;auto controls=compControls(timing);
    if(std::abs(2.f+98.f*controls[2]-12.f)>.001f||std::abs(20.f+480.f*controls[3]-120.f)>.001f)return 49;
    timing.attackMs=24.f;timing.releaseMs=190.f;controls=compControls(timing);
    if(std::abs(2.f+98.f*controls[2]-24.f)>.001f||std::abs(20.f+480.f*controls[3]-190.f)>.001f)return 50;
    timing.attackMs=-10;timing.releaseMs=900;controls=compControls(timing);if(controls[2]!=0.f||controls[3]!=1.f)return 51;
    auto corrupt=audio;corrupt.stereo.back()=std::numeric_limits<float>::infinity();
    if(validMatchAudio(corrupt,nullptr)||optimizeAutoMix(corrupt,dry,MixStyle::Balanced).tested!=0||optimize(corrupt,dry).tested!=0)return 52;
    // Finite dry profile cannot conceal corruption elsewhere in a reference.
    if(optimize(audio,dry,&dry,nullptr,nullptr,&corrupt).tested!=0)return 53;
    // Frequency learning must follow actual burst location and not confuse a steady tone with sibilance.
    for(double burstHz:{3200.,7200.}){Audio fixture;fixture.rate=48000.;
        for(int i=0;i<48000*3;++i){const double t=i/48000.;const bool burst=std::fmod(t,.5)<.12;const float x=(float)(.12*std::sin(6.283185307179586*220.*t)+(burst?.10:0.)*std::sin(6.283185307179586*burstHz*t));fixture.stereo.push_back(x);fixture.stereo.push_back(x);}
        const auto learned=learnPresence(fixture,{0,fixture.frames()});std::cout<<"Presence "<<burstHz<<" -> "<<learned.harshHz<<" / "<<learned.sibilantHz<<"\n";
        if(burstHz<4000&&learned.harshHz!=3200.f)return 54;
        if(burstHz>4000&&learned.sibilantHz!=7200.f)return 55;
    }
    std::atomic<bool> canceled{true};if(optimizeAutoMix(audio,dry,MixStyle::Pop,&canceled).valid)return 35;if(optimize(audio,dry,nullptr,&canceled).valid)return 12;
    std::cout<<"Live vocal match capture and bounded profile decisions passed at four sample rates\n";
}
