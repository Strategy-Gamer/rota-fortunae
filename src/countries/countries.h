#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace rota::countries {

using CountryID = std::uint32_t;

inline constexpr CountryID INVALID_COUNTRY_ID =
    std::numeric_limits<CountryID>::max();

// Core country store with administrative & miscellaneous information
struct CountryStore {
    // Indexed by CountryID.

    std::vector<std::uint8_t> alive;
    std::vector<std::uint32_t> generation;
    std::vector<std::uint32_t> founded_year;
    std::vector<std::string> names;
    std::vector<std::uint32_t> display_color_rgb;
    std::vector<std::uint32_t> location_count;

    // Not keyed by CountryID
    std::vector<CountryID> free_list;
    std::vector<DeadCountryRecord> dead_countries;

    [[nodiscard]]
    CountryID allocate(
        const std::string& name,
        std::uint32_t display_color
    );

    void free(CountryID id);

    [[nodiscard]]
    bool is_valid(CountryID id) const noexcept;

    [[nodiscard]]
    std::uint32_t count() const noexcept;

    void clear();
};

struct DeadCountryRecord {
    CountryID id;
    std::uint32_t generation;
    std::string name;
    std::uint32_t display_color_rgb;
    std::uint32_t founded_year;
    std::uint32_t destroyed_year;
};

}