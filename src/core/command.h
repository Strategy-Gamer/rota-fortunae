#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>

#include <godot_cpp/variant/dictionary.hpp>

namespace rota::core { struct World; }

namespace rota::core::Command {

/* 
Command.h

Includes backend for commands related to serialization and execution.

Godot-side calls creation methods in GDScript to build command payloads (via UI, debug interfaces, etc), recieves commands via networking, verifies user identity, and orders commands.

C++ side executes each command, one by one. It owns the registry of what type of commands exist, serializes the commands, and executes the commands.
*/

bool execute_command(rota::core::World& world, int actor_civ, int type, const godot::Dictionary& payload);

// Commands, in order of the enum list

bool cmd_step_clock(rota::core::World& world, int actor_civ, const godot::Dictionary& payload);
bool cmd_toggle_pause(rota::core::World& world, int actor_civ, const godot::Dictionary& payload);
bool cmd_set_game_speed(rota::core::World& world, int actor_civ, const godot::Dictionary& payload);
bool cmd_add_date(rota::core::World& world, int actor_civ, const godot::Dictionary& payload);
bool cmd_set_date(rota::core::World& world, int actor_civ, const godot::Dictionary& payload);
bool cmd_set_location_owner(rota::core::World& world, int actor_civ, const godot::Dictionary& payload);
}