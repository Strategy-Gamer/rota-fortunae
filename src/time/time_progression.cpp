#include "time_progression.h"

#include "economy/economy_system.h"
#include "../core/world.h"
#include "../utility/FixedDecimal.h"
#include "../godot/sim_world.h"

namespace rota::time {
void TimeProgression::on_tick(rota::core::World& world){
    uint32_t days_advanced = world.calendar.advance();

    // Advance through each day incrementally
    // Doing this as you can account for each day, month, and year tick 
    // without knowing the internals of how days/months/years work.
    for(uint32_t i = 0; i < days_advanced; i++){
        //uint32_t curr_month = world.calendar.get_month();
        uint32_t curr_year = world.calendar.get_year();

        world.calendar.add_date(0, 1); // Increment calendar day
        on_daily(world);

        // TODO: Monthly
        // if(curr_month != world.calendar.get_month())
        // on_monthly(world);

        if(curr_year != world.calendar.get_year()) {
            on_yearly(world);
            world.render_dirty |= godot::SimWorld::DIRTY_ALL;
        }
    }
}
void TimeProgression::on_daily(rota::core::World& world){
    return;
}
void TimeProgression::on_monthly(rota::core::World& world){
    return;
}
void TimeProgression::on_yearly(rota::core::World& world){
    rota::economy::EconomySystem::run_economy(world);
}

}