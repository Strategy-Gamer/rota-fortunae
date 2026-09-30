#pragma once

#include "../map/geography.h"
#include "../countries/countries.h"
#include "../countries/location_politics.h"
#include "../economy/location_economy.h"
#include "../time/calendar.h"
#include "../time/synch_clock.h"

#include <cstdint>
#include <vector>

// Master World Store

namespace rota::core {

    struct World{
        std::uint32_t render_dirty = 0;

        rota::map::Geography geography;
        rota::countries::CountryStore countries;
        rota::countries::LocationPolitics location_politics; 
        rota::economy::LocationEconomy location_economy; 
        // location_economy, location_secular_cycles, country_economy, so on

        rota::time::Calendar calendar; 
        rota::time::SynchClock synch_clock; 
        
        void clear();
    };
}