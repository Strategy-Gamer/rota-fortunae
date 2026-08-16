#include "world.h"

namespace rota::core {

void World::clear() {
    geography.clear();
    countries.clear();
    location_politics.clear();
    calendar.clear();
    synch_clock.clear();
}

}