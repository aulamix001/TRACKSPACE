#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpectralEngine.h"

class TrackspaceProcessor final : public juce::AudioProcessor {
public:
    TrackspaceProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TRACKSPACE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorValueTreeState parameters;
    trackspace::SpectralEngine engine;
    std::atomic<int> editorSize{500};
    std::atomic<bool> advancedPanel{false}, sidechainConnected{false};
private:
    void process(juce::AudioBuffer<float>&, bool forceBypass);
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    std::array<std::atomic<float>*, 11> values{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackspaceProcessor)
};
