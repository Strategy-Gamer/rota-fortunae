#pragma once

#include "geography.h"

#include <cstdint>
#include <vector>

namespace rota::map {

struct RGBImageData {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bytes;
};

class MapImages {
public:
    static RGBImageData create_id_image(
        const Geography& geography
    );

    static RGBImageData create_palette_image(
        const Geography& geography
    );
};

}