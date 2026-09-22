#include <cmath>
#include <limits>

#include "../app/src/main/cpp/dsp/output_limiter.h"
#include "test_framework.h"

using onc::dsp::OutputLimiter;

ONC_TEST(OutputLimiter_PassesSmallSignalsThroughApproximately) {
    OutputLimiter limiter(0.9f);
    float y = 0.0f;
    for (int i = 0; i < 100; ++i) {
        y = limiter.process(0.1f);
    }
    ONC_CHECK_TRUE_MSG(y > 0.05f && y <= 0.9f, "small steady signal should pass through close to unity gain");
}

ONC_TEST(OutputLimiter_ClampsLargeSignalsToCeiling) {
    OutputLimiter limiter(0.9f);
    for (int i = 0; i < 200; ++i) {
        const float y = limiter.process(5.0f);
        ONC_CHECK(y <= 0.9f + 1e-6f);
        ONC_CHECK(y >= -0.9f - 1e-6f);
    }
}

ONC_TEST(OutputLimiter_NeverExceedsCeilingForAnyFiniteInput) {
    OutputLimiter limiter(0.9f);
    const float values[] = {0.0f, 1.0f, -1.0f, 1000.0f, -1000.0f, 1e20f, -1e20f, 0.9f, -0.9f};
    for (float v : values) {
        const float y = limiter.process(v);
        ONC_CHECK_TRUE_MSG(std::isfinite(y), "limiter output must always be finite");
        ONC_CHECK_TRUE_MSG(y <= 0.9f + 1e-6f && y >= -0.9f - 1e-6f, "limiter output must respect ceiling");
    }
}

ONC_TEST(OutputLimiter_SanitizesNanAndInf) {
    OutputLimiter limiter(0.9f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float v : {nan, inf, -inf, nan}) {
        const float y = limiter.process(v);
        ONC_CHECK(std::isfinite(y));
        ONC_CHECK(y <= 0.9f + 1e-6f && y >= -0.9f - 1e-6f);
    }
}

ONC_TEST(OutputLimiter_ResetClearsEnvelope) {
    OutputLimiter limiter(0.9f);
    for (int i = 0; i < 100; ++i) limiter.process(5.0f);
    limiter.reset();
    const float y = limiter.process(0.05f);
    // Right after reset, a small signal should not still be suppressed by a
    // stale, large envelope value from before.
    ONC_CHECK_TRUE_MSG(y > 0.0f, "small signal right after reset should not be near-zero");
}
