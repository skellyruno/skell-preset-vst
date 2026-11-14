// Diagnostic-only Alchemist WAV renderer. Compiles and runs the SAME
// SkellPresetAudioProcessor::processBlock and factory chain as the VST3;
// this is not an external VST3-host or FL Studio session validation.
// Does not package, inspect or transmit the owner's WAV files.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
constexpr int blockSize = 512;

int process(juce::AudioBuffer<float>& block, SkellPresetAudioProcessor& plugin,
            double& peak, double& energy, std::int64_t& count) {
    juce::MidiBuffer midi;
    plugin.processBlock(block, midi);
    for (int ch = 0; ch < block.getNumChannels(); ++ch)
        for (int n = 0; n < block.getNumSamples(); ++n) {
            const float v = block.getSample(ch,n);
            if (!std::isfinite(v)) {
                std::fprintf(stderr, "ERROR: non-finite native Alchemist sample\n");
                return 1;
            }
            peak = std::max(peak, static_cast<double>(std::abs(v)));
            energy += static_cast<double>(v)*v;
            ++count;
        }
    return 0;
}

int smoke() {
    SkellPresetAudioProcessor plugin;
    plugin.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if (plugin.loadedFactoryPresetIndex() != skellpreset::alchemist::factoryIndex
        || plugin.slotType(6) != SkellPresetAudioProcessor::AlchemistMatchFinishFX
        || plugin.activeSlotCount() < 7) {
        std::fprintf(stderr,"ERROR: native Alchemist factory chain did not load\n");
        return 2;
    }
    plugin.prepareToPlay(44100.,blockSize);
    // This seven-stage factory has no limiter, spectral pitch, or hosted
    // processor; Match Finish itself must not introduce host PDC.
    if(plugin.getLatencySamples()!=0) {
        std::fprintf(stderr,"ERROR: Alchemist unexpectedly reports %d latency samples\\n",
                     plugin.getLatencySamples());
        return 6;
    }
    double peak=0.,energy=0.,difference=0.;
    std::int64_t count=0;
    for (int blockIndex=0;blockIndex<100;++blockIndex) {
        juce::AudioBuffer<float> audio(2,blockSize);
        for (int n=0;n<blockSize;++n)
            for (int ch=0;ch<2;++ch) {
                const double t=(blockIndex*blockSize+n)/44100.;
                audio.setSample(ch,n,static_cast<float>(
                    .075*std::sin(2.*juce::MathConstants<double>::pi*211.*t)
                    +.012*std::sin(2.*juce::MathConstants<double>::pi*4900.*t)));
            }
        const float initial=audio.getSample(0,blockSize/2);
        if(process(audio,plugin,peak,energy,count)!=0) return 3;
        if(blockIndex>5)
            difference+=std::abs(audio.getSample(0,blockSize/2)-initial);
    }
    plugin.releaseResources();
    if (!std::isfinite(peak) || peak > 8. || energy < 1.e-5 ||
        difference < 1.e-4 || count != 100*blockSize*2) {
        std::fprintf(stderr,"ERROR: native output out of bounds or unchanged\n");
        return 4;
    }
    std::printf("PASS: compiled seven-stage Alchemist native processing; peak %.5f RMS %.5f\n",
                peak,std::sqrt(energy / count));
    return 0;
}

int render(const juce::File& source, const juce::File& destination) {
    if (!source.existsAsFile() || source == destination) {
        std::fprintf(stderr,"ERROR: source missing or output equals input\n");
        return 2;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (!reader || reader->numChannels != 2 || reader->lengthInSamples < 1
        || reader->sampleRate < 8000. || reader->sampleRate > 192000.) {
        std::fprintf(stderr,"ERROR: source must be stereo WAV (8k-192kHz)\n");
        return 3;
    }
    if(destination.existsAsFile()) {
        std::fprintf(stderr,"ERROR: refusing to overwrite existing output\n");
        return 4;
    }
    std::unique_ptr<juce::FileOutputStream> fileStream(destination.createOutputStream());
    if (!fileStream || !fileStream->openedOk()) {
        std::fprintf(stderr,"ERROR: cannot create destination WAV\n");
        return 5;
    }
    std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriter::Options{}
        .withSampleRate(reader->sampleRate).withNumChannels(2)
        .withBitsPerSample(32)
        // IEEE float WAV preserves >0 dBFS samples for meaningful clipping
        // and intersample-peak diagnostics. PCM32 would silently clip them.
        .withSampleFormat(juce::AudioFormatWriter::Options::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor(stream,options);
    if(!writer) {
        destination.deleteFile();
        std::fprintf(stderr,"ERROR: cannot prepare output WAV writer\n");
        return 6;
    }
    SkellPresetAudioProcessor plugin;
    plugin.loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(plugin.loadedFactoryPresetIndex()!=skellpreset::alchemist::factoryIndex
       || plugin.slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
        std::fprintf(stderr,"ERROR: Alchemist stage order or factory selection wrong\n");
        return 7;
    }
    plugin.prepareToPlay(reader->sampleRate,blockSize);
    juce::AudioBuffer<float> audio(2,blockSize);
    double peak=0.,energy=0.;
    std::int64_t total=0;
    bool ok=true;
    for(juce::int64 pos=0;pos<reader->lengthInSamples;pos+=blockSize) {
        const auto count=static_cast<int>(
            std::min(static_cast<juce::int64>(blockSize),
                     reader->lengthInSamples-pos));
        audio.setSize(2,count,false,false,true);
        audio.clear();
        if(!reader->read(&audio,0,count,pos,true,true)) {ok=false;break;}
        if(process(audio,plugin,peak,energy,total)!=0) {ok=false;break;}
        if(!writer->writeFromAudioSampleBuffer(audio,0,count)) {ok=false;break;}
    }
    plugin.releaseResources();
    writer.reset(); // finalize RIFF size and flush outside the audio thread
    if(!ok || total<=0) {
        destination.deleteFile();
        std::fprintf(stderr,"ERROR: native render aborted; partial WAV removed\n");
        return 8;
    }
    std::printf("Rendered native Alchemist factory %d: %lld samples, %.0f Hz, peak %.5f, RMS %.5f\n",
                skellpreset::alchemist::factoryIndex,
                static_cast<long long>(total/2),reader->sampleRate,
                peak,std::sqrt(energy/total));
    std::puts("NOTE: this uses the plugin's compiled processBlock, not a hosted VST3. A/B and FL Studio checks are separate.");
    return 0;
}
} // namespace

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    if(argc==2 && juce::String(argv[1])=="--smoke")return smoke();
    if(argc!=3) {
        std::fprintf(stderr,"Usage: SkellPresetAlchemistRender INPUT_STEREO.wav NEW_OUTPUT.wav\n");
        return 1;
    }
    return render(juce::File(juce::String::fromUTF8(argv[1])),
                  juce::File(juce::String::fromUTF8(argv[2])));
}
