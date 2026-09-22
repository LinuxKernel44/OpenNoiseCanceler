#pragma once

#include <cstddef>

namespace onc::dsp {

// Mirrors the Kotlin AncPreset (see config/AncPreset.kt). Kept as a plain
// struct so it can be passed across the JNI boundary and reused unchanged by
// the host-side unit tests in native-test/.
struct AncParameters {
    float sampleRateHz = 48000.0f;

    // Adaptive predictor.
    size_t predictorTaps = 32;
    size_t predictorDelaySamples = 256;
    float predictorStepSize = 0.5f;
    float predictorLeakage = 1.0e-5f;

    // Reference-signal band limiting: only the frequency range below
    // lowPassCutoffHz is where the measured pipeline latency still leaves a
    // usable fraction of the wavelength for phase-plausible cancellation.
    float highPassCutoffHz = 20.0f;
    float lowPassCutoffHz = 800.0f;
    float filterQ = 0.707f; // Butterworth-ish, maximally flat

    // 0..1 user-facing "strength" trim. There is no error microphone to
    // auto-calibrate this against, so it is intentionally a manual control
    // (see docs/DSP_DESIGN.md).
    float strength = 0.7f;

    float outputCeiling = 0.9f;

    bool isValid() const {
        return sampleRateHz > 0.0f && predictorTaps > 0 && predictorDelaySamples > 0 &&
               lowPassCutoffHz > highPassCutoffHz && highPassCutoffHz >= 0.0f &&
               strength >= 0.0f && strength <= 1.0f && outputCeiling > 0.0f && outputCeiling <= 1.0f;
    }
};

} // namespace onc::dsp
