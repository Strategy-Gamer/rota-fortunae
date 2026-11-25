#pragma once

#include "../map/geography.h"
#include "../utility/FixedDecimal.h"

namespace rota::core { struct World; }

namespace rota::economy {

// One year of FinalSim (Planning/new_model.py) for every country, split per DESIGN.md §10
void run_secular_cycle(rota::core::World& world);

// Elite positions a location's own land provides
Fixed64 local_elite_positions(const rota::core::World& world, rota::map::LocationID location);

}
