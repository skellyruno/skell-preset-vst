#pragma once
// Independent Skell Air: two-band dynamic enhancement, no lookahead or allocation.
// Normalised controls: mid, high, protection, trim (-12..+12 dB).
#include <algorithm>
#include <array>
#include <cmath>
namespace skell::research {
class SkellAir {
public:
    static constexpr int latencySamples = 0;
    void prepare(double fs) noexcept {
        rate = std::clamp(std::isfinite(fs) ? fs : 48000., 8000., 384000.);
        auto pole = [this](double f) { return float(-std::expm1(-6.283185307179586 * std::min(f,.45*rate)/rate)); };
        auto time = [this](double ms) { return float(-std::expm1(-1000./(ms*rate))); };
        midPole=pole(3200.); highPole=pole(9500.);
        midAttack=time(2.); midRelease=time(90.); highAttack=time(.75); highRelease=time(55.);
        gainAttack=time(3.); gainRelease=time(60.); control=time(15.);
        reset();
    }
    void set(float m,float h,float p,float t=.5f) noexcept {
        targets={unit(m,0),unit(h,0),unit(p,1),std::exp(2.7631021116f*(unit(t,.5f)-.5f))};
    }
    void reset() noexcept {
        midLP={}; highLP={}; midEnv=highEnv=0; midGain=highGain=1; values=targets; seeded=false;
    }
    std::array<float,2> process(float l,float r) noexcept {
        std::array<float,2> x{finite(l),finite(r)}, m{},h{};
        if(!seeded){values=targets;seeded=true;}
        for(int j=0;j<4;++j)values[j]+=control*(targets[j]-values[j]);
        for(int c=0;c<2;++c){
            midLP[c]+=midPole*(x[c]-midLP[c]); highLP[c]+=highPole*(x[c]-highLP[c]);
            if(std::abs(midLP[c])<1.e-20f)midLP[c]=0;
            if(std::abs(highLP[c])<1.e-20f)highLP[c]=0;
            m[c]=highLP[c]-midLP[c]; h[c]=x[c]-highLP[c];
        }
        const float pm=std::max(std::abs(m[0]),std::abs(m[1])), ph=std::max(std::abs(h[0]),std::abs(h[1]));
        midEnv+=(pm>midEnv?midAttack:midRelease)*(pm-midEnv);
        highEnv+=(ph>highEnv?highAttack:highRelease)*(ph-highEnv);
        if(midEnv<1.e-20f)midEnv=0;
        if(highEnv<1.e-20f)highEnv=0;
        // Bounded gain reduction only on the added band, never on dry audio.
        const float gm=1.f/(1.f+5.f*std::max(0.f,midEnv-.025f));
        const float gh=1.f/(1.f+10.f*std::max(0.f,highEnv-.012f));
        midGain+=(gm<midGain?gainAttack:gainRelease)*(gm-midGain);
        highGain+=(gh<highGain?gainAttack:gainRelease)*(gh-highGain);
        const float a=1.65f*values[0]*(1.f-values[2]*(1.f-midGain));
        const float b=2.1f*values[1]*(1.f-values[2]*(1.f-highGain));
        for(int c=0;c<2;++c)x[c]=(x[c]+a*m[c]+b*h[c])*values[3];
        return x;
    }
private:
    static float finite(float x) noexcept{return std::isfinite(x)?std::clamp(x,-32.f,32.f):0.f;}
    static float unit(float x,float fallback) noexcept{return std::isfinite(x)?std::clamp(x,0.f,1.f):fallback;}
    double rate=48000.;
    float midPole=0,highPole=0,midAttack=0,midRelease=0,highAttack=0,highRelease=0,gainAttack=0,gainRelease=0,control=0;
    float midEnv=0,highEnv=0,midGain=1,highGain=1;
    std::array<float,2> midLP{},highLP{};
    std::array<float,4> targets{0,0,1,1},values{0,0,1,1};
    bool seeded=false;
};
}
