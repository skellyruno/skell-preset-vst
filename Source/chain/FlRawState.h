#pragma once
// Lossless, offline FL state interchange. No host/audio-thread dependencies.
// Layouts validated against supplied state readers/writers. Unknown versions
// and out-of-range dynamics controls are rejected, never silently clamped.
#include <array>
#include <cstdint>
#include <vector>
#include <optional>
#include <cmath>
#include <string_view>
#include <limits>
namespace skell::state {
using Bytes = std::vector<std::uint8_t>;
enum class Kind { limiter, compressor, multiband, eq2, stereo, softClipper };
inline constexpr std::array<int,18> limiterMin{0,0,0,0,1,0,1,0,0,-1000,-1000,0,0,1,0,0,0,0};
inline constexpr std::array<int,18> limiterMax{2000,1000,2000,10000,8,10000,8,10000,1000,1000,1000,10000,10000,8,10000,1000,1000,10000};
inline constexpr std::array<int,6> compressorMin{-600,4,-300,0,1,0};
inline constexpr std::array<int,6> compressorMax{0,300,300,4000,4000,7};
inline std::optional<Kind> kind(std::string_view s) {
    if(s=="Fruity Limiter") return Kind::limiter;
    if(s=="Fruity Compressor") return Kind::compressor;
    if(s=="Fruity Multiband Compressor") return Kind::multiband;
    if(s=="Fruity Parametric EQ 2") return Kind::eq2;
    if(s=="Fruity Stereo Enhancer") return Kind::stereo;
    if(s=="Fruity Soft Clipper") return Kind::softClipper;
    return {};
}
inline std::int32_t read32(const Bytes& b, std::size_t i) {
    // Internal callers validate the full state length first.
    std::uint32_t v=0; for(unsigned k=0;k<4;++k) v|=std::uint32_t(b.at(i+k))<<(8*k);
    return v<=0x7fffffffu?static_cast<std::int32_t>(v):static_cast<std::int32_t>(static_cast<std::int64_t>(v)-4294967296LL);
}
inline void put32(Bytes& b,std::size_t i,std::int32_t v) {
    auto u=static_cast<std::uint32_t>(v);for(unsigned k=0;k<4;++k)b.at(i+k)=static_cast<std::uint8_t>(u>>(8*k));
}
struct State {
    Kind kind;
    Bytes original;
    int version=-1;
    std::vector<std::int32_t> controls;
    // Sidechain index is source routing metadata, not a destination bus ID.
    int sidechain=0;
    bool exactCeiling=false, reportLatency=false;
    std::array<std::int32_t,2> editorWords{};
    std::int32_t eqDesign=0, eqQuality=0, eqFade=0;
    std::uint8_t eqOptions=0;
    std::int32_t eqMask=0;
};
inline std::optional<State> decode(Kind k,const Bytes& b) {
    if(b.size()<4 || b.size()>4096) return {};
    State s{};s.kind=k;s.original=b;
    std::size_t offset=4,n=0;
    if(k==Kind::limiter) {
        s.version=read32(b,0); if(s.version!=7 || b.size()!=169) return {};
        n=18;s.sidechain=read32(b,164);s.exactCeiling=(b[168]&1)!=0;
    } else if(k==Kind::compressor) {
        s.version=read32(b,0);if(s.version<1||s.version>3||b.size()!=std::size_t(28+4*(s.version-1)))return {};
        n=6;if(s.version>=2)s.reportLatency=(read32(b,28)&1)!=0;if(s.version==3)s.sidechain=read32(b,32);
    } else if(k==Kind::multiband) {
        if(b.size()!=124||read32(b,0)!=0)return {};
        s.version=0;n=28;s.editorWords={read32(b,116),read32(b,120)};
    } else if(k==Kind::eq2) {
        if(b.size()!=354||read32(b,0)!=8)return {};
        s.version=8;n=36;s.eqDesign=read32(b,0x124);s.eqQuality=read32(b,0x135);s.eqFade=read32(b,0x139);s.eqOptions=b[0x13d];s.eqMask=read32(b,0x13e);
    } else if(k==Kind::stereo) {
        if(b.size()!=24)return {};
        offset=0;n=6;
    } else if(k==Kind::softClipper) {
        if(b.size()!=8)return {};
        offset=0;n=2;
    } else return {};
    if(s.sidechain<0)return {};
    for(std::size_t i=0;i<n;++i) {
        auto v=read32(b,offset+4*i);
        if(k==Kind::limiter&&(v<limiterMin[i]||v>limiterMax[i]))return {};
        if(k==Kind::compressor&&(v<compressorMin[i]||v>compressorMax[i]))return {};
        if(k==Kind::softClipper&&(i==0?(v<1||v>127):(v<0||v>160)))return {};
        s.controls.push_back(v);
    }
    return s;
}
inline std::optional<Bytes> encode(const State& s) {
    auto base=decode(s.kind,s.original);
    if(!base||base->version!=s.version||base->controls.size()!=s.controls.size()||s.sidechain<0)return {};
    // For now only known dynamics controls are editable; other formats are
    // decoded for inspection and retained losslessly, without guessing ranges.
    if(s.kind!=Kind::limiter&&s.kind!=Kind::compressor) {
        if(s.controls!=base->controls||s.sidechain!=base->sidechain||s.exactCeiling!=base->exactCeiling||s.reportLatency!=base->reportLatency||s.editorWords!=base->editorWords||s.eqDesign!=base->eqDesign||s.eqQuality!=base->eqQuality||s.eqFade!=base->eqFade||s.eqOptions!=base->eqOptions||s.eqMask!=base->eqMask)return {};
        return s.original;
    }
    if(s.editorWords!=base->editorWords||s.eqDesign!=base->eqDesign||s.eqQuality!=base->eqQuality||s.eqFade!=base->eqFade||s.eqOptions!=base->eqOptions||s.eqMask!=base->eqMask)return {};
    auto b=s.original;
    if(s.kind==Kind::limiter) {
        if(s.reportLatency!=base->reportLatency)return {};
        put32(b,164,s.sidechain);b[168]=static_cast<std::uint8_t>((b[168]&~1u)|(s.exactCeiling?1u:0u));
    } else {
        if(s.exactCeiling!=base->exactCeiling||(s.version<3&&s.sidechain!=0)||(s.version<2&&s.reportLatency))return {};
        if(s.version>=2)put32(b,28,(read32(b,28)&~1)|(s.reportLatency?1:0));
        if(s.version==3)put32(b,32,s.sidechain);
    }
    for(std::size_t i=0;i<s.controls.size();++i)put32(b,4+4*i,s.controls[i]);
    if(!decode(s.kind,b))return {};
    return b;
}
inline std::optional<float> normalize(int v,int lo,int hi) {
    if(lo>=hi||v<lo||v>hi)return {};
    return static_cast<float>((double(v)-lo)/(double(hi)-lo));
}
inline std::optional<int> denormalize(float v,int lo,int hi) {
    if(lo>=hi||!std::isfinite(v)||v<0.f||v>1.f)return {};
    return static_cast<int>(lo+std::round(double(v)*(double(hi)-lo)));
}
} // namespace skell::state
