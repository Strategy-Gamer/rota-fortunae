#pragma once

#include "../map/geography.h"
#include "../countries/countries.h"
#include "../countries/country_state.h"
#include "../countries/location_politics.h"
#include "../economy/location_economy.h"
#include "../economy/location_secular_cycle.h"
#include "../time/calendar.h"
#include "../time/synch_clock.h"

#include <cstdint>
#include <vector>

// Master World Store

namespace rota::core {

    // The only definition: SimWorld binds these as its DIRTY_* constants for GDScript
    enum RenderDirty : std::uint32_t {
        DIRTY_ALL = 1 << 0,
        DIRTY_POLITICAL = 1 << 1
    };

    struct World{
        std::uint32_t render_dirty = 0;
        std::uint64_t seed = 0; // keys every random draw in the sim

        rota::map::Geography geography;
        rota::countries::CountryStore countries;
        rota::countries::CountryState country_state;
        rota::countries::LocationPolitics location_politics;
        rota::economy::LocationEconomy location_economy;
        rota::economy::LocationSecularCycle location_secular_cycle;

        rota::time::Calendar calendar; 
        rota::time::SynchClock synch_clock; 
        
        void clear();
    };
}