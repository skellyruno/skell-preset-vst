#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted C++17 DSP port from Lemon Studio's GPL-3.0-or-later chorus,
// Copyright (C) 2026 ItsAlphaNeon:
// https://github.com/ItsAlphaNeon/Lemon-Studio/blob/main/crates/os-engine/src/effects/chorus.rs
//
// The original Rust DSP's delay, three-LFO tables, crossover and fractional
// delay reads are ported directly. The 4-control rack adapter maps Preset's
// current 4 normalized parameters onto the Studio chorus settings.
// Distributing the combined code requires GPL-3.0-or-later compliance.
// No allocations, table creation, locks or trigonometry in process().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace skellpreset::studio {
class Chorus final {
public:
    static constexpr int parameterCount=12;
    static constexpr std::size_t lineSize=4096;
    using Values=std::array<int,parameterCount>;
    static constexpr Values defaults{
        512,2250,341,1500,2500,3500,23,0,0,0,2500,0
    };
    static constexpr std::array<int,parameterCount> maxima{
        1024,5000,1024,5000,5000,5000,1024,1024,1024,100,5000,1024
    };

    void prepare(double sampleRate) noexcept {
        rate_=(std::uint32_t)std::clamp(std::isfinite(sampleRate)?
            sampleRate:48000.,8000.,192000.);
        (void)tables(); // Initialize LFO lookup outside process(), once.
        applied_=defaults;
        configure();
        seed_=0;
        reset();
    }
    void reset() noexcept {
        for(auto& side:line_)side.fill(0.f);
        low_.fill(0.f);
        previous_.fill(0.f);
        write_=0;
        for(auto& phase:phase_) {
            seed_=seed_*0x08088405u+1u;
            phase=(std::uint32_t)(((std::uint64_t)seed_*32767u)>>32u);
        }
    }
    void setRaw(Values values) noexcept {
        for(int i=0;i<parameterCount;++i)
            values[(size_t)i]=std::clamp(values[(size_t)i],0,maxima[(size_t)i]);
        if(values!=applied_) {
            applied_=values;
            configure();
        }
    }
    const Values& raw() const noexcept {return applied_;}
    // Preset's existing four automated controls: rate Hz, depth ms,
    // voice richness, stereo width. Map to the three Studio LFOs.
    // The global slot wet knob remains the output dry/wet control.
    void set(float hz,float depthMs,float voices,float stereo) noexcept {
        const auto safe=[](float v,float lo,float hi) {
            return std::clamp(std::isfinite(v)?v:lo,lo,hi);
        };
        const float speed=safe(hz,.05f,5.f);
        const float rich=(safe(voices,2.f,5.f)-2.f)/3.f;
        Values v=defaults;
        v[0]=512; // Studio default 15ms base delay.
        v[1]=(int)std::lround(safe(depthMs,0.f,12.f)*5000.f/12.f);
        v[2]=(int)std::lround(safe(stereo,0.f,1.f)*1024.f);
        const auto speedKnob=[](float f) {
            return (int)std::lround(
                std::sqrt(std::clamp(f/5.f,0.f,1.f))*5000.f);
        };
        v[3]=speedKnob(speed);
        v[4]=speedKnob(speed*(.68f+.20f*rich));
        v[5]=speedKnob(speed*(1.12f+.32f*rich));
        v[6]=23; // Default arc LFO table.
        v[7]=0;
        v[8]=(int)std::lround(rich*340.f); // Arc -> arc cubed.
        v[9]=0; v[10]=2500;
        v[11]=0; // Keep Studio's normal direct + modulated output.
        // The rack wet knob crossfades the complete Studio Chorus result,
        // not a delayed-only signal that would erase vocal consonants.
        setRaw(v);
    }
    std::array<float,2> process(float left,float right) noexcept {
        const auto& t=tables();
        auto safe=[](float x) noexcept -> float {
            if(!std::isfinite(x)||std::abs(x)<1.e-20f)return 0.f;
            return std::clamp(x,-8.f,8.f);
        };
        const std::array<float,2> x{(float)((double)safe(left)*.7),
                                     (float)((double)safe(right)*.7)};
        std::array<std::array<std::uint32_t,3>,2> offsets{};
        for(int k=0;k<3;++k) {
            const auto& table=t[(size_t)work_.shape[(size_t)k]];
            const auto index=phase_[(size_t)k]>>12;
            offsets[0][(size_t)k]=
                (table[(size_t)(index&mask)]*work_.depth)>>9;
            offsets[1][(size_t)k]=
                (table[(size_t)((index+work_.stereo)&mask)]*work_.depth)>>9;
            phase_[(size_t)k]+=work_.step[(size_t)k];
        }
        const auto base=((std::uint32_t)write_<<12)-work_.delay;
        std::array<float,2> output{};
        for(int ch=0;ch<2;++ch) {
            const auto i=(size_t)ch;
            const float low=(x[i]+previous_[i])*work_.gain +
                              work_.pole*low_[i];
            low_[i]=std::isfinite(low)?low:0.f;
            previous_[i]=x[i];
            line_[i][write_]=work_.lowBand?low_[i]:x[i]-low_[i];
            float total=0.f;
            for(int k=0;k<3;++k)
                total+=read(line_[i],base-offsets[i][(size_t)k]);
            const float wet=(float)((double)total*.6);
            const float y=work_.wetOnly?wet:wet+x[i];
            output[i]=safe(y);
            if(std::abs(low_[i])<1.e-11f)low_[i]=0.f;
        }
        write_=(write_+1)&mask;
        return output;
    }
private:
    static constexpr std::uint32_t mask=4095u;
    struct Work {
        std::uint32_t delay=0,depth=0,stereo=0;
        std::array<std::uint32_t,3> step{};
        std::array<int,3> shape{};
        float pole=0.f,gain=0.f;
        bool lowBand=false,wetOnly=false;
    } work_;
    std::uint32_t rate_=48000u,seed_=0u;
    Values applied_=defaults;
    std::array<std::uint32_t,3> phase_{};
    std::array<float,2> low_{},previous_{};
    std::array<std::array<float,lineSize>,2> line_{};
    std::size_t write_=0;
    static const std::array<std::array<std::uint32_t,lineSize>,4>&
    tables() noexcept {
        static const auto t=[] {
            std::array<std::array<std::uint32_t,lineSize>,4> out{};
            constexpr double pi=3.14159265358979323846;
            constexpr double tau=2.*pi;
            const double norm=2.*std::sqrt(2.)+1.;
            for(std::size_t i=0;i<lineSize;++i) {
                const float x=(float)((double)i/2048.);
                const float arc=x<1.f?x*4.f*(1.f-x):
                    (x-1.f)*-4.f*(2.f-x);
                const double d=(double)x;
                const float sine=(float)((std::sin(pi*d)+
                    (std::sin(3.*pi*d)+std::sin(tau*d))/3.)*3./norm);
                const std::array<float,4> wave{
                    arc,arc*arc*arc,sine,sine*sine*sine};
                for(int k=0;k<4;++k)
                    out[(size_t)k][i]=(std::uint32_t)std::max(0.,
                        (double)((wave[(size_t)k]+1.f)*16383.f));
            }
            return out;
        }();
        return t;
    }
    static float position(int index,int raw) noexcept {
        return std::clamp((float)((double)raw/(double)maxima[(size_t)index]),
                          0.f,1.f);
    }
    static float read(const std::array<float,lineSize>& line,
                      std::uint32_t sample) noexcept {
        const auto j=(size_t)((sample>>12)&mask);
        const auto frac=sample&mask;
        const float a=line[j],d=line[(j+1)&mask]-a;
        return (float)((double)a+(double)d*(double)frac*(1./4096.));
    }
    void configure() noexcept {
        const auto knob=[this](int k) {return position(k,applied_[(size_t)k]);};
        const float d=knob(0),depth=knob(1),st=knob(2);
        work_.delay=(std::uint32_t)((((double)d*29.95)+.05)
                     *(double)rate_*4.096);
        const float scaled=(depth*5.f)*(float)rate_;
        work_.depth=(std::uint32_t)(((double)((scaled*4.096f)/64.f)));
        work_.stereo=(std::uint32_t)((st*4096.f)*.5f)-1u;
        for(int k=0;k<3;++k) {
            const auto x=knob(3+k),shape=knob(6+k);
            work_.step[(size_t)k]=(std::uint32_t)
                (((x*x)*83886080.f)/(float)rate_);
            work_.shape[(size_t)k]=std::min(3,(int)(shape*3.f));
        }
        work_.lowBand=knob(9)>.25f;
        work_.wetOnly=knob(11)>.25f;
        constexpr double pi=3.14159265358979323846;
        const double x=(double)knob(10);
        const float hz=(float)(std::exp(((127.*x-69.)/12.)*
                                         .69314718055994530942)*440.);
        const double angle=std::min((double)hz,(double)rate_/4.1)/
                           (double)rate_*(2.*pi);
        const float pole=(float)((1.-std::sin(angle))/std::cos(angle));
        work_.pole=std::isfinite(pole)?pole:0.f;
        work_.gain=(float)(((float)(1.-(double)work_.pole))*.5f);
    }
};
} // namespace skellpreset::studio
