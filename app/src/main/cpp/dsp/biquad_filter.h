#pragma once

namespace onc::dsp {

// Direct Form I biquad, coefficients derived from the Audio EQ Cookbook
// (Robert Bristow-Johnson). Used to band-limit the reference/error signals
// before adaptive filtering: the NLMS filter is only asked to model the low
// frequency range where the system's end-to-end latency budget still allows
// phase-accurate cancellation (see docs/DSP_DESIGN.md).
class BiquadFilter {
public:
    void reset() {
        x1_ = x2_ = y1_ = y2_ = 0.0f;
    }

    void setLowPass(float sampleRateHz, float cutoffHz, float q) {
        const float w0 = kPi2 * (cutoffHz / sampleRateHz);
        const float cosw0 = cosApprox(w0);
        const float sinw0 = sinApprox(w0);
        const float alpha = sinw0 / (2.0f * q);

        const float b0 = (1.0f - cosw0) / 2.0f;
        const float b1 = 1.0f - cosw0;
        const float b2 = (1.0f - cosw0) / 2.0f;
        const float a0 = 1.0f + alpha;
        const float a1 = -2.0f * cosw0;
        const float a2 = 1.0f - alpha;
        normalizeAndStore(b0, b1, b2, a0, a1, a2);
    }

    void setHighPass(float sampleRateHz, float cutoffHz, float q) {
        const float w0 = kPi2 * (cutoffHz / sampleRateHz);
        const float cosw0 = cosApprox(w0);
        const float sinw0 = sinApprox(w0);
        const float alpha = sinw0 / (2.0f * q);

        const float b0 = (1.0f + cosw0) / 2.0f;
        const float b1 = -(1.0f + cosw0);
        const float b2 = (1.0f + cosw0) / 2.0f;
        const float a0 = 1.0f + alpha;
        const float a1 = -2.0f * cosw0;
        const float a2 = 1.0f - alpha;
        normalizeAndStore(b0, b1, b2, a0, a1, a2);
    }

    float process(float x0) {
        const float y0 = b0_ * x0 + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
        x2_ = x1_;
        x1_ = x0;
        y2_ = y1_;
        y1_ = y0;
        return y0;
    }

private:
    static constexpr float kPi2 = 6.28318530717958647692f;

    void normalizeAndStore(float b0, float b1, float b2, float a0, float a1, float a2) {
        const float invA0 = 1.0f / a0;
        b0_ = b0 * invA0;
        b1_ = b1 * invA0;
        b2_ = b2 * invA0;
        a1_ = a1 * invA0;
        a2_ = a2 * invA0;
    }

    // Real trig functions are perfectly fine here: coefficients are computed
    // only when a preset/sample-rate changes, never inside the per-sample
    // hot loop.
    static float cosApprox(float x);
    static float sinApprox(float x);

    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float x1_ = 0.0f, x2_ = 0.0f;
    float y1_ = 0.0f, y2_ = 0.0f;
};

} // namespace onc::dsp
