#include "../Source/chain/StudioNativeDSP.h"
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
#include <iostream>
#include <limits>
int main(){
    using namespace skellpreset::studio;
    // New dynamic path is opt-in and frequency-selective at all rates.
    for(double sr:{44100.,48000.,96000.,192000.}){
        ParametricEQ a,b;a.prepare(sr);b.prepare(sr);
        std::array<float,16> p{};p.fill(.5f);p[2]=(float)(std::log(2000./20.)/std::log(1000.));
        std::array<float,8> q{};q.fill(.3f);std::array<float,8> depth{};depth[1]=1.f;
        a.set(p,q);b.set(p,q);a.setDynamics(depth);
        auto measure=[&](double f,float amp){a.prepare(sr);b.prepare(sr);a.set(p,q);b.set(p,q);a.setDynamics(depth);double wet=0.,dry=0.;for(int i=0;i<(int)(sr*.40);++i){const float x=amp*(float)std::sin(2.*skellpreset::dsp::pi*f*i/sr);auto y=a.process(x,x);auto z=b.process(x,x);if(i>sr*.25){wet+=y[0]*y[0];dry+=z[0]*z[0];}}return std::sqrt(wet/dry);};
        SKELL_REQUIRE(measure(2000.,.5f)<.75);
        SKELL_REQUIRE(measure(2000.,.005f)>.995);
        SKELL_REQUIRE(measure(100.,.5f)>.95);
        depth.fill(0.f);a.prepare(sr);b.prepare(sr);a.set(p,q);b.set(p,q);a.setDynamics(depth);
        for(int i=0;i<2000;++i){float x=.3f*std::sin(i*.031f);SKELL_REQUIRE(a.process(x,-x)==b.process(x,-x));}
    }
    FastDist dist;dist.prepare(48000.);
    struct Sample{std::array<int,5> p;float x;float expected;};
    // Reference vectors from Skell Studio's Rust tests; the original FL reference values.
    const Sample values[]{
       {{{128,1,0,128,128}},.021931838f,.02192916f},
       {{{128,1,0,128,128}},.0591505f,.059143275f},
       {{{192,1,0,104,93}},.087571196f,.11551188f},
       {{{192,10,1,128,80}},.06572277f,.14467822f},
       {{{192,10,1,128,128}},-.25366217f,-.8983255f}
    };
    for(const auto& v:values){
        dist.setRaw(v.p[0],v.p[1],v.p[2],v.p[3],v.p[4]);
        SKELL_REQUIRE(std::abs(dist.sample(v.x)-v.expected)<.00001f);
        SKELL_REQUIRE(std::isfinite(dist.sample(std::numeric_limits<float>::infinity())));
    }
    for(double sr:{44100.,48000.,96000.}){
        ParametricEQ eq;eq.prepare(sr);
        std::array<float,16> p{};p.fill(.5f);
        std::array<float,8> q{};q.fill(.5f);
        eq.set(p,q);
        double deviation=0.;
        for(int n=0;n<10000;++n){
            const float x=.35f*std::sin((float)n*.013f);
            const auto y=eq.process(x,-x);
            deviation=std::max(deviation,(double)std::abs(y[0]-x));
            SKELL_REQUIRE(std::isfinite(y[0])&&std::isfinite(y[1]));
        }
        SKELL_REQUIRE(deviation<.00001);
        // Studio EQ's high-shelf should change tone at high frequencies.
        p[13]=.83f; p[12]=.8f;eq.set(p,q);
        double loud=0.,dry=0.;
        for(int n=0;n<(int)(sr*.08);++n){
            const float x=.10f*std::sin((float)(2.*skellpreset::dsp::pi*9000.*n/sr));
            const auto y=eq.process(x,x);
            if(n>sr*.04){loud+=(double)y[0]*y[0];dry+=(double)x*x;}
        }
        SKELL_REQUIRE(loud>dry*1.1);
        Compressor comp;comp.prepare(sr);
        comp.set(.2f,.9f,.5f,.5f,.5f);
        double inEnergy=0.,outEnergy=0.;
        for(int n=0;n<(int)sr;++n){
            const float x=.8f*std::sin((float)n*.08f);
            const auto y=comp.process(x,x);
            SKELL_REQUIRE(std::isfinite(y[0])&&std::isfinite(y[1]));
            if(n>sr*.5){inEnergy+=(double)x*x;outEnergy+=(double)y[0]*y[0];}
        }
        SKELL_REQUIRE(outEnergy<inEnergy*.85);
        Gate gate;gate.prepare(sr);gate.set(.5f,0.f,.5f);
        float low=0.f;
        for(int n=0;n<(int)sr;++n)low=gate.processGain(.002f,.002f);
        SKELL_REQUIRE(low<.01f);
        gate.set(0.f,0.f,.5f);
        for(int n=0;n<(int)sr/2;++n)low=gate.processGain(.2f,.2f);
        SKELL_REQUIRE(low>.99f);
    }

    // New opt-in Studio EQ extensions: filter types must be audible, stable,
    // and safe at different rates; disabled B8 must not alter classic tone.
    for(double sr:{44100.,48000.,96000.}) {
        ParametricEQ eq;eq.prepare(sr);
        std::array<float,16> p{};p.fill(.5f);
        std::array<float,8> q{};q.fill(.5f);
        auto kinds=ParametricEQ::defaultKinds;
        auto active=ParametricEQ::defaultEnabled;
        // B8 disabled: all neutral, including its nonzero gain data.
        p[15]=1.f;
        eq.set(p,q,kinds,active);
        double maxNeutral=0.;
        for(int i=0;i<6000;++i) {
            const float x=.3f*std::sin((float)i*.026f);
            maxNeutral=std::max(maxNeutral,(double)std::abs(eq.process(x,-x)[0]-x));
        }
        SKELL_REQUIRE(maxNeutral<1.e-5);
        // B8 HP at 1 kHz rejects a 40Hz bass sine while allowing 4kHz.
        kinds[7]=1;active[7]=true;p[14]=(float)(std::log(1000./20.)/std::log(1000.));
        eq.set(p,q,kinds,active);
        auto rms=[&](double frequency){
            double sum=0.;
            for(int i=0;i<22000;++i){
                const float x=.2f*(float)std::sin(2.*skellpreset::dsp::pi*frequency*i/sr);
                const auto out=eq.process(x,x);
                if(!std::isfinite(out[0])||!std::isfinite(out[1]))return -1.;
                if(i>=12000)sum+=(double)out[0]*out[0];
            }
            return std::sqrt(sum/10000.);
        };
        const double low=rms(40.),high=rms(4000.);
        SKELL_REQUIRE(low>=0.&&high>=0.&&low<high*.15);
        // Low-pass gives the opposite relationship.
        kinds[7]=2;eq.set(p,q,kinds,active);eq.reset();
        const double lowPassBass=rms(40.),lowPassTreble=rms(4000.);
        SKELL_REQUIRE(lowPassTreble>=0.&&lowPassBass>=0.&&lowPassTreble<lowPassBass*.15);
    }
    std::cout<<"Studio native EQ/comp/gate/dist reference and safety tests passed\n";
}
