#pragma once

#include "map/map_images.h"

#include <cstdint>

namespace rota::core { struct World; }

namespace rota::map {

// The only definition: SimWorld binds these as its MAP_* constants for GDScript
enum class MapMode : std::uint8_t {
    LocationColor = 0,
    Political = 1,
    Phase = 2,
    Unrest = 3,
    PopulationPressure = 4,
    EliteOverproduction = 5,
    Legitimacy = 6,
    Count
};

RGBImageData create_map_mode_palette(const rota::core::World& world, MapMode mode);

}
