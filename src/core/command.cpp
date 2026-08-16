#include "command.h"
#include "world.h"
#include "../godot/sim_world.h"

namespace rota::core::Command {
    namespace{
        using ExecuteFn = bool(*)(rota::core::World&, int, const godot::Dictionary&);

        // Registry to link enum to function
        const std::unordered_map<int, ExecuteFn> k_handlers = {
            { godot::SimWorld::CMD_STEP_CLOCK,              &cmd_step_clock },
            { godot::SimWorld::CMD_TOGGLE_PAUSE,            &cmd_toggle_pause },
            { godot::SimWorld::CMD_SET_GAME_SPEED,          &cmd_set_game_speed },
            { godot::SimWorld::CMD_ADD_DATE,                &cmd_add_date },
            { godot::SimWorld::CMD_SET_DATE,                &cmd_set_date },
            { godot::SimWorld::CMD_SET_LOCATION_OWNER,      &cmd_set_location_owner }
        };
    }

bool execute_command(rota::core::World& world, int actor_civ, int type, const godot::Dictionary& payload){
    auto it = k_handlers.find(type);
    if (it == k_handlers.end()) return false;
    return it->second(world, actor_civ, payload);
}

bool cmd_step_clock(rota::core::World& world, int actor_civ, const godot::Dictionary& payload){
    // Carbon copy of sim_world.step_tick

    world.synch_clock.step();
    world.calendar.advance();
    return true;
}

bool cmd_toggle_pause(rota::core::World& world, int actor_civ, const godot::Dictionary& payload){
    if(!payload.has("paused_state")) return false;
    godot::Variant paused_state = payload.get("paused_state", false);

    if(paused_state.get_type() != godot::Variant::BOOL) return false;

    world.calendar.set_paused((bool)paused_state);

    return true;
}

bool cmd_set_game_speed(rota::core::World& world, int actor_civ, const godot::Dictionary& payload){
    if(!payload.has("speed")) return false;
    godot::Variant speed = payload.get("speed", 1);

    if(speed.get_type() != godot::Variant::INT) return false;

    world.calendar.set_tick_multiplier(static_cast<uint32_t>((int)speed));

    return true;
}

bool cmd_add_date(rota::core::World& world, int actor_civ, const godot::Dictionary& payload){
    // No checking if years/days exist b/c by default it's zero
    godot::Variant years = payload.get("years", 0);
    godot::Variant days = payload.get("days", 0);

    if(years.get_type() != godot::Variant::INT) return false;
    if(days.get_type() != godot::Variant::INT) return false;

    if((int)years == 0 && (int)days == 0) return true; // Billions must acknowledge that Nothing Ever Happens

    world.calendar.add_date(
        static_cast<uint32_t>((int)years),
        static_cast<uint32_t>((int)days)
    );

    return true;
}

bool cmd_set_date(rota::core::World& world, int actor_civ, const godot::Dictionary& payload){
    if(!payload.has("year")) return false;
    if(!payload.has("day")) return false;
    godot::Variant year = payload.get("year", 1);
    godot::Variant day = payload.get("day", 0);

    if(year.get_type() != godot::Variant::INT) return false;
    if(day.get_type() != godot::Variant::INT) return false;

    world.calendar.set_date(
        static_cast<uint32_t>((int)year),
        static_cast<uint32_t>((int)day)
    );

    return true;
}

bool cmd_set_location_owner(rota::core::World& world, int actor_civ, const godot::Dictionary& payload){
    if(!payload.has("country_id")) return false;
    if(!payload.has("location_id")) return false;
    godot::Variant country_id = payload.get("country_id", -1);
    godot::Variant location_id = payload.get("location_id", 0);

    if(country_id.get_type() != godot::Variant::INT) return false;
    if(location_id.get_type() != godot::Variant::INT) return false;

    if(!world.geography.is_valid_location(location_id)) return false;
    
    if((int)country_id < 0){
        world.location_politics.set_owner(
            static_cast<rota::map::LocationID>((int)location_id),
            rota::countries::INVALID_COUNTRY_ID
        );
        world.render_dirty |= godot::SimWorld::DIRTY_POLITICAL;
        return true;
    }

    if(!world.countries.is_valid(country_id)) return false;

    world.location_politics.set_owner(
        static_cast<rota::map::LocationID>((int)location_id),
        static_cast<rota::countries::CountryID>((int)country_id)
    );
    world.render_dirty |= godot::SimWorld::DIRTY_POLITICAL;
    return true;
}
}