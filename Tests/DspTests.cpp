#include "SpectralEngine.h"
#include "RealtimeSafety.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
constexpr double pi = 3.14159265358979323846;
void check(bool pass, const std::string& name) {
    ++checks; if (!pass) ++failures;
    std::cout << (pass ? "PASS " : "FAIL ") << name << '\n';
}
double energy(const std::vector<float>& v, size_t start) {
    double sum = 0; for (auto i = start; i < v.size(); ++i) sum += v[i] * v[i];
    return std::sqrt(sum / static_cast<double>(v.size() - start));
}
std::vector<float> tone(int n, double f, double rate) {
    std::vector<float> result(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) result[static_cast<size_t>(i)] = .3f * static_cast<float>(std::sin(2 * pi * f * i / rate));
    return result;
}
void run(trackspace::SpectralEngine& e, std::vector<float>& l, std::vector<float>* r,
         const std::vector<float>* scL, const std::vector<float>* scR, const trackspace::Settings& s, int block = 257) {
    for (size_t i = 0; i < l.size(); i += static_cast<size_t>(block)) {
        const auto count = std::min(block, static_cast<int>(l.size() - i));
        e.process(l.data() + i, r ? r->data() + i : nullptr, scL ? scL->data() + i : nullptr,
            scR ? scR->data() + i : nullptr, count, s);
    }
}
}
int main() {
    for (const double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
        const int n = static_cast<int>(sr * .5);
        auto input = tone(n, 1000, sr), sc = input, l = input, r = input;
        trackspace::SpectralEngine engine; engine.prepare(sr);
        trackspace::Settings s; s.amount = 100; s.attackMs = 1; s.releaseMs = 80;
        run(engine, l, &r, nullptr, nullptr, s);
        float error = 0; for (int i = 0; i < n; ++i) error = std::max(error, std::abs(l[static_cast<size_t>(i)] - input[static_cast<size_t>(i)]));
        check(error < 1.e-5f, "No sidechain is transparent @" + std::to_string(sr));
        engine.reset(); l = input; r = input; s.amount = 0;
        run(engine, l, &r, &sc, &sc, s);
        error = 0; for (int i = 0; i < n; ++i) error = std::max(error, std::abs(l[static_cast<size_t>(i)] - input[static_cast<size_t>(i)]));
        check(error < 1.e-5f, "Amount zero is transparent @" + std::to_string(sr));
        engine.reset(); l = input; r = input; s.amount = 100;
        run(engine, l, &r, &sc, &sc, s);
        check(energy(l, l.size() / 2) < energy(input, input.size() / 2) * .2, "Matching spectrum attenuates @" + std::to_string(sr));
        double difference = 0; for (size_t i = 0; i < l.size(); ++i) difference += std::abs(l[i] - r[i]);
        check(difference < 1.e-4, "Linked LR preserves stereo centre @" + std::to_string(sr));
        engine.reset(); l = input; r = input; s.lowCut = 5000;
        run(engine, l, &r, &sc, &sc, s);
        check(energy(l, l.size() / 2) > energy(input, input.size() / 2) * .94, "Low-cut protects excluded tone @" + std::to_string(sr));
        engine.reset(); l = input; s.lowCut = 20; s.highCut = 200;
        run(engine, l, nullptr, &sc, nullptr, s);
        check(energy(l, l.size() / 2) > energy(input, input.size() / 2) * .94, "High-cut protects excluded tone / mono @" + std::to_string(sr));
    }
    constexpr int n = 48000;
    auto input = tone(n, 1000, 48000), sc = input, l = input, r = input;
    trackspace::Settings s; s.amount = 100; s.attackMs = 1; s.releaseMs = 40;
    trackspace::SpectralEngine engine;
    auto distant = tone(n, 10000, 48000); l = distant; r = distant;
    run(engine, l, &r, &sc, &sc, s);
    check(energy(l, n / 2) > energy(distant, n / 2) * .8, "Spectral selectivity protects distant 10 kHz tone");
    engine.reset(); l = input; r = input; s.pan = -1;
    run(engine, l, &r, &sc, &sc, s);
    check(energy(l, n / 2) < .04 && energy(r, n / 2) > .20, "Pan left processes only left");
    engine.reset(); l = input; r = input; s.pan = 1; s.midSide = true;
    run(engine, l, &r, &sc, &sc, s);
    check(energy(l, n / 2) > .20, "M/S side-only preserves mono centre");
    engine.reset(); l = input; r = input; s.pan = -1;
    run(engine, l, &r, &sc, &sc, s);
    check(energy(l, n / 2) < .04, "M/S mid-only attenuates mono centre");
    auto negative = input; for (auto& v : negative) v = -v;
    engine.reset(); l = input; r = negative;
    run(engine, l, &r, &sc, &sc, s);
    check(energy(l, n / 2) > .20, "M/S mid-only preserves side signal");
    engine.reset(); l = input; r = input; s = {}; s.amount = 100; s.attackMs = 1;
    run(engine, l, &r, &sc, &sc, s);
    s.freeze = true; l = input; r = input;
    run(engine, l, &r, nullptr, nullptr, s);
    check(energy(l, n / 2) < .04, "Freeze holds learned curve after sidechain disappears");
    s.freeze = false; s.releaseMs = 10; l = input; r = input;
    run(engine, l, &r, nullptr, nullptr, s);
    check(energy(l, n * 3 / 4) > .20, "Release returns audio to dry after sidechain disappears");
    engine.reset(); l = input; r = input; s.bypass = true;
    run(engine, l, &r, &sc, &sc, s);
    check(std::abs(energy(l, n / 2) - energy(input, n / 2)) < .0001, "Bypass restores dry audio");
    engine.reset(); l.assign(n, 0); r = l; s.bypass = false; s.listen = true;
    run(engine, l, &r, &sc, &sc, s);
    check(energy(l, n / 2) > .20, "Listen routes external sidechain to output");
    engine.reset(); l = input; r = input; s = {}; s.lowCut = 12000; s.highCut = 100;
    run(engine, l, &r, &sc, &sc, s);
    check(std::abs(energy(l, n / 2) - energy(input, n / 2)) < .0001, "Crossed cutoff range is transparent");
    engine.reset(); l = input; r = input; l[0] = std::numeric_limits<float>::quiet_NaN(); sc[7] = std::numeric_limits<float>::infinity();
    s = {}; s.amount = 100; s.attackMs = .01f; s.releaseMs = .01f;
    run(engine, l, &r, &sc, &sc, s, 1);
    check(std::all_of(l.begin(), l.end(), [](float v) { return std::isfinite(v) && std::abs(v) < 4; }), "Extreme time constants / poisoned samples remain finite");
    engine.reset(); l = input; r = input; auto l2 = input, r2 = input; sc = input; s = {};
    run(engine, l, &r, &sc, &sc, s, 1);
    engine.reset(); run(engine, l2, &r2, &sc, &sc, s, 1024);
    check(l == l2 && r == r2, "Output invariant to host block size");
    // Regression: a single hot channel must drive exactly the same linked
    // reduction as two hot channels, including opposite polarity.
    auto silence = std::vector<float>(n, 0);
    auto referenceL = input, referenceR = input; s = {}; s.amount = 65;
    engine.reset(); run(engine, referenceL, &referenceR, &input, &input, s);
    for (int scenario = 0; scenario < 3; ++scenario) {
        engine.reset(); l = input; r = input;
        const auto* leftSC = scenario == 1 ? &silence : &input;
        const auto* rightSC = scenario == 0 ? &silence : scenario == 1 ? &input : &negative;
        run(engine, l, &r, leftSC, rightSC, s);
        check(l == referenceL && r == referenceR,
            scenario == 0 ? "Left-only SC equals dual-channel peak detection" :
            scenario == 1 ? "Right-only SC equals dual-channel peak detection" :
                            "Anti-phase SC does not cancel in linked LR detector");
    }
    auto quadrature = input;
    for (int i = 0; i < n; ++i) quadrature[static_cast<size_t>(i)] = .3f * static_cast<float>(std::cos(2 * pi * 1000 * i / 48000));
    auto detectorDb = [&](const std::vector<float>& a, const std::vector<float>& b) {
        engine.reset(); auto main = input;
        trackspace::Settings settings; settings.amount = 0; settings.attackMs = settings.releaseMs = 10;
        run(engine, main, nullptr, &a, &b, settings);
        return engine.spectrum[18].load();
    };
    const auto individualPeakDb = std::max(detectorDb(input, silence), detectorDb(silence, quadrature));
    check(detectorDb(input, quadrature) > individualPeakDb + 2,
        "Quadrature peaks link before smoothing, rather than max of independently smoothed channels");
    // Observe detector spectrum directly: an output-band mask alone cannot
    // pass this check if the HPF is only wired to the monitor path.
    auto bass = tone(n, 60, 48000); s = {}; s.amount = 0;
    engine.reset(); l = bass; r = bass; run(engine, l, &r, &bass, &bass, s);
    const auto rawBassDb = engine.spectrum[5].load();
    engine.reset(); l = bass; r = bass; s.lowCut = 1000;
    run(engine, l, &r, &bass, &bass, s);
    check(engine.spectrum[5].load() < rawBassDb - 35, "SC HPF suppresses 60 Hz inside the detector by over 35 dB");
    check(l == bass && r == bass, "SC HPF does not high-pass the main audio");
    auto treble = tone(n, 10000, 48000); s = {}; s.amount = 0;
    engine.reset(); l = treble; r = treble; run(engine, l, &r, &treble, &treble, s);
    const auto rawTrebleDb = engine.spectrum[28].load();
    engine.reset(); l = treble; r = treble; s.highCut = 1000;
    run(engine, l, &r, &treble, &treble, s);
    check(engine.spectrum[28].load() < rawTrebleDb - 35, "SC LPF actually filters the detector input");
    check(l == treble && r == treble, "SC LPF does not low-pass the main audio");
    auto quietSC = input; for (auto& sample : quietSC) sample *= .0004f;
    engine.reset(); l = input; r = input; s = {};
    run(engine, l, &r, &quietSC, &quietSC, s);
    float quietReduction = 0;
    for (auto& band : engine.reduction) quietReduction = std::max(quietReduction, -band.load());
    check(quietReduction > .005f && quietReduction < .1f,
        "Quiet sidechain produces proportional subtle reduction rather than a normalized deep cut");
    auto transientReduction = [&](float attackTime) {
        trackspace::SpectralEngine test;
        trackspace::Settings settings; settings.amount = 100; settings.attackMs = attackTime;
        auto warm = std::vector<float>(4096, 0); run(test, warm, nullptr, nullptr, nullptr, settings);
        auto burst = tone(24, 4000, 48000), main = burst;
        run(test, main, nullptr, &burst, nullptr, settings);
        float peak = 0; for (auto& band : test.reduction) peak = std::max(peak, -band.load());
        return peak;
    };
    const auto fastTransient = transientReduction(.001f), slowTransient = transientReduction(100);
    std::cout << "Transient reduction after 0.5 ms: fast=" << fastTransient << " dB slow=" << slowTransient << " dB\n";
    check(fastTransient > 6 && slowTransient < 1 && fastTransient > slowTransient * 10,
        "Fast attack catches the first 0.5 ms of a transient more firmly than slow attack");
    // The actual attenuation must track signal LEVEL, not just its normalized shape.
    auto attenuation = [&](float sidechainLevel, float amountValue) {
        auto source = input; for (auto& x : source) x *= sidechainLevel;
        engine.reset(); auto main = input; trackspace::Settings settings; settings.amount = amountValue;
        run(engine, main, nullptr, &source, nullptr, settings);
        return energy(main, n / 2) / energy(input, n / 2);
    };
    const auto weak = attenuation(.01f, 50), medium = attenuation(.1f, 50), strong = attenuation(1, 50);
    check(weak > medium && medium > strong && medium > strong * 2,
        "Same spectrum at increasing SC amplitude produces increasing attenuation");
    check(attenuation(1, 25) > attenuation(1, 75), "Amount scales spectral reduction depth monotonically");
    // Mono external voice can control Side-only; the centre must remain intact.
    engine.reset(); l = input; r = negative; s = {}; s.amount = 100; s.midSide = true; s.pan = 1;
    run(engine, l, &r, &input, nullptr, s);
    check(energy(l, n / 2) < .04, "Mono sidechain can attenuate Side-only stereo material");
    engine.reset(); l = input; r = input;
    run(engine, l, &r, &input, nullptr, s);
    check(energy(l, n / 2) > .20, "Side-only ducking preserves Mid with a mono auxiliary");
    // Independent tones let us null the unprocessed M/S component.
    const auto sideTone = tone(n, 2700, 48000);
    auto testMatrix = [&](float balance, bool preserveMid) {
        engine.reset(); std::vector<float> a(n), b(n);
        for (int i = 0; i < n; ++i) { a[i] = input[i] + sideTone[i]; b[i] = input[i] - sideTone[i]; }
        auto settings = s; settings.pan = balance;
        run(engine, a, &b, &input, nullptr, settings);
        float error = 0;
        for (int i = n / 2; i < n; ++i) {
            const auto recovered = preserveMid ? (a[i] + b[i]) * .5f : (a[i] - b[i]) * .5f;
            error = std::max(error, std::abs(recovered - (preserveMid ? input[i] : sideTone[i])));
        }
        return error;
    };
    check(testMatrix(1, true) < 1.e-5f, "Side-only processing nulls unchanged Mid in a mixed stereo signal");
    check(testMatrix(-1, false) < 1.e-5f, "Mid-only processing nulls unchanged Side in a mixed stereo signal");
    // The region mask must settle to zero gain for every excluded centre band.
    engine.reset(); l = input; r = input; s = {}; s.lowCut = 500; s.highCut = 2000;
    run(engine, l, &r, &input, nullptr, s);
    bool excludedUnity = true;
    for (int i = 0; i < trackspace::bandCount; ++i)
        if (engine.frequency(i) < 500 || engine.frequency(i) > 2000)
            excludedUnity &= engine.reduction[i].load() == 0;
    check(excludedUnity, "All excluded band gains are exactly zero dB after cutoff ramps settle");
    // Time-domain transitions on DC expose a one-sample gain step directly.
    engine.reset(); s = {}; s.amount = 100; s.attackMs = .001f;
    std::vector<float> dc(n, .2f), dcRight = dc;
    auto sweepSource = input;
    run(engine, dc, &dcRight, &sweepSource, nullptr, s);
    std::fill(dc.begin(), dc.end(), .2f); std::fill(dcRight.begin(), dcRight.end(), .2f);
    s.lowCut = 4000; s.pan = 1; s.midSide = true;
    run(engine, dc, &dcRight, &sweepSource, nullptr, s);
    float biggestStep = std::abs(dc[0] - .2f);
    for (int i = 1; i < n; ++i) biggestStep = std::max(biggestStep, std::abs(dc[i] - dc[i - 1]));
    check(biggestStep < .01f, "Combined cutoff, Pan and mode automation does not introduce a DC gain click");
    // Restore exact dry samples after release, including when M/S stays active.
    engine.reset(); l = input; r = negative; s = {}; s.amount = 100; s.midSide = true; s.releaseMs = 10;
    run(engine, l, &r, &input, nullptr, s);
    l = input; r = negative; run(engine, l, &r, nullptr, nullptr, s);
    check(std::equal(l.begin() + n * 3 / 4, l.end(), input.begin() + n * 3 / 4)
        && std::equal(r.begin() + n * 3 / 4, r.end(), negative.begin() + n * 3 / 4),
        "Release restores bit-exact dry stereo samples with no persistent M/S coloration");
    engine.reset(); l = input; r = input; s = {};
    heapProbe::allocations.store(0); heapProbe::active = true;
    engine.process(l.data(), r.data(), input.data(), nullptr, n, s);
    s.lowCut = 1000; s.midSide = true; s.pan = 1;
    engine.process(l.data(), r.data(), input.data(), nullptr, n, s);
    heapProbe::active = false;
    check(heapProbe::allocations.load() == 0, "Audio processing and live parameter changes allocate zero heap memory");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
