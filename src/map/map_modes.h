#pragma once

#include "countries/countries.h"
#include "countries/location_politics.h"
#include "map/geography.h"
#include "map/map_images.h"

#include <cstdint>

namespace rota::map {

enum class MapMode : std::uint8_t {
    LocationColor = 0,
    Political = 1
};

RGBImageData create_map_mode_palette(
    const Geography& geography,
    const rota::countries::CountryStore& countries,
    const rota::countries::LocationPolitics& politics,
    MapMode mode
);

}