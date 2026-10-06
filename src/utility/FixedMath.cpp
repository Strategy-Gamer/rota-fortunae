#include "FixedMath.h"

namespace rota::fmath {

namespace {

constexpr int64_t SCALE = Fixed64::SCALE;               // 1'000'000
const Fixed64 ONE = Fixed64(static_cast<int64_t>(1));
const Fixed64 TWO = Fixed64(static_cast<int64_t>(2));
const Fixed64 LN2 = Fixed64::from_raw(693147);          // 0.693147

// Widest unsigned type for the sqrt intermediate. __int128 where available (matches
// FixedDecimal); the uint64 fallback (MSVC w/o __int128) still covers the model's domain
// (values < ~1e6, so raw*SCALE < 1e18 < uint64 max).
#if defined(__SIZEOF_INT128__)
using wide_t = unsigned __int128;
constexpr wide_t SQRT_TOP_BIT = (wide_t)1 << 126;
#else
using wide_t = uint64_t;
constexpr wide_t SQRT_TOP_BIT = (wide_t)1 << 62;
#endif

// Bit-by-bit integer sqrt (deterministic, no float).
uint64_t isqrt_wide(wide_t n) {
    wide_t res = 0, bit = SQRT_TOP_BIT;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
        else                {                 res >>= 1; }
        bit >>= 2;
    }
    return static_cast<uint64_t>(res);
}

} // namespace

// sqrt(x) = isqrt(raw * SCALE), since (sqrt(x)*SCALE)^2 = raw*SCALE.
Fixed64 sqrt(Fixed64 x) {
    int64_t r = x.raw_value();
    if (r <= 0) return Fixed64();
    wide_t v = (wide_t)(uint64_t)r * (uint64_t)SCALE;
    return Fixed64::from_raw(static_cast<int64_t>(isqrt_wide(v)));
}

// exp(x): range-reduce x = n*ln2 + r (|r| <= ln2/2), exp = 2^n * exp(r); exp(r) by Taylor.
Fixed64 exp(Fixed64 x) {
    Fixed64 q = x / LN2;
    int64_t half = SCALE / 2;
    int64_t n = (q.raw_value() + (q.raw_value() >= 0 ? half : -half)) / SCALE;  // round to nearest
    if (n >  60) n =  60;                                   // saturate: out of domain
    if (n < -60) n = -60;

    Fixed64 r = x - LN2 * n;                                // remainder in [-ln2/2, ln2/2]
    Fixed64 sum = ONE, term = ONE;
    for (int64_t k = 1; k <= 12; k++) { term = term * r / k; sum = sum + term; }

    if (n > 0) for (int64_t i = 0; i < n;  i++) sum = sum * static_cast<int64_t>(2);
    else       for (int64_t i = 0; i < -n; i++) sum = sum / static_cast<int64_t>(2);
    return sum;
}

// ln(x): x = m * 2^k with m in [1,2); ln(x) = k*ln2 + ln(m), ln(m) = 2*(u + u^3/3 + ...), u=(m-1)/(m+1).
Fixed64 ln(Fixed64 x) {
    if (x.raw_value() <= 0) x = Fixed64::from_raw(1);       // domain guard: clamp to tiny positive
    Fixed64 m = x;
    int64_t k = 0;
    while (m >= TWO) { m = m / static_cast<int64_t>(2); k++; }
    while (m <  ONE) { m = m * static_cast<int64_t>(2); k--; }

    Fixed64 u = (m - ONE) / (m + ONE);
    Fixed64 u2 = u * u;
    Fixed64 term = u, sum = u;
    for (int64_t j = 3; j <= 13; j += 2) { term = term * u2; sum = sum + term / j; }
    return LN2 * k + sum * static_cast<int64_t>(2);
}

// pow(base, e) = exp(e * ln(base)).
Fixed64 pow(Fixed64 base, Fixed64 exponent) {
    if (base.raw_value() <= 0) return Fixed64();
    return exp(exponent * ln(base));
}

// logistic(x,k,x0) = 1 / (1 + exp(-k*(x-x0))).
Fixed64 logistic(Fixed64 x, Fixed64 k, Fixed64 x0) {
    Fixed64 t = -(k * (x - x0));
    return ONE / (ONE + exp(t));
}

} // namespace rota::fmath
