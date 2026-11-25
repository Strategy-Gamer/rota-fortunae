#pragma once

#include <cstdint>

#include "../utility/FixedDecimal.h"
#include "../map/geography.h"

namespace rota::core { struct World; }

namespace rota::queries {
struct LocationSummary {
    std::uint64_t   population;         // commoners + elites
    std::uint64_t   commoners;
    std::uint64_t   elites;
    Fixed64         wealth;
    Fixed64         wealth_per_capita;  // Derived
    Fixed64         population_density; // commoners per head of carrying capacity
    Fixed64         elite_positions;    // its land's plus its elites' share of the owner's
    Fixed64         elite_overproduction; // the E gauge for this location
    Fixed64         unrest;
    Fixed64         effective_unrest;
    Fixed64         security;
    Fixed64         control;            // its owner's, 1 at the capital
    std::int32_t    owner_country;      // -1 = unowned
    std::int32_t    controller;         // who holds it: the owner unless occupied; -1 = unowned
};
LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc);
}
