#include "knobs.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace knobs {
namespace {

struct Entry {
    std::string name, source, used;
};

std::vector<Entry>& registry() {
    static std::vector<Entry> entries; // constants initialize before main, in any order
    return entries;
}

std::string decimal(std::int64_t raw) {
    const bool negative = raw < 0;
    const std::uint64_t mag = negative ? static_cast<std::uint64_t>(-raw) : static_cast<std::uint64_t>(raw);
    std::string frac = std::to_string(mag % Fixed64::SCALE);
    frac = std::string(6 - frac.size(), '0') + frac;
    while (!frac.empty() && frac.back() == '0') frac.pop_back();
    return (negative ? "-" : "") + std::to_string(mag / Fixed64::SCALE) + (frac.empty() ? "" : "." + frac);
}

[[noreturn]] void reject(const char* name, const char* text) {
    std::fprintf(stderr, "knob %s: '%s' is not a decimal with at most 6 places\n", name, text);
    std::exit(2);
}

// Exact: digits map straight onto Fixed64's raw micro-units, so no float touches sim state
std::int64_t parse(const char* name, const char* text) {
    const char* p = text;
    const bool negative = *p == '-';
    if (negative || *p == '+') p++;
    std::int64_t whole = 0, frac = 0;
    int places = 0;
    bool digits = false;
    for (; *p >= '0' && *p <= '9'; p++, digits = true) whole = whole * 10 + (*p - '0');
    if (*p == '.') {
        for (p++; *p >= '0' && *p <= '9'; p++, digits = true) {
            if (++places > 6) reject(name, text);
            frac = frac * 10 + (*p - '0');
        }
    }
    if (*p != '\0' || !digits) reject(name, text);
    for (; places < 6; places++) frac *= 10;
    const std::int64_t raw = whole * Fixed64::SCALE + frac;
    return negative ? -raw : raw;
}

}

Fixed64 fixed(const char* name, Fixed64 value) {
    const char* text = std::getenv(name);
    const Fixed64 used = text ? Fixed64::from_raw(parse(name, text)) : value;
    registry().push_back({ name, decimal(value.raw_value()), decimal(used.raw_value()) });
    return used;
}

std::uint32_t whole(const char* name, std::uint32_t value) {
    const char* text = std::getenv(name);
    std::uint32_t used = value;
    if (text) {
        const std::int64_t raw = parse(name, text);
        if (raw < 0 || raw % Fixed64::SCALE != 0) reject(name, text);
        used = static_cast<std::uint32_t>(raw / Fixed64::SCALE);
    }
    registry().push_back({ name, std::to_string(value), std::to_string(used) });
    return used;
}

void list(std::FILE* out) {
    for (const Entry& e : registry()) {
        std::fprintf(out, "%s %s %s\n", e.name.c_str(), e.source.c_str(), e.used.c_str());
    }
}

}
