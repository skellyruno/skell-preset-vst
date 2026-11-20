#pragma once
// Skell Mic Model: original five-zone source-to-target response modeling.
// Generalized source and target frequency profiles; these are NOT measured
// hardware microphone IRs or a proprietary Mic Mod engine.
// Same four slot parameters / five front-end parameters as before.
// Message/audio setup is allocation-free; process() is lock-free and stable.
#include <algorithm>
#include <array>
#include <cmath>

namespace skellpreset::dsp {
class MicModelTone final {
public:
    static constexpr int profileCount=4;
    static constexpr int spectralZones=5;
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
    static std::array<float,5> spectralDeltaDb(int input,int output,float amount) noexcept {
        if(!std::isfinite(amount))amount=0.f;
        amount=std::clamp(amount,0.f,1.f);
        const auto from=inputResponse(std::clamp(input,0,3));
        const auto to=targetResponse(std::clamp(output,0,3));
        std::array<float,5> delta{};
        for(int i=0;i<5;++i)delta[(size_t)i]=(to[(size_t)i]-from[(size_t)i])*amount;
        return delta;
    }
    static std::array<float,3> bandDeltaDb(int input,int output,float amount) noexcept {
        // The editor's three anchor nodes track the same five-zone DSP model,
        // not an invented curve or a separate source of truth.
        const auto d=spectralDeltaDb(input,output,amount);
        return {{d[0],d[2],d[4]}};
    }
    void prepare(double rate) noexcept {
        if(!std::isfinite(rate))rate=48000.;
        const float sr=(float)std::clamp(rate,8000.,192000.);
        // Avoid shadowing skellpreset::dsp::pi under MSVC /WX (C4459).
        constexpr float micModelPi=3.14159265358979323846f;
        constexpr std::array<float,4> crossover{{140.f,600.f,3000.f,8200.f}};
        for(int k=0;k<4;++k)
            coeff[(size_t)k]=1.f-std::exp(-2.f*micModelPi*crossover[(size_t)k]/sr);
        smoothing=1.f-std::exp(-1.f/(sr*.025f));
        reset();
    }
    void reset() noexcept {
        for(auto& channel:states)channel.fill(0.f);
        current.fill(1.f);
        target.fill(1.f);
    }
    void set(float input,float model,float amount,float trim) noexcept {
        if(!std::isfinite(trim))trim=.5f;
        const auto deltas=spectralDeltaDb(index(input),index(model),amount);
        for(int b=0;b<5;++b)
            target[(size_t)b]=std::pow(10.f,deltas[(size_t)b]/20.f);
        target[5]=std::pow(10.f,(-6.f+12.f*std::clamp(trim,0.f,1.f))/20.f);
    }
    std::array<float,2> process(float l,float r) noexcept {
        for(int b=0;b<6;++b)
            current[(size_t)b]+=(target[(size_t)b]-current[(size_t)b])*smoothing;
        return {{processOne(l,0),processOne(r,1)}};
    }
private:
    static std::array<float,5> inputResponse(int i) noexcept {
        switch(i) {
            case 1: return {{-1.0f,-.2f, .2f, .6f, 2.0f}}; // condenser family
            case 2: return {{ 1.2f, .7f, .4f,-1.4f,-3.0f}}; // dynamic family
            case 3: return {{ 2.0f,1.1f,-.7f,-3.1f,-5.4f}}; // ribbon family
            default:return {{0.f,0.f,0.f,0.f,0.f}};
        }
    }
    static std::array<float,5> targetResponse(int i) noexcept {
        switch(i) {
            case 1: return {{ 1.6f,.9f, .2f,-.5f,-1.1f}}; // warm
            case 2: return {{-1.0f,-.5f,1.5f,2.1f,2.7f}}; // focus
            case 3: return {{-1.1f,-.5f,.2f,2.3f,4.0f}}; // air
            default:return {{0.f,0.f,0.f,0.f,0.f}};
        }
    }
    float processOne(float sample,int channel) noexcept {
        if(!std::isfinite(sample))sample=0.f;
        const float in=std::clamp(sample,-8.f,8.f);
        auto& state=states[(size_t)channel];
        std::array<float,4> lowPass{};
        for(int k=0;k<4;++k) {
            auto& s=state[(size_t)k];
            s+=(in-s)*coeff[(size_t)k];
            lowPass[(size_t)k]=s;
        }
        // Telescope the complementary zones: at unity gains their sum is
        // exactly the input, with no crossover gap or lookahead/host PDC.
        const float shaped=lowPass[0]*current[0]
            +(lowPass[1]-lowPass[0])*current[1]
            +(lowPass[2]-lowPass[1])*current[2]
            +(lowPass[3]-lowPass[2])*current[3]
            +(in-lowPass[3])*current[4];
        const float out=shaped*current[5];
        return std::isfinite(out)?std::clamp(out,-16.f,16.f):0.f;
    }
    std::array<std::array<float,4>,2> states{};
    std::array<float,4> coeff{{.1f,.1f,.2f,.5f}};
    std::array<float,6> current{{1.f,1.f,1.f,1.f,1.f,1.f}};
    std::array<float,6> target{{1.f,1.f,1.f,1.f,1.f,1.f}};
    float smoothing=.001f;
};
} // namespace skellpreset::dsp
