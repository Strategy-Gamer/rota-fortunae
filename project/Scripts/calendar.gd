# res://simulation/calendar.gd
class_name GameCalendar
extends RefCounted

## Deterministic in-game calendar.
## Advances based on speed and pause state.
## Integer only, no floats.

var total_days: int = 0
var speed: int = 1          # 1, 2, 5, 10 etc. Multiplier for progress.
var paused: bool = false
var accumulation: int = 0   # Sub-day progress units

const ADVANCE_THRESHOLD: int = 60  # At speed=1, advance 1 day every ~1 real second (at 60Hz)

func advance() -> void:
	"""Call once per synchronization tick from the simulation step."""
	if paused:
		return
	accumulation += speed
	while accumulation >= ADVANCE_THRESHOLD:
		total_days += 1
		accumulation -= ADVANCE_THRESHOLD
		# Note: Daily/periodic sim updates can be triggered here or by listening to total_days change.

func get_year() -> int:
	return (total_days / 365) + 1

func get_day_of_year() -> int:
	return (total_days % 365) + 1

func get_date_string() -> String:
	# Simple representation for UI/debug. Expand later with months if needed.
	return "%d.%03d" % [get_year(), get_day_of_year()]

func set_speed(new_speed: int) -> void:
	speed = max(1, new_speed)

func set_paused(p: bool) -> void:
	paused = p

func get_state_for_hash() -> Dictionary:
	return {
		"total_days": total_days,
		"speed": speed,
		"paused": paused,
		"accumulation": accumulation
	}
