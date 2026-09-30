#include "world.h"

namespace rota::core {

void World::clear() {
    geography.clear();
    countries.clear();
    location_politics.clear();
    location_economy.clear();
    calendar.clear();
    synch_clock.clear();
}

}