#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

// Fixed-point decimal classes for deterministic arithmetic in Rota Fortunae.
// Uses pure integer math (base-10 scaling) to ensure identical results across all hardware/compilers.
// No floating point used in any arithmetic operations or comparisons.
//
// Fixed32: int32_t storage, 3 decimal places (SCALE=1000). Range ~ -2.147M to +2.147M
// Fixed64: int64_t storage, 6 decimal places (SCALE=1_000_000). Range ~ -9.22T to +9.22T
//
// WARNING: Arithmetic can overflow the storage type (wraps in 2's complement, which is deterministic but loses correctness).
//          Keep values within representable range for your game systems. Consider saturation in hot paths if needed.
//          For MSVC portability with Fixed64 mul/div: __int128 is used (supported in recent MSVC/clang-cl or use /Zc:__int128).
//          If compiling without __int128, replace the __int128 sections with a portable 128-bit mul/div implementation.

namespace rota {

class Fixed64; // forward declaration for conversion operators

namespace detail {

// Round half away from zero (ties round away from zero). Deterministic, no float.
inline int64_t round_div(int64_t num, int64_t den) {
    if (den == 0) {
        throw std::runtime_error("Fixed point division by zero");
    }
    int64_t abs_num = (num < 0 ? -num : num);
    int64_t abs_den = (den < 0 ? -den : den);
    int64_t res = (abs_num + (abs_den / 2)) / abs_den;
    if ((num < 0) != (den < 0)) {
        res = -res;
    }
    return res;
}

// Safe scaled value computation for comparisons/ops with large ints (prevents signed overflow UB)
template <typename Storage>
inline int64_t safe_scale_whole(int64_t whole, int64_t scale) {
    constexpr int64_t max_val = std::numeric_limits<int64_t>::max();
    constexpr int64_t min_val = std::numeric_limits<int64_t>::min();
    if (whole > (max_val / scale)) return max_val; // saturate for safety in compare/assign path
    if (whole < (min_val / scale)) return min_val;
    return whole * scale;
}

} // namespace detail

// ============================================================================
// Fixed32 - 32-bit storage, 3 decimal places
// ============================================================================
class Fixed32 {
public:
    using storage_type = int32_t;
    static constexpr storage_type SCALE = 1000;

private:
    storage_type value = 0;

    // Internal raw constructor
    explicit Fixed32(storage_type raw, bool /*tag*/) : value(raw) {}

public:
    Fixed32() = default;

    // From whole number (e.g. Fixed32(5) == 5.000)
    explicit Fixed32(storage_type whole) : value(static_cast<storage_type>(static_cast<int64_t>(whole) * SCALE)) {}
    explicit Fixed32(int64_t whole) : value(static_cast<storage_type>(detail::safe_scale_whole<int32_t>(whole, SCALE))) {}

    // From raw scaled integer (e.g. Fixed32::from_raw(1234) == 1.234)
    static Fixed32 from_raw(storage_type raw_value) {
        Fixed32 f;
        f.value = raw_value;
        return f;
    }

    storage_type raw_value() const { return value; }

    // For debug/UI only - never use in simulation logic
    double to_double() const { return static_cast<double>(value) / SCALE; }
    int32_t to_int_trunc() const { return value / SCALE; } // towards zero

    // Assignment from whole numbers
    Fixed32& operator=(storage_type whole) {
        value = static_cast<storage_type>(static_cast<int64_t>(whole) * SCALE);
        return *this;
    }
    Fixed32& operator=(int64_t whole) {
        value = static_cast<storage_type>(detail::safe_scale_whole<int32_t>(whole, SCALE));
        return *this;
    }

