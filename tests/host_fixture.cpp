#include <JuceHeader.h>
#include <cmath>

// Owned, deliberately tiny hosted-effect fixture: gain x0.5, no lookahead.
class SkellHostFixtureProcessor final : public juce::AudioProcessor {
public:
    SkellHostFixtureProcessor()
        : juce::AudioProcessor(BusesProperties()
            .withInput("Input",juce::AudioChannelSet::stereo(),true)
            .withOutput("Output",juce::AudioChannelSet::stereo(),true)) {}
    void prepareToPlay(double,int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {
        // If an actual host forwards the expected transport snapshot, switch
        // to quarter gain. The Windows probe must then detect the difference
        // in audio output, not merely inspect an unattached playhead object.
        float gain=.5f;
        if(auto* playhead=getPlayHead())
            if(const auto info=playhead->getPosition()) {
                const auto bpm=info->getBpm();
                const auto samples=info->getTimeInSamples();
                const auto ppq=info->getPpqPosition();
                if(info->getIsPlaying() && bpm && samples && ppq
                   && std::abs(*bpm-96.)<1.e-8 && *samples==48240
                   && std::abs(*ppq-12.008)<1.e-8)
                    gain=.25f;
            }
        for(int ch=0;ch<buffer.getNumChannels();++ch)
            buffer.applyGain(ch,0,buffer.getNumSamples(),gain);
    }
    bool hasEditor() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}
    const juce::String getName() const override {return "Skell Host Fixture"; }
    bool acceptsMidi() const override{return false;}
    bool producesMidi() const override{return false;}
    bool isMidiEffect() const override{return false;}
    double getTailLengthSeconds() const override {return 0.;}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Half Gain";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& data) override {
        const char state[]="skell-host-fixture-1";
        data.append(state,sizeof(state));
    }
    void setStateInformation(const void*,int) override {}
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SkellHostFixtureProcessor;
}
