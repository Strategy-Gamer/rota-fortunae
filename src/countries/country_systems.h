#pragma once

#include <cstdint>
#include <string>
#include "countries.h"
#include "../map/geography.h"

namespace rota::core { struct World; }

namespace rota::countries {

// Creates a wholly new country
CountryID create_country(
    rota::core::World& w,
    const std::string& name,
    std::uint32_t display_color,
    std::uint32_t founded = 0
);

// Revives an archived country into a fresh slot. The old record stays in the archive.
CountryID revive_country(
    rota::core::World& w,
    CountryID id,
    std::uint32_t generation
);
CountryID revive_country(
    rota::core::World& w,
    const DeadCountryRecord& record
);

// Marks an existing country to be destroyed at the end of the tick
void mark_destroy_country(rota::core::World& w, CountryID id);

// Destroys all marked countries, unowns their land and archives them
void destroy_countries(rota::core::World& w);

// The only write path for ownership, so location_count stays in sync
void set_location_owner(
    rota::core::World& w,
    rota::map::LocationID location_id,
    CountryID country_id
);

}
