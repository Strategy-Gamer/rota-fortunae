#include "world.h"

namespace rota::core {

void World::clear() {
    seed = 0;
    geography.clear();
    countries.clear();
    country_state.clear();
    location_politics.clear();
    location_economy.clear();
    location_secular_cycle.clear();
    calendar.clear();
    synch_clock.clear();
}

}