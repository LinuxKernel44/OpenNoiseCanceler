#include "biquad_filter.h"

#include <cmath>

namespace onc::dsp {

float BiquadFilter::cosApprox(float x) { return std::cos(x); }
float BiquadFilter::sinApprox(float x) { return std::sin(x); }

} // namespace onc::dsp
