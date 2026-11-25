#pragma once

#include "countries/countries.h"
#include "map/geography.h"
#include "utility/FixedDecimal.h"

#include <vector>

namespace rota::core { struct World; }

namespace rota::countries {

// What a year of civil war leaves for the rest of the cycle
struct WarYear {
    std::vector<Fixed64> elite_keep;    // per LocationID: the share of elites the fighting spares
    std::vector<Fixed64> commoner_keep; // per LocationID
    std::vector<CountryID> decided;     // countries whose war was won outright this year
};

// One year of civil war on the map, run after the crises roll and before the elite pass: rebels
// rise where a civil-war crisis just began, every front is fought, and a war ends when a side wins
// or the crisis runs out. Elites die across the warring country; commoners where the armies are.
WarYear run_civil_wars(rota::core::World& w);

// The rebel side fighting each country, indexed by CountryID; INVALID_COUNTRY_ID at peace
std::vector<CountryID> rebel_sides(const rota::core::World& w);

// Whether land held by the other side of its owner's war touches the location, over a border, a
// strait or one stretch of sea
bool is_front(const rota::core::World& w, rota::map::LocationID loc);

}
