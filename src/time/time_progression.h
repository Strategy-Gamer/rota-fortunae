#pragma once

namespace rota::core { class World; }

namespace rota::time {

// Controls all systems with regards to time progression (daily, monthly, yearly).
// Whilst this determines ordering of each day/month/year tick, it does not progress time itself
class TimeProgression{
public:
    static void on_tick(rota::core::World&);
    static void on_daily(rota::core::World&);
    static void on_monthly(rota::core::World&);
    static void on_yearly(rota::core::World&);
};
}