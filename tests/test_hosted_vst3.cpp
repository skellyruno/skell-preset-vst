#include <JuceHeader.h>
#include "../Source/host/HostedVST3.h"
#include "../Source/host/InstalledVST3Index.h"
#include <cmath>
#include <cstdio>

// Must be given the .vst3 bundle directory of Skell Host Fixture.
int main(int argc,char** argv) {
    if(argc<2){std::fprintf(stderr,"Expected plugin bundle path\n");return 2;}
    juce::ScopedJuceInitialiser_GUI gui;
    // Index only directory names. A nested .vst3 is an opaque bundle,
    // not a folder to recurse through, and duplicate roots are harmless.
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("skell-vst3-index-"+juce::Uuid().toString());
    const auto nested=root.getChildFile("FX").getChildFile("FabFilter Example.vst3");
    const auto other=root.getChildFile("Adlib Example.vst3");
    const auto hidden=nested.getChildFile("DoNotDiscover.vst3");
    const auto prepared=root.createDirectory().wasOk()
        && nested.createDirectory().wasOk()
        && other.createDirectory().wasOk()
        && hidden.createDirectory().wasOk();
    if(!prepared) {
        std::fprintf(stderr,"Could not create VST3 discovery fixture\n");return 8;
    }
    juce::Array<juce::File> roots;
    roots.add(root);roots.add(root);
    skellpreset::host::InstalledVST3Index index;
    index.start(roots);
    for(int pass=0;pass<100 && !index.finished();++pass)index.step(1);
    const auto found=index.entries();
    const bool valid=index.finished() && !index.truncated() && found.size()==2
        && found[0].name=="Adlib Example"
        && found[1].name=="FabFilter Example"
        && index.filter("fabfilter").size()==1
        && index.filter("ADLIB").size()==1
        && index.filter("not-present").empty()
        && index.filter("DoNotDiscover").empty();
    root.deleteRecursively();
    if(!valid){
        std::fprintf(stderr,"Installed VST3 bundle discovery/search regression\n");
        return 9;
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
    plugin->processBlock(audio,midi);
    double maxError=0.;
    for(int ch=0;ch<2;++ch)
        for(int n=0;n<512;++n) {
            const double ideal=.125*std::sin(.02*(double)n);
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
    std::printf("PASS nested JUCE VST3 load, stereo processing, PDC and state\n");
    return 0;
}
