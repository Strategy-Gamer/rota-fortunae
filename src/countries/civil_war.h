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
};

// One year of civil war on the map, run after the crises roll and before the elite pass. A war
// lasts as long as its civil-war crisis: rebels rise, every front is fought, and a rising that takes
// the capital or loses all its land is followed by another until the crisis runs out. Fronts are
// devastated, and the land behind the rebels' lines is pillaged.
WarYear run_civil_wars(rota::core::World& w);

// The rebel side fighting each country, indexed by CountryID; INVALID_COUNTRY_ID at peace
std::vector<CountryID> rebel_sides(const rota::core::World& w);

// Whether land held by the other side of its owner's war touches the location, over a border, a
// strait or one stretch of sea
bool is_front(const rota::core::World& w, rota::map::LocationID loc);

}
