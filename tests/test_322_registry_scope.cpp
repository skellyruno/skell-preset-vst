// 322-informed effect registry and actual-output, no-fake-data scope tests.
// Portable: no JUCE or browser AudioContext dependencies.
#include "../Source/chain/EffectRegistry.h"
#include "../Source/chain/LiveAudioScope.h"
#include <array>
#include <cstdio>
// CTest must evaluate its real assertions even in MSVC Release /DNDEBUG.
#define SKELL_REQUIRE(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\\n", #expression, __FILE__, __LINE__); \
        return 1; \
    } \
} while (false)
#include <cmath>
#include <limits>
#include <cstring>
#include <string_view>
using skellpreset::registry::effects;
using skellpreset::visual::LiveAudioScope;
int main(){
    static_assert(skellpreset::registry::count()==47);
    for(int id=0;id<skellpreset::registry::count();++id){
        const auto& effect=skellpreset::registry::at(id);
        SKELL_REQUIRE(effect.id==id);
        SKELL_REQUIRE(effect.name&&std::strlen(effect.name)>0);
        SKELL_REQUIRE(effect.category&&std::strlen(effect.category)>0);
        for(int prior=0;prior<id;++prior)
            SKELL_REQUIRE(std::string_view(effect.name)!=effects[(size_t)prior].name);
    }
    int matched=0;
    for(const auto& name:skellpreset::registry::reference322Names) {
        bool found=false;
        for(const auto& effect:effects) if(name==effect.reference322)found=true;
        SKELL_REQUIRE(found);++matched;
    }
    SKELL_REQUIRE(matched==6);
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(26).reference322)=="322.EQ");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(20).reference322)=="322.CHORUS");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(32).name)=="Mic Modeling");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(33).name)=="Skelldizer");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(34).name).find("Studio")!=std::string_view::npos);
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(37).name)=="Studio Fast Dist");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(38).name)=="Studio Delay");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(39).name)=="Adlib CHOP");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(40).name)=="Adlib MOTION");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(41).name)=="Adlib FILTER");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(42).name)=="Adlib SATURATE");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(43).name)=="Vocal Feel Opto");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(44).name)=="Vocal Feel Suite");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(45).name)=="Skell Air");
    SKELL_REQUIRE(std::string_view(skellpreset::registry::at(46).name)=="Skell De-Esser");

    LiveAudioScope scope;
    std::array<float,128> display{};
    scope.snapshot(display);
    for(float s:display)SKELL_REQUIRE(s==0.f); // no fake/animated waveform

    std::array<float,8192> left{},right{};
    for(int i=0;i<(int)left.size();++i){
        left[(size_t)i]=.6f*std::sin((float)i*.09f);
        right[(size_t)i]=left[(size_t)i];
    }
    scope.push(left.data(),right.data(),(int)left.size());
    SKELL_REQUIRE(scope.samplesCaptured()==8192/LiveAudioScope::decimation);
    scope.snapshot(display);
    float maximum=0.f;
    for(float x:display) {
        SKELL_REQUIRE(std::isfinite(x));
        maximum=std::max(maximum,std::abs(x));
    }
    SKELL_REQUIRE(maximum>.5f);
    scope.reset();
    scope.snapshot(display);
    for(float x:display)SKELL_REQUIRE(x==0.f);

    right.fill(0.f);
    left.fill(1.f);
    scope.push(left.data(),right.data(),(int)left.size());
    scope.snapshot(display);
    for(float x:display)SKELL_REQUIRE(std::abs(x-.5f)<1e-6f);

    left.fill(10.f);right.fill(10.f);
    scope.push(left.data(),right.data(),(int)left.size());
    scope.snapshot(display);
    for(float x:display)SKELL_REQUIRE(x==1.f); // bounded display, unaffected audio

    const auto before=scope.samplesCaptured();
    scope.push(nullptr,nullptr,1024);
    scope.push(left.data(),right.data(),0);
    SKELL_REQUIRE(scope.samplesCaptured()==before);

    // Variable host blocks must produce exactly the same sampled signal.
    for(size_t i=0;i<left.size();++i){
        left[i]=.8f*std::sin(float(i)*.031f);
        right[i]=.3f*std::cos(float(i)*.047f);
    }
    LiveAudioScope contiguous,partitioned;
    contiguous.push(left.data(),right.data(),(int)left.size());
    constexpr int chunks[]{1,17,63,64,65,127,3,511};
    int offset=0,chunk=0;
    while(offset<(int)left.size()){
        const int n=std::min(chunks[chunk++%8],(int)left.size()-offset);
        partitioned.push(left.data()+offset,right.data()+offset,n);
        offset+=n;
    }
    std::array<float,128> a{},b{};
    contiguous.snapshot(a);partitioned.snapshot(b);
    SKELL_REQUIRE(a==b);
    SKELL_REQUIRE(contiguous.samplesCaptured()==partitioned.samplesCaptured());

    // Do not publish a capture until a full decimation interval has elapsed.
    LiveAudioScope mono;
    left.fill(.25f);
    mono.push(left.data(),nullptr,63);
    SKELL_REQUIRE(mono.samplesCaptured()==0);
    mono.push(left.data(),nullptr,1);
    SKELL_REQUIRE(mono.samplesCaptured()==1);
    mono.snapshot(a);
    SKELL_REQUIRE(a.back()==.25f && a.front()==0.f);
    mono.reset();
    mono.push(left.data(),nullptr,64);
    SKELL_REQUIRE(mono.samplesCaptured()==1);

    // Invalid or extreme host samples must never poison the UI.
    left.fill(std::numeric_limits<float>::quiet_NaN());
    right.fill(std::numeric_limits<float>::infinity());
    mono.reset();mono.push(left.data(),right.data(),(int)left.size());
    mono.snapshot(a);
    for(float x:a)SKELL_REQUIRE(x==0.f);
    left.fill(std::numeric_limits<float>::max());
    right.fill(-std::numeric_limits<float>::max());
    mono.reset();mono.push(left.data(),right.data(),(int)left.size());
    mono.snapshot(a);
    for(float x:a)SKELL_REQUIRE(x==0.f);
    left.fill(-.7f);
    mono.push(left.data(),nullptr,(int)left.size());
    mono.snapshot(a);
    for(float x:a)SKELL_REQUIRE(x==-.7f);
    // Ring wrap keeps the newest 128 genuine samples.
    mono.push(left.data(),nullptr,(int)left.size());
    mono.snapshot(a);
    for(float x:a)SKELL_REQUIRE(x==-.7f);
    std::puts("PASS: effect registry, actual audio scope, block partition invariance, invalid samples and ring wrap");
    return 0;
}
