#pragma once

#include "geography.h"

#include <cstdint>

namespace rota::map {

class MapBuilder {
public:
    static void build_from_rgb8(
        Geography& geography,
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* rgb_bytes,
        std::size_t rgb_byte_count
    );

private:
    static void finalize_centroids(
        Geography& geography,
        const std::vector<std::uint64_t>& centroid_sum_x,
        const std::vector<std::uint64_t>& centroid_sum_y
    );
};

}