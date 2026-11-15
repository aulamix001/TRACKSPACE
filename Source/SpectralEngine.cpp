#include "SpectralEngine.h"
#include <algorithm>

namespace trackspace {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double q = 4.45;
float clean(float x) { return std::isfinite(x) ? x : 0.0f; }
float clamp(float x, float lo, float hi) { return std::clamp(clean(x), lo, hi); }
}
void SpectralEngine::Biquad::bandpass(double w, double quality) {
    const auto alpha = std::sin(w) / (2 * quality), a0 = 1 + alpha;
    b0 = alpha / a0; b1 = 0; b2 = -b0;
    a1 = -2 * std::cos(w) / a0; a2 = (1 - alpha) / a0;
}
void SpectralEngine::Biquad::peak(double w, double quality, double db) {
    const auto a = std::pow(10.0, db / 40.0), alpha = std::sin(w) / (2 * quality);
    const auto a0 = 1 + alpha / a;
    b0 = (1 + alpha * a) / a0; b1 = -2 * std::cos(w) / a0;
    b2 = (1 - alpha * a) / a0; a1 = b1; a2 = (1 - alpha / a) / a0;
}
void SpectralEngine::Biquad::cutoff(double w, bool hp) {
    const auto c = std::cos(w), alpha = std::sin(w) / std::sqrt(2.0), a0 = 1 + alpha;
    b0 = (hp ? 1 + c : 1 - c) / (2 * a0);
    b1 = (hp ? -2 : 2) * b0; b2 = b0;
    a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
}
SpectralEngine::SpectralEngine() { prepare(48000); }
void SpectralEngine::prepare(double sr) {
    rate = std::isfinite(sr) && sr >= 8000 ? sr : 48000;
    for (int i = 0; i < bandCount; ++i) {
        auto& b = bands[static_cast<size_t>(i)];
        frequencies[static_cast<size_t>(i)] = static_cast<float>(20 * std::pow(1000.0, (i + .5) / bandCount));
        b.active = frequencies[static_cast<size_t>(i)] < rate * .475;
        b.omega = 2 * pi * std::min<double>(frequencies[static_cast<size_t>(i)], rate * .475) / rate;
        for (auto& f : b.detector) f.bandpass(b.omega, q);
    }
    reset();
}
void SpectralEngine::reset() {
    for (auto& b : bands) {
        for (auto& f : b.detector) f.clear();
        for (auto& f : b.equaliser) { f = Biquad{}; }
        b.envelope.fill(0);
    }
    for (auto& f : listenHP) f = Biquad{};
    for (auto& f : listenLP) f = Biquad{};
    for (auto& v : reduction) v.store(0, std::memory_order_relaxed);
    for (auto& v : spectrum) v.store(-90, std::memory_order_relaxed);
    amount = mode = bypassMix = listenMix = 0;
    oldLow = oldHigh = -1;
    coefficientCountdown = 0;
}
void SpectralEngine::process(float* l, float* r, const float* scL,
                            const float* scR, int count, const Settings& s) {
    if (!l || count <= 0) return;
    const auto low = clamp(s.lowCut, 20, 20000), high = clamp(s.highCut, 20, 20000);
    if (oldLow != low || oldHigh != high) {
        for (auto& f : listenHP) f.cutoff(2 * pi * std::min<double>(low, rate * .475) / rate, true);
        for (auto& f : listenLP) f.cutoff(2 * pi * std::min<double>(high, rate * .475) / rate, false);
        oldLow = low; oldHigh = high;
    }
    const auto attack = static_cast<float>(std::exp(-1.0 / (.001 * clamp(s.attackMs, .01f, 100) * rate)));
    const auto release = static_cast<float>(std::exp(-1.0 / (.001 * clamp(s.releaseMs, .01f, 1000) * rate)));
    const auto smooth = static_cast<float>(1 - std::exp(-1.0 / (.004 * rate)));
    const auto amountTarget = clamp(s.amount, 0, 100) * .01f;
    const auto pan = clamp(s.pan, -1, 1);
    const auto leftWeight = pan > 0 ? 1 - pan : 1.0f;
    const auto rightWeight = pan < 0 ? 1 + pan : 1.0f;
    for (int n = 0; n < count; ++n) {
        const float dryL = clean(l[n]), dryR = r ? clean(r[n]) : dryL;
        const float sourceL = scL ? clean(scL[n]) : 0;
        const float sourceR = scR ? clean(scR[n]) : sourceL;
        std::array<float, 4> source{sourceL, sourceR, (sourceL + sourceR) * .5f, (sourceL - sourceR) * .5f};
        std::array<float, 4> audio{dryL, dryR, (dryL + dryR) * .5f, (dryL - dryR) * .5f};
        amount += smooth * (amountTarget - amount);
        mode += smooth * ((s.midSide ? 1.0f : 0.0f) - mode);
        bypassMix += smooth * ((s.bypass ? 1.0f : 0.0f) - bypassMix);
        listenMix += smooth * ((s.listen ? 1.0f : 0.0f) - listenMix);
        const bool update = coefficientCountdown-- <= 0;
        if (update) coefficientCountdown = 15;
        std::array<float, 4> peakLevel{};
        if (update) for (const auto& band : bands) if (band.active)
            for (size_t c = 0; c < 4; ++c) peakLevel[c] = std::max(peakLevel[c], band.envelope[c]);
        peakLevel[0] = peakLevel[1] = std::max(peakLevel[0], peakLevel[1]);
        for (int i = 0; i < bandCount; ++i) {
            auto& b = bands[static_cast<size_t>(i)];
            for (int c = 0; c < 4; ++c) {
                auto& e = b.envelope[static_cast<size_t>(c)];
                const auto detected = std::abs(b.detector[static_cast<size_t>(c)].tick(source[static_cast<size_t>(c)]));
                if (!s.freeze) e = detected + (detected > e ? attack : release) * (e - detected);
            }
            if (update) {
                const auto linked = std::max(b.envelope[0], b.envelope[1]);
                const std::array<float, 4> env{linked, linked, b.envelope[2], b.envelope[3]};
                const bool enabled = b.active && frequencies[static_cast<size_t>(i)] >= low
                    && frequencies[static_cast<size_t>(i)] <= high && low <= high;
                float shownReduction = 0;
                for (int c = 0; c < 4; ++c) {
                    const auto db = 20 * std::log10(std::max(env[static_cast<size_t>(c)], 1.0e-9f));
                    const auto peakDb = 20 * std::log10(std::max(peakLevel[static_cast<size_t>(c)], 1.0e-9f));
                    const auto gate = std::clamp((peakDb + 72.0f) / 36.0f, 0.0f, 1.0f);
                    const auto strength = std::clamp((db - peakDb + 24.0f) / 24.0f, 0.0f, 1.0f) * gate;
                    const auto weight = c % 2 == 0 ? leftWeight : rightWeight;
                    const auto gain = enabled ? -30.0f * amount * strength * weight : 0;
                    b.equaliser[static_cast<size_t>(c)].peak(b.omega, q, gain);
                    if ((s.midSide && c >= 2) || (!s.midSide && c < 2)) shownReduction = std::min(shownReduction, gain);
                }
                reduction[static_cast<size_t>(i)].store(shownReduction, std::memory_order_relaxed);
                spectrum[static_cast<size_t>(i)].store(20 * std::log10(std::max(linked, 1.0e-9f)), std::memory_order_relaxed);
            }
            for (int c = 0; c < 4; ++c) audio[static_cast<size_t>(c)] = b.equaliser[static_cast<size_t>(c)].tick(audio[static_cast<size_t>(c)]);
        }
        const auto msL = audio[2] + audio[3], msR = audio[2] - audio[3];
        float wetL = audio[0] + mode * (msL - audio[0]);
        float wetR = audio[1] + mode * (msR - audio[1]);
        const auto monitorL = listenLP[0].tick(listenHP[0].tick(sourceL));
        const auto monitorR = listenLP[1].tick(listenHP[1].tick(sourceR));
        wetL += listenMix * ((low <= high ? monitorL : 0) - wetL);
        wetR += listenMix * ((low <= high ? monitorR : 0) - wetR);
        l[n] = clean(wetL + bypassMix * (dryL - wetL));
        if (r) r[n] = clean(wetR + bypassMix * (dryR - wetR));
    }
}
}
