#pragma once

namespace onc::dsp {

// Final safety stage before samples reach the output stream.
//
// This is a hard requirement, not a quality feature: the adaptive filter
// upstream must never be able to produce unbounded, NaN, or Inf output that
// reaches the headphones. This stage guarantees a bounded, finite signal no
// matter what the filter does.
class OutputLimiter {
public:
    explicit OutputLimiter(float ceiling = 0.9f) : ceiling_(ceiling) {}

    void reset() { envelope_ = 0.0f; }

    // Processes one sample: sanitizes non-finite input, applies a fast-attack
    // / slower-release peak limiter, then a final hard clamp as a last
    // resort so the returned value is always within [-ceiling, ceiling].
    float process(float x);

    void setCeiling(float ceiling) { ceiling_ = ceiling; }

private:
    float ceiling_;
    float envelope_ = 0.0f;
    static constexpr float kAttack = 0.6f;   // fast: clamp down quickly
    static constexpr float kRelease = 0.01f; // slow: recover gently
};

} // namespace onc::dsp
