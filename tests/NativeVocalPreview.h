#pragma once
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/chain/VocalPreviewAnalysis.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
namespace skellpreset::preview {
// Separate offline processor: never alter a playing DAW instance or load
// external plug-ins. The native preset package reader rejects those slots.
using Metrics=audition::Metrics;
inline bool measure(Metrics& m,const juce::AudioBuffer<float>& audio,int start,int n) noexcept {
    for(int i=0;i<n;++i)if(!m.add(audio.getSample(0,start+i),audio.getSample(1,start+i)))return false;
    return true;
}
struct PreviewTransport final:juce::AudioPlayHead {
    double bpm=120.,rate=48000.;juce::int64 sample=0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo pos;pos.setBpm(bpm);pos.setTimeInSamples(sample);
        pos.setTimeInSeconds(sample/rate);pos.setPpqPosition(sample/rate*bpm/60.);
        pos.setIsPlaying(true);return pos;
    }
};
inline void printMetrics(const char* label,const Metrics& m){
    std::printf("%s: RMS %.2f dBFS | peak %.2f dBFS | active-frame RMS %.2f dBFS | crest %.2f dB | correlation %.3f | side %.2f dBFS | >FS samples %llu\n",
        label,audition::db(m.rms()),audition::db(m.peak),audition::db(m.activeRms()),m.crestDb(),m.correlation(),m.sideDb(),(unsigned long long)m.overSamples);
}
inline juce::Result render(const juce::File& source,const juce::File& destination,
                          int starter,const juce::File& package={},
                          double tailSeconds=0.,double bpm=120.,int variant=0) {
    if(!source.existsAsFile() || source==destination || destination.exists() ||
       destination.getFileExtension().toLowerCase()!=".wav" ||
       !std::isfinite(tailSeconds) || tailSeconds<0. || tailSeconds>10. ||
       !std::isfinite(bpm) || bpm<40. || bpm>260.)
        return juce::Result::fail("Select an existing input and a new WAV destination; tail 0-10 seconds, tempo 40-260 BPM.");
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if(!reader || (reader->numChannels!=1 && reader->numChannels!=2) ||
       !std::isfinite(reader->sampleRate) || reader->sampleRate<8000. ||
       reader->sampleRate>192000. || reader->lengthInSamples<1 ||
       reader->lengthInSamples>600.*reader->sampleRate)
        return juce::Result::fail("Preview accepts mono/stereo audio, 8-192 kHz, up to ten minutes.");
    PreviewTransport transport;transport.bpm=bpm;transport.rate=reader->sampleRate;
    auto processor=std::make_unique<SkellPresetAudioProcessor>();processor->setPlayHead(&transport);
    const auto loaded=starter>=0?processor->loadVocalStarter(starter,variant):processor->importPreset(package);
    if(loaded.failed())return loaded;
    for(int i=0;i<SkellPresetAudioProcessor::slotCount;++i)
        if(processor->slotType(i)==SkellPresetAudioProcessor::ExternalVST3)
            return juce::Result::fail("Offline preview supports native chains only.");
    // A preset's saved workspace bypass is honored, just as in the DAW.
    constexpr int block=512;
    processor->prepareToPlay(reader->sampleRate,block);
    const int latency=processor->getLatencySamples();
    if(latency<0 || latency>65536){processor->releaseResources();return juce::Result::fail("Latency outside preview safety budget.");}
    const auto sourceLength=reader->lengthInSamples;
    const auto outputLength=sourceLength+(juce::int64)std::llround(tailSeconds*reader->sampleRate);
    const auto processLength=outputLength+latency;
    // Same-directory temporary output. Never intentionally replace an existing file.
    juce::TemporaryFile temp(destination);
    std::unique_ptr<juce::OutputStream> stream(temp.getFile().createOutputStream());
    if(!stream){processor->releaseResources();return juce::Result::fail("Cannot create temporary WAV.");}
    juce::WavAudioFormat wav;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriter::Options{}
        .withSampleRate(reader->sampleRate).withNumChannels(2).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriter::Options::SampleFormat::floatingPoint));
    if(!writer){processor->releaseResources();return juce::Result::fail("Cannot create floating-point WAV writer.");}
    juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
    Metrics dry,wet,tail,fullWet;juce::int64 written=0;
    bool ok=true;
    for(juce::int64 pos=0;pos<processLength;pos+=block){
        const int n=(int)std::min((juce::int64)block,processLength-pos);
        audio.setSize(2,n,false,false,true);audio.clear();midi.clear();
        const int read=(int)std::max((juce::int64)0,std::min((juce::int64)n,sourceLength-pos));
        if(read){
            if(!reader->read(&audio,0,read,pos,true,reader->numChannels==2)){ok=false;break;}
            if(reader->numChannels==1)audio.copyFrom(1,0,audio,0,0,read);
            if(!measure(dry,audio,0,read)){ok=false;break;}
        }
        transport.sample=pos;processor->processBlock(audio,midi);
        if(processor->getLatencySamples()!=latency){ok=false;break;}
        const int skip=(int)std::clamp((juce::int64)latency-pos,(juce::int64)0,(juce::int64)n);
        const int emit=n-skip;
        if(emit){
            const int vocal=(int)std::clamp(sourceLength-written,(juce::int64)0,(juce::int64)emit);
            if(!measure(wet,audio,skip,vocal) || !measure(tail,audio,skip+vocal,emit-vocal) ||
               !measure(fullWet,audio,skip,emit) || !writer->writeFromAudioSampleBuffer(audio,skip,emit)){ok=false;break;}
            written+=emit;
        }
    }
    processor->releaseResources();writer.reset();
    if(!ok || written!=outputLength)return juce::Result::fail("Preview aborted: read/write, nonfinite signal or latency changed.");
    if(destination.exists() || !temp.getFile().moveFileTo(destination))return juce::Result::fail("Cannot publish new WAV.");
    std::printf("Preview: %s | rate %.0f | removed latency %d samples | frames %lld\n",
                processor->presetName().toRawUTF8(),reader->sampleRate,latency,(long long)written);
    std::printf("Tempo: %.2f BPM\n",bpm);
    printMetrics("Dry source",dry);printMetrics("Wet aligned source window",wet);
    if(tail.frames)printMetrics("Wet tail only",tail);
    auto gains=audition::match(dry,wet);
    if(gains.valid){
        const double peak=std::max(dry.peak*gains.dry,fullWet.peak*gains.wet);
        const double trim=peak>0.?std::min(1.,.891250938/peak):1.;
        std::printf("Suggested audition trims (not applied): dry %.2f dB | wet %.2f dB; matched source RMS, <= -1 dBFS sample peak including tail\n",
            audition::db(gains.dry*trim),audition::db(gains.wet*trim));
    }else std::puts("A/B trim unavailable: silence or level difference exceeds 12 dB.");
    std::puts("RMS, gated sample-frame RMS and sample peak are diagnostics, not LUFS, true peak or quality scores.");
    return juce::Result::ok();
}
}
