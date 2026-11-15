#pragma once
#include <array>
#include <atomic>
#include <cmath>

namespace trackspace {
constexpr int bandCount = 32;
struct Settings {
    float amount = 50.0f, lowCut = 20.0f, highCut = 20000.0f;
    float attackMs = 2.0f, releaseMs = 100.0f, pan = 0.0f;
    bool midSide = false, freeze = false, listen = false, bypass = false;
};

// No heap allocation, locks, host calls or UI access in the processing path.
class SpectralEngine {
public:
    SpectralEngine();
    void prepare(double sampleRate);
    void reset();
    void process(float* left, float* right, const float* sideLeft,
                 const float* sideRight, int count, const Settings& settings);
    float frequency(int band) const { return frequencies[static_cast<size_t>(band)]; }
    std::array<std::atomic<float>, bandCount> reduction{}, spectrum{};
private:
    struct Biquad {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        std::array<double, 5> target{1, 0, 0, 0, 0}, step{};
        int rampRemaining = 0;
        float tick(float x) {
            if (rampRemaining > 0) {
                if (--rampRemaining == 0) {
                    b0 = target[0]; b1 = target[1]; b2 = target[2]; a1 = target[3]; a2 = target[4];
                } else {
                    b0 += step[0]; b1 += step[1]; b2 += step[2]; a1 += step[3]; a2 += step[4];
                }
            }
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
            return static_cast<float>(y);
        }
        void clear() { z1 = z2 = 0; }
        void setTarget(double, double, double, double, double, int rampSamples);
        void bandpass(double omega, double q);
        void peak(double cosine, double alpha, double db, int rampSamples);
        void cutoff(double omega, bool highpass, int rampSamples);
    };
    struct Band {
        std::array<Biquad, 2> detector;
        std::array<Biquad, 4> equaliser;
        float envelope = 0, regionMix = 0;
        std::array<float, 2> gainDb{};
        double omega = 0, cosine = 1, alpha = 0;
        bool active = true;
    };
    std::array<Band, bandCount> bands;
    std::array<float, bandCount> frequencies{};
    // Applied to the auxiliary input only, before spectral detection and listen.
    std::array<Biquad, 2> sidechainHP, sidechainLP;
    double rate = 48000;
    float amount = 0, mode = 0, bypassMix = 0, listenMix = 0, balance = 0;
    float hpMix = 0, lpMix = 0, activityMix = 0;
    float oldLow = -1, oldHigh = -1;
    int coefficientCountdown = 0;
};
}
