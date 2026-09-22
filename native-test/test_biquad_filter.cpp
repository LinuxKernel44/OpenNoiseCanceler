#include <cmath>
#include <vector>

#include "../app/src/main/cpp/dsp/biquad_filter.h"
#include "test_framework.h"

using onc::dsp::BiquadFilter;

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Steady-state RMS gain of a filter at a given frequency, measured after
// letting transients settle.
float measureRmsGain(BiquadFilter& filter, float sampleRateHz, float toneHz, int numSamples) {
    filter.reset();
    double sumIn = 0.0;
    double sumOut = 0.0;
    const int settle = numSamples / 4;
    for (int i = 0; i < numSamples; ++i) {
        const float x = std::sin(2.0f * kPi * toneHz * static_cast<float>(i) / sampleRateHz);
        const float y = filter.process(x);
        if (i >= settle) {
            sumIn += static_cast<double>(x) * x;
            sumOut += static_cast<double>(y) * y;
        }
    }
    return static_cast<float>(std::sqrt(sumOut / sumIn));
}

} // namespace

ONC_TEST(BiquadFilter_LowPassAttenuatesHighFrequencyMoreThanLow) {
    BiquadFilter lp;
    const float fs = 48000.0f;
    lp.setLowPass(fs, 800.0f, 0.707f);

    const float gainLow = measureRmsGain(lp, fs, 100.0f, 4000);
    const float gainHigh = measureRmsGain(lp, fs, 8000.0f, 4000);

    ONC_CHECK_TRUE_MSG(gainLow > 0.9f, "passband (100 Hz) should be close to unity gain");
    ONC_CHECK_TRUE_MSG(gainHigh < 0.2f, "stopband (8 kHz) should be strongly attenuated");
    ONC_CHECK_TRUE_MSG(gainLow > gainHigh, "low-pass must pass low freq more than high freq");
}

ONC_TEST(BiquadFilter_HighPassAttenuatesLowFrequencyMoreThanHigh) {
    BiquadFilter hp;
    const float fs = 48000.0f;
    hp.setHighPass(fs, 20.0f, 0.707f);

    const float gainDcRegion = measureRmsGain(hp, fs, 2.0f, 8000);
    const float gainMid = measureRmsGain(hp, fs, 1000.0f, 4000);

    ONC_CHECK_TRUE_MSG(gainDcRegion < 0.3f, "near-DC rumble should be attenuated");
    ONC_CHECK_TRUE_MSG(gainMid > 0.9f, "mid-band should pass close to unity gain");
}

ONC_TEST(BiquadFilter_StaysFiniteOnImpulse) {
    BiquadFilter lp;
    lp.setLowPass(48000.0f, 800.0f, 0.707f);
    float y = lp.process(1.0f);
    ONC_CHECK(std::isfinite(y));
    for (int i = 0; i < 1000; ++i) {
        y = lp.process(0.0f);
        ONC_CHECK(std::isfinite(y));
    }
}
