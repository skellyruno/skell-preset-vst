#pragma once
#include <array>

namespace skellpreset::dsp {
// Right-first is a mono send into the right delay line. Existing stereo
// input and normal ping-pong are untouched unless explicitly selected.
inline std::array<float,2> delaySendInput(float l,float r,int mode,bool rightFirst) noexcept {
    if(mode==2 && rightFirst)return {0.f,.5f*(l+r)};
    if(mode==0)return {.5f*(l+r),.5f*(l+r)};
    return {l,r};
}
// AdlibDoubler emits a delayed-only signal; the rack owns its dry blend.
// Duck just that return, not the dry signal or the delay history.
inline float duckDubReturn(float delayed,float gain) noexcept {
    return gain<1.f?delayed*gain:delayed;
}
}
