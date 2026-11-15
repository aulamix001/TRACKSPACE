#include "PluginEditor.h"
#include <BinaryData.h>

namespace {
constexpr float designSize = 1254;
const juce::Colour blue{0xff6cbaff};
}
SkinKnob::SkinKnob(float position) : original(position) {
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                        juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity(250);
    setVelocityModeParameters(.3, 1, 0, true, juce::ModifierKeys::shiftModifier);
    setPopupDisplayEnabled(true, true, nullptr, 1000);
    setScrollWheelEnabled(true);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}
void SkinKnob::setSkin(const juce::Image& full, juce::Rectangle<int> rect) { sprite = full.getClippedImage(rect); repaint(); }
void SkinKnob::paint(juce::Graphics& g) {
    const auto normalised = static_cast<float>(valueToProportionOfLength(getValue()));
    const auto box = getLocalBounds().toFloat();
    if (small) {
        const auto circle = box.reduced(2);
        g.setColour(juce::Colour(0xff16191b)); g.fillEllipse(circle.reduced(2));
        g.setColour(blue); g.drawEllipse(circle, 1.3f);
        const auto angle = (normalised - .5f) * juce::MathConstants<float>::pi * 1.5f;
        const auto radius = circle.getWidth() * .36f;
        g.fillEllipse(box.getCentreX() + std::sin(angle) * radius - 1.8f,
                      box.getCentreY() - std::cos(angle) * radius - 1.8f, 3.6f, 3.6f);
        return;
    }
    juce::Graphics::ScopedSaveState save(g);
    juce::Path clip; clip.addEllipse(box.reduced(1)); g.reduceClipRegion(clip);
    g.addTransform(juce::AffineTransform::rotation((normalised - original) * juce::MathConstants<float>::pi * 1.5f,
                    box.getCentreX(), box.getCentreY()));
    g.drawImage(sprite, box);
}
void SkinButton::paintButton(juce::Graphics& g, bool hover, bool) {
    const auto box = getLocalBounds().toFloat();
    if (freezeStyle) {
        if (getToggleState()) { g.setColour(blue.withAlpha(.8f)); g.fillEllipse(box.reduced(box.getWidth() * .36f)); }
        if (hover) { g.setColour(blue.withAlpha(.4f)); g.drawEllipse(box.reduced(3), 1); }
        return;
    }
    g.setColour(juce::Colour(0xff16191b)); g.fillEllipse(box.reduced(1));
    g.setColour(blue.withAlpha(hover ? 1 : .85f)); g.drawEllipse(box.reduced(2), 1.3f);
    if (getToggleState()) g.fillEllipse(box.reduced(5));
}
TrackspaceEditor::TrackspaceEditor(TrackspaceProcessor& p) : AudioProcessorEditor(p), processor(p) {
    normal = juce::ImageCache::getFromMemory(Assets::normal_png, Assets::normal_pngSize);
    advanced = juce::ImageCache::getFromMemory(Assets::advanced_png, Assets::advanced_pngSize);
    auto knob = [this](SkinKnob& control, const char* id, const char* help) {
        addAndMakeVisible(control); control.setName(id); control.setComponentID(id); control.setTooltip(help);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, control));
    };
    knob(amount, "amount", "Amount: intensidade da reducao. Arraste; Shift para ajuste fino; duplo clique restaura.");
    knob(low, "lowCut", "Limite inferior e HPF do detector de sidechain (20 Hz a 20 kHz).");
    knob(high, "highCut", "Limite superior da faixa de reducao (20 Hz a 20 kHz).");
    knob(pan, "pan", "Balanco: L/R ou Mid/Side. Centro processa ambos; extremos preservam o outro canal.");
    knob(attack, "attack", "Attack: resposta do detector (0.001 a 100 ms).");
    knob(release, "release", "Release: recuperacao do detector (0.01 a 1000 ms).");
    pan.small = attack.small = release.small = true;
    freeze.freezeStyle = true;
    for (auto* b : {&freeze, &listen, &lr, &ms}) addAndMakeVisible(b);
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "freeze", freeze));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "listen", listen));
    freeze.setTooltip("Freeze: conserva o espectro atual de reducao.");
    listen.setTooltip("Escutar apenas o sidechain dentro da faixa de frequencias.");
    lr.setTooltip("Processamento Left/Right"); ms.setTooltip("Processamento Mid/Side");
    lr.onClick = [this] { setMode(0); }; ms.onClick = [this] { setMode(1); };
    addAndMakeVisible(panelToggle); panelToggle.setAlpha(.01f); panelToggle.setComponentID("panelToggle");
    panelToggle.setTooltip("Abrir/fechar painel avancado");
    panelToggle.onClick = [this] { processor.advancedPanel.store(!showingAdvanced); updatePanel(); };
    addAndMakeVisible(menuButton); menuButton.setAlpha(.01f); menuButton.setTooltip("Tamanho, bypass e informacoes");
    menuButton.onClick = [this] { showMenu(); };
    setResizable(true, true); setResizeLimits(360, 360, 800, 800);
    getConstrainer()->setFixedAspectRatio(1.0);
    const auto size = juce::jlimit(360, 800, p.editorSize.load()); setSize(size, size);
    updatePanel(); timerCallback(); startTimerHz(30);
}
void TrackspaceEditor::setMode(float v) {
    auto* p = processor.parameters.getParameter("mode");
    p->beginChangeGesture(); p->setValueNotifyingHost(v); p->endChangeGesture();
}
void TrackspaceEditor::updatePanel() {
    showingAdvanced = processor.advancedPanel.load();
    for (auto* c : std::array<juce::Component*, 6>{&pan, &attack, &release, &listen, &lr, &ms}) c->setVisible(showingAdvanced);
    const auto& image = showingAdvanced ? advanced : normal;
    amount.setSkin(image, {438, showingAdvanced ? 539 : 548, 380, 380});
    low.setSkin(image, {115, showingAdvanced ? 862 : 872, 250, 250});
    high.setSkin(image, {889, showingAdvanced ? 862 : 872, 250, 250});
    resized(); repaint();
}
void TrackspaceEditor::resized() {
    const auto scale = getWidth() / designSize;
    auto place = [scale](juce::Component& c, int x, int y, int w, int h) {
        c.setBounds(juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).operator* (scale).toNearestInt());
    };
    place(amount, 438, showingAdvanced ? 539 : 548, 380, 380);
    place(low, 115, showingAdvanced ? 862 : 872, 250, 250);
    place(high, 889, showingAdvanced ? 862 : 872, 250, 250);
    place(freeze, 185, 590, 105, 105);
    place(pan, 201, 312, 108, 108); place(attack, 629, 312, 108, 108); place(release, 778, 312, 108, 108);
    place(lr, 389, 339, 54, 54); place(ms, 494, 339, 54, 54); place(listen, 965, 339, 54, 54);
    place(panelToggle, 1068, showingAdvanced ? 408 : 424, 80, 80);
    place(menuButton, 571, 1037, 108, 108);
    processor.editorSize.store(getWidth());
}
juce::String TrackspaceEditor::formatFrequency(float frequency) {
    return frequency < 1000 ? juce::String(juce::roundToInt(frequency)) + " Hz"
                            : juce::String(frequency / 1000.0f, 1) + " kHz";
}
void TrackspaceEditor::paint(juce::Graphics& g) {
    g.drawImage(showingAdvanced ? advanced : normal, getLocalBounds().toFloat());
    const auto scale = getWidth() / designSize;
    g.addTransform(juce::AffineTransform::scale(scale));
    // Read the automatable values directly: DAW automation and knob gestures
    // update these same atomics. The existing 30 Hz timer schedules repaints.
    const auto lowHz = processor.parameters.getRawParameterValue("lowCut")->load(std::memory_order_relaxed);
    const auto highHz = processor.parameters.getRawParameterValue("highCut")->load(std::memory_order_relaxed);
    if (!showingAdvanced) {
        const juce::Rectangle<float> graph{123, 145, 1007, 270};
        juce::Graphics::ScopedSaveState saved(g); g.reduceClipRegion(graph.toNearestInt());
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1a1b1c), 0, 145, juce::Colour(0xff141617), 0, 415, false));
        g.fillRect(graph);
        auto frequencyX = [](float hz) { return 123 + std::log(hz / 20.0f) / std::log(1000.0f) * 1007; };
        const auto lowX = frequencyX(lowHz), highX = frequencyX(highHz);
        g.setColour(juce::Colour(0xffb9cedf).withAlpha(.13f));
        if (lowHz > highHz) g.fillRect(graph);
        else {
            g.fillRect(juce::Rectangle<float>{graph.getX(), graph.getY(), lowX - graph.getX(), graph.getHeight()});
            g.fillRect(juce::Rectangle<float>{highX, graph.getY(), graph.getRight() - highX, graph.getHeight()});
        }
        g.setColour(blue.withAlpha(.45f));
        g.drawVerticalLine(juce::roundToInt(lowX), graph.getY(), graph.getBottom());
        g.drawVerticalLine(juce::roundToInt(highX), graph.getY(), graph.getBottom());
        g.setColour(juce::Colours::white.withAlpha(.4f)); g.drawLine(123, 306, 1130, 306, 1.0f / scale);
        juce::Path reduction, spectrum;
        for (int i = 0; i < trackspace::bandCount; ++i) {
            const auto x = 123 + static_cast<float>(std::log(processor.engine.frequency(i) / 20) / std::log(1000.0)) * 1007;
            const auto db = processor.engine.reduction[static_cast<size_t>(i)].load(std::memory_order_relaxed);
            const auto level = processor.engine.spectrum[static_cast<size_t>(i)].load(std::memory_order_relaxed);
            const auto y = 306 - db * 3;
            const auto sy = juce::jlimit(200.0f, 402.0f, 160 - level * 2.7f);
            if (i == 0) { reduction.startNewSubPath(x, y); spectrum.startNewSubPath(x, sy); }
            else { reduction.lineTo(x, y); spectrum.lineTo(x, sy); }
        }
        // Compensate the design-space transform: these remain 2 / 2.5 logical
        // pixels wide even when the square plugin window is resized.
        g.setColour(blue.withAlpha(.7f));
        g.strokePath(spectrum, juce::PathStrokeType(2.0f / scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(blue);
        g.strokePath(reduction, juce::PathStrokeType(2.5f / scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (!processor.sidechainConnected.load()) {
            g.setColour(blue.withAlpha(.7f)); g.setFont(19);
            g.drawText("Ative o sidechain na DAW", 160, 365, 650, 30, juce::Justification::left);
        }
    }
    // Paint after the curves so peaks never obscure either numeric readout.
    g.setColour(juce::Colour(0xff151719).withAlpha(.96f));
    g.fillRoundedRectangle(140, 141, 245, 44, 7);
    g.fillRoundedRectangle(868, 141, 245, 44, 7);
    g.setColour(juce::Colour(0xffeaf4ff)); g.setFont(juce::Font(11.5f / scale));
    g.drawText(formatFrequency(lowHz), 151, 142, 225, 42, juce::Justification::centredLeft);
    g.drawText(formatFrequency(highHz), 877, 142, 225, 42, juce::Justification::centredRight);
}
void TrackspaceEditor::timerCallback() {
    if (processor.advancedPanel.load() != showingAdvanced) updatePanel();
    const bool midSide = processor.parameters.getRawParameterValue("mode")->load() > .5f;
    lr.setToggleState(!midSide, juce::dontSendNotification); ms.setToggleState(midSide, juce::dontSendNotification);
    repaint();
}
void TrackspaceEditor::showMenu() {
    juce::PopupMenu menu;
    menu.addSectionHeader("TRACKSPACE - Windows x64 VST3");
    menu.addItem(1, "Compacto (400 x 400)"); menu.addItem(2, "Padrao (500 x 500)"); menu.addItem(3, "Maior (650 x 650)");
    menu.addSeparator();
    menu.addItem(4, "Bypass", true, processor.parameters.getRawParameterValue("bypass")->load() > .5f);
    menu.addItem(5, "Escutar reducao (Delta)", true, processor.parameters.getRawParameterValue("delta")->load() > .5f);
    juce::Component::SafePointer<TrackspaceEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton), [safe](int result) {
        if (!safe) return;
        if (result >= 1 && result <= 3) { const int size = result == 1 ? 400 : result == 2 ? 500 : 650; safe->setSize(size, size); }
        if (result == 4 || result == 5) {
            auto* p = safe->processor.parameters.getParameter(result == 4 ? "bypass" : "delta");
            p->beginChangeGesture(); p->setValueNotifyingHost(p->getValue() > .5f ? 0.0f : 1.0f); p->endChangeGesture();
        }
    });
}
