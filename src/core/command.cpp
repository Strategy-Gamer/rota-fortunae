#include "command.h"
#include "world.h"
#include "../time/time_progression.h"
#include "../countries/country_systems.h"

#include <optional>
#include <unordered_map>

namespace rota::core::Command {
    namespace{
        using ExecuteFn = bool(*)(rota::core::World&, int, const Payload&);

        // Registry to link enum to function
        const std::unordered_map<int, ExecuteFn> k_handlers = {
            { CMD_STEP_CLOCK,              &cmd_step_clock },
            { CMD_TOGGLE_PAUSE,            &cmd_toggle_pause },
            { CMD_SET_GAME_SPEED,          &cmd_set_game_speed },
            { CMD_ADD_DATE,                &cmd_add_date },
            { CMD_SET_DATE,                &cmd_set_date },
            { CMD_SET_LOCATION_OWNER,      &cmd_set_location_owner }
        };

        std::optional<std::int32_t> field(const Payload& payload, const char* key){
            const auto it = payload.find(key);
            if (it == payload.end()) return std::nullopt;
            return it->second;
        }
    }

bool execute_command(rota::core::World& world, int actor_civ, int type, const Payload& payload){
    auto it = k_handlers.find(type);
    if (it == k_handlers.end()) return false;
    return it->second(world, actor_civ, payload);
}

bool cmd_step_clock(rota::core::World& world, int actor_civ, const Payload& payload){
    world.synch_clock.step();
    rota::time::TimeProgression::on_tick(world);
    return true;
}

bool cmd_toggle_pause(rota::core::World& world, int actor_civ, const Payload& payload){
    const auto paused_state = field(payload, "paused_state");
    if(!paused_state) return false;

    world.calendar.set_paused(*paused_state != 0);

    return true;
}

bool cmd_set_game_speed(rota::core::World& world, int actor_civ, const Payload& payload){
    const auto speed = field(payload, "speed");
    if(!speed) return false;

    world.calendar.set_tick_multiplier(static_cast<uint32_t>(*speed));

    return true;
}

bool cmd_add_date(rota::core::World& world, int actor_civ, const Payload& payload){
    // No checking if years/days exist b/c by default it's zero
    const std::int32_t years = field(payload, "years").value_or(0);
    const std::int32_t days = field(payload, "days").value_or(0);

    if(years == 0 && days == 0) return true; // Billions must acknowledge that Nothing Ever Happens

    world.calendar.add_date(
        static_cast<uint32_t>(years),
        static_cast<uint32_t>(days)
    );

    return true;
}

bool cmd_set_date(rota::core::World& world, int actor_civ, const Payload& payload){
    const auto year = field(payload, "year");
    const auto day = field(payload, "day");
    if(!year || !day) return false;

    world.calendar.set_date(
        static_cast<uint32_t>(*year),
        static_cast<uint32_t>(*day)
    );

    return true;
}

bool cmd_set_location_owner(rota::core::World& world, int actor_civ, const Payload& payload){
    const auto country_id = field(payload, "country_id");
    const auto location_id = field(payload, "location_id");
    if(!country_id || !location_id) return false;

    if(*location_id < 0 || !world.geography.is_valid_location(static_cast<rota::map::LocationID>(*location_id))) return false;

    if(*country_id < 0){
        rota::countries::set_location_owner(
            world,
            static_cast<rota::map::LocationID>(*location_id),
            rota::countries::INVALID_COUNTRY_ID
        );
        return true;
    }

    if(!world.countries.is_valid(static_cast<rota::countries::CountryID>(*country_id))) return false;

    rota::countries::set_location_owner(
        world,
        static_cast<rota::map::LocationID>(*location_id),
        static_cast<rota::countries::CountryID>(*country_id)
    );
    return true;
}
}