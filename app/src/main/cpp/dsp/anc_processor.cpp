#include "anc_processor.h"

#include <algorithm>
#include <cmath>

namespace onc::dsp {

AncProcessor::AncProcessor()
    : predictor_(params_.predictorTaps, params_.predictorDelaySamples, params_.predictorStepSize,
                 params_.predictorLeakage),
      limiter_(params_.outputCeiling) {
    configure(params_);
}

void AncProcessor::configure(const AncParameters& params) {
    if (!params.isValid()) {
        // Refuse an invalid configuration rather than risk an unstable or
        // degenerate filter; keep whatever configuration was previously
        // active.
        return;
    }
    params_ = params;

    highPass_.reset();
    highPass_.setHighPass(params_.sampleRateHz, params_.highPassCutoffHz, params_.filterQ);

    lowPass_.reset();
    lowPass_.setLowPass(params_.sampleRateHz, params_.lowPassCutoffHz, params_.filterQ);

    predictor_ = AdaptivePredictor(params_.predictorTaps, params_.predictorDelaySamples,
                                    params_.predictorStepSize, params_.predictorLeakage);

    limiter_.reset();
    limiter_.setCeiling(params_.outputCeiling);
}

void AncProcessor::reset() {
    highPass_.reset();
    lowPass_.reset();
    predictor_.reset();
    limiter_.reset();
}

void AncProcessor::processBlock(const float* input, float* output, size_t numFrames) {
    const float strength = params_.strength;
    for (size_t i = 0; i < numFrames; ++i) {
        float x = input[i];
        if (!std::isfinite(x)) x = 0.0f;
        x = std::clamp(x, -kMaxInputMagnitude, kMaxInputMagnitude);

        x = highPass_.process(x);
        x = lowPass_.process(x);

        const float predicted = predictor_.process(x);

        // Inverted, strength-scaled forecast of the noise: the core
        // "anti-noise" generation step.
        float antiNoise = -strength * predicted;

        output[i] = limiter_.process(antiNoise);
    }
}

} // namespace onc::dsp
