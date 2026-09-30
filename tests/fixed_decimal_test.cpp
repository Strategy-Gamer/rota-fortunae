// Standalone correctness/determinism test for src/utility/FixedDecimal.h
//
// This file is the safety net for the single most correctness-critical header in the
// project: if a future edit reintroduces a SCALE error (as the original Fixed*int /
// Fixed/int overloads had), these checks fail loudly instead of silently corrupting
// the economy or the desync checksum.
//
// It intentionally has NO godot-cpp dependency, so it compiles with a plain compiler.
// Run from the repo root:
//     g++ -std=c++17 -O2 -I. tests/fixed_decimal_test.cpp -o fixed_decimal_test && ./fixed_decimal_test
// Exit code is 0 on success, non-zero (number of failures) otherwise.
//
// Assertions compare raw_value() (the underlying scaled integer) for EXACTNESS.
// to_double() is used only in failure messages, never in a pass/fail decision.

#include "src/utility/FixedDecimal.h"

#include <cstdint>
#include <cstdio>
#include <limits>

using rota::Fixed32;
using rota::Fixed64;

static int g_pass = 0;
static int g_fail = 0;

// Compare a Fixed's raw storage against an expected raw integer (fully deterministic).
#define CHECK_RAW(label, expr, expected)                                              \
    do {                                                                              \
        auto _v = (expr);                                                             \
        long long _got = static_cast<long long>(_v.raw_value());                      \
        long long _exp = static_cast<long long>(expected);                            \
        if (_got == _exp) {                                                           \
            ++g_pass;                                                                 \
        } else {                                                                      \
            ++g_fail;                                                                 \
            std::printf("FAIL: %-28s raw=%lld (%.6f)  expected raw=%lld\n",           \
                        label, _got, _v.to_double(), _exp);                           \
        }                                                                             \
    } while (0)

#define CHECK_BOOL(label, expr)                                                       \
    do {                                                                              \
        if (expr) { ++g_pass; }                                                       \
        else { ++g_fail; std::printf("FAIL: %-28s (bool false)\n", label); }          \
    } while (0)

