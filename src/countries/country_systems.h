#pragma once

#include <cstdint>
#include "countries.h"

namespace rota::core { struct World; }

namespace rota::countries {

// Creates a wholly new country
CountryID create_country(
    rota::core::World& w, 
    const std::string& name, 
    std::uint32_t display_color, 
    std::uint32_t founded = 0
);

// Revives a dead country
CountryID revive_country(
    rota::core::World& w, 
    CountryID id,
    std::uint32_t generation
);
// Revives a dead country
CountryID revive_country(
    rota::core::World& w, 
    DeadCountryRecord record
);

// Marks an existing country to be destroyed
void mark_destroy_country(rota::core::World& w, CountryID id);

}

// Destroys all dead countries and sends them to the Dead Country Record
void destroy_countries(rota::core::World& w);