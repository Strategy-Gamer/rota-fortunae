#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace rota::map {

using LocationID = std::uint32_t;

inline constexpr LocationID INVALID_LOCATION_ID =
    std::numeric_limits<LocationID>::max();

// Only land can be owned. Sea is crossed, never held; impassable land is neither.
enum class LocationKind : std::uint8_t {
    Land = 0,
    Sea = 1,
    Impassable = 2
};

struct Geography {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // pixel_location[y * width + x] -> LocationID
    std::vector<LocationID> pixel_location;

    // All arrays below are indexed by LocationID.
    std::vector<std::uint32_t> display_color_rgb;
    std::vector<std::uint32_t> area;
    std::vector<std::int32_t> centroid_x;
    std::vector<std::int32_t> centroid_y;
    std::vector<LocationKind> kind;

    // Neighbors of id are neighbors[neighbor_offset[id] .. neighbor_offset[id + 1]), ascending.
    // Includes sea and impassable locations, and straits between land across the water.
    std::vector<std::uint32_t> neighbor_offset;
    std::vector<LocationID> neighbors;

    [[nodiscard]]
    std::uint32_t location_count() const noexcept {
        return static_cast<std::uint32_t>(display_color_rgb.size());
    }

    [[nodiscard]]
    bool is_valid_location(LocationID id) const noexcept {
        return id < location_count();
    }

    [[nodiscard]]
    bool is_land(LocationID id) const noexcept {
        return kind[id] == LocationKind::Land;
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