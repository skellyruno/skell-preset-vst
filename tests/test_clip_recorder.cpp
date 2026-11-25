// Integration smoke test: own temp WAV capture, no DAW and no microphone.
// Test is run from JUCE/Windows and verifies real RIFF data, finalization,
// channel count, sample-rate and no drops at normal callback sizes.
#include <JuceHeader.h>
#include "../Source/chain/ClipRecorder.h"
#include <array>
#include <cstdio>
#include <limits>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)
#include <cmath>
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto target=folder.getNonexistentChildFile("SkellPresetClipTest",".wav",false);
    skellpreset::capture::ClipRecorder recorder;
    SKELL_REQUIRE(recorder.start(target,48000.).wasOk());
    SKELL_REQUIRE(recorder.isRecording());
    SKELL_REQUIRE(recorder.start(target,48000.).failed());
    SKELL_REQUIRE(recorder.isRecording());
    SKELL_REQUIRE(recorder.start(target.withFileExtension(".txt"),48000.).failed());
    SKELL_REQUIRE(recorder.start(target,std::numeric_limits<double>::quiet_NaN()).failed());
    SKELL_REQUIRE(recorder.isRecording());
    std::array<float,256> left{},right{};
    for(int block=0;block<180;++block) {
        for(int i=0;i<256;++i) {
            const auto phase=(float)(block*256+i)*.039f;
            left[(size_t)i]=.22f*std::sin(phase);
            right[(size_t)i]=.19f*std::sin(phase*.93f);
        }
        recorder.push(left.data(),right.data(),256);
    }
    recorder.stop();
    SKELL_REQUIRE(!recorder.isRecording());
    SKELL_REQUIRE(recorder.droppedFrames()==0);
    SKELL_REQUIRE(recorder.framesWritten()==180ull*256);
    SKELL_REQUIRE(std::abs(recorder.secondsWritten()-.96)<1.e-8);
    recorder.stop(); // repeat stop is safe and does not erase duration
    SKELL_REQUIRE(std::abs(recorder.secondsWritten()-.96)<1.e-8);
    SKELL_REQUIRE(target.existsAsFile() && target.getSize()>1024);

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(target));
    SKELL_REQUIRE(reader!=nullptr);
    SKELL_REQUIRE(reader->sampleRate==48000.);
    SKELL_REQUIRE(reader->numChannels==2);
    SKELL_REQUIRE(reader->bitsPerSample==24);
    SKELL_REQUIRE(reader->lengthInSamples==180ll*256);
    juce::AudioBuffer<float> verify(2,512);
    SKELL_REQUIRE(reader->read(&verify,0,512,0,true,true));
    float energy=0.f;
    for(int i=0;i<512;++i) {
        SKELL_REQUIRE(std::isfinite(verify.getSample(0,i)));
        SKELL_REQUIRE(std::isfinite(verify.getSample(1,i)));
        energy+=std::abs(verify.getSample(0,i));
    }
    SKELL_REQUIRE(energy>10.f);
    reader.reset();
    const auto bytes=target.getSize();
    SKELL_REQUIRE(recorder.start(target,48000.).failed());
    SKELL_REQUIRE(target.getSize()==bytes);
    const auto mono=folder.getNonexistentChildFile("SkellPresetMonoTest",".wav",false);
    SKELL_REQUIRE(recorder.start(mono,44100.).wasOk());
    left.fill(.125f);
    recorder.push(nullptr,nullptr,256);
    recorder.push(left.data(),nullptr,0);
    recorder.push(left.data(),nullptr,256);
    recorder.stop();
    recorder.push(left.data(),nullptr,256);
    SKELL_REQUIRE(recorder.framesWritten()==256 && recorder.droppedFrames()==0);
    reader.reset(manager.createReaderFor(mono));
    SKELL_REQUIRE(reader!=nullptr && reader->lengthInSamples==256 &&
                  reader->sampleRate==44100. && reader->numChannels==2);
    SKELL_REQUIRE(reader->read(&verify,0,256,0,true,true));
    for(int i=0;i<256;++i){
        SKELL_REQUIRE(std::abs(verify.getSample(0,i)-.125f)<1.e-5f);
        SKELL_REQUIRE(verify.getSample(0,i)==verify.getSample(1,i));
    }
    reader.reset();
    mono.deleteFile();
    target.deleteFile();
    return 0;
}
