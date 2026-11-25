#pragma once

// The harness builds generated copies of the cycle and civil-war sources in which every constant is
// read through these, so a run can override any of them from the environment without a rebuild.

#include "utility/FixedDecimal.h"

#include <cstdint>
#include <cstdio>

namespace knobs {

using rota::Fixed64;

// The environment variable NAME as a decimal ("0.015", "-10"), else the source's value
Fixed64 fixed(const char* name, Fixed64 value);
std::uint32_t whole(const char* name, std::uint32_t value);

// Every knob with its source value and the value in use
void list(std::FILE* out);

}
