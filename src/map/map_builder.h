#pragma once

#include "geography.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rota::map {

// A map's sidecar data, keyed by packed location color. Locations it doesn't name are grassland
// at one kilometre per pixel.
struct LocationData {
    struct Location {
        std::uint32_t color;
        LocationKind kind;
        Terrain terrain;
        std::uint32_t metres_per_pixel_x;
        std::uint32_t metres_per_pixel_y;
    };
    std::vector<Location> locations;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> straits;
};

class MapBuilder {
public:
    // Parses the `;`-separated files (header row first):
    // `color;name;kind;terrain;metres_per_pixel_x;metres_per_pixel_y` and `from;to;name`
    static LocationData parse_location_data(
        const std::string& locations_text,
        const std::string& straits_text
    );

    static void build_from_rgb8(
        Geography& geography,
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* rgb_bytes,
        std::size_t rgb_byte_count,
        const LocationData& data
    );

private:
    static void finalize_centroids(
        Geography& geography,
        const std::vector<std::uint64_t>& centroid_sum_x,
        const std::vector<std::uint64_t>& centroid_sum_y
    );

    static void build_adjacency(
        Geography& geography,
        const std::vector<std::pair<LocationID, LocationID>>& straits
    );
};

}