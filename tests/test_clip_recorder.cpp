// Integration smoke test: own temp WAV capture, no DAW and no microphone.
// Test is run from JUCE/Windows and verifies real RIFF data, finalization,
// channel count, sample-rate and no drops at normal callback sizes.
#include <JuceHeader.h>
#include "../Source/chain/ClipRecorder.h"
#include <array>
#include <cassert>
#include <cmath>
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto target=folder.getNonexistentChildFile("SkellPresetClipTest",".wav",false);
    skellpreset::capture::ClipRecorder recorder;
    assert(recorder.start(target,48000.).wasOk());
    assert(recorder.isRecording());
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
    assert(!recorder.isRecording());
    assert(recorder.droppedFrames()==0);
    assert(recorder.framesWritten()==180ull*256);
    assert(target.existsAsFile() && target.getSize()>1024);

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(target));
    assert(reader!=nullptr);
    assert(reader->sampleRate==48000.);
    assert(reader->numChannels==2);
    assert(reader->bitsPerSample==24);
    assert(reader->lengthInSamples==180ll*256);
    juce::AudioBuffer<float> verify(2,512);
    assert(reader->read(&verify,0,512,0,true,true));
    float energy=0.f;
    for(int i=0;i<512;++i) {
        assert(std::isfinite(verify.getSample(0,i)));
        assert(std::isfinite(verify.getSample(1,i)));
        energy+=std::abs(verify.getSample(0,i));
    }
    assert(energy>10.f);
    reader.reset();
    target.deleteFile();
    return 0;
}
