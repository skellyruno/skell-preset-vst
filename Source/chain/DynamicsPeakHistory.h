#pragma once
// UI-thread-only history for already aggregated input/output peak meters.
// Based on the supplied axis/time law; no claim of sample-aligned gain reduction.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
namespace skell::meter {
inline constexpr double columnsPerSecond=87.0;
inline constexpr double columnMs=1000./columnsPerSecond;
inline float axis(float level) noexcept {
    if(!std::isfinite(level)||level<=0.f)return 0.f;
    return std::clamp(static_cast<float>(std::log1p(2.*level)/(2.*std::log(3.))),0.f,1.f);
}
struct Column {float input=0.f,output=0.f,peakRatio=1.f;bool started=false;};
template<std::size_t Capacity=1024> class History {
    static_assert(Capacity>=2&&Capacity<=8192);
public:
    void feed(float in,float out) noexcept {
        if(std::isfinite(in))pendingIn=std::max(pendingIn,in);
        if(std::isfinite(out))pendingOut=std::max(pendingOut,out);
    }
    int advance(double elapsedMs,bool playing) noexcept {
        if(!playing){pendingIn=pendingOut=0.f;return 0;}
        if(!std::isfinite(elapsedMs)||elapsedMs<=0.)return 0;
        clock+=std::min(elapsedMs,1000.);int added=0;
        while(clock>=columnMs){
            clock-=columnMs;
            if(pendingIn>1.e-4f){lastRatio=std::clamp(pendingOut/pendingIn,0.f,4.f);seen=true;}
            columns[head]={pendingIn,pendingOut,lastRatio,seen};head=(head+1)%Capacity;count=std::min(count+1,Capacity);++added;
        }
        if(added){pendingIn=pendingOut=0.f;}
        return added;
    }
    std::size_t size()const noexcept{return count;}
    std::optional<Column> at(std::size_t age)const noexcept {
        if(age>=count)return {};
        return columns[(head+Capacity-1-age)%Capacity];
    }
    void clear() noexcept {head=count=0;clock=0.;pendingIn=pendingOut=0.f;lastRatio=1.f;seen=false;}
private:
    std::array<Column,Capacity> columns{};std::size_t head=0,count=0;
    double clock=0.;float pendingIn=0.f,pendingOut=0.f,lastRatio=1.f;bool seen=false;
};
}
