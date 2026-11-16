// Diagnostic-only Alchemist WAV renderer. Compiles and runs the SAME
// SkellPresetAudioProcessor::processBlock and factory chain as the VST3;
// this is not an external VST3-host or FL Studio session validation.
// Does not package, inspect or transmit the owner's WAV files.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/chain/AlchemistNativePreset.h"
#include "../Source/chain/NativeSoundMatchTrials.h"
#include "../Source/chain/ABMetrics.h"
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

// Newly added instances must start without an effect or hidden lookahead.
// A restored FL Studio state must still recall its former preset/latency.
int blankSmoke() {
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    if(plugin->loadedFactoryPresetIndex()!=-1
       || plugin->presetName()!="Empty Rack"
       || plugin->activeSlotCount()!=SkellPresetAudioProcessor::defaultSlotCount)
        return 31;
    for(int s=0;s<SkellPresetAudioProcessor::slotCount;++s) {
        const auto* active=plugin->state.getRawParameterValue(
            "s"+juce::String(s+1)+"_on");
        if(plugin->slotType(s)!=SkellPresetAudioProcessor::Empty
           || !active || active->load()>.5f)return 32;
    }
    plugin->prepareToPlay(48000.,128);
    if(plugin->getLatencySamples()!=0)return 33;
    juce::AudioBuffer<float> signal(2,128);
    juce::MidiBuffer midi;
    for(int block=0;block<8;++block) {
        for(int i=0;i<128;++i) {
            const float x=.10f*std::sin((float)(.017*(block*128+i)));
            signal.setSample(0,i,x);
            signal.setSample(1,i,-.83f*x);
        }
        juce::AudioBuffer<float> original;
        original.makeCopyOf(signal);
        plugin->processBlock(signal,midi);
        for(int ch=0;ch<2;++ch)
            for(int i=0;i<128;++i)
                if(std::abs(signal.getSample(ch,i)-original.getSample(ch,i))>1.e-6f)
                    return 34;
    }
    // Test actual state save/restore, not just loading a factory preset.
    plugin->loadFactoryPreset(0); // published legacy limiter chain
    if(plugin->slotType(6)!=SkellPresetAudioProcessor::Limiter)return 35;
    juce::MemoryBlock save;
    plugin->getStateInformation(save);
    auto restored=std::make_unique<SkellPresetAudioProcessor>();
    if(restored->loadedFactoryPresetIndex()!=-1)return 36;
    restored->setStateInformation(save.getData(),(int)save.getSize());
    restored->prepareToPlay(48000.,128);
    if(restored->loadedFactoryPresetIndex()!=0
        || restored->slotType(6)!=SkellPresetAudioProcessor::Limiter
        || restored->getLatencySamples()<=0)return 37;
    restored->releaseResources();
    plugin->releaseResources();
    std::puts("PASS: blank rack 0-PDC identity and legacy FL Studio preset-state restore");
    return 0;
}

