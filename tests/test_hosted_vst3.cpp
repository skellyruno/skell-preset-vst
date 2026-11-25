#include <JuceHeader.h>
#include "../Source/host/HostedVST3.h"
#include "../Source/host/InstalledVST3Index.h"
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
        std::fprintf(stderr,"Could not create VST3 discovery fixture\n");return 12;
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
        return 13;
    }

    skellpreset::host::HostedVST3 host;
    const juce::File fixture(juce::String::fromUTF8(argv[1]));
    using Host=skellpreset::host::HostedVST3;
    juce::OwnedArray<juce::PluginDescription> catalog;
    for(int i=0;i<3;++i){
        auto* d=catalog.add(new juce::PluginDescription());
        d->name="Owned fixture "+juce::String(i);d->fileOrIdentifier=fixture.getFullPathName();
        d->pluginFormatName="VST3";d->uniqueId=100+i;d->isInstrument=i==0;
    }
    if(Host::chooseEffect(catalog,{})!=catalog[1] ||
       Host::chooseEffect(catalog,Host::classKey(*catalog[2]))!=catalog[2] ||
       Host::chooseEffect(catalog,Host::classKey(*catalog[0]))!=nullptr ||
       Host::chooseEffect(catalog,"unavailable")!=nullptr)return 18;
    std::vector<Host::EffectChoice> choices;
    if(Host::inspectEffects(fixture,choices).failed() || choices.size()!=1)return 19;
    const auto loaded=host.load(0,fixture,48000.,512,choices[0].identifier);
    if(loaded.failed()) {
        std::fprintf(stderr,"Host instantiate failed: %s\n",
            loaded.getErrorMessage().toRawUTF8());
        return 3;
    }
    auto* plugin=host.get(0);
    if(!plugin || host.getLatency(0)!=0 || plugin->getTotalNumInputChannels()!=2 ||
       plugin->getTotalNumOutputChannels()!=2 || plugin->getBusCount(true)!=2 ||
       plugin->getBus(true,1)->isEnabled() || host.getIdentifier(0)!=choices[0].identifier) {
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
    // A replacement must not invalidate an already-running audio callback.
    {
        skellpreset::host::HostedVST3::AudioReadScope held(host);
        if(held.get(0)!=plugin || host.load(0,fixture,48000.,512).failed() ||
           host.get(0)==plugin || held.get(0)!=plugin || host.retainedCountForTest(0)!=2)return 14;
        audio.clear();held.get(0)->processBlock(audio,midi);
    }
    host.pollLatencies();
    if(host.retainedCountForTest(0)!=1)return 15;
    // Exceed the former eight-replacements-per-session limit without leaks.
    for(int i=0;i<12;++i) {
        if(host.load(0,fixture,48000.,512).failed())return 16;
        host.pollLatencies();
        if(host.retainedCountForTest(0)!=1)return 17;
    }
    host.release();host.prepare(44100.,256);host.release();
    std::printf("PASS nested JUCE VST3 load, transport, stereo processing, PDC, state and deferred replacement\n");
    return 0;
}
