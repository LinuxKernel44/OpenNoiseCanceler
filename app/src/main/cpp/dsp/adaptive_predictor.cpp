#include "adaptive_predictor.h"

#include <algorithm>
#include <cmath>

namespace onc::dsp {

namespace {
inline float sanitize(float v) {
    return std::isfinite(v) ? v : 0.0f;
}
} // namespace

AdaptivePredictor::AdaptivePredictor(size_t numTaps, size_t delaySamples, float stepSize, float leakage)
    : numTaps_(std::max<size_t>(numTaps, 1)),
      delaySamples_(std::max<size_t>(delaySamples, 1)),
      stepSize_(stepSize),
      leakage_(leakage),
      historyCapacity_(numTaps_ + delaySamples_),
      history_(historyCapacity_, 0.0f),
      weights_(numTaps_, 0.0f),
      scratchPast_(numTaps_, 0.0f),
      scratchNow_(numTaps_, 0.0f) {}

void AdaptivePredictor::reset() {
    std::fill(history_.begin(), history_.end(), 0.0f);
    std::fill(weights_.begin(), weights_.end(), 0.0f);
    writeIndex_ = 0;
    samplesSeen_ = 0;
    errorRms_ = 0.0f;
}

void AdaptivePredictor::setDelaySamples(size_t delaySamples) {
    delaySamples = std::max<size_t>(delaySamples, 1);
    if (delaySamples == delaySamples_) return;
    delaySamples_ = delaySamples;
    historyCapacity_ = numTaps_ + delaySamples_;
    history_.assign(historyCapacity_, 0.0f);
    writeIndex_ = 0;
    samplesSeen_ = 0;
    // Weights are kept: the predictor's frequency-domain model of the noise
    // is still a reasonable starting point even after a delay re-estimate.
}

void AdaptivePredictor::readWindow(size_t samplesAgo, float* out) const {
    const size_t cap = historyCapacity_;
    for (size_t k = 0; k < numTaps_; ++k) {
        const size_t offset = samplesAgo + k;
        // Index of x[n - offset], where x[n] was the most recently written
        // sample (at index writeIndex_ - 1, mod cap).
        const size_t idx = (writeIndex_ + cap - 1 - (offset % cap)) % cap;
        out[k] = history_[idx];
    }
}

float AdaptivePredictor::process(float currentSample) {
    currentSample = sanitize(currentSample);
    currentSample = std::clamp(currentSample, -kMaxSampleMagnitude, kMaxSampleMagnitude);

    history_[writeIndex_] = currentSample;
    writeIndex_ = (writeIndex_ + 1) % historyCapacity_;
    ++samplesSeen_;

    float* uPast = scratchPast_.data();
    readWindow(delaySamples_, uPast);

    float predictedForNow = 0.0f;
    for (size_t k = 0; k < numTaps_; ++k) {
        predictedForNow += weights_[k] * uPast[k];
    }
    predictedForNow = sanitize(predictedForNow);

    const float error = currentSample - predictedForNow;

    float pastEnergy = kEpsilon;
    for (size_t k = 0; k < numTaps_; ++k) {
        pastEnergy += uPast[k] * uPast[k];
    }

    const float scale = stepSize_ * error / pastEnergy;
    for (size_t k = 0; k < numTaps_; ++k) {
        float w = weights_[k] + scale * uPast[k] - leakage_ * weights_[k];
        w = sanitize(w);
        w = std::clamp(w, -kMaxWeightMagnitude, kMaxWeightMagnitude);
        weights_[k] = w;
    }

    const float errSq = error * error;
    const float smoothedSq = kErrorRmsSmoothing * (errorRms_ * errorRms_) + (1.0f - kErrorRmsSmoothing) * errSq;
    errorRms_ = std::sqrt(std::max(smoothedSq, 0.0f));

    float* uNow = scratchNow_.data();
    readWindow(0, uNow);

    float predictedFuture = 0.0f;
    for (size_t k = 0; k < numTaps_; ++k) {
        predictedFuture += weights_[k] * uNow[k];
    }
    return sanitize(predictedFuture);
}

} // namespace onc::dsp
