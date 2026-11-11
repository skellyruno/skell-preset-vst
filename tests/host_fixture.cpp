#include <JuceHeader.h>

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
        for(int ch=0;ch<buffer.getNumChannels();++ch)
            buffer.applyGain(ch,0,buffer.getNumSamples(),.5f);
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
