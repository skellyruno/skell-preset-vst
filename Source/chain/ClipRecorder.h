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
        // start/stop can originate from GUI or host preparation. Serialize
        // control operations only; the callback never acquires this lock.
        const juce::ScopedLock lock(controlLock);
        if(!std::isfinite(sampleRate)||sampleRate<8000.||sampleRate>192000.)
            return juce::Result::fail("Invalid host sample rate");
        if(path.getFullPathName().isEmpty()||path.getFileExtension().toLowerCase()!=".wav")
            return juce::Result::fail("Choose a .wav file");
        if(!path.getParentDirectory().isDirectory())
            return juce::Result::fail("Recording folder does not exist");
        // JUCE's output stream can append to an existing file. Never
        // damage an earlier recording, even if a chooser supplied its name.
        if(path.exists())return juce::Result::fail("Recording already exists; choose a new WAV filename");
        stopLocked();
        std::unique_ptr<juce::FileOutputStream> fileStream(path.createOutputStream());
        if(!fileStream||!fileStream->openedOk())
            return juce::Result::fail("Cannot create recording file");
        std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
        juce::WavAudioFormat wav;
        const auto options=juce::AudioFormatWriter::Options{}
            .withSampleRate(sampleRate).withNumChannels(2)
            .withBitsPerSample(24);
        auto next=wav.createWriterFor(stream,options);
        if(!next){
            stream.reset();
            path.deleteFile(); // newly created destination only
            return juce::Result::fail("Cannot prepare 24-bit WAV writer");
        }
        // The new JUCE 8 writer API transfers stream ownership only on
        // success. No leaked/double-deleted OutputStream is possible.
        writer=std::move(next);
        const int frames=(int)std::clamp(sampleRate*4.,32768.,768000.);
        fifo.setTotalSize(frames);
        fifo.reset();
        ring.setSize(2,frames,false,true,true);
        scratch.setSize(2,2048,false,true,true);
        written.store(0,std::memory_order_relaxed);
        dropped.store(0,std::memory_order_relaxed);
        sampleRateHz.store(sampleRate,std::memory_order_release);
        if(!startThread()){
            writer.reset();
            path.deleteFile();
            sampleRateHz.store(0.,std::memory_order_release);
            return juce::Result::fail("Cannot start WAV writer thread");
        }
        // A unique token prevents a delayed old callback from being
        // admitted to a different recording after stop/start.
        if(++sessionSerial==0)++sessionSerial;
        activeSession.store(sessionSerial,std::memory_order_seq_cst);
        return juce::Result::ok();
    }

    void stop() {
        const juce::ScopedLock lock(controlLock);
        stopLocked();
    }
private:
    void stopLocked() {
        activeSession.store(0,std::memory_order_seq_cst);
        // A callback may have passed the initial armed check. Do not delete
        // ring/writer until every in-flight producer has finished writing.
        while(inFlight.load(std::memory_order_seq_cst)!=0)
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
        // Retain duration and counters of the completed take until next start.
    }

public:
    // Realtime callback: no allocations, sleeps, locks, disk access, or host
    // operations. Excess audio is dropped rather than blocking the DAW.
    void push(const float* left,const float* right,int frames) noexcept {
        if(!left||frames<=0)return;
        const auto session=activeSession.load(std::memory_order_seq_cst);
        if(session==0)return;
        inFlight.fetch_add(1,std::memory_order_seq_cst);
        if(activeSession.load(std::memory_order_seq_cst)!=session){
            inFlight.fetch_sub(1,std::memory_order_seq_cst);return;
        }
        int remain=frames,offset=0;
        while(remain>0) {
            const int ready=std::min(remain,fifo.getFreeSpace());
            if(ready<=0)break;
            int start1=0,size1=0,start2=0,size2=0;
            fifo.prepareToWrite(ready,start1,size1,start2,size2);
            if(size1+size2<=0)break;
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
        inFlight.fetch_sub(1,std::memory_order_seq_cst);
    }

    bool isRecording() const noexcept {return activeSession.load(std::memory_order_acquire)!=0;}
    std::uint64_t framesWritten() const noexcept {return written.load(std::memory_order_relaxed);}
    std::uint64_t droppedFrames() const noexcept {return dropped.load(std::memory_order_relaxed);}
    double secondsWritten() const noexcept {
        const double hz=sampleRateHz.load(std::memory_order_acquire);
        return hz>0.?static_cast<double>(framesWritten())/hz:0.;
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
                activeSession.store(0,std::memory_order_seq_cst);
                // Count the entire unwritten queue after admitted producers
                // finish; a disk error must not silently hide lost frames.
                while(inFlight.load(std::memory_order_seq_cst)!=0)juce::Thread::yield();
                dropped.fetch_add(static_cast<std::uint64_t>(fifo.getNumReady()),
                    std::memory_order_relaxed);
                break;
            }
            written.fetch_add((std::uint64_t)count,std::memory_order_relaxed);
        }
        writer.reset(); // Finalize WAV header on worker, outside audio thread.
    }
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> ring,scratch;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::CriticalSection controlLock; // never used by push() or run()
    std::uint64_t sessionSerial=0; // control lock only
    std::atomic<std::uint64_t> activeSession{0};
    std::atomic<int> inFlight{0};
    std::atomic<std::uint64_t> written{0},dropped{0};
    std::atomic<double> sampleRateHz{0.};
};
} // namespace skellpreset::capture
