#pragma once

#include "FixedDecimal.h"

// Deterministic fixed-point transcendentals for Fixed64 (6-decimal). Integer-only math,
// so results are bit-identical across platforms (needed for the lockstep checksum).
// Accuracy target ~1e-4 over the model's domain; the display gauges reuse these too.
// Domain assumptions: sqrt(x>=0); ln(x>0); pow(base>0); exp(x) roughly in [-40, 40].

namespace rota::fmath {

Fixed64 sqrt(Fixed64 x);
Fixed64 exp(Fixed64 x);
Fixed64 ln(Fixed64 x);
Fixed64 pow(Fixed64 base, Fixed64 exponent);
Fixed64 logistic(Fixed64 x, Fixed64 k, Fixed64 x0);

}
