#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "RealtimeSafety.h"
#include <iostream>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    int failures = 0;
    auto check = [&failures](bool ok, const char* name) { std::cout << (ok ? "PASS " : "FAIL ") << name << '\n'; failures += !ok; };
    TrackspaceProcessor p;
    check(p.getTotalNumOutputChannels() == 2, "Stereo output bus");
    check(p.getBus(true, 1)->isEnabled(), "Auxiliary stereo sidechain enabled by default");
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
    p.prepareToPlay(48000, 1025); buffer.setSize(4, 1025); buffer.clear();
    for (int n = 0; n < 1025; ++n) {
        buffer.setSample(2, n, .25f * std::sin(n * .13f));
        buffer.setSample(3, n, .37f * std::sin(n * .19f));
    }
    juce::AudioBuffer<float> auxiliaryCopy(buffer);
    heapProbe::allocations.store(0); heapProbe::active = true;
    p.processBlock(buffer, midi);
    heapProbe::active = false;
    check(heapProbe::allocations.load() == 0, "processBlock with stereo auxiliary allocates no heap memory");
    check(buffer.getMagnitude(0, 0, 1025) == 0 && buffer.getMagnitude(1, 0, 1025) == 0,
        "Auxiliary channels do not leak into silent main output across 256-sample chunks");
    check(std::equal(buffer.getReadPointer(2), buffer.getReadPointer(2) + 1025, auxiliaryCopy.getReadPointer(2))
        && std::equal(buffer.getReadPointer(3), buffer.getReadPointer(3) + 1025, auxiliaryCopy.getReadPointer(3)),
        "Both auxiliary input channels remain read-only");
    TrackspaceProcessor disconnected;
    auto noSide = disconnected.getBusesLayout(); noSide.inputBuses.set(1, juce::AudioChannelSet::disabled());
    check(disconnected.setBusesLayout(noSide), "Host can disable auxiliary bus");
    disconnected.prepareToPlay(48000, 1025);
    buffer.setSize(2, 1025);
    for (int c = 0; c < 2; ++c) for (int n = 0; n < 1025; ++n) buffer.setSample(c, n, .5f * std::sin(n * .2f));
    juce::AudioBuffer<float> dryCopy(buffer);
    disconnected.processBlock(buffer, midi);
    check(std::equal(buffer.getReadPointer(0), buffer.getReadPointer(0) + 1025, dryCopy.getReadPointer(0))
        && std::equal(buffer.getReadPointer(1), buffer.getReadPointer(1) + 1025, dryCopy.getReadPointer(1)),
        "Disabled SC does not fall back to processing loud main audio");
    check(p.getParameters().size() == 11, "All eleven controls exposed for host automation");
    check(p.parameters.getParameter("attack")->getText(0, 32).contains("0.001"),
        "DAW displays the minimum attack accurately instead of rounding it to zero");
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
    check(TrackspaceEditor::formatFrequency(20) == "20 Hz"
        && TrackspaceEditor::formatFrequency(120) == "120 Hz"
        && TrackspaceEditor::formatFrequency(999) == "999 Hz"
        && TrackspaceEditor::formatFrequency(1000) == "1.0 kHz"
        && TrackspaceEditor::formatFrequency(1200) == "1.2 kHz"
        && TrackspaceEditor::formatFrequency(5400) == "5.4 kHz"
        && TrackspaceEditor::formatFrequency(20000) == "20.0 kHz",
        "Frequency readouts format Hz/kHz correctly at the 1000 Hz boundary");
    auto screenshot = [&](const juce::String& name) {
        juce::Image image(juce::Image::RGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics graphics(image); editor->paintEntireComponent(graphics, true);
        check(image.isValid(), "Editor renders to image");
        if (argc == 2) {
            const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]); folder.createDirectory();
            auto stream = folder.getChildFile(name).createOutputStream();
            if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat{}.writeImageToStream(image, *stream); }
        }
        return image;
    };
    const auto initialImage = screenshot("normal.png");
    auto* lowKnob = dynamic_cast<juce::Slider*>(editor->findChildWithID("lowCut"));
    auto* highKnob = dynamic_cast<juce::Slider*>(editor->findChildWithID("highCut"));
    if (lowKnob) lowKnob->setValue(1200, juce::sendNotificationSync);
    if (highKnob) highKnob->setValue(5400, juce::sendNotificationSync);
    const auto knobImage = screenshot("frequencies.png");
    auto cornerChanged = [&](const juce::Image& before, const juce::Image& after, bool right) {
        const auto scale = editor->getWidth() / 1254.0f;
        const auto region = juce::Rectangle<float>(right ? 877.0f : 151.0f, 142, 225, 42) * scale;
        const auto pixels = region.toNearestInt();
        for (int y = pixels.getY(); y < pixels.getBottom(); ++y)
            for (int x = pixels.getX(); x < pixels.getRight(); ++x)
                if (before.getPixelAt(x, y) != after.getPixelAt(x, y)) return true;
        return false;
    };
    check(lowKnob && highKnob && std::abs(p.parameters.getRawParameterValue("lowCut")->load() - 1200) < .1f
        && std::abs(p.parameters.getRawParameterValue("highCut")->load() - 5400) < .1f
        && cornerChanged(initialImage, knobImage, false) && cornerChanged(initialImage, knobImage, true),
        "Turning mapped cutoff knobs updates both numeric corners of the rendered display");
    for (const auto& change : {std::pair<const char*, float>{"lowCut", 2000}, {"highCut", 8000}}) {
        auto* parameter = p.parameters.getParameter(change.first);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(change.second));
    }
    const auto automatedImage = screenshot("automated.png");
    check(cornerChanged(knobImage, automatedImage, false) && cornerChanged(knobImage, automatedImage, true),
        "DAW automation updates the frequency readouts without a knob gesture");
    editor->setSize(360, 360); screenshot("compact.png");
    editor->setSize(800, 800); screenshot("large.png");
    editor->setSize(420, 420);
    auto* toggle = dynamic_cast<juce::TextButton*>(editor->findChildWithID("panelToggle"));
    if (toggle && toggle->onClick) toggle->onClick();
    check(toggle && p.advancedPanel.load(), "Blue indicator toggles advanced panel");
    screenshot("advanced.png");
    return failures ? 1 : 0;
}
