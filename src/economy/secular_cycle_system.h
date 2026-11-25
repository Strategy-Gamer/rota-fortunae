#pragma once

#include "../map/geography.h"
#include "../utility/FixedDecimal.h"

namespace rota::core { struct World; }

namespace rota::economy {

// One year of FinalSim (Planning/new_model.py) for every country, split per DESIGN.md §10
void run_secular_cycle(rota::core::World& world);

// Elite positions a location's own land provides
Fixed64 local_elite_positions(const rota::core::World& world, rota::map::LocationID location);

// Positions a location's elites hold or compete for: its own plus its elites' share of the state's
Fixed64 elite_positions(const rota::core::World& world, rota::map::LocationID location, std::int64_t country_elites);

// FinalSim's display gauges, in [0, 1]. Read-outs only: nothing in the cycle reads them.
Fixed64 population_pressure_gauge(std::int64_t commoners, std::int64_t capacity);
Fixed64 elite_overproduction_gauge(Fixed64 elites_per_position);

}