// Saved state with corrupt slotOrder must not overwrite current rack order.
// Prior getIntValue() accepted "oops" as slot ID 0; preserve valid legacy 10
// and full 16-position permutations without touching a published parameter.
int strictSlotOrderSmoke(){
    auto owner=std::make_unique<SkellPresetAudioProcessor>();
    juce::MemoryBlock baseline;
    owner->getStateInformation(baseline);
    auto xml=juce::AudioProcessor::getXmlFromBinary(baseline.getData(),(int)baseline.getSize());
    if(!xml)return 81;
    const auto parsed=juce::ValueTree::fromXml(*xml);
    if(!parsed.isValid())return 82;
    const auto verify=[&](const juce::String& order,bool valid)->bool{
        auto document=parsed.createCopy();
        document.setProperty("slotOrder",order,nullptr);
        auto treeXml=document.createXml();
        if(!treeXml)return false;
        juce::MemoryBlock stateBytes;
        juce::AudioProcessor::copyXmlToBinary(*treeXml,stateBytes);
        auto processor=std::make_unique<SkellPresetAudioProcessor>();
        processor->reorderSlot(0,3); // [1,2,3,0,4,...], not the identity.
        if(processor->slotIdAt(0)!=1||processor->slotIdAt(3)!=0)return false;
        processor->setStateInformation(stateBytes.getData(),(int)stateBytes.getSize());
        if(!valid)return processor->slotIdAt(0)==1 &&
                         processor->slotIdAt(1)==2 &&
                         processor->slotIdAt(2)==3 &&
                         processor->slotIdAt(3)==0;
        for(int position=0;position<16;++position)
            if(processor->slotIdAt(position)!=position)return false;
        return true;
    };
    juce::StringArray natural,ten;
    for(int i=0;i<16;++i){
        natural.add(juce::String(i));
        if(i<10)ten.add(juce::String(i));
    }
    juce::StringArray malformed=natural;
    malformed.set(0,"oops"); // used to coerce to valid ID=0
    if(!verify(malformed.joinIntoString(","),false))return 83;
    malformed.set(0,"-0");
    if(!verify(malformed.joinIntoString(","),false))return 84;
    if(!verify(natural.joinIntoString(",")+",",false))return 85;
    if(!verify(","+natural.joinIntoString(","),false))return 86;
    if(!verify(natural.joinIntoString(",,"),false))return 87;
    if(!verify(natural.joinIntoString(","),true))return 88;
    if(!verify(ten.joinIntoString(","),true))return 89;
    std::puts("PASS: strict 10/16 slot-order state recall; malformed order unchanged");
    return 0;
}

