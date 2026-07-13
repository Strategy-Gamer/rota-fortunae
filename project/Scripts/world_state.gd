# res://simulation/world_state.gd
class_name WorldState
extends RefCounted

## Authoritative deterministic world state.
## All simulation logic and command execution happens here.
## UI and prediction layers should NEVER mutate this directly.

var calendar: GameCalendar = GameCalendar.new()
var test_value: int = 0
var player_resources: Dictionary = {}  # player_id (int) -> resource_amount (int)

# For skeleton: 2 players
var player_ids: Array[int] = [1, 2]

# Future: provinces: Array, countries, pop structures (Turchin-style age cohorts, elite numbers, etc.)
# For now, simple counters to prove command execution and determinism.

func _init() -> void:
	for pid in player_ids:
		player_resources[pid] = 1000

func step_on_tick() -> void:
	"""Advance calendar and any per-tick or daily simulation.
	Called after command execution for the tick (or integrated)."""
	calendar.advance()
	# Placeholder for economy/pop/warfare updates that depend on calendar.
	# e.g. if calendar.total_days changed since last, run_daily_economy()
	# Keep all logic deterministic: fixed iteration order, integer math, seeded RNG if any.

func execute_command(cmd: Dictionary) -> bool:
	"""Execute a confirmed command. Must be fully deterministic.
	Returns true if executed successfully."""
	match cmd.get("type", -1):
		Command.Type.PAUSE:
			calendar.set_paused(bool(cmd.get("payload", false)))
			return true
		Command.Type.SET_SPEED:
			calendar.set_speed(int(cmd.get("payload", 1)))
			return true
		Command.Type.SET_TEST_VALUE:
			test_value = int(cmd.get("payload", 0))
			return true
		Command.Type.INCREMENT_TEST:
			test_value += int(cmd.get("payload", 1))
			return true
		_:
			push_warning("Unknown command type in execute: %s" % cmd.get("type"))
			return false

func get_state_hash() -> int:
	"""Simple deterministic hash for desync detection.
	In production use a better cross-platform hash (e.g. custom FNV-1a over serialized state)."""
	var h: int = 0
	h = h ^ hash(calendar.total_days)
	h = h ^ (hash(calendar.speed) << 1)
	h = h ^ (hash(calendar.paused) << 2)
	h = h ^ (hash(calendar.accumulation) << 3)
	h = h ^ (hash(test_value) << 4)
	for pid in player_ids:
		h = h ^ (hash(player_resources.get(pid, 0)) << (5 + pid))
	return h

func get_debug_string() -> String:
	return "Day: %s | TestValue: %d | Speed: %dx | Paused: %s" % [
		calendar.get_date_string(), test_value, calendar.speed, calendar.paused
	]
