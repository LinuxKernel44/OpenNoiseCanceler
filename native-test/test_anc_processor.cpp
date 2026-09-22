#include <cmath>
#include <limits>
#include <vector>

#include "../app/src/main/cpp/dsp/anc_processor.h"
#include "test_framework.h"

using onc::dsp::AncParameters;
using onc::dsp::AncProcessor;

namespace {
constexpr float kPi = 3.14159265358979323846f;

AncParameters defaultParams() {
    AncParameters p;
    p.sampleRateHz = 48000.0f;
    p.predictorTaps = 32;
    p.predictorDelaySamples = 128;
    p.predictorStepSize = 0.5f;
    p.predictorLeakage = 1e-5f;
    p.highPassCutoffHz = 20.0f;
    p.lowPassCutoffHz = 800.0f;
    p.filterQ = 0.707f;
    p.strength = 0.7f;
    p.outputCeiling = 0.9f;
    return p;
}
} // namespace

ONC_TEST(AncProcessor_DefaultConstructionProducesValidConfig) {
    AncProcessor proc;
    ONC_CHECK(proc.parameters().isValid());
}

ONC_TEST(AncProcessor_ProcessBlockNeverExceedsCeilingOnNormalAudio) {
    AncProcessor proc;
    auto params = defaultParams();
    proc.configure(params);

    std::vector<float> input(4096);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.3f * std::sin(2.0f * kPi * 150.0f * static_cast<float>(i) / params.sampleRateHz);
    }
    std::vector<float> output(input.size());
    proc.processBlock(input.data(), output.data(), input.size());

    for (float y : output) {
        ONC_CHECK(std::isfinite(y));
        ONC_CHECK_TRUE_MSG(y <= params.outputCeiling + 1e-6f && y >= -params.outputCeiling - 1e-6f,
                            "output must never exceed the configured ceiling");
    }
}

ONC_TEST(AncProcessor_SurvivesPathologicalInputWithoutExceedingCeiling) {
    AncProcessor proc;
    auto params = defaultParams();
    proc.configure(params);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    std::vector<float> input;
    for (int i = 0; i < 32; ++i) input.push_back(1.0e30f);
    for (int i = 0; i < 32; ++i) input.push_back(nan);
    for (int i = 0; i < 32; ++i) input.push_back(inf);
    for (int i = 0; i < 32; ++i) input.push_back(-inf);
    // Then a long run of normal audio to confirm full recovery.
    for (int i = 0; i < 8000; ++i) {
        input.push_back(0.5f * std::sin(2.0f * kPi * 300.0f * static_cast<float>(i) / params.sampleRateHz));
    }

    std::vector<float> output(input.size());
    proc.processBlock(input.data(), output.data(), input.size());

    for (size_t i = 0; i < output.size(); ++i) {
        ONC_CHECK_TRUE_MSG(std::isfinite(output[i]), "output must stay finite even for pathological input");
        ONC_CHECK_TRUE_MSG(output[i] <= params.outputCeiling + 1e-6f && output[i] >= -params.outputCeiling - 1e-6f,
                            "output must never exceed ceiling, even right after pathological input");
    }

    // The tail (well after the pathological burst and the predictor's
    // history window has flushed) should look like a normal, non-degenerate
    // signal again, not stuck at zero or saturated.
    bool sawVariation = false;
    for (size_t i = output.size() - 1000; i < output.size() - 1; ++i) {
        if (std::fabs(output[i] - output[i + 1]) > 1e-4f) {
            sawVariation = true;
            break;
        }
    }
    ONC_CHECK_TRUE_MSG(sawVariation, "processor should recover to producing a varying signal again");
}

ONC_TEST(AncProcessor_ConfigureRejectsInvalidParameters) {
    AncProcessor proc;
    const auto goodParams = defaultParams();
    proc.configure(goodParams);

    AncParameters bad = goodParams;
    bad.lowPassCutoffHz = 10.0f; // below highPassCutoffHz -> invalid band
    proc.configure(bad);

    // Invalid configuration must be rejected; the previously valid
    // configuration should remain active.
    ONC_CHECK(proc.parameters().lowPassCutoffHz == goodParams.lowPassCutoffHz);
}

ONC_TEST(AncProcessor_ConfigureRejectsOutOfRangeStrength) {
    AncProcessor proc;
    const auto goodParams = defaultParams();
    proc.configure(goodParams);

    AncParameters bad = goodParams;
    bad.strength = 1.5f;
    proc.configure(bad);

    ONC_CHECK(proc.parameters().strength == goodParams.strength);
}

ONC_TEST(AncProcessor_ResetProducesCleanState) {
    AncProcessor proc;
    proc.configure(defaultParams());

    std::vector<float> input(1000, 0.4f);
    std::vector<float> output(1000);
    proc.processBlock(input.data(), output.data(), input.size());

    proc.reset();

    std::vector<float> silentInput(100, 0.0f);
    std::vector<float> silentOutput(100);
    proc.processBlock(silentInput.data(), silentOutput.data(), silentInput.size());
    for (float y : silentOutput) {
        ONC_CHECK_NEAR(y, 0.0, 1e-5);
    }
}

ONC_TEST(AncProcessor_InPlaceProcessingIsSafe) {
    AncProcessor proc;
    proc.configure(defaultParams());

    std::vector<float> buffer(512);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = 0.2f * std::sin(0.1f * static_cast<float>(i));
    }
    proc.processBlock(buffer.data(), buffer.data(), buffer.size());
    for (float y : buffer) {
        ONC_CHECK(std::isfinite(y));
    }
}
