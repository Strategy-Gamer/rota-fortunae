#include "economy_system.h"

#include "../core/world.h"
#include "../utility/FixedDecimal.h"

#include <cstdint>

namespace rota::economy{

void EconomySystem::run_economy(rota::core::World& world){
    // Technically some of these functions are going to be separated with the real system
    std::uint32_t num_locations = world.geography.location_count();
    
    for(std::uint32_t loc_id = 0; loc_id < num_locations; loc_id++){
        std::uint64_t population = world.location_economy.population[loc_id];
        population = population + population / 100;
        world.location_economy.population[loc_id] = population;

        Fixed32 productivity = world.location_economy.productivity[loc_id];
        productivity = productivity + productivity / 200;
        world.location_economy.productivity[loc_id] = productivity;
        
        world.location_economy.wealth[loc_id] = Fixed64(productivity) * static_cast<int64_t>(population);
    }
}

}