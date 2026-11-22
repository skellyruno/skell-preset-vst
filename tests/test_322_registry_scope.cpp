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
#include <cstring>
#include <string_view>
using skellpreset::registry::effects;
using skellpreset::visual::LiveAudioScope;
int main(){
    static_assert(skellpreset::registry::count()==43);
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

    scope.push(nullptr,nullptr,1024);
    scope.push(left.data(),right.data(),0);
    return 0;
}
