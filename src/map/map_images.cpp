#include "map_images.h"

namespace rota::map {

namespace {

void write_rgb24(
    std::vector<std::uint8_t>& bytes,
    std::size_t byte_index,
    std::uint32_t value
) {
    bytes[byte_index] =
        static_cast<std::uint8_t>(value & 0xFFU);

    bytes[byte_index + 1] =
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU);

    bytes[byte_index + 2] =
        static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
}

}

RGBImageData MapImages::create_id_image(
    const Geography& geography
) {
    RGBImageData result;

    result.width = geography.width;
    result.height = geography.height;
    result.bytes.resize(
        geography.pixel_location.size() * 3
    );

    for (
        std::size_t pixel_index = 0;
        pixel_index < geography.pixel_location.size();
        ++pixel_index
    ) {
        write_rgb24(
            result.bytes,
            pixel_index * 3,
            geography.pixel_location[pixel_index]
        );
    }

    return result;
}

RGBImageData MapImages::create_palette_image(
    const Geography& geography
) {
    RGBImageData result;

    result.width = geography.location_count();
    result.height = 1;
    result.bytes.resize(
        static_cast<std::size_t>(result.width) * 3
    );

    for (
        LocationID id = 0;
        id < geography.location_count();
        ++id
    ) {
        write_rgb24(
            result.bytes,
            static_cast<std::size_t>(id) * 3,
            geography.display_color_rgb[id]
        );
    }

    return result;
}

}