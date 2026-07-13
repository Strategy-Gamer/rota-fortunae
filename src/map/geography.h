#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace rota::map {

using LocationID = std::uint32_t;
using CountryID = std::int32_t;

inline constexpr LocationID INVALID_LOCATION_ID =
    std::numeric_limits<LocationID>::max();

inline constexpr CountryID INVALID_COUNTRY_ID = -1;

struct Geography {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // pixel_location[y * width + x] -> LocationID
    std::vector<LocationID> pixel_location;

    // All arrays below are indexed by LocationID.
    std::vector<std::uint32_t> display_color_rgb;
    std::vector<CountryID> owner_id;
    std::vector<std::uint32_t> area;
    std::vector<std::int32_t> centroid_x;
    std::vector<std::int32_t> centroid_y;

    [[nodiscard]]
    std::uint32_t location_count() const noexcept {
        return static_cast<std::uint32_t>(display_color_rgb.size());
    }

    [[nodiscard]]
    bool is_valid_location(LocationID id) const noexcept {
        return id < location_count();
    }

    [[nodiscard]]
    bool is_valid_pixel(std::int32_t x, std::int32_t y) const noexcept {
        return x >= 0 &&
               y >= 0 &&
               x < static_cast<std::int32_t>(width) &&
               y < static_cast<std::int32_t>(height);
    }

    [[nodiscard]]
    LocationID location_at(std::int32_t x, std::int32_t y) const noexcept {
        if (!is_valid_pixel(x, y)) {
            return INVALID_LOCATION_ID;
        }

        const std::size_t index =
            static_cast<std::size_t>(y) * width +
            static_cast<std::size_t>(x);

        return pixel_location[index];
    }

    void clear();
};

}