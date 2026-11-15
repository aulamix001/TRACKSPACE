#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SkinKnob final : public juce::Slider {
public:
    explicit SkinKnob(float originalPosition = .5f);
    void setSkin(const juce::Image&, juce::Rectangle<int>);
    void paint(juce::Graphics&) override;
    bool small = false;
private:
    juce::Image sprite;
    float original;
};
class SkinButton final : public juce::Button {
public:
    explicit SkinButton(const juce::String& name) : Button(name) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics&, bool, bool) override;
    bool freezeStyle = false;
};
class TrackspaceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TrackspaceEditor(TrackspaceProcessor&);
    ~TrackspaceEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
    static juce::String formatFrequency(float frequency);
private:
    void timerCallback() override;
    void updatePanel();
    void showMenu();
    void setMode(float);
    TrackspaceProcessor& processor;
    juce::Image normal, advanced;
    SkinKnob amount{.5f}, low{0}, high{1}, pan{.5f}, attack{.67f}, release{.67f};
    SkinButton freeze{"Freeze"}, listen{"Sidechain listen"}, lr{"L/R"}, ms{"M/S"};
    juce::TextButton panelToggle, menuButton;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    juce::TooltipWindow tooltip{this, 500};
    bool showingAdvanced = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackspaceEditor)
};
