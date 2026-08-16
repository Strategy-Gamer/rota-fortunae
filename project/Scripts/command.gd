# res://simulation/command.gd
class_name Command
extends RefCounted

## Command definition and helpers.
## Keep payload simple for skeleton. Use specific fields or a Variant payload.

enum Type {
	NONE = 0,
	PAUSE = 1,
	SET_SPEED = 2,
	SET_TEST_VALUE = 3,
	INCREMENT_TEST = 4,
	# Future examples:
	# REQUEST_SET_TAX_RATE = 10,
	# ISSUE_ARMY_ORDER = 20,
}

static func make_step_clock() -> Dictionary:
	return { "type_id": SimWorld.CMD_STEP_CLOCK, 
			 "actor_civ": -1,
			 "payload": {} }
static func make_toggle_pause(paused_state: bool) -> Dictionary:
	return { "type_id": SimWorld.CMD_TOGGLE_PAUSE, 
			 "actor_civ": -1,
			 "payload": {"paused_state": paused_state} }
static func make_set_game_speed(speed: int) -> Dictionary:
	return { "type_id": SimWorld.CMD_SET_GAME_SPEED, 
			 "actor_civ": -1,
			 "payload": {"speed": speed} }
static func make_add_date(years: int = 0, days: int = 0) -> Dictionary:
	return { "type_id": SimWorld.CMD_ADD_DATE, 
			 "actor_civ": -1,
			 "payload": {"years": years, "days": days } }
static func make_set_date(year: int = 1, day: int = 0) -> Dictionary:
	return { "type_id": SimWorld.CMD_SET_DATE, 
			 "actor_civ": -1,
			 "payload": {"year": year, "day": day } }
static func make_set_location_owner(location_id: int, country_id: int = -1) -> Dictionary:
	return { "type_id": SimWorld.CMD_SET_LOCATION_OWNER, 
			 "actor_civ": -1,
			 "payload": {"country_id": country_id, "location_id": location_id } }

static func is_valid_submission(cmd: Dictionary, current_player_id: int) -> bool:
	"""Basic submission validation (format + permission). Separate from execution validation."""
	if not cmd.has_all(["type_id", "player_id", "actor_civ"]):
		return false
	if cmd.player_id != current_player_id:
		return false  # Spoof check, in real use cryptographic or session token
	if cmd.type_id <= SimWorld.CMD_NONE or cmd.type_id > SimWorld.CMD_SET_LOCATION_OWNER:  # expand range later
		return false
	return true
