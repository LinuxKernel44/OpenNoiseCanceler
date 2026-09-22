#pragma once

#include "adaptive_predictor.h"
#include "anc_parameters.h"
#include "biquad_filter.h"
#include "output_limiter.h"

namespace onc::dsp {

// Ties the DSP building blocks together into the per-block ANC signal path.
//
// Real-time contract: `processBlock` performs no allocation, no locking, no
// I/O, and is safe to call repeatedly from the audio callback thread.
// `configure` DOES allocate (it resizes the predictor's history/weights) and
// must only be called while the audio stream is stopped.
class AncProcessor {
public:
    AncProcessor();

    // Not real-time safe. Call only while the audio stream is stopped.
    void configure(const AncParameters& params);

    // Real-time safe. `input` and `output` may alias.
    void processBlock(const float* input, float* output, size_t numFrames);

    void reset();

    const AncParameters& parameters() const { return params_; }

    // Diagnostics (see AdaptivePredictor for what this RMS does and does not
    // represent).
    float predictionErrorRms() const { return predictor_.predictionErrorRms(); }

private:
    AncParameters params_;
    BiquadFilter highPass_;
    BiquadFilter lowPass_;
    AdaptivePredictor predictor_;
    OutputLimiter limiter_;

    // Real audio samples never exceed [-1, 1] (normalized float PCM).
    // Clamping to generous headroom above that before the samples reach the
    // IIR band-limiting filters keeps a single pathological input sample
    // from leaving the filters' recursive state at an enormous magnitude
    // for a very long time afterward (see AdaptivePredictor::kMaxSampleMagnitude
    // for the matching rationale on the predictor side).
    static constexpr float kMaxInputMagnitude = 8.0f;
};

} // namespace onc::dsp
