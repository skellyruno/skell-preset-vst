#pragma once
// Post-FX WAV capture for SkellPreset. Inspired by 322 DAW's arm/record/clip
// workflow, but adapted to a host-owned VST3 stream: no browser microphone,
// WebM/Opus, invented waveform, or disk I/O on the audio callback.
// One SPSC lock-free AbstractFifo transfers interleaved stereo blocks to a
// dedicated background WAV writer thread. The original DSP processor remains.
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skellpreset::capture {
class ClipRecorder final : private juce::Thread {
public:
    ClipRecorder() : juce::Thread("SkellPreset WAV Capture"), fifo(1) {}
    ~ClipRecorder() override {stop();}
    ClipRecorder(const ClipRecorder&)=delete;
    ClipRecorder& operator=(const ClipRecorder&)=delete;

    juce::Result start(const juce::File& path,double sampleRate) {
        stop(); // Message thread; never called by processBlock().
        if(!std::isfinite(sampleRate)||sampleRate<8000.||sampleRate>192000.)
            return juce::Result::fail("Invalid host sample rate");
        if(path.getFullPathName().isEmpty()||path.getFileExtension().toLowerCase()!=".wav")
            return juce::Result::fail("Choose a .wav file");
        if(!path.getParentDirectory().isDirectory())
            return juce::Result::fail("Recording folder does not exist");
        std::unique_ptr<juce::FileOutputStream> stream(path.createOutputStream());
        if(!stream||!stream->openedOk())
            return juce::Result::fail("Cannot create recording file");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> next(
            wav.createWriterFor(stream.get(),sampleRate,2,24,{},0));
        if(!next)return juce::Result::fail("Cannot prepare 24-bit WAV writer");
        stream.release(); // JUCE AudioFormatWriter owns and deletes the stream.
        writer=std::move(next);
        const int frames=(int)std::clamp(sampleRate*4.,32768.,768000.);
        fifo.setTotalSize(frames);
        ring.setSize(2,frames,false,true,true);
        scratch.setSize(2,2048,false,true,true);
        written.store(0,std::memory_order_relaxed);
        dropped.store(0,std::memory_order_relaxed);
        sampleRateHz=sampleRate;
        recording.store(true,std::memory_order_release);
        startThread();
        return juce::Result::ok();
    }

    void stop() {
        recording.store(false,std::memory_order_release);
        // A callback may have passed the initial armed check. Do not delete
        // ring/writer until every in-flight producer has finished writing.
        while(inFlight.load(std::memory_order_acquire)!=0)
            juce::Thread::yield();
        signalThreadShouldExit();
        if(isThreadRunning()) {
            // Wait until pending FIFO is drained and WAV header finalized.
            // Never forcibly terminate a WAV writer while it owns the file.
            // If a slow disk delays the flush, keeping the data intact is
            // safer than killing the worker halfway through the RIFF header.
            waitForThreadToExit(-1);
        }
        writer.reset();
        sampleRateHz=0.;
    }

    // Realtime callback: no allocations, sleeps, locks, disk access, or host
    // operations. Excess audio is dropped rather than blocking the DAW.
    void push(const float* left,const float* right,int frames) noexcept {
        if(!recording.load(std::memory_order_acquire)||!left||frames<=0)return;
        inFlight.fetch_add(1,std::memory_order_acq_rel);
        if(!recording.load(std::memory_order_acquire)){
            inFlight.fetch_sub(1,std::memory_order_release);return;
        }
        int remain=frames,offset=0;
        while(remain>0) {
            const int ready=std::min(remain,fifo.getFreeSpace());
            if(ready<=0)break;
            int start1=0,size1=0,start2=0,size2=0;
            fifo.prepareToWrite(ready,start1,size1,start2,size2);
            if(size1>0){
                ring.copyFrom(0,start1,left+offset,size1);
                ring.copyFrom(1,start1,(right?right:left)+offset,size1);
            }
            if(size2>0){
                ring.copyFrom(0,start2,left+offset+size1,size2);
                ring.copyFrom(1,start2,(right?right:left)+offset+size1,size2);
            }
            fifo.finishedWrite(size1+size2);
            offset+=size1+size2;remain-=size1+size2;
        }
        if(remain>0)dropped.fetch_add((std::uint64_t)remain,std::memory_order_relaxed);
        inFlight.fetch_sub(1,std::memory_order_release);
    }

    bool isRecording() const noexcept {return recording.load(std::memory_order_acquire);}
    std::uint64_t framesWritten() const noexcept {return written.load(std::memory_order_relaxed);}
    std::uint64_t droppedFrames() const noexcept {return dropped.load(std::memory_order_relaxed);}
    double secondsWritten() const noexcept {
        return sampleRateHz>0.?framesWritten()/sampleRateHz:0.;
    }

private:
    void run() override {
        for(;;){
            int pos1=0,count1=0,pos2=0,count2=0;
            fifo.prepareToRead(2048,pos1,count1,pos2,count2);
            const int count=count1+count2;
            if(count==0){
                if(threadShouldExit())break;
                juce::Thread::sleep(3);continue;
            }
            if(count1>0){
                scratch.copyFrom(0,0,ring,0,pos1,count1);
                scratch.copyFrom(1,0,ring,1,pos1,count1);
            }
            if(count2>0){
                scratch.copyFrom(0,count1,ring,0,pos2,count2);
                scratch.copyFrom(1,count1,ring,1,pos2,count2);
            }
            const bool ok=writer&&writer->writeFromAudioSampleBuffer(scratch,0,count);
            fifo.finishedRead(count);
            if(!ok){
                dropped.fetch_add((std::uint64_t)count,std::memory_order_relaxed);
                recording.store(false,std::memory_order_release);
                break;
            }
            written.fetch_add((std::uint64_t)count,std::memory_order_relaxed);
        }
        writer.reset(); // Finalize WAV header on worker, outside audio thread.
    }
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> ring,scratch;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::atomic<bool> recording{false};
    std::atomic<int> inFlight{0};
    std::atomic<std::uint64_t> written{0},dropped{0};
    double sampleRateHz=0.;
};
} // namespace skellpreset::capture
