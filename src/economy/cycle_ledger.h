#pragma once

#include "../utility/FixedDecimal.h"

#include <vector>

namespace rota::economy {

// What last year's secular cycle did to one country: every flow into and out of its commoners and
// elites, in people before rounding to whole heads, and the readings its rules ran on.
struct CycleRecord {
    Fixed64 births;
    Fixed64 natural_deaths;   // the base death rate and child mortality
    Fixed64 crisis_deaths;    // revolt famine
    Fixed64 war_deaths;       // fronts and pillage

    Fixed64 elite_births;
    Fixed64 promotions;
    Fixed64 demotions;
    Fixed64 attrition;          // political violence
    Fixed64 elite_crisis_deaths; // revolt and coup culls
    Fixed64 elite_war_deaths;

    // Commoner-weighted means, as the phase and the crises read them; elite satisfaction is elite-weighted
    Fixed64 wage;
    Fixed64 immiseration;
    Fixed64 security;
    Fixed64 commoner_satisfaction;
    Fixed64 commoner_satisfaction_target;
    Fixed64 elite_satisfaction;
    Fixed64 elite_satisfaction_target;
    Fixed64 radicalism; // the country's stock, before it is spread over the locations and suppressed
    Fixed64 radicalism_target;
    Fixed64 effective_radicalism;
    Fixed64 violence;
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
