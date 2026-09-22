#pragma once

#include <cstddef>
#include <vector>

namespace onc::dsp {

// Self-supervised NLMS adaptive linear predictor.
//
// Why a *predictor* and not a classic error-driven LMS/FxLMS filter: genuine
// closed-loop ANC adaptation needs an error microphone at (or very near) the
// ear so the algorithm can measure how well cancellation is working and
// correct itself. The target hardware (phone body microphone + passive
// Sennheiser CX 80S with no in-ear mic) has no such signal available during
// normal operation. There is therefore nothing physically available to
// adapt a classic FxLMS filter against.
//
// What IS available, without any error microphone, is the reference signal
// itself observed over time. This class adapts an FIR predictor to forecast
// `delaySamples` into the future using only past reference samples, trained
// by comparing each new real sample against the prediction made for it
// `delaySamples` ago (a fully self-supervised target — no ear signal
// required). Feeding the forecast, inverted, to the output lets the
// anti-noise signal be roughly time-aligned with the real noise once it
// reaches the ear after the measured pipeline latency, instead of always
// lagging behind it by that same latency. This only helps for the
// slowly-varying, narrowband, low-frequency content the predictor can
// actually forecast (e.g. engine/fan/HVAC drone) — it cannot and does not
// attempt to predict broadband or transient noise, which is filtered out
// upstream (see BiquadFilter low-pass stage in AncProcessor).
class AdaptivePredictor {
public:
    // numTaps: predictor filter length.
    // delaySamples: how many samples ahead the predictor forecasts; should
    //   track the measured end-to-end pipeline latency.
    // stepSize: NLMS adaptation rate (mu), typically 0 < mu <= 1.
    // leakage: per-sample weight leakage (0 = none, small positive value
    //   e.g. 1e-5 pulls weights gently toward zero to bound them during
    //   silence or non-stationary input).
    AdaptivePredictor(size_t numTaps, size_t delaySamples, float stepSize, float leakage);

    void reset();

    void setDelaySamples(size_t delaySamples);
    void setStepSize(float stepSize) { stepSize_ = stepSize; }
    void setLeakage(float leakage) { leakage_ = leakage; }

    size_t numTaps() const { return numTaps_; }
    size_t delaySamples() const { return delaySamples_; }

    // Feeds one new reference sample, adapts the filter against the
    // self-supervised target, and returns the forecast of the sample
    // `delaySamples_` steps into the future.
    float process(float currentSample);

    // Root-mean-square of recent self-supervised prediction error, useful
    // as a diagnostics/convergence indicator. Not a measurement of acoustic
    // cancellation at the ear (see class comment).
    float predictionErrorRms() const { return errorRms_; }

private:
    size_t numTaps_;
    size_t delaySamples_;
    float stepSize_;
    float leakage_;

    // History buffer holds the most recent (numTaps_ + delaySamples_)
    // samples so both the "now" window and the "delaySamples_ ago" window
    // used for training can be read from the same storage.
    //
    // historyCapacity_ MUST stay declared before history_: members are
    // constructed in declaration order regardless of member-initializer-list
    // order, and history_'s constructor reads historyCapacity_'s value.
    size_t historyCapacity_;
    std::vector<float> history_;
    size_t writeIndex_ = 0;
    size_t samplesSeen_ = 0;

    std::vector<float> weights_;

    // Pre-allocated scratch space reused every call so `process()` never
    // allocates on the real-time audio thread.
    std::vector<float> scratchPast_;
    std::vector<float> scratchNow_;

    float errorRms_ = 0.0f;
    static constexpr float kErrorRmsSmoothing = 0.99f;
    static constexpr float kEpsilon = 1.0e-6f;
    static constexpr float kMaxWeightMagnitude = 8.0f;
    // Real audio samples (normalized float PCM) never exceed [-1, 1]. This
    // bound is generous headroom above that, purely defensive: without it, a
    // single pathological sample (e.g. a hardware glitch producing a huge
    // but finite value) can make `pastEnergy` overflow to +Inf in
    // `process()`, which permanently zeroes the NLMS gradient and freezes
    // the filter's weights at zero until that one sample has fully aged out
    // of every internal delay line — for a large enough outlier this can
    // take far longer than is practical to wait, so it is clamped away here
    // instead of merely sanitized for finiteness.
    static constexpr float kMaxSampleMagnitude = 16.0f;

    // Reads `numTaps_` samples ending `samplesAgo` steps back into `out`,
    // most-recent-first.
    void readWindow(size_t samplesAgo, float* out) const;
};

} // namespace onc::dsp
