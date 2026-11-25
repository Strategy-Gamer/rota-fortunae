#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace rota::core { struct World; }

namespace rota::core::Command {

/*
Command.h

Includes backend for commands related to serialization and execution.

Godot-side calls creation methods in GDScript to build command payloads (via UI, debug interfaces, etc), recieves commands via networking, verifies user identity, and orders commands.

C++ side executes each command, one by one. It owns the registry of what type of commands exist, serializes the commands, and executes the commands.
*/

// The only definition: SimWorld binds these as its CMD_* constants for GDScript.
// A contract with GDScript, saves and replays: never renumber or reuse an ID.
enum CommandType : std::int32_t {
    CMD_NONE = 0,

    // Time & Debug (1-999)
    // Time Controls (10-19)

    CMD_TOGGLE_CLOCK = 10,
    CMD_STEP_CLOCK = 11,

    CMD_TOGGLE_PAUSE = 12,
    CMD_SET_GAME_SPEED = 13,
    CMD_ADD_DATE = 14,
    CMD_SET_DATE = 15,

    // Locations (50-199)

    CMD_SET_LOCATION_OWNER = 20,

    // Countries (200-299)

    // Civilizations (300-399)

    // In-Game Commands (1_000+)
};

// Payload fields by name, decoded from GDScript by SimWorld (bools arrive as 0/1)
using Payload = std::map<std::string, std::int32_t>;

bool execute_command(rota::core::World& world, int actor_civ, int type, const Payload& payload);

// Commands, in order of the enum list

bool cmd_step_clock(rota::core::World& world, int actor_civ, const Payload& payload);
bool cmd_toggle_pause(rota::core::World& world, int actor_civ, const Payload& payload);
bool cmd_set_game_speed(rota::core::World& world, int actor_civ, const Payload& payload);
bool cmd_add_date(rota::core::World& world, int actor_civ, const Payload& payload);
bool cmd_set_date(rota::core::World& world, int actor_civ, const Payload& payload);
bool cmd_set_location_owner(rota::core::World& world, int actor_civ, const Payload& payload);
}