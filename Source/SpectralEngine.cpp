#include "SpectralEngine.h"
#include <algorithm>

namespace trackspace {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double q = 4.45;
constexpr int controlInterval = 8;
constexpr float noiseFloor = 1.0e-7f;
float clean(float x) { return std::isfinite(x) ? x : 0.0f; }
float clamp(float x, float lo, float hi) { return std::clamp(clean(x), lo, hi); }
void approach(float& value, float target, float speed) {
    value += speed * (target - value);
    if (std::abs(target - value) < 1.0e-7f) value = target;
}
}
void SpectralEngine::Biquad::setTarget(double c0, double c1, double c2, double d1, double d2, int samples) {
    target = {c0, c1, c2, d1, d2}; rampRemaining = samples;
    if (samples == 0) { b0 = c0; b1 = c1; b2 = c2; a1 = d1; a2 = d2; return; }
    const std::array<double, 5> current{b0, b1, b2, a1, a2};
    for (size_t i = 0; i < step.size(); ++i) step[i] = (target[i] - current[i]) / samples;
}
void SpectralEngine::Biquad::bandpass(double w, double quality) {
    const auto a = std::sin(w) / (2 * quality), a0 = 1 + a;
    setTarget(a / a0, 0, -a / a0, -2 * std::cos(w) / a0, (1 - a) / a0, 0);
}
void SpectralEngine::Biquad::peak(double cosine, double alpha, double db, int samples) {
    const auto a = std::pow(10.0, db / 40.0), a0 = 1 + alpha / a;
    setTarget((1 + alpha * a) / a0, -2 * cosine / a0, (1 - alpha * a) / a0,
              -2 * cosine / a0, (1 - alpha / a) / a0, samples);
}
void SpectralEngine::Biquad::cutoff(double w, bool hp, int samples) {
    const auto c = std::cos(w), alpha = std::sin(w) / std::sqrt(2.0), a0 = 1 + alpha;
    const auto numerator = (hp ? 1 + c : 1 - c) / (2 * a0);
    setTarget(numerator, (hp ? -2 : 2) * numerator, numerator, -2 * c / a0, (1 - alpha) / a0, samples);
}
SpectralEngine::SpectralEngine() {
    // Immutable after construction: paint() may read centres while the host
    // prepares a new sample rate on another thread.
    for (int i = 0; i < bandCount; ++i)
        frequencies[static_cast<size_t>(i)] = static_cast<float>(20 * std::pow(1000.0, (i + .5) / bandCount));
    prepare(48000);
}
void SpectralEngine::prepare(double sr) {
    rate = std::isfinite(sr) && sr >= 8000 ? sr : 48000;
    for (int i = 0; i < bandCount; ++i) {
        auto& b = bands[static_cast<size_t>(i)];
        b.active = frequencies[static_cast<size_t>(i)] < rate * .475;
        b.omega = 2 * pi * std::min<double>(frequencies[static_cast<size_t>(i)], rate * .475) / rate;
        b.cosine = std::cos(b.omega); b.alpha = std::sin(b.omega) / (2 * q);
        for (auto& f : b.detector) f.bandpass(b.omega, q);
    }
    reset();
}
void SpectralEngine::reset() {
    for (auto& b : bands) {
        for (auto& f : b.detector) f.clear();
        for (auto& f : b.equaliser) f = Biquad{};
        b.envelope = b.regionMix = 0; b.gainDb.fill(0);
    }
    for (auto& f : sidechainHP) f = Biquad{};
    for (auto& f : sidechainLP) f = Biquad{};
    for (auto& v : reduction) v.store(0, std::memory_order_relaxed);
    for (auto& v : spectrum) v.store(-140, std::memory_order_relaxed);
    amount = mode = bypassMix = listenMix = balance = hpMix = lpMix = activityMix = 0;
    oldLow = oldHigh = -1; coefficientCountdown = 0;
}
void SpectralEngine::process(float* l, float* r, const float* scL,
                            const float* scR, int count, const Settings& s) {
    if (!l || count <= 0) return;
    const auto low = clamp(s.lowCut, 20, 20000), high = clamp(s.highCut, 20, 20000);
    if (oldLow != low || oldHigh != high) {
        const auto ramp = oldLow < 0 ? 0 : static_cast<int>(.003 * rate);
        for (auto& f : sidechainHP) f.cutoff(2 * pi * std::min<double>(low, rate * .475) / rate, true, ramp);
        for (auto& f : sidechainLP) f.cutoff(2 * pi * std::min<double>(high, rate * .475) / rate, false, ramp);
        oldLow = low; oldHigh = high;
    }
    const auto attack = static_cast<float>(std::exp(-1.0 / (.001 * clamp(s.attackMs, .001f, 100) * rate)));
    const auto release = static_cast<float>(std::exp(-1.0 / (.001 * clamp(s.releaseMs, .01f, 1000) * rate)));
    const auto smooth = static_cast<float>(1 - std::exp(-1.0 / (.004 * rate)));
    const auto controlSmooth = static_cast<float>(1 - std::exp(-controlInterval / (.0005 * rate)));
    const auto amountTarget = clamp(s.amount, 0, 100) * .01f, panTarget = clamp(s.pan, -1, 1);
    for (int n = 0; n < count; ++n) {
        const float dryL = clean(l[n]), dryR = r ? clean(r[n]) : dryL;
        float sourceL = scL ? clean(scL[n]) : 0, sourceR = scR ? clean(scR[n]) : sourceL;
        approach(hpMix, low > 20 ? 1.0f : 0, smooth); approach(lpMix, high < 20000 ? 1.0f : 0, smooth);
        const auto hpL = sidechainHP[0].tick(sourceL), hpR = sidechainHP[1].tick(sourceR);
        sourceL += hpMix * (hpL - sourceL); sourceR += hpMix * (hpR - sourceR);
        const auto lpL = sidechainLP[0].tick(sourceL), lpR = sidechainLP[1].tick(sourceR);
        sourceL += lpMix * (lpL - sourceL); sourceR += lpMix * (lpR - sourceR);
        approach(amount, amountTarget, smooth); approach(balance, panTarget, smooth);
        approach(mode, s.midSide ? 1.0f : 0, smooth);
        approach(bypassMix, s.bypass ? 1.0f : 0, smooth); approach(listenMix, s.listen ? 1.0f : 0, smooth);
        const bool update = coefficientCountdown-- <= 0;
        if (update) coefficientCountdown = controlInterval - 1;
        float peakEnvelope = 0;
        // A shared linked L/R spectrum can drive either Mid or Side, including
        // a mono auxiliary. Only the MAIN signal is decoded to Mid/Side.
        for (size_t i = 0; i < bands.size(); ++i) {
            auto& b = bands[i];
            const auto bandL = b.detector[0].tick(sourceL), bandR = b.detector[1].tick(sourceR);
            const auto detected = std::max(std::abs(bandL), std::abs(bandR));
            if (!s.freeze) {
                b.envelope = detected + (detected > b.envelope ? attack : release) * (b.envelope - detected);
                if (b.envelope < noiseFloor && detected < noiseFloor) b.envelope = 0;
            }
            if (b.active && frequencies[i] >= low && frequencies[i] <= high)
                peakEnvelope = std::max(peakEnvelope, b.envelope);
        }
        // Relative energy rejects distant bandpass leakage; actual envelope
        // amplitude, rather than a normalized shape, determines reduction depth.
        const auto peakDb = update ? 20 * std::log10(std::max(peakEnvelope, noiseFloor)) : 0;
        std::array<float, 4> audio{dryL, dryR, (dryL + dryR) * .5f, (dryL - dryR) * .5f};
        const std::array<float, 2> weights{balance > 0 ? 1 - balance : 1, balance < 0 ? 1 + balance : 1};
        float maxAppliedGain = 0;
        for (int i = 0; i < bandCount; ++i) {
            auto& b = bands[static_cast<size_t>(i)];
            const bool inRange = b.active && frequencies[static_cast<size_t>(i)] >= low
                && frequencies[static_cast<size_t>(i)] <= high && low <= high;
            approach(b.regionMix, inRange ? 1.0f : 0, smooth);
            if (update) {
                const auto db = 20 * std::log10(std::max(b.envelope, noiseFloor));
                const auto shape = std::clamp((db - peakDb + 24.0f) / 24.0f, 0.0f, 1.0f);
                // Inverse amplitude law, scaled linearly in dB by Amount:
                // baseGain = 1/(1+40*Envelope), finalGain = baseGain^Amount.
                const auto depth = std::min(36.0f, 20 * std::log10(1 + 40 * b.envelope));
                const auto targetDb = -depth * amount * shape * b.regionMix;
                for (size_t c = 0; c < 2; ++c) {
                    approach(b.gainDb[c], targetDb * weights[c], controlSmooth);
                    if (std::abs(b.gainDb[c]) < 1.0e-5f && targetDb == 0) b.gainDb[c] = 0;
                    b.equaliser[c].peak(b.cosine, b.alpha, b.gainDb[c], controlInterval);
                    b.equaliser[c + 2].peak(b.cosine, b.alpha, b.gainDb[c], controlInterval);
                }
                reduction[static_cast<size_t>(i)].store(std::min(b.gainDb[0], b.gainDb[1]), std::memory_order_relaxed);
                spectrum[static_cast<size_t>(i)].store(db, std::memory_order_relaxed);
            }
            maxAppliedGain = std::max(maxAppliedGain, std::max(-b.gainDb[0], -b.gainDb[1]));
            for (size_t c = 0; c < 4; ++c) audio[c] = b.equaliser[c].tick(audio[c]);
        }
        // Restore an exact dry path after release, also eliminating residual
        // IIR state and M/S roundoff coloration while there is no reduction.
        approach(activityMix, maxAppliedGain > 0 ? 1.0f : 0, smooth);
        const auto msL = audio[2] + audio[3], msR = audio[2] - audio[3];
        float wetL = audio[0] + mode * (msL - audio[0]), wetR = audio[1] + mode * (msR - audio[1]);
        wetL = dryL + activityMix * (wetL - dryL); wetR = dryR + activityMix * (wetR - dryR);
        wetL += listenMix * ((low <= high ? sourceL : 0) - wetL); wetR += listenMix * ((low <= high ? sourceR : 0) - wetR);
        l[n] = clean(wetL + bypassMix * (dryL - wetL));
        if (r) r[n] = clean(wetR + bypassMix * (dryR - wetR));
    }
}
}