int main() {
    // ---- Construction & from_raw -------------------------------------------------
    CHECK_RAW("F32 default",        Fixed32(),                              0);
    CHECK_RAW("F32 whole(int64)",   Fixed32(int64_t(5)),                    5000);
    CHECK_RAW("F32 from_raw",       Fixed32::from_raw(1234),                1234);
    CHECK_RAW("F64 default",        Fixed64(),                              0);
    CHECK_RAW("F64 whole(int64)",   Fixed64(int64_t(5)),                    5000000LL);
    CHECK_RAW("F64 from_raw",       Fixed64::from_raw(1234567),             1234567);

    // ---- Assignment --------------------------------------------------------------
    { Fixed32 a; a = int64_t(3); CHECK_RAW("F32 assign int64", a, 3000); }
    { Fixed64 b; b = int64_t(3); CHECK_RAW("F64 assign int64", b, 3000000LL); }

    // ---- Add / sub / negate (Fixed, Fixed) --------------------------------------
    CHECK_RAW("F32 2+3",            Fixed32(int64_t(2)) + Fixed32(int64_t(3)),  5000);
    CHECK_RAW("F32 2-5",            Fixed32(int64_t(2)) - Fixed32(int64_t(5)), -3000);
    CHECK_RAW("F32 negate",        -Fixed32(int64_t(2)),                    -2000);
    CHECK_RAW("F64 2+3",            Fixed64(int64_t(2)) + Fixed64(int64_t(3)),  5000000LL);
    CHECK_RAW("F64 2-5",            Fixed64(int64_t(2)) - Fixed64(int64_t(5)), -3000000LL);

    // ---- Mul / div (Fixed, Fixed) — untouched by the fix, must still hold --------
    CHECK_RAW("F32 1.5*2.0",        Fixed32::from_raw(1500) * Fixed32(int64_t(2)), 3000);
    CHECK_RAW("F32 3.0/2.0",        Fixed32(int64_t(3)) / Fixed32(int64_t(2)),     1500);
    CHECK_RAW("F32 1.0/3.0",        Fixed32(int64_t(1)) / Fixed32(int64_t(3)),      333);
    CHECK_RAW("F64 2.0*3.0",        Fixed64(int64_t(2)) * Fixed64(int64_t(3)),  6000000LL);
    CHECK_RAW("F64 1.0/3.0",        Fixed64(int64_t(1)) / Fixed64(int64_t(3)),   333333LL);

    // Division by zero saturates to type extremes, sign of numerator.
    CHECK_RAW("F32 5/0 -> max",     Fixed32(int64_t(5))  / Fixed32(),  std::numeric_limits<int32_t>::max());
    CHECK_RAW("F32 -5/0 -> min",    Fixed32(int64_t(-5)) / Fixed32(),  std::numeric_limits<int32_t>::min());

    // ---- Mul / div (Fixed, whole int) — THE REGRESSION GUARD --------------------
    // Before the fix these divided by SCALE and were wrong by a factor of 1000 / 1e6.
    CHECK_RAW("F32 2.0*3(i32)",     Fixed32(int64_t(2)) * 3,             6000);
    CHECK_RAW("F32 -7.0*3(i32)",    Fixed32(int64_t(-7)) * 3,          -21000);
    CHECK_RAW("F32 2.0*3(i64)",     Fixed32(int64_t(2)) * int64_t(3),    6000);
    CHECK_RAW("F32 1.0/200(i32)",   Fixed32(int64_t(1)) / 200,              5);   // 0.005
    CHECK_RAW("F32 1.0/200(i64)",   Fixed32(int64_t(1)) / int64_t(200),     5);
    CHECK_RAW("F32 7.0/2(i32)",     Fixed32(int64_t(7)) / 2,             3500);   // 3.5
    CHECK_RAW("F32 0.005/2 round",  Fixed32::from_raw(5) / 2,               3);   // 0.0025 -> 0.003 (half away)
    CHECK_RAW("F32 -0.005/2 round", Fixed32::from_raw(-5) / 2,             -3);
    CHECK_RAW("F32 5/0(i32)->max",  Fixed32(int64_t(5))  / 0, std::numeric_limits<int32_t>::max());
    CHECK_RAW("F32 -5/0(i32)->min", Fixed32(int64_t(-5)) / 0, std::numeric_limits<int32_t>::min());

    CHECK_RAW("F64 1.0*1000(i64)",  Fixed64(int64_t(1)) * int64_t(1000), 1000000000LL);  // 1000.0
    CHECK_RAW("F64 1.0/3(i64)",     Fixed64(int64_t(1)) / int64_t(3),        333333LL);
    CHECK_RAW("F64 2.0*3(i32)",     Fixed64(int64_t(2)) * 3,             6000000LL);
    CHECK_RAW("F64 6.0/2(i32)",     Fixed64(int64_t(6)) / 2,             3000000LL);

    // The actual economy use: wealth = productivity(Fixed32->Fixed64) * population(int).
    // productivity 2.5, population 10,000,000,000 -> wealth 25,000,000,000.
    // Exercises the __int128 path (raw = 2.5e16, well within int64).
    CHECK_RAW("F64 wealth 2.5*1e10",
              Fixed64(Fixed32::from_raw(2500)) * int64_t(10000000000LL),
              25000000000000000LL);

    // ---- Add / sub (Fixed, whole int) — were already correct --------------------
    CHECK_RAW("F32 2.0+3(i32)",     Fixed32(int64_t(2)) + 3,             5000);
    CHECK_RAW("F32 2.0-5(i32)",     Fixed32(int64_t(2)) - 5,            -3000);
    CHECK_RAW("F64 2.0+3(i64)",     Fixed64(int64_t(2)) + int64_t(3),   5000000LL);

    // ---- Non-member  int (op) Fixed  (routes through member operators) ----------
    CHECK_RAW("i32 3*2.5",          3 * Fixed32::from_raw(2500),         7500);
    CHECK_RAW("i32 3+2.0",          3 + Fixed32(int64_t(2)),             5000);
    CHECK_RAW("i32 10-3.0",         10 - Fixed32(int64_t(3)),            7000);
    CHECK_RAW("i32 6/2.0",          6 / Fixed32(int64_t(2)),             3000);
    CHECK_RAW("i32 3*2.0 (F64)",    3 * Fixed64(int64_t(2)),         6000000LL);
    CHECK_RAW("i64 10+2.0 (F64)",   int64_t(10) + Fixed64(int64_t(2)), 12000000LL);

    // ---- Comparisons -------------------------------------------------------------
    CHECK_BOOL("F32 == F32",        Fixed32(int64_t(2)) == Fixed32(int64_t(2)));
    CHECK_BOOL("F32 < F32",         Fixed32::from_raw(1500) < Fixed32::from_raw(1501));
    CHECK_BOOL("F32 == i32",        Fixed32(int64_t(2)) == int32_t(2));
    CHECK_BOOL("F32 > i32",         Fixed32::from_raw(2500) > int32_t(2));
    CHECK_BOOL("F32 < i32",         Fixed32::from_raw(2500) < int32_t(3));
    CHECK_BOOL("F32 == i64",        Fixed32(int64_t(2)) == int64_t(2));
    CHECK_BOOL("F64 == i64",        Fixed64(int64_t(2)) == int64_t(2));
    CHECK_BOOL("F64 > i64",         Fixed64(int64_t(2)) > int64_t(1));

    // ---- Conversions Fixed32 <-> Fixed64 ----------------------------------------
    CHECK_RAW("F32->F64 1.005",     Fixed64(Fixed32::from_raw(1005)),    1005000LL);
    CHECK_RAW("F32->F64 via cast",  static_cast<Fixed64>(Fixed32::from_raw(1500)), 1500000LL);
    CHECK_RAW("F64->F32 exact",     static_cast<Fixed32>(Fixed64::from_raw(1005000)), 1005);
    CHECK_RAW("F64->F32 round up",  static_cast<Fixed32>(Fixed64::from_raw(1005500)), 1006);
    CHECK_RAW("F64->F32 round dn",  static_cast<Fixed32>(Fixed64::from_raw(1004499)), 1004);

    // ---- Summary -----------------------------------------------------------------
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
