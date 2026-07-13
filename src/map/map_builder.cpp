#include "map_builder.h"

#include <stdexcept>
#include <unordered_map>

namespace rota::map {

namespace {

std::uint32_t pack_rgb(
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b
) noexcept {
    return static_cast<std::uint32_t>(r) |
           (static_cast<std::uint32_t>(g) << 8U) |
           (static_cast<std::uint32_t>(b) << 16U);
}

}

void MapBuilder::build_from_rgb8(
    Geography& geography,
    std::uint32_t width,
    std::uint32_t height,
    const std::uint8_t* rgb_bytes,
    std::size_t rgb_byte_count
) {
    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * height;

    const std::size_t expected_byte_count = pixel_count * 3;

    if (rgb_bytes == nullptr) {
        throw std::invalid_argument("RGB byte pointer is null.");
    }

    if (rgb_byte_count != expected_byte_count) {
        throw std::invalid_argument(
            "RGB byte count does not match map dimensions."
        );
    }

    geography.clear();

    geography.width = width;
    geography.height = height;
    geography.pixel_location.resize(pixel_count);

    // Maps the original RGB color to the LocationID assigned to it.
    std::unordered_map<std::uint32_t, LocationID> color_to_location;

    std::vector<std::uint64_t> centroid_sum_x;
    std::vector<std::uint64_t> centroid_sum_y;

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t pixel_index =
                static_cast<std::size_t>(y) * width + x;

            const std::size_t byte_index = pixel_index * 3;

            const std::uint8_t r = rgb_bytes[byte_index];
            const std::uint8_t g = rgb_bytes[byte_index + 1];
            const std::uint8_t b = rgb_bytes[byte_index + 2];

            const std::uint32_t packed_color = pack_rgb(r, g, b);

            LocationID location_id;

            const auto existing = color_to_location.find(packed_color);

            if (existing == color_to_location.end()) {
                location_id = geography.location_count();

                color_to_location.emplace(packed_color, location_id);

                geography.display_color_rgb.push_back(packed_color);
                geography.owner_id.push_back(INVALID_COUNTRY_ID);
                geography.area.push_back(0);
                geography.centroid_x.push_back(0);
                geography.centroid_y.push_back(0);

                centroid_sum_x.push_back(0);
                centroid_sum_y.push_back(0);
            } else {
                location_id = existing->second;
            }

            geography.pixel_location[pixel_index] = location_id;

            geography.area[location_id] += 1;
            centroid_sum_x[location_id] += x;
            centroid_sum_y[location_id] += y;
        }
    }

    finalize_centroids(
        geography,
        centroid_sum_x,
        centroid_sum_y
    );
}

void MapBuilder::finalize_centroids(
    Geography& geography,
    const std::vector<std::uint64_t>& centroid_sum_x,
    const std::vector<std::uint64_t>& centroid_sum_y
) {
    const std::uint32_t count = geography.location_count();

    for (LocationID id = 0; id < count; ++id) {
        const std::uint32_t location_area = geography.area[id];

        if (location_area == 0) {
            continue;
        }

        geography.centroid_x[id] =
            static_cast<std::int32_t>(
                centroid_sum_x[id] / location_area
            );

        geography.centroid_y[id] =
            static_cast<std::int32_t>(
                centroid_sum_y[id] / location_area
            );
    }
}

}