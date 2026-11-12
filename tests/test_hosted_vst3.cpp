#include <JuceHeader.h>
#include "../Source/host/HostedVST3.h"
#include <cmath>
#include <cstdio>

// Must be given the .vst3 bundle directory of Skell Host Fixture.
int main(int argc,char** argv) {
    if(argc<2){std::fprintf(stderr,"Expected plugin bundle path\n");return 2;}
    juce::ScopedJuceInitialiser_GUI gui;
    // A hosted Skell-Preset would recursively host its own DSP/UI. Reject
    // product-name spelling variants before instantiating external code.
    for(const auto* name:{"Skell-Preset", "SKELLPRESET", "Skell Preset", "Skell_Preset"}) {
        if(!skellpreset::host::isSkellPresetProductName(name)) {
            std::fprintf(stderr,"Recursive host name was not recognized: %s\n",name);
            return 8;
        }
    }
    if(skellpreset::host::isSkellPresetProductName("SkellyTune") ||
       skellpreset::host::isSkellPresetProductName("Skell DeEsser")) {
        std::fprintf(stderr,"Unrelated Skell products must remain hostable\n");
        return 9;
    }
    // The nested effect must see accurate tempo, transport and chunk position.
    juce::AudioPlayHead::PositionInfo position;
    position.setBpm(96.);
    position.setPpqPosition(12.);
    position.setTimeInSamples(std::int64_t{48000});
    position.setTimeInSeconds(1.);
    position.setIsPlaying(true);
    skellpreset::host::HostedTransport transport;
    transport.update(position,240,48000.);
    const auto observed=transport.getPosition();
    if(!observed || !observed->getIsPlaying()
       || observed->getBpm()!=position.getBpm()
       || observed->getTimeInSamples()!=juce::Optional<std::int64_t>{48240}
       || !observed->getTimeInSeconds()
       || std::abs(*observed->getTimeInSeconds()-1.005)>1.e-9
       || !observed->getPpqPosition()
       || std::abs(*observed->getPpqPosition()-12.008)>1.e-9) {
        std::fprintf(stderr,"Hosted transport BPM, playing or chunk offset wrong\n");
        return 10;
    }
    transport.update({},0,48000.);
    if(transport.getPosition()) {
        std::fprintf(stderr,"Missing DAW transport must remain unavailable\n");
        return 11;
    }

    skellpreset::host::HostedVST3 host;
    const juce::File fixture(juce::String::fromUTF8(argv[1]));
    const auto loaded=host.load(0,fixture,48000.,512);
    if(loaded.failed()) {
        std::fprintf(stderr,"Host instantiate failed: %s\n",
            loaded.getErrorMessage().toRawUTF8());
        return 3;
    }
    auto* plugin=host.get(0);
    if(!plugin || host.getLatency(0)!=0) {
        std::fprintf(stderr,"Fixture not loaded or wrong latency\n");return 4;
    }
    juce::AudioBuffer<float> audio(2,512);
    for(int ch=0;ch<2;++ch)
        for(int n=0;n<512;++n)
            audio.setSample(ch,n,.25f*std::sin(.02f*(float)n));
    juce::MidiBuffer midi;
    host.updateTransport(position,240,48000.);
    plugin->processBlock(audio,midi);
    double maxError=0.;
    for(int ch=0;ch<2;++ch)
        for(int n=0;n<512;++n) {
            const double ideal=.0625*std::sin(.02*(double)n);
            const double value=audio.getSample(ch,n);
            if(!std::isfinite(value))return 5;
            maxError=std::max(maxError,std::abs(value-ideal));
        }
    if(maxError>1.e-5) {
        std::fprintf(stderr,"Hosted audio error %g\n",maxError);return 6;
    }
    if(host.getState(0).isEmpty()) {
        std::fprintf(stderr,"Hosted plug-in state was not captured\n");return 7;
    }
    host.release();
    std::printf("PASS nested JUCE VST3 load, forwarded transport, stereo processing, PDC and state\n");
    return 0;
}
