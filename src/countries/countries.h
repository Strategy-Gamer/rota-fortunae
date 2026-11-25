#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::countries {

using CountryID = std::uint32_t;

inline constexpr CountryID INVALID_COUNTRY_ID =
    std::numeric_limits<CountryID>::max();

// Archived on destruction. {id, generation} is the country's permanent historical key.
struct DeadCountryRecord {
    CountryID id;
    std::uint32_t generation;
    std::string name;
    std::uint32_t display_color_rgb;
    std::uint32_t founded_year;
    std::uint32_t destroyed_year;
};

// Core country store with administrative & miscellaneous information
struct CountryStore {
    // Indexed by CountryID. Dead slots are reset to defaults so they hash the same regardless of history.

    std::vector<std::uint8_t> alive;
    std::vector<std::uint8_t> marked_destroy;
    std::vector<std::uint32_t> generation;
    std::vector<std::uint32_t> founded_year;
    std::vector<std::string> names;
    std::vector<std::uint32_t> display_color_rgb;
    std::vector<std::uint32_t> location_count; // derived from owner_country

    // Not keyed by CountryID
    std::vector<CountryID> free_list;
    std::vector<DeadCountryRecord> dead_countries; // save/load only, not hashed

    [[nodiscard]]
    CountryID allocate(
        const std::string& name,
        std::uint32_t display_color,
        std::uint32_t founded
    );

    void free(CountryID id);

    [[nodiscard]]
    bool is_valid(CountryID id) const noexcept;

    // Live countries, not slots
    [[nodiscard]]
    std::uint32_t count() const noexcept;

    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};

}
