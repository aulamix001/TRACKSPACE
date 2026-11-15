#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout TrackspaceProcessor::makeParameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&p](const char* id, const char* name, float lo, float hi, float value,
                    float centre, const char* unit) {
        juce::NormalisableRange<float> range(lo, hi);
        if (centre > lo && centre < hi) range.setSkewForCentre(centre);
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, range, value,
            juce::AudioParameterFloatAttributes().withLabel(unit)
                .withStringFromValueFunction([unit](float v, int) {
                    if (juce::String(unit) == "Hz" && v >= 1000) return juce::String(v / 1000, 2) + " kHz";
                    return juce::String(v, v < 10 ? 2 : 1) + " " + unit;
                }).withValueFromStringFunction([](const juce::String& text) {
                    auto v = text.getFloatValue();
                    return text.containsIgnoreCase("k") ? v * 1000 : v;
                })));
    };
    add("amount", "Amount", 0, 100, 50, 50, "%");
    add("lowCut", "Low-cut", 20, 20000, 20, 632.4555f, "Hz");
    add("highCut", "High-cut", 20, 20000, 20000, 632.4555f, "Hz");
    add("attack", "Attack", .01f, 100, 10, 1, "ms");
    add("release", "Release", .01f, 1000, 100, 10, "ms");
    add("pan", "Pan / Mid-Side balance", -1, 1, 0, 0, "");
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Processing mode", juce::StringArray{"L/R", "M/S"}, 0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"freeze", 1}, "Freeze", false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"listen", 1}, "Sidechain listen", false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delta", 1}, "Delta monitor", false));
    return p;
}
TrackspaceProcessor::TrackspaceProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "TRACKSPACE_STATE", makeParameters()) {
    constexpr const char* ids[]{"amount", "lowCut", "highCut", "attack", "release", "pan", "mode", "freeze", "listen", "bypass", "delta"};
    for (size_t i = 0; i < values.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
}
bool TrackspaceProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto main = layout.getMainInputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo()) return false;
    if (layout.getMainOutputChannelSet() != main || layout.inputBuses.size() != 2) return false;
    const auto side = layout.getChannelSet(true, 1);
    return side.isDisabled() || side == juce::AudioChannelSet::mono() || side == juce::AudioChannelSet::stereo();
}
void TrackspaceProcessor::prepareToPlay(double sr, int) { engine.prepare(sr); setLatencySamples(0); }
void TrackspaceProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process(b, false); }
void TrackspaceProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process(b, true); }
void TrackspaceProcessor::process(juce::AudioBuffer<float>& buffer, bool forceBypass) {
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer(buffer, false, 0);
    if (main.getNumChannels() == 0) return;
    auto side = getBusBuffer(buffer, true, 1);
    const auto scChannels = side.getNumChannels();
    sidechainConnected.store(scChannels > 0, std::memory_order_relaxed);
    auto read = [this](size_t i) { return values[i]->load(std::memory_order_relaxed); };
    trackspace::Settings s;
    s.amount = read(0); s.lowCut = read(1); s.highCut = read(2);
    s.attackMs = read(3); s.releaseMs = read(4); s.pan = read(5);
    s.midSide = read(6) > .5f; s.freeze = read(7) > .5f; s.listen = read(8) > .5f;
    s.bypass = forceBypass || read(9) > .5f;
    // Delta monitoring uses fixed-size stack storage; arbitrary host block sizes are chunked.
    constexpr int chunkSize = 256;
    std::array<float, chunkSize> dryL{}, dryR{};
    const bool delta = read(10) > .5f && !s.bypass && !s.listen;
    for (int offset = 0; offset < main.getNumSamples(); offset += chunkSize) {
        const auto count = std::min(chunkSize, main.getNumSamples() - offset);
        auto* l = main.getWritePointer(0, offset);
        auto* r = main.getNumChannels() > 1 ? main.getWritePointer(1, offset) : nullptr;
        if (delta) { std::copy_n(l, count, dryL.data()); if (r) std::copy_n(r, count, dryR.data()); }
        engine.process(l, r, scChannels > 0 ? side.getReadPointer(0, offset) : nullptr,
                       scChannels > 1 ? side.getReadPointer(1, offset) : nullptr, count, s);
        if (delta) for (int n = 0; n < count; ++n) { l[n] = dryL[n] - l[n]; if (r) r[n] = dryR[n] - r[n]; }
    }
}
juce::AudioProcessorParameter* TrackspaceProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
void TrackspaceProcessor::getStateInformation(juce::MemoryBlock& out) {
    auto state = parameters.copyState();
    state.setProperty("editorSize", editorSize.load(), nullptr);
    state.setProperty("advancedPanel", advancedPanel.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, out);
}
void TrackspaceProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        if (!xml->hasTagName(parameters.state.getType())) return;
        auto state = juce::ValueTree::fromXml(*xml);
        editorSize.store(juce::jlimit(360, 800, static_cast<int>(state.getProperty("editorSize", 500))));
        advancedPanel.store(static_cast<bool>(state.getProperty("advancedPanel", false)));
        parameters.replaceState(state);
    }
}
juce::AudioProcessorEditor* TrackspaceProcessor::createEditor() { return new TrackspaceEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TrackspaceProcessor(); }
