// 322-informed effect registry and actual-output, no-fake-data scope tests.
// Portable: no JUCE or browser AudioContext dependencies.
#include "../Source/chain/EffectRegistry.h"
#include "../Source/chain/LiveAudioScope.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string_view>
using skellpreset::registry::effects;
using skellpreset::visual::LiveAudioScope;
int main(){
    static_assert(skellpreset::registry::count()==34);
    for(int id=0;id<skellpreset::registry::count();++id){
        const auto& effect=skellpreset::registry::at(id);
        assert(effect.id==id);
        assert(effect.name&&std::strlen(effect.name)>0);
        assert(effect.category&&std::strlen(effect.category)>0);
        for(int prior=0;prior<id;++prior)
            assert(std::string_view(effect.name)!=effects[(size_t)prior].name);
    }
    int matched=0;
    for(const auto& name:skellpreset::registry::reference322Names) {
        bool found=false;
        for(const auto& effect:effects) if(name==effect.reference322)found=true;
        assert(found);++matched;
    }
    assert(matched==6);
    assert(std::string_view(skellpreset::registry::at(26).reference322)=="322.EQ");
    assert(std::string_view(skellpreset::registry::at(20).reference322)=="322.CHORUS");
    assert(std::string_view(skellpreset::registry::at(32).name)=="Mic Modeling");
    assert(std::string_view(skellpreset::registry::at(33).name)=="Skelldizer");

    LiveAudioScope scope;
    std::array<float,128> display{};
    scope.snapshot(display);
    for(float s:display)assert(s==0.f); // no fake/animated waveform

    std::array<float,8192> left{},right{};
    for(int i=0;i<(int)left.size();++i){
        left[(size_t)i]=.6f*std::sin((float)i*.09f);
        right[(size_t)i]=left[(size_t)i];
    }
    scope.push(left.data(),right.data(),(int)left.size());
    assert(scope.samplesCaptured()==8192/LiveAudioScope::decimation);
    scope.snapshot(display);
    float maximum=0.f;
    for(float x:display) {
        assert(std::isfinite(x));
        maximum=std::max(maximum,std::abs(x));
    }
    assert(maximum>.5f);
    scope.reset();
    scope.snapshot(display);
    for(float x:display)assert(x==0.f);

    right.fill(0.f);
    left.fill(1.f);
    scope.push(left.data(),right.data(),(int)left.size());
    scope.snapshot(display);
    for(float x:display)assert(std::abs(x-.5f)<1e-6f);

    left.fill(10.f);right.fill(10.f);
    scope.push(left.data(),right.data(),(int)left.size());
    scope.snapshot(display);
    for(float x:display)assert(x==1.f); // bounded display, unaffected audio

    scope.push(nullptr,nullptr,1024);
    scope.push(left.data(),right.data(),0);
    return 0;
}
