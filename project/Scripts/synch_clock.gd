# res://simulation/synch_clock.gd
class_name SynchClock
extends RefCounted

## Monotonic synchronization tick clock.
## Advances at fixed rate (tied to _physics_process ideally).
## Continues even when game is paused (calendar-wise).

signal ticked(new_tick: int)

var current_tick: int = 0
var tick_rate: float = 60.0  # informational, actual pacing via engine fixed timestep

func step() -> void:
	"""Advance one synchronization tick. Call from fixed timestep loop."""
	current_tick += 1
	ticked.emit(current_tick)

func get_current_tick() -> int:
	return current_tick

func reset(start_tick: int = 0) -> void:
	current_tick = start_tick
