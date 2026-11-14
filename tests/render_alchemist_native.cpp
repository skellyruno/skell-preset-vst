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
#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>

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
    // Large rack engines must be allocated on the heap: Windows main thread
    // defaults to a substantially smaller stack than the Linux renderer.
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if (plugin->loadedFactoryPresetIndex() != skellpreset::alchemist::factoryIndex
        || plugin->slotType(6) != SkellPresetAudioProcessor::AlchemistMatchFinishFX
        || plugin->activeSlotCount() < 7) {
        std::fprintf(stderr,"ERROR: native Alchemist factory chain did not load\n");
        return 2;
    }
    plugin->prepareToPlay(44100.,blockSize);
    // This seven-stage factory has no limiter, spectral pitch, or hosted
    // processor; Match Finish itself must not introduce host PDC.
    if(plugin->getLatencySamples()!=0) {
        std::fprintf(stderr,"ERROR: Alchemist unexpectedly reports %d latency samples\\n",
                     plugin->getLatencySamples());
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
        if(process(audio,*plugin,peak,energy,count)!=0) return 3;
        if(blockIndex>5)
            difference+=std::abs(audio.getSample(0,blockSize/2)-initial);
    }
    plugin->releaseResources();
    if (!std::isfinite(peak) || peak > 8. || energy < 1.e-5 ||
        difference < 1.e-4 || count != 100*blockSize*2) {
        std::fprintf(stderr,"ERROR: native output out of bounds or unchanged\n");
        return 4;
    }
    std::printf("PASS: compiled seven-stage Alchemist native processing; peak %.5f RMS %.5f\n",
                peak,std::sqrt(energy / count));
    return 0;
}

