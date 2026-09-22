#include "output_limiter.h"

#include <algorithm>
#include <cmath>

namespace onc::dsp {

float OutputLimiter::process(float x) {
    if (!std::isfinite(x)) {
        x = 0.0f;
    }

    const float absX = std::fabs(x);
    const float coeff = (absX > envelope_) ? kAttack : kRelease;
    envelope_ = envelope_ + coeff * (absX - envelope_);

    float gain = 1.0f;
    if (envelope_ > ceiling_) {
        gain = ceiling_ / (envelope_ + 1.0e-9f);
    }

    float y = x * gain;
    // Final hard clamp: guarantees the contract regardless of any edge case
    // above (e.g. a single-sample spike the envelope follower hasn't caught
    // up to yet).
    y = std::clamp(y, -ceiling_, ceiling_);
    return y;
}

} // namespace onc::dsp
