#pragma once
// SkellPreset central effect registry. Stable slot IDs are historical API;
// never reorder/remove entries or change host parameter choice ranges.
// Architecture informed by the user-supplied 322 DAW's BUILTIN_PLUGINS
// definitions. This is metadata, not a parallel DSP/audio processor.
#include <array>
#include <cstddef>
#include <string_view>

namespace skellpreset::registry {
struct Effect {
    int id;
    const char* name;
    const char* category;
    const char* terms;
    const char* reference322; // provenance/equivalent only, no Web Audio import
};
inline constexpr std::array<Effect,32> effects{{
    {0,"Empty","Utility","none clear empty bypass",""},
    {1,"EQ (Classic)","Tone","frequency bands low high shelf bell filter","322.EQ"},
    {2,"Compressor","Dynamics","compression level loud voice threshold knee","322.COMP"},
    {3,"Limiter","Dynamics","peak safety brickwall ceiling",""},
    {4,"Gate","Dynamics","noise threshold suppression silence hold",""},
    {5,"Saturator","Tone","drive distortion grit warmth waveshaper","322.DIST"},
    {6,"Reverb","Space","room reverb ambience algorithmic fdn tail","322.VERB"},
    {7,"Delay","Space","echo throw time feedback stereo tape delay","322.DELAY"},
    {8,"Exciter","Tone","harmonics treble air clarity",""},
    {9,"Adlib Throw","Space","echo throw delay adlib phrase",""},
    {10,"Adlib Doubler","Space","stereo layers doubler adlibs",""},
    {11,"De-Esser","Dynamics","sibilance consonants vocal deesser",""},
    {12,"External VST3","External","vst3 third party insert host bridge",""},
    {13,"Broadcast Radio","Tone","radio telephone lo-fi cut nasal",""},
    {14,"Retro Crush","Tone","drive grit distortion lofi bitcrush",""},
    {15,"Tremolo","Motion","sweep wobble amplitude mod",""},
    {16,"Stereo Width","Motion","stereo layers doubler mono bass width",""},
    {17,"Three-Band Dynamics","Dynamics","multiband compression loud voice",""},
    {18,"VoxRack Room","Space","room reverb ambience reflections",""},
    {19,"VoxRack Smooth Comp","Dynamics","compression loud voice opto",""},
    {20,"Vocal Chorus","Motion","chorus modulated ensemble layers stereo","322.CHORUS"},
    {21,"Vocal Flanger","Motion","sweep wobble flange feedback",""},
    {22,"Ring Modulator","Motion","alien metallic voice modulation",""},
    {23,"Vocal Rider","Dynamics","auto gain phrase levelling vocal chain",""},
    {24,"Ducking Ping-Pong","Space","duck echo ping pong tempo synced delay stereo adlib",""},
    {25,"Vocal Vibrato","Motion","vibrato voice pitch wobble flutter stereo",""},
    {26,"Parametric EQ / 8-Band","Tone","eight band parametric eq frequency gain bell q equalizer","322.EQ"},
    {27,"FET + Opto","Dynamics","serial compressor peak rms parallel smash","322.COMP"},
    {28,"Harshness Tamer","Dynamics","dynamic resonances sibilance soothe harshness",""},
    {29,"Adaptive Noise Control","Dynamics","noise control gate suppress background floor",""},
    {30,"Studio Pitch + Formant","Pitch","studio pitch shift formant spectral phase vocoder",""},
    {31,"Alchemist Match Finish","Dynamics","alchemist match harmonic cubic envelope learned vocal reference finishing",""}
}};
static_assert(effects.size()==32,"Keep registry in sync with ModuleType");
constexpr const Effect& at(int id) noexcept {
    return effects[static_cast<std::size_t>(id<0||id>=(int)effects.size()?0:id)];
}
constexpr int count() noexcept {return static_cast<int>(effects.size());}
// Useful for automated tests and documentation. The 322 processors all have
// existing native counterparts, so none is imported or duplicated in rack.
inline constexpr std::array<std::string_view,6> reference322Names{
    "322.EQ","322.COMP","322.VERB","322.DELAY","322.DIST","322.CHORUS"
};
} // namespace skellpreset::registry
