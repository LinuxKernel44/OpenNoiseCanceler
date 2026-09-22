#include <cmath>
#include <limits>

#include "../app/src/main/cpp/dsp/adaptive_predictor.h"
#include "test_framework.h"

using onc::dsp::AdaptivePredictor;

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

ONC_TEST(AdaptivePredictor_ConvergesOnPeriodicSignal) {
    // A pure low-frequency tone is the easiest possible signal to forecast;
    // the predictor's self-supervised error should shrink substantially as
    // it adapts.
    AdaptivePredictor predictor(/*numTaps=*/32, /*delaySamples=*/16, /*stepSize=*/0.5f, /*leakage=*/1e-5f);
    const float fs = 48000.0f;
    const float toneHz = 200.0f;

    float earlyErrSum = 0.0f;
    int earlyCount = 0;
    float lateErrSum = 0.0f;
    int lateCount = 0;

    const int totalSamples = 20000;
    for (int i = 0; i < totalSamples; ++i) {
        const float x = std::sin(2.0f * kPi * toneHz * static_cast<float>(i) / fs);
        predictor.process(x);
        const float err = predictor.predictionErrorRms();
        if (i < 500) {
            earlyErrSum += err;
            ++earlyCount;
        } else if (i >= totalSamples - 500) {
            lateErrSum += err;
            ++lateCount;
        }
    }

    const float earlyAvg = earlyErrSum / static_cast<float>(earlyCount);
    const float lateAvg = lateErrSum / static_cast<float>(lateCount);
    ONC_CHECK_TRUE_MSG(lateAvg < earlyAvg * 0.5f, "prediction error should shrink well after convergence on a tone");
}

ONC_TEST(AdaptivePredictor_StableUnderSilence) {
    AdaptivePredictor predictor(32, 16, 0.5f, 1e-5f);
    for (int i = 0; i < 10000; ++i) {
        const float y = predictor.process(0.0f);
        ONC_CHECK(std::isfinite(y));
        ONC_CHECK_NEAR(y, 0.0, 1e-6);
    }
}

ONC_TEST(AdaptivePredictor_SanitizesNanInput) {
    AdaptivePredictor predictor(16, 8, 0.5f, 1e-5f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (int i = 0; i < 50; ++i) {
        const float y = predictor.process(nan);
        ONC_CHECK(std::isfinite(y));
    }
    // Recovery: after the NaN burst stops, the predictor should keep
    // producing finite output on normal input.
    for (int i = 0; i < 200; ++i) {
        const float y = predictor.process(std::sin(0.1f * i));
        ONC_CHECK(std::isfinite(y));
    }
}

ONC_TEST(AdaptivePredictor_SanitizesInfInput) {
    AdaptivePredictor predictor(16, 8, 0.5f, 1e-5f);
    const float inf = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 50; ++i) {
        const float y = predictor.process(i % 2 == 0 ? inf : -inf);
        ONC_CHECK(std::isfinite(y));
    }
}

ONC_TEST(AdaptivePredictor_BoundedUnderExtremeAmplitudeInput) {
    AdaptivePredictor predictor(16, 8, 0.9f, 1e-5f);
    for (int i = 0; i < 20; ++i) {
        const float y = predictor.process(1.0e30f);
        ONC_CHECK(std::isfinite(y));
    }
    // Weight magnitudes are clamped internally (see kMaxWeightMagnitude), so
    // the predictor recovers and keeps producing finite output once normal
    // input resumes. Note this test only checks *finiteness* here: while a
    // pathologically large raw sample (1e30, far outside anything a real
    // audio stream produces) is still inside the predictor's short history
    // window, the dot product with even small, clamped weights can briefly
    // be very large. Bounding the actual output *magnitude* under all
    // conditions is the OutputLimiter's job and is verified end-to-end in
    // test_anc_processor.cpp.
    for (int i = 0; i < 200; ++i) {
        const float y = predictor.process(std::sin(0.1f * i));
        ONC_CHECK(std::isfinite(y));
    }
}

ONC_TEST(AdaptivePredictor_ResetClearsState) {
    AdaptivePredictor predictor(16, 8, 0.5f, 1e-5f);
    for (int i = 0; i < 500; ++i) {
        predictor.process(std::sin(0.3f * i));
    }
    predictor.reset();
    const float y = predictor.process(0.0f);
    ONC_CHECK_NEAR(y, 0.0, 1e-6);
    ONC_CHECK_NEAR(predictor.predictionErrorRms(), 0.0, 1e-6);
}

ONC_TEST(AdaptivePredictor_SetDelaySamplesResizesWithoutCrashing) {
    AdaptivePredictor predictor(16, 8, 0.5f, 1e-5f);
    for (int i = 0; i < 100; ++i) predictor.process(std::sin(0.2f * i));
    predictor.setDelaySamples(64);
    ONC_CHECK(predictor.delaySamples() == 64);
    for (int i = 0; i < 100; ++i) {
        const float y = predictor.process(std::sin(0.2f * i));
        ONC_CHECK(std::isfinite(y));
    }
}
