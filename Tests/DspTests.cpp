#include "SpectralEngine.h"
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
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
