#include "map/map_modes.h"

namespace rota::map {

namespace {

void write_rgb24(
    std::vector<std::uint8_t>& bytes,
    std::size_t byte_index,
    std::uint32_t packed_rgb
) {
    bytes[byte_index] =
        static_cast<std::uint8_t>(
            packed_rgb & 0xFFU
        );

    bytes[byte_index + 1] =
        static_cast<std::uint8_t>(
            (packed_rgb >> 8U) & 0xFFU
        );

    bytes[byte_index + 2] =
        static_cast<std::uint8_t>(
            (packed_rgb >> 16U) & 0xFFU
        );
}

constexpr std::uint32_t UNOWNED_COLOR =
    0x00505050U;

RGBImageData create_location_color_palette(
    const Geography& geography
) {
    RGBImageData result;

    result.width = geography.location_count();
    result.height = 1;
    result.bytes.resize(
        static_cast<std::size_t>(result.width) * 3
    );

    for (
        LocationID location_id = 0;
        location_id < geography.location_count();
        ++location_id
    ) {
        write_rgb24(
            result.bytes,
            static_cast<std::size_t>(location_id) * 3,
            geography.display_color_rgb[location_id]
        );
    }

    return result;
}

RGBImageData create_political_palette(
    const Geography& geography,
    const rota::countries::CountryStore& countries,
    const rota::countries::LocationPolitics& politics
) {
    RGBImageData result;

    result.width = geography.location_count();
    result.height = 1;
    result.bytes.resize(
        static_cast<std::size_t>(result.width) * 3
    );

    for (
        LocationID location_id = 0;
        location_id < geography.location_count();
        ++location_id
    ) {
        const rota::countries::CountryID owner =
            politics.get_owner(location_id);

        std::uint32_t color = UNOWNED_COLOR;

        if (countries.is_valid(owner)) {
            color = countries.display_color_rgb[owner];
        }

        write_rgb24(
            result.bytes,
            static_cast<std::size_t>(location_id) * 3,
            color
        );
    }

    return result;
}

}

RGBImageData create_map_mode_palette(
    const Geography& geography,
    const rota::countries::CountryStore& countries,
    const rota::countries::LocationPolitics& politics,
    MapMode mode
) {
    switch (mode) {
        case MapMode::Political:
            return create_political_palette(
                geography,
                countries,
                politics
            );

        case MapMode::LocationColor:
        default:
            return create_location_color_palette(
                geography
            );
    }
}

}