// Windows desktop UI interaction/layout smoke; does NOT capture screenshots,
// trigger the screenshot workflow, or open FL Studio. Uses the real editor.
int uiSmoke() {
    auto plugin=std::make_unique<SkellPresetAudioProcessor>();
    plugin->prepareToPlay(48000.,256);
    std::unique_ptr<SkellPresetAudioProcessorEditor> editor(
        static_cast<SkellPresetAudioProcessorEditor*>(plugin->createEditor()));
    if(!editor)return 51;
    auto report=[](const juce::StringArray& issues,const char* stage) {
        for(const auto& issue:issues)
            std::fprintf(stderr,"UI %s: %s\n",stage,issue.toRawUTF8());
    };
    const auto before=editor->getScreenshotLayoutIssues();
    report(before,"initial-layout");
    if(!before.isEmpty())return 52;
    const auto actions=editor->runScreenshotInteractionTests();
    report(actions,"interactions");
    if(!actions.isEmpty())return 53;
    const auto after=editor->getScreenshotLayoutIssues();
    report(after,"final-layout");
    if(!after.isEmpty())return 54;
    editor.reset();
    plugin->releaseResources();
    std::puts("PASS: real JUCE editor geometry, rack wheel, MIC, and picker controls; no screenshot captures");
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

// Load all four append-only Studio effects in the actual JUCE processor;
// measure host-facing PDC and exercise the real Windows rack signal path.
int percentDisplaySmoke(){
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    const auto check=[&](const juce::String& id,float physical,
                         const juce::String& expected){
        auto* parameter=processor->state.getParameter(id);
        if(!parameter)return false;
        const auto shown=parameter->getText(parameter->convertTo0to1(physical),64);
        if(shown!=expected) {
            std::fprintf(stderr,"Host %s displayed '%s', expected '%s'\n",
                         id.toRawUTF8(),shown.toRawUTF8(),expected.toRawUTF8());
            return false;
        }
        return parameter->getLabel().isEmpty(); // no extra '%' appended
    };
    if(!check("input",-24.f,"0%") ||
       !check("input",0.f,"50%") ||
       !check("input",24.f,"100%") ||
       !check("output",0.f,"50%") ||
       !check("mix",1.f,"100%") ||
       !check("macro1",.37f,"37%") ||
       !check("s1_wet",.82f,"82%"))
        return 92;
    std::puts("PASS: host Input/Mix/Output, macros and rack wet map to 0-100% text");
    return 0;
}

int studioSmoke() {
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    processor->prepareToPlay(48000.,257);
    const int types[]{
        SkellPresetAudioProcessor::StudioEQFX,
        SkellPresetAudioProcessor::StudioLimiterFX,
        SkellPresetAudioProcessor::StudioCompressorFX,
        SkellPresetAudioProcessor::StudioFastDistFX
    };
    for(const int type:types){
        processor->setSlotOn(0,false);
        processor->setSlotType(0,type);
        processor->setSlotOn(0,true);
        // Set audible, deterministic non-default parameters.
        if(type==SkellPresetAudioProcessor::StudioEQFX) {
            processor->setSlotParam(0,0,.8f);
            processor->setSlotParam(0,1,.9f);
        }else if(type==SkellPresetAudioProcessor::StudioCompressorFX) {
            processor->setSlotParam(0,0,.35f);
            processor->setSlotParam(0,1,.85f);
        }else if(type==SkellPresetAudioProcessor::StudioFastDistFX) {
            processor->setSlotParam(0,0,.7f);
            processor->setSlotParam(0,3,.6f);
        }else{
            processor->setSlotParam(0,0,.89f);
            processor->setSlotParam(0,1,.1f);
            processor->setSlotParam(0,2,.35f);
            processor->setSlotParam(0,3,.85f);
        }
        const int latency=processor->getLatencySamples();
        if((type==SkellPresetAudioProcessor::StudioLimiterFX &&
            (latency<48||latency>1024)) ||
           (type!=SkellPresetAudioProcessor::StudioLimiterFX && latency!=0)) {
            std::fprintf(stderr,"Studio effect %d PDC invalid: %d\n",type,latency);
            return 72;
        }
        double peak=0.,difference=0.,energy=0.;
        for(int block=0;block<75;++block){
            juce::AudioBuffer<float> audio(2,257);
            for(int n=0;n<257;++n){
                const double t=(block*257+n)/48000.;
                const float input=(float)(.34*std::sin(2.*juce::MathConstants<double>::pi*713.*t)
                                 +.22*std::sin(2.*juce::MathConstants<double>::pi*4300.*t));
                audio.setSample(0,n,input);
                audio.setSample(1,n,input*.73f);
            }
            juce::MidiBuffer midi;
            processor->processBlock(audio,midi);
            for(int n=0;n<257;++n){
                const float out=audio.getSample(0,n);
                if(!std::isfinite(out))return 73;
                peak=std::max(peak,(double)std::abs(out));
                energy+=(double)out*out;
                if(block>20){
                    const double t=(block*257+n)/48000.;
                    const float input=(float)(.34*std::sin(2.*juce::MathConstants<double>::pi*713.*t)
                                         +.22*std::sin(2.*juce::MathConstants<double>::pi*4300.*t));
                    difference+=std::abs((double)out-(double)input);
                }
            }
        }
        if(peak>8.||energy<.0001||difference<.05){
            std::fprintf(stderr,"Studio effect %d audio validation failed: %.5f %.5f %.5f\n",
                         type,peak,energy,difference);
            return 74;
        }
        juce::MemoryBlock state;
        processor->getStateInformation(state);
        auto restored=std::make_unique<SkellPresetAudioProcessor>();
        restored->setStateInformation(state.getData(),(int)state.getSize());
        if(restored->slotType(0)!=type){
            std::fprintf(stderr,"Studio effect %d FL project state recall failed\n",type);
            return 75;
        }
        std::printf("PASS: Studio ID %d, PDC %d, peak %.4f, change %.4f, state recall\n",
                    type,latency,peak,difference);
    }
    processor->releaseResources();
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

int render(const juce::File& source, const juce::File& destination,
           int factoryIndex=skellpreset::alchemist::factoryIndex) {
    if (!source.existsAsFile() || source == destination) {
        std::fprintf(stderr,"ERROR: source missing or output equals input\n");
        return 2;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (!reader || (reader->numChannels != 1 && reader->numChannels != 2)
        || reader->lengthInSamples < 1
        || reader->sampleRate < 8000. || reader->sampleRate > 192000.) {
        std::fprintf(stderr,"ERROR: source must be mono or stereo WAV (8k-192kHz)\n");
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
    if(factoryIndex!=skellpreset::alchemist::factoryIndex &&
       factoryIndex!=skellpreset::alchemist_trial::factoryIndex &&
       factoryIndex!=skellpreset::deuce::factoryIndex &&
       factoryIndex!=skellpreset::deuce_v25::factoryIndex) {
        std::fprintf(stderr,"ERROR: factory selection must be 43, 44, 45 or 46\n");
        return 7;
    }
    plugin->loadFactoryPreset(factoryIndex);
    if(plugin->loadedFactoryPresetIndex()!=factoryIndex ||
       (factoryIndex==skellpreset::deuce::factoryIndex ||
        factoryIndex==skellpreset::deuce_v25::factoryIndex
          ?plugin->activeSlotCount()!=(factoryIndex==skellpreset::deuce::factoryIndex?12:13)
          :plugin->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX)) {
        std::fprintf(stderr,"ERROR: incorrect native factory rack loaded\n");
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
        const bool stereoInput=reader->numChannels==2;
        if(!reader->read(&audio,0,count,pos,true,stereoInput)) {ok=false;break;}
        // A mono vocal is represented by identical L/R signals throughout
        // the stereo native chain. Never leave the second channel stale.
        if(!stereoInput)audio.copyFrom(1,0,audio,0,0,count);
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
    std::printf("Rendered native factory %d: %lld samples, %.0f Hz, peak %.5f, RMS %.5f\n",
                factoryIndex,
                static_cast<long long>(total/2),reader->sampleRate,
                peak,std::sqrt(energy/total));
    std::puts("NOTE: this uses the plugin's compiled processBlock, not a hosted VST3. A/B and FL Studio checks are separate.");
    return 0;
}
// Compare the first 15 seconds of sample-aligned same-take stereo (or mono)
// WAVs. No hidden time alignment, resampling, signal modification or LUFS
// claims. Use with Alchemist render (candidate) and FL Studio wet print (ref).
int compareWavs(const juce::File& candidateFile,const juce::File& referenceFile) {
    if(candidateFile.getFileExtension().toLowerCase()!=".wav"
        || referenceFile.getFileExtension().toLowerCase()!=".wav") {
        std::fprintf(stderr,"ERROR: --compare expects two WAV files\n");
        return 23;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> candidate(
        formats.createReaderFor(candidateFile));
    std::unique_ptr<juce::AudioFormatReader> reference(
        formats.createReaderFor(referenceFile));
    if(!candidate || !reference
        || candidate->numChannels<1 || candidate->numChannels>2
        || reference->numChannels<1 || reference->numChannels>2
        || std::abs(candidate->sampleRate-reference->sampleRate)>.001
        || candidate->lengthInSamples<=0 || reference->lengthInSamples<=0) {
        std::fprintf(stderr,
            "ERROR: WAVs need equal sample rates, 1 or 2 channels, and audio\n");
        return 24;
    }
    const auto length=std::min({
        (juce::int64)candidate->lengthInSamples,
        (juce::int64)reference->lengthInSamples,
        (juce::int64)(candidate->sampleRate*15.)});
    juce::AudioBuffer<float> candidateBlock(2,2048),referenceBlock(2,2048);
    skellpreset::analysis::ComparisonAccumulator stats;
    for(juce::int64 pos=0;pos<length;pos+=2048) {
        const int count=(int)std::min((juce::int64)2048,length-pos);
        candidateBlock.clear();
        referenceBlock.clear();
        if(!candidate->read(&candidateBlock,0,count,pos,true,
                            candidate->numChannels==2)
           ||!reference->read(&referenceBlock,0,count,pos,true,
                              reference->numChannels==2)) {
            std::fprintf(stderr,"ERROR: WAV decode failed during A/B comparison\n");
            return 25;
        }
        if(candidate->numChannels==1)
            candidateBlock.copyFrom(1,0,candidateBlock,0,0,count);
        if(reference->numChannels==1)
            referenceBlock.copyFrom(1,0,referenceBlock,0,0,count);
        for(int n=0;n<count;++n)
            for(int ch=0;ch<2;++ch)
                stats.push(candidateBlock.getSample(ch,n),
                           referenceBlock.getSample(ch,n));
    }
    const auto result=stats.result();
    if(!result.hasSignal || result.rejected!=0) {
        std::fprintf(stderr,
            "ERROR: A/B signal silent, nonfinite or otherwise unusable\n");
        return 26;
    }
    std::printf("SKELL A/B first %.3f seconds at %.0f Hz\n",
                (double)length/candidate->sampleRate,candidate->sampleRate);
    std::printf("Candidate minus reference RMS: %+.3f dB\n",
                result.candidateMinusReferenceRmsDb);
    std::printf("Candidate peak: %+.3f dBFS; reference peak: %+.3f dBFS\n",
                result.candidatePeakDbfs,result.referencePeakDbfs);
    std::printf("Sample correlation: %.6f (signed); normalized level-matched "
                "RMS error: %.2f%%\n",
                result.sampleCorrelation,result.levelMatchedNrmse*100.);
    std::puts("NOTE: same performance, start sample and sample rate required.");
    std::puts("NOTE: RMS/correlation are diagnostics, not LUFS or perceptual quality.");
    return 0;
}

// Exercise real mono WAV decoding, native Alchemist processing and stereo WAV
// encoding. File cleanup is automatic even if a regression assertion fails.
int monoSmoke() {
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto input=temp.getNonexistentChildFile("SkellPresetMonoSmoke",".wav",false);
    const auto output=temp.getNonexistentChildFile("SkellPresetMonoSmokeProcessed",".wav",false);
    struct Cleanup {
        juce::File in,out;
        ~Cleanup() {in.deleteFile();out.deleteFile();}
    } cleanup{input,output};

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(input.createOutputStream());
    if(!stream)return 14;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriter::Options{}
        .withSampleRate(48000.).withNumChannels(1).withBitsPerSample(24));
    if(!writer)return 15;
    constexpr int samples=4096;
    juce::AudioBuffer<float> signal(1,samples);
    for(int n=0;n<samples;++n)
        signal.setSample(0,n,.12f*std::sin((float)(
            2.*juce::MathConstants<double>::pi*230.*n/48000.)));
    if(!writer->writeFromAudioSampleBuffer(signal,0,samples))return 16;
    writer.reset(); // Finalize WAV header before reopening input.
    if(render(input,output)!=0)return 17;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));
    if(!reader || reader->numChannels!=2 || reader->lengthInSamples!=samples)
        return 18;
    juce::AudioBuffer<float> rendered(2,samples);
    if(!reader->read(&rendered,0,samples,0,true,true))return 19;
    double energy=0.,stereoDifference=0.;
    for(int n=0;n<samples;++n) {
        const float l=rendered.getSample(0,n),r=rendered.getSample(1,n);
        if(!std::isfinite(l)||!std::isfinite(r))return 20;
        energy+=(double)l*l;
        stereoDifference+=std::abs((double)l-r);
    }
    if(energy<1.e-5 || stereoDifference>1.e-3)return 21;
    if(compareWavs(output,output)!=0)return 22;
    std::puts("PASS: mono WAV processed through full Alchemist rack to finite stereo output");
    return 0;
}

int nativeTrialsSmoke() {
    for(int factory:{skellpreset::deuce::factoryIndex,
                     skellpreset::alchemist_trial::factoryIndex,
                     skellpreset::deuce_v25::factoryIndex}) {
        auto plugin=std::make_unique<SkellPresetAudioProcessor>();
        // Recall comparison must use the SAME prepare/order for both
        // instances. Preparing the source before loading its program while
        // preparing the restored copy after recall compared different
        // smoothed initial DSP states, not actual serialization fidelity.
        plugin->loadFactoryPreset(factory);
        plugin->prepareToPlay(48000.,256);
        const int expectedCount=factory==skellpreset::deuce_v25::factoryIndex?14:
            factory==skellpreset::deuce::factoryIndex?12:10;
        if(plugin->loadedFactoryPresetIndex()!=factory ||
           plugin->activeSlotCount()!=expectedCount)return 201;
        if(factory==skellpreset::deuce::factoryIndex &&
           (plugin->slotType(6)!=SkellPresetAudioProcessor::Compressor ||
            plugin->slotType(7)!=SkellPresetAudioProcessor::DeEsser))
            return 202;
        if(factory==skellpreset::alchemist_trial::factoryIndex &&
           (plugin->slotType(6)!=SkellPresetAudioProcessor::AlchemistMatchFinishFX ||
            plugin->getSlotParam(6,12)<.75f))return 203;
        if(factory==skellpreset::deuce_v25::factoryIndex &&
           (plugin->slotType(12)!=SkellPresetAudioProcessor::EightBandToneFX ||
            std::abs(plugin->getSlotParam(12,13)-.525f)>.001f))return 208;
        juce::MemoryBlock state;
        plugin->getStateInformation(state);
        auto recalled=std::make_unique<SkellPresetAudioProcessor>();
        recalled->setStateInformation(state.getData(),(int)state.getSize());
        recalled->prepareToPlay(48000.,256);
        if(recalled->loadedFactoryPresetIndex()!=factory ||
           recalled->activeSlotCount()!=expectedCount)return 204;
        double squared=0.;
        for(int block=0;block<60;++block) {
            juce::AudioBuffer<float> signal(2,256),other(2,256);
            for(int n=0;n<256;++n) {
                const float t=(float)(block*256+n)/48000.f;
                constexpr float testPi=3.14159265358979323846f;
                const float l=.06f*std::sin(2.f*testPi*213.f*t)
                   +.02f*std::sin(2.f*testPi*6593.f*t);
                signal.setSample(0,n,l);
                signal.setSample(1,n,.87f*l);
            }
            other.makeCopyOf(signal);
            juce::MidiBuffer midi,otherMidi;
            plugin->processBlock(signal,midi);
            recalled->processBlock(other,otherMidi);
            for(int ch=0;ch<2;++ch)for(int n=0;n<256;++n) {
                const float value=signal.getSample(ch,n);
                const float saved=other.getSample(ch,n);
                if(!std::isfinite(value) || !std::isfinite(saved) ||
                   std::abs(value)>8.f ||
                   std::abs(value-saved)>1.e-3f) {
                    std::fprintf(stderr,
                      "Factory %d recall output mismatch block %d ch %d n %d: %.7f vs %.7f\n",
                      factory,block,ch,n,(double)value,(double)saved);
                    return 205;
                }
                squared+=(double)value*value;
            }
        }
        if(squared<1.e-6)return 206;
    }
    auto protectedPreset=std::make_unique<SkellPresetAudioProcessor>();
    protectedPreset->loadFactoryPreset(skellpreset::alchemist::factoryIndex);
    if(protectedPreset->getSlotParam(6,12)!=.5f)return 207;
    std::puts("PASS: Deuce and Alchemist trial recall, 12-slot rack, 0-NaN stereo, legacy bypass");
    return 0;
}

} // namespace

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    if(argc==2 && juce::String(argv[1])=="--smoke")return smoke();
    if(argc==2 && juce::String(argv[1])=="--smoke-blank")return blankSmoke();
    if(argc==2 && juce::String(argv[1])=="--smoke-slot-order")return strictSlotOrderSmoke();
    if(argc==2 && juce::String(argv[1])=="--ui-smoke")return uiSmoke();
    if(argc==2 && juce::String(argv[1])=="--studio-smoke")return studioSmoke();
    if(argc==2 && juce::String(argv[1])=="--native-trials-smoke")return nativeTrialsSmoke();
    if(argc==2 && juce::String(argv[1])=="--percent-smoke")return percentDisplaySmoke();
    if(argc==2 && juce::String(argv[1])=="--stress")return stress();
    if(argc==2 && juce::String(argv[1])=="--smoke-mono")return monoSmoke();
    if(argc==4 && juce::String(argv[1])=="--compare")
        return compareWavs(juce::File(juce::String::fromUTF8(argv[2])),
                           juce::File(juce::String::fromUTF8(argv[3])));
    if(argc==5 && juce::String(argv[1])=="--factory") {
        const auto requested=juce::String(argv[2]);
        if(requested!="43" && requested!="44" && requested!="45" && requested!="46")return 7;
        return render(juce::File(juce::String::fromUTF8(argv[3])),
                      juce::File(juce::String::fromUTF8(argv[4])),
                      requested.getIntValue());
    }
    if(argc!=3) {
        std::fprintf(stderr,"Usage: SkellPresetAlchemistRender --factory 43|44|45|46 INPUT.wav NEW_OUTPUT.wav\n");
        std::fprintf(stderr,"       SkellPresetAlchemistRender --compare CANDIDATE.wav REFERENCE.wav\n");
        return 1;
    }
    return render(juce::File(juce::String::fromUTF8(argv[1])),
                  juce::File(juce::String::fromUTF8(argv[2])));
}
