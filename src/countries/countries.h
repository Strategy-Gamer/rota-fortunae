#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace rota::countries {

using CountryID = std::uint32_t;

inline constexpr CountryID INVALID_COUNTRY_ID =
    std::numeric_limits<CountryID>::max();

struct CountryStore {
    // All arrays are indexed by CountryID.
    std::vector<std::uint8_t> alive;
    std::vector<std::string> names;
    std::vector<std::uint32_t> display_color_rgb;

    [[nodiscard]]
    CountryID create_country(
        const std::string& name,
        std::uint32_t display_color
    );

    [[nodiscard]]
    bool is_valid(CountryID id) const noexcept;

    [[nodiscard]]
    std::uint32_t count() const noexcept;

    void clear();
};

}