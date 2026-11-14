#pragma once
// Skell Preset Mic Modeling — first native TONAL-profile stage.
// IMPORTANT: Neutral, Condenser, Dynamic and Ribbon are generalized response
// sketches, not measured emulations of RODE/Aston/Neumann hardware. No impulse
// responses, product names, third-party code or lookahead are shipped.
// Four slot parameters: input (p1), target (p2), amount (p3), trim (p4).
// Real-time safe: no allocations, locks, dynamic file I/O or resampling.
#include <algorithm>
#include <array>
#include <cmath>

namespace skellpreset::dsp {
class MicModelTone final {
public:
    static constexpr int profileCount=4;
    static int index(float normalized) noexcept {
        if(!std::isfinite(normalized))return 0;
        return std::clamp((int)(std::clamp(normalized,0.f,1.f)*3.999f),0,3);
    }
    static const char* inputName(int i) noexcept {
        static constexpr const char* names[]{"Neutral","Condenser","Dynamic","Ribbon"};
        return names[std::clamp(i,0,3)];
    }
    static const char* targetName(int i) noexcept {
        static constexpr const char* names[]{"Flat","Warm","Focus","Air"};
        return names[std::clamp(i,0,3)];
    }
    void prepare(double rate) noexcept {
        if(!std::isfinite(rate))rate=48000.;
        const float sr=(float)std::clamp(rate,8000.,192000.);
        constexpr float pi=3.14159265358979323846f;
        // Recombined bands have exact unity transfer when all gains = 1.
        lowCoeff=1.f-std::exp(-2.f*pi*260.f/sr);
        midCoeff=1.f-std::exp(-2.f*pi*3400.f/sr);
        smoothing=1.f-std::exp(-1.f/(sr*.025f));
        reset();
    }
    void reset() noexcept {
        lowState={0.f,0.f};midState={0.f,0.f};
        current={1.f,1.f,1.f,1.f};
        target={1.f,1.f,1.f,1.f};
    }
    void set(float input,float model,float amount,float trim) noexcept {
        const int in=index(input), out=index(model);
        if(!std::isfinite(amount))amount=0.f;
        if(!std::isfinite(trim))trim=.5f;
        amount=std::clamp(amount,0.f,1.f);
        trim=std::clamp(trim,0.f,1.f);
        const auto from=inputResponse(in), to=targetResponse(out);
        for(int b=0;b<3;++b) {
            const float db=(to[(size_t)b]-from[(size_t)b])*amount;
            target[(size_t)b]=std::pow(10.f,db/20.f);
        }
        target[3]=std::pow(10.f,(-6.f+12.f*trim)/20.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        for(int b=0;b<4;++b)
            current[(size_t)b]+=(target[(size_t)b]-current[(size_t)b])*smoothing;
        return {processOne(l,0),processOne(r,1)};
    }
private:
    static std::array<float,3> inputResponse(int i) noexcept {
        switch(i) {
            case 1: return {-1.0f, 0.2f, 2.0f}; // generic condenser
            case 2: return { 1.2f, 0.4f,-3.0f}; // generic dynamic
            case 3: return { 2.0f,-0.7f,-5.4f}; // generic ribbon
            default:return {0.f,0.f,0.f};
        }
    }
    static std::array<float,3> targetResponse(int i) noexcept {
        switch(i) {
            case 1: return {1.6f, 0.2f,-1.1f}; // warm
            case 2: return {-1.f,1.5f, 2.7f}; // focused presence
            case 3: return {-1.1f,0.2f,4.0f}; // open / airy
            default:return {0.f,0.f,0.f};
        }
    }
    float processOne(float in,int channel) noexcept {
        if(!std::isfinite(in))in=0.f;
        in=std::clamp(in,-8.f,8.f);
        auto& low=lowState[(size_t)channel];
        auto& mid=midState[(size_t)channel];
        low+=(in-low)*lowCoeff;
        mid+=(in-mid)*midCoeff;
        const float color=low*current[0]+(mid-low)*current[1]
                         +(in-mid)*current[2];
        const float result=color*current[3];
        return std::isfinite(result)?std::clamp(result,-16.f,16.f):0.f;
    }
    std::array<float,2> lowState{},midState{};
    std::array<float,4> target{{1.f,1.f,1.f,1.f}},current{{1.f,1.f,1.f,1.f}};
    float lowCoeff=.03f,midCoeff=.3f,smoothing=.001f;
};
} // namespace skellpreset::dsp