    // --- Arithmetic with Fixed32 ---
    Fixed32 operator+(Fixed32 rhs) const {
        int64_t res = static_cast<int64_t>(value) + rhs.value;
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator+=(Fixed32 rhs) { *this = *this + rhs; return *this; }

    Fixed32 operator-(Fixed32 rhs) const {
        int64_t res = static_cast<int64_t>(value) - rhs.value;
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator-=(Fixed32 rhs) { *this = *this - rhs; return *this; }

    Fixed32 operator-() const { return from_raw(static_cast<storage_type>(-static_cast<int64_t>(value))); }

    Fixed32 operator*(Fixed32 rhs) const {
        int64_t prod = static_cast<int64_t>(value) * rhs.value;
        int64_t res = detail::round_div(prod, SCALE);
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator*=(Fixed32 rhs) { *this = *this * rhs; return *this; }

    Fixed32 operator/(Fixed32 rhs) const {
        if (rhs.value == 0) {
            return from_raw((value >= 0) ? std::numeric_limits<storage_type>::max() : std::numeric_limits<storage_type>::min());
        }
        int64_t num = static_cast<int64_t>(value) * SCALE;
        int64_t res = detail::round_div(num, rhs.value);
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator/=(Fixed32 rhs) { *this = *this / rhs; return *this; }

    // --- Arithmetic with int32_t (whole numbers) ---
    Fixed32 operator+(int32_t rhs) const {
        int64_t res = static_cast<int64_t>(value) + static_cast<int64_t>(rhs) * SCALE;
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator+=(int32_t rhs) { *this = *this + rhs; return *this; }

    Fixed32 operator-(int32_t rhs) const {
        int64_t res = static_cast<int64_t>(value) - static_cast<int64_t>(rhs) * SCALE;
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator-=(int32_t rhs) { *this = *this - rhs; return *this; }

    Fixed32 operator*(int32_t rhs) const {
        int64_t prod = static_cast<int64_t>(value) * rhs;
        int64_t half = SCALE / 2;
        int64_t res = (prod >= 0) ? ((prod + half) / SCALE) : ((prod - half) / SCALE);
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator*=(int32_t rhs) { *this = *this * rhs; return *this; }

    Fixed32 operator/(int32_t rhs) const {
        if (rhs == 0) {
            return from_raw((value >= 0) ? std::numeric_limits<storage_type>::max() : std::numeric_limits<storage_type>::min());
        }
        int64_t num = value;
        int64_t den = static_cast<int64_t>(rhs) * SCALE;
        int64_t res = detail::round_div(num, den);
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator/=(int32_t rhs) { *this = *this / rhs; return *this; }

    // --- Arithmetic with int64_t (whole numbers) - same pattern, safe scaling ---
    Fixed32 operator+(int64_t rhs) const {
        int64_t add = detail::safe_scale_whole<int32_t>(rhs, SCALE);
        int64_t res = static_cast<int64_t>(value) + add;
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator+=(int64_t rhs) { *this = *this + rhs; return *this; }

    Fixed32 operator-(int64_t rhs) const {
        int64_t sub = detail::safe_scale_whole<int32_t>(rhs, SCALE);
        int64_t res = static_cast<int64_t>(value) - sub;
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator-=(int64_t rhs) { *this = *this - rhs; return *this; }

    Fixed32 operator*(int64_t rhs) const {
        // For large rhs this will likely overflow range - documented behavior
        int64_t prod = static_cast<int64_t>(value) * rhs;
        int64_t half = SCALE / 2;
        int64_t res = (prod >= 0) ? ((prod + half) / SCALE) : ((prod - half) / SCALE);
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator*=(int64_t rhs) { *this = *this * rhs; return *this; }

    Fixed32 operator/(int64_t rhs) const {
        if (rhs == 0) {
            return from_raw((value >= 0) ? std::numeric_limits<storage_type>::max() : std::numeric_limits<storage_type>::min());
        }
        int64_t den = detail::safe_scale_whole<int32_t>(rhs, SCALE);
        if (den == 0) { // underflowed to 0 from huge rhs? unlikely
            return from_raw(0);
        }
        int64_t res = detail::round_div(value, den);
        return from_raw(static_cast<storage_type>(res));
    }
    Fixed32& operator/=(int64_t rhs) { *this = *this / rhs; return *this; }

    // --- Comparisons with Fixed32 ---
    bool operator==(Fixed32 rhs) const { return value == rhs.value; }
    bool operator!=(Fixed32 rhs) const { return value != rhs.value; }
    bool operator<(Fixed32 rhs) const { return value < rhs.value; }
    bool operator>(Fixed32 rhs) const { return value > rhs.value; }
    bool operator<=(Fixed32 rhs) const { return value <= rhs.value; }
    bool operator>=(Fixed32 rhs) const { return value >= rhs.value; }

    // --- Comparisons with int32_t ---
    bool operator==(int32_t rhs) const { return value == static_cast<int64_t>(rhs) * SCALE; }
    bool operator!=(int32_t rhs) const { return !(*this == rhs); }
    bool operator<(int32_t rhs) const { return static_cast<int64_t>(value) < static_cast<int64_t>(rhs) * SCALE; }
    bool operator>(int32_t rhs) const { return static_cast<int64_t>(value) > static_cast<int64_t>(rhs) * SCALE; }
    bool operator<=(int32_t rhs) const { return !(*this > rhs); }
    bool operator>=(int32_t rhs) const { return !(*this < rhs); }

    // --- Comparisons with int64_t (safe) ---
    bool operator==(int64_t rhs) const {
        int64_t s = detail::safe_scale_whole<int32_t>(rhs, SCALE);
        if (rhs != 0 && s == 0) return false; // scaled to 0 but rhs !=0 means out of range
        return static_cast<int64_t>(value) == s;
    }
    bool operator!=(int64_t rhs) const { return !(*this == rhs); }
    bool operator<(int64_t rhs) const {
        constexpr int64_t max_s = std::numeric_limits<int64_t>::max() / SCALE;
        constexpr int64_t min_s = std::numeric_limits<int64_t>::min() / SCALE;
        if (rhs > max_s) return true;
        if (rhs < min_s) return false;
        int64_t s = rhs * SCALE;
        return static_cast<int64_t>(value) < s;
    }
    bool operator>(int64_t rhs) const {
        constexpr int64_t max_s = std::numeric_limits<int64_t>::max() / SCALE;
        constexpr int64_t min_s = std::numeric_limits<int64_t>::min() / SCALE;
        if (rhs > max_s) return false;
        if (rhs < min_s) return true;
        int64_t s = rhs * SCALE;
        return static_cast<int64_t>(value) > s;
    }
    bool operator<=(int64_t rhs) const { return !(*this > rhs); }
    bool operator>=(int64_t rhs) const { return !(*this < rhs); }

    // Conversion to Fixed64 (higher precision, exact)
    explicit operator Fixed64() const;
};

// ============================================================================
// Fixed64 - 64-bit storage, 6 decimal places
// ============================================================================
class Fixed64 {
public:
    using storage_type = int64_t;
    static constexpr storage_type SCALE = 1000000LL;

private:
    storage_type value = 0;

    explicit Fixed64(storage_type raw, bool /*tag*/) : value(raw) {}

public:
    Fixed64() = default;

    explicit Fixed64(storage_type whole) : value(whole * SCALE) {}

    static Fixed64 from_raw(storage_type raw_value) {
        Fixed64 f;
        f.value = raw_value;
        return f;
    }

    storage_type raw_value() const { return value; }

    double to_double() const { return static_cast<double>(value) / SCALE; }
    int64_t to_int_trunc() const { return value / SCALE; }

    Fixed64& operator=(int64_t whole) {
        value = whole * SCALE;
        return *this;
    }

    // --- Arithmetic with Fixed64 ---
    Fixed64 operator+(Fixed64 rhs) const {
        // May overflow int64 - wraps (documented)
        return from_raw(value + rhs.value);
    }
    Fixed64& operator+=(Fixed64 rhs) { *this = *this + rhs; return *this; }

    Fixed64 operator-(Fixed64 rhs) const {
        return from_raw(value - rhs.value);
    }
    Fixed64& operator-=(Fixed64 rhs) { *this = *this - rhs; return *this; }

    Fixed64 operator-() const { return from_raw(-value); }

    Fixed64 operator*(Fixed64 rhs) const {
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && defined(__SIZEOF_INT128__))
        __int128 prod = (__int128)value * rhs.value;
        __int128 half = SCALE / 2;
        __int128 res128 = (prod >= 0) ? ((prod + half) / SCALE) : ((prod - half) / SCALE);
        return from_raw(static_cast<storage_type>(res128));
#else
        // Portable fallback: may lose precision for very large values (>~2^53). Prefer __int128 build.
        // For full portability implement 128-bit mul here using high/low 32-bit parts.
        double approx = (static_cast<double>(value) * rhs.value) / SCALE;
        return from_raw(static_cast<storage_type>(detail::round_div(static_cast<int64_t>(approx), 1)));
#endif
    }
    Fixed64& operator*=(Fixed64 rhs) { *this = *this * rhs; return *this; }

    Fixed64 operator/(Fixed64 rhs) const {
        if (rhs.value == 0) {
            return from_raw((value >= 0) ? std::numeric_limits<storage_type>::max() : std::numeric_limits<storage_type>::min());
        }
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && defined(__SIZEOF_INT128__))
        __int128 num = (__int128)value * SCALE;
        __int128 den = rhs.value;
        __int128 half = den / 2;
        __int128 res128 = (num >= 0) ? ((num + half) / den) : ((num - half) / den);
        return from_raw(static_cast<storage_type>(res128));
#else
        double approx = (static_cast<double>(value) * SCALE) / rhs.value;
        return from_raw(static_cast<storage_type>(detail::round_div(static_cast<int64_t>(approx), 1)));
#endif
    }
    Fixed64& operator/=(Fixed64 rhs) { *this = *this / rhs; return *this; }

    // --- Arithmetic with int32_t / int64_t (whole) ---
    Fixed64 operator+(int64_t rhs) const {
        int64_t add = rhs * SCALE;
        return from_raw(value + add);
    }
    Fixed64& operator+=(int64_t rhs) { *this = *this + rhs; return *this; }

    Fixed64 operator-(int64_t rhs) const {
        int64_t sub = rhs * SCALE;
        return from_raw(value - sub);
    }
    Fixed64& operator-=(int64_t rhs) { *this = *this - rhs; return *this; }

    Fixed64 operator*(int64_t rhs) const {
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && defined(__SIZEOF_INT128__))
        __int128 prod = (__int128)value * rhs;
        __int128 half = SCALE / 2;
        __int128 res128 = (prod >= 0) ? ((prod + half) / SCALE) : ((prod - half) / SCALE);
        return from_raw(static_cast<storage_type>(res128));
#else
        double approx = static_cast<double>(value) * rhs / SCALE;
        return from_raw(static_cast<storage_type>(detail::round_div(static_cast<int64_t>(approx), 1)));
#endif
    }
    Fixed64& operator*=(int64_t rhs) { *this = *this * rhs; return *this; }

    Fixed64 operator/(int64_t rhs) const {
        if (rhs == 0) {
            return from_raw((value >= 0) ? std::numeric_limits<storage_type>::max() : std::numeric_limits<storage_type>::min());
        }
        int64_t den = rhs * SCALE;
        if (den == 0) return from_raw(0);
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && defined(__SIZEOF_INT128__))
        __int128 num = value;
        __int128 d = den;
        __int128 half = d / 2;
        __int128 res128 = (num >= 0) ? ((num + half) / d) : ((num - half) / d);
        return from_raw(static_cast<storage_type>(res128));
#else
        double approx = static_cast<double>(value) / den;
        return from_raw(static_cast<storage_type>(detail::round_div(static_cast<int64_t>(approx), 1)));
#endif
    }
    Fixed64& operator/=(int64_t rhs) { *this = *this / rhs; return *this; }

    // Note: int32_t versions delegate to int64_t above (implicit promotion fine)
    Fixed64 operator+(int32_t rhs) const { return *this + static_cast<int64_t>(rhs); }
    Fixed64& operator+=(int32_t rhs) { return *this += static_cast<int64_t>(rhs); }
    Fixed64 operator-(int32_t rhs) const { return *this - static_cast<int64_t>(rhs); }
    Fixed64& operator-=(int32_t rhs) { return *this -= static_cast<int64_t>(rhs); }
    Fixed64 operator*(int32_t rhs) const { return *this * static_cast<int64_t>(rhs); }
    Fixed64& operator*=(int32_t rhs) { return *this *= static_cast<int64_t>(rhs); }
    Fixed64 operator/(int32_t rhs) const { return *this / static_cast<int64_t>(rhs); }
    Fixed64& operator/=(int32_t rhs) { return *this /= static_cast<int64_t>(rhs); }

    // --- Comparisons with Fixed64 ---
    bool operator==(Fixed64 rhs) const { return value == rhs.value; }
    bool operator!=(Fixed64 rhs) const { return value != rhs.value; }
    bool operator<(Fixed64 rhs) const { return value < rhs.value; }
    bool operator>(Fixed64 rhs) const { return value > rhs.value; }
    bool operator<=(Fixed64 rhs) const { return value <= rhs.value; }
    bool operator>=(Fixed64 rhs) const { return value >= rhs.value; }

    // --- Comparisons with int64_t (and int32_t via promotion) ---
    bool operator==(int64_t rhs) const {
        int64_t s = rhs * SCALE;
        return value == s;
    }
    bool operator!=(int64_t rhs) const { return !(*this == rhs); }
    bool operator<(int64_t rhs) const {
        // For safety with extreme rhs (rare)
        if (rhs > (std::numeric_limits<int64_t>::max() / SCALE)) return true;
        if (rhs < (std::numeric_limits<int64_t>::min() / SCALE)) return false;
        return value < (rhs * SCALE);
    }
    bool operator>(int64_t rhs) const {
        if (rhs > (std::numeric_limits<int64_t>::max() / SCALE)) return false;
        if (rhs < (std::numeric_limits<int64_t>::min() / SCALE)) return true;
        return value > (rhs * SCALE);
    }
    bool operator<=(int64_t rhs) const { return !(*this > rhs); }
    bool operator>=(int64_t rhs) const { return !(*this < rhs); }

    // int32_t comparisons delegate
    bool operator==(int32_t rhs) const { return *this == static_cast<int64_t>(rhs); }
    bool operator!=(int32_t rhs) const { return !(*this == rhs); }
    bool operator<(int32_t rhs) const { return *this < static_cast<int64_t>(rhs); }
    bool operator>(int32_t rhs) const { return *this > static_cast<int64_t>(rhs); }
    bool operator<=(int32_t rhs) const { return *this <= static_cast<int64_t>(rhs); }
    bool operator>=(int32_t rhs) const { return *this >= static_cast<int64_t>(rhs); }

    // Conversion from/to Fixed32
    explicit Fixed64(Fixed32 f) : value(static_cast<storage_type>(f.raw_value()) * 1000LL) {}

    // Down-conversion from Fixed64 to Fixed32 (rounds extra 3 decimals, loses precision)
    explicit operator Fixed32() const;
};

// Out-of-class conversion Fixed32 -> Fixed64
inline Fixed32::operator Fixed64() const {
    return Fixed64(*this);
}

// Out-of-class conversion Fixed64 -> Fixed32 (rounds to 3 decimal places)
inline Fixed64::operator Fixed32() const {
    // Round the extra precision: divide raw by 1000 with proper rounding
    int64_t rounded = detail::round_div(value, 1000LL);
    // Clamp to int32_t range if necessary (rare for normal game values)
    if (rounded > std::numeric_limits<int32_t>::max()) {
        return Fixed32::from_raw(std::numeric_limits<int32_t>::max());
    }
    if (rounded < std::numeric_limits<int32_t>::min()) {
        return Fixed32::from_raw(std::numeric_limits<int32_t>::min());
    }
    return Fixed32::from_raw(static_cast<int32_t>(rounded));
}

// Non-member symmetric operators for int + Fixed (commutative ops)
inline Fixed32 operator+(int32_t lhs, Fixed32 rhs) { return rhs + lhs; }
inline Fixed32 operator+(int64_t lhs, Fixed32 rhs) { return rhs + lhs; }
inline Fixed32 operator*(int32_t lhs, Fixed32 rhs) { return rhs * lhs; }
inline Fixed32 operator*(int64_t lhs, Fixed32 rhs) { return rhs * lhs; }

inline Fixed64 operator+(int64_t lhs, Fixed64 rhs) { return rhs + lhs; }
inline Fixed64 operator+(int32_t lhs, Fixed64 rhs) { return rhs + static_cast<int64_t>(lhs); }
inline Fixed64 operator*(int64_t lhs, Fixed64 rhs) { return rhs * lhs; }
inline Fixed64 operator*(int32_t lhs, Fixed64 rhs) { return rhs * static_cast<int64_t>(lhs); }

// For subtraction/division the order matters, so int - Fixed32 etc. can be expressed as Fixed32(int) - fixed if needed.
// Example non-member for convenience:
inline Fixed32 operator-(int32_t lhs, Fixed32 rhs) { return Fixed32(lhs) - rhs; }
inline Fixed32 operator/(int32_t lhs, Fixed32 rhs) { return Fixed32(lhs) / rhs; }
inline Fixed64 operator-(int64_t lhs, Fixed64 rhs) { return Fixed64(lhs) - rhs; }
inline Fixed64 operator/(int64_t lhs, Fixed64 rhs) { return Fixed64(lhs) / rhs; }

} // namespace rota