// End-to-end native processor workload: not a simulated individual effect.
// Measures serialized callbacks for several plugin instances sharing one audio
// device thread. Hosted CI machine timings are NOT FL Studio CPU results.
int stress() {
    const int instances[]{5,10,20};
    const int buffers[]{64,128,256};
    const double rates[]{44100.,48000.,96000.};
    for(const double sr:rates) {
        // Check actual host-facing processor PDC, not a helper's estimate.
        for(const int preset:{skellpreset::alchemist::factoryIndex,25,0}) {
            auto p=std::make_unique<SkellPresetAudioProcessor>();
            p->loadFactoryPreset(preset);
            p->prepareToPlay(sr,64);
            const int expected=preset==0?std::max(48,(int)(sr*.003)):0;
            if(p->getLatencySamples()!=expected) {
                std::fprintf(stderr,
                    "FAIL factory %d / %.0f Hz: reported=%d expected=%d samples\n",
                    preset,sr,p->getLatencySamples(),expected);
                return 9;
            }
            p->releaseResources();
        }
        for(const int frames:buffers) {
            for(const int count:instances) {
                // Construct/prepare and allocate audio buffers outside timing.
                std::vector<std::unique_ptr<SkellPresetAudioProcessor>> plugins;
                std::vector<std::unique_ptr<juce::AudioBuffer<float>>> work;
                plugins.reserve((size_t)count);
                work.reserve((size_t)count);
                for(int n=0;n<count;++n) {
                    auto p=std::make_unique<SkellPresetAudioProcessor>();
                    p->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
                    p->prepareToPlay(sr,frames);
                    if(p->getLatencySamples()!=0)return 10;
                    plugins.push_back(std::move(p));
                    work.push_back(std::make_unique<juce::AudioBuffer<float>>(2,frames));
                }
                juce::AudioBuffer<float> source(2,frames);
                juce::MidiBuffer midi;
                const int blocks=std::max(1,(int)(sr*.20/frames));
                double peak=0.,energy=0.,dspSeconds=0.,worstCallback=0.;
                for(int k=0;k<blocks;++k) {
                    // Generate once per device callback, outside DSP timing.
                    // Source contains vocals, upper harmonics, transients,
                    // silence windows and deliberately invalid input.
                    for(int i=0;i<frames;++i) {
                        const auto n=(std::int64_t)k*frames+i;
                        const double t=(double)n/sr;
                        const float x=(float)(
                            .12*std::sin(2.*juce::MathConstants<double>::pi*207.*t)
                            +.035*std::sin(2.*juce::MathConstants<double>::pi*6100.*t)
                            +((n%1223)==0?.21:0.));
                        const float y=(n%7117)==0
                            ?std::numeric_limits<float>::quiet_NaN()
                            :(n%9001)==0?std::numeric_limits<float>::infinity():x;
                        source.setSample(0,i,y);
                        source.setSample(1,i,((n/500)%3)==0?-y:y);
                    }
                    for(int inst=0;inst<count;++inst) {
                        auto& dst=*work[(size_t)inst];
                        dst.copyFrom(0,0,source,0,0,frames);
                        dst.copyFrom(1,0,source,1,0,frames);
                    }
                    // Measure ONLY processBlock() calls, not test-signal
                    // synthesis, buffer copies, assertions or metrics.
                    const auto callbackStart=std::chrono::steady_clock::now();
                    for(int inst=0;inst<count;++inst)
                        plugins[(size_t)inst]->processBlock(*work[(size_t)inst],midi);
                    const double elapsed=std::chrono::duration<double>(
                        std::chrono::steady_clock::now()-callbackStart).count();
                    dspSeconds+=elapsed;
                    worstCallback=std::max(worstCallback,elapsed);
                    for(int inst=0;inst<count;++inst) {
                        const auto& dst=*work[(size_t)inst];
                        for(int i=0;i<frames;++i) {
                            for(int ch=0;ch<2;++ch) {
                                const double value=dst.getSample(ch,i);
                                if(!std::isfinite(value) || std::abs(value)>32.) {
                                    std::fprintf(stderr,
                                        "FAIL nonfinite/unbounded count=%d rate=%.0f block=%d sample=%d\n",
                                        count,sr,frames,i);
                                    return 11;
                                }
                                peak=std::max(peak,std::abs(value));
                                energy+=value*value;
                            }
                        }
                    }
                }
                const double audioSeconds=(double)(blocks*frames)/sr;
                const double callbackBudget=(double)frames/sr;
                std::printf("REAL-RACK instances=%d rate=%.0f block=%d audio_s=%.3f dsp_s=%.3f "
                            "dsp_cpu_vs_realtime=%.3f worst_callback_vs_budget=%.3f "
                            "peak=%.4f RMS=%.4f\n",
                            count,sr,frames,audioSeconds,dspSeconds,
                            dspSeconds/audioSeconds,worstCallback/callbackBudget,peak,
                            std::sqrt(energy/(2.*blocks*frames*count)));
                if(!std::isfinite(energy) || energy<=0. || peak<=0.)return 12;
                for(auto& p:plugins)p->releaseResources();
            }
        }
    }
    std::puts("PASS compiled 5/10/20-instance full rack finite-output and latency stress");
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
    // Large rack engines must be allocated on the heap: Windows main thread
    // defaults to a substantially smaller stack than the Linux renderer.
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(plugin->loadedFactoryPresetIndex()!=skellpreset::alchemist::factoryIndex
       || plugin->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX) {
        std::fprintf(stderr,"ERROR: Alchemist stage order or factory selection wrong\n");
        return 7;
    }
    plugin->prepareToPlay(reader->sampleRate,blockSize);
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
        if(process(audio,*plugin,peak,energy,total)!=0) {ok=false;break;}
        if(!writer->writeFromAudioSampleBuffer(audio,0,count)) {ok=false;break;}
    }
    plugin->releaseResources();
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
    if(argc==2 && juce::String(argv[1])=="--stress")return stress();
    if(argc!=3) {
        std::fprintf(stderr,"Usage: SkellPresetAlchemistRender INPUT_STEREO.wav NEW_OUTPUT.wav\n");
        return 1;
    }
    return render(juce::File(juce::String::fromUTF8(argv[1])),
                  juce::File(juce::String::fromUTF8(argv[2])));
}
