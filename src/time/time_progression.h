#pragma once

#include <cstdint>

namespace rota::core { class World; }

namespace rota::time {

// Controls all systems with regards to time progression (daily, monthly, yearly).
// Whilst this determines ordering of each day/month/year tick, it does not progress time itself
class TimeProgression{
public:
    static void on_tick(rota::core::World&);
    // Steps whole days without the clock. Tools only; the game advances through on_tick.
    static void advance_years(rota::core::World&, uint32_t years);
    static void advance_day(rota::core::World&);
    static void on_daily(rota::core::World&);
    static void on_monthly(rota::core::World&);
    static void on_yearly(rota::core::World&);
};
}