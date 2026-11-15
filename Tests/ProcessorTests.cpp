#include "PluginProcessor.h"
#include <iostream>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    int failures = 0;
    auto check = [&failures](bool ok, const char* name) { std::cout << (ok ? "PASS " : "FAIL ") << name << '\n'; failures += !ok; };
    TrackspaceProcessor p;
    check(p.getTotalNumOutputChannels() == 2, "Stereo output bus");
    auto layout = p.getBusesLayout(); layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    check(p.setBusesLayout(layout), "Host can enable stereo external sidechain");
    p.prepareToPlay(48000, 512);
    check(p.getLatencySamples() == 0, "No reported audio delay");
    juce::AudioBuffer<float> buffer(4, 512); juce::MidiBuffer midi;
    double inputPower = 0, outputPower = 0;
    for (int block = 0; block < 100; ++block) {
        for (int n = 0; n < 512; ++n) {
            const auto x = .3f * std::sin(juce::MathConstants<double>::twoPi * 1000 * (block * 512 + n) / 48000);
            for (int c = 0; c < 4; ++c) buffer.setSample(c, n, static_cast<float>(x));
            if (block > 80) inputPower += x * x;
        }
        p.processBlock(buffer, midi);
        if (block > 80) for (int n = 0; n < 512; ++n) outputPower += buffer.getSample(0, n) * buffer.getSample(0, n);
    }
    check(outputPower < inputPower * .2, "Host aux bus drives spectral reduction");
    check(p.getParameters().size() == 11, "All eleven controls exposed for host automation");
    auto* amount = p.parameters.getParameter("amount"); amount->setValueNotifyingHost(.73f);
    p.editorSize.store(400); p.advancedPanel.store(true);
    juce::MemoryBlock state; p.getStateInformation(state);
    TrackspaceProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(std::abs(restored.parameters.getRawParameterValue("amount")->load() - 73) < .01f, "Parameter state round-trip");
    check(restored.editorSize.load() == 400 && restored.advancedPanel.load(), "Editor size and panel restored");
    auto mono = restored.getBusesLayout(); mono.inputBuses.set(0, juce::AudioChannelSet::mono());
    mono.outputBuses.set(0, juce::AudioChannelSet::mono()); mono.inputBuses.set(1, juce::AudioChannelSet::mono());
    check(restored.setBusesLayout(mono), "Mono main / mono sidechain layout");
    restored.prepareToPlay(44100, 127); buffer.setSize(2, 127); buffer.clear(); restored.processBlock(buffer, midi);
    check(buffer.getMagnitude(0, 127) == 0, "Silent mono session stays silent");
    restored.setStateInformation("invalid", 7);
    check(restored.editorSize.load() == 400, "Invalid state rejected safely");
    p.advancedPanel.store(false);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    check(editor->getWidth() <= 500 && editor->getWidth() == editor->getHeight(), "Compact square editor");
    auto* slider = dynamic_cast<juce::Slider*>(editor->findChildWithID("amount"));
    if (slider) slider->setValue(61, juce::sendNotificationSync);
    check(slider && std::abs(p.parameters.getRawParameterValue("amount")->load() - 61) < .01f, "Image knob controls automatable audio parameter");
    editor->setSize(420, 420);
    check(p.editorSize.load() == 420, "Resizing persisted");
    auto screenshot = [&](const juce::String& name) {
        juce::Image image(juce::Image::RGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics graphics(image); editor->paintEntireComponent(graphics, true);
        check(image.isValid(), "Editor renders to image");
        if (argc == 2) {
            const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]); folder.createDirectory();
            auto stream = folder.getChildFile(name).createOutputStream();
            if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat{}.writeImageToStream(image, *stream); }
        }
    };
    screenshot("normal.png");
    auto* toggle = dynamic_cast<juce::TextButton*>(editor->findChildWithID("panelToggle"));
    if (toggle && toggle->onClick) toggle->onClick();
    check(toggle && p.advancedPanel.load(), "Blue indicator toggles advanced panel");
    screenshot("advanced.png");
    return failures ? 1 : 0;
}
