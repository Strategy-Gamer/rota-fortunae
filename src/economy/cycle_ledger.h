#pragma once

#include "../utility/FixedDecimal.h"

#include <vector>

namespace rota::economy {

// What last year's secular cycle did to one country: every flow into and out of its commoners and
// elites, in people before rounding to whole heads, and the readings its rules ran on.
struct CycleRecord {
    Fixed64 births;
    Fixed64 natural_deaths;   // the base death rate and child mortality
    Fixed64 overshoot_deaths; // beyond the population security lets the land carry
    Fixed64 crisis_deaths;    // revolt famine
    Fixed64 war_deaths;       // fronts and pillage

    Fixed64 elite_births;
    Fixed64 promotions;
    Fixed64 demotions;
    Fixed64 attrition;          // instability: effective unrest
    Fixed64 elite_crisis_deaths; // revolt and coup culls
    Fixed64 elite_war_deaths;

    // Commoner-weighted means, as the phase and the crises read them
    Fixed64 wage;
    Fixed64 immiseration;
    Fixed64 security;
    Fixed64 effective_unrest;
    Fixed64 overproduction;      // elites over baseline positions
    Fixed64 felt_overproduction; // elites over funded positions
    Fixed64 army_shortfall;
};

// Derived: rewritten every year and never read by the sim, so it is not hashed
struct CycleLedger {
    std::vector<CycleRecord> countries; // indexed by CountryID; zero for countries without land

    void clear() { countries.clear(); }
};

}
