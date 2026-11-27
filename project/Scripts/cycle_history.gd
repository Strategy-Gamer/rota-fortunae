class_name CycleHistory
extends Node

## Each country's yearly secular-cycle read-outs, for the debug trajectory graphs.
## Presentation state: recorded here from queries, never hashed, and empty again after a load.

const MAX_YEARS := 400
const SERIES := ["population_pressure", "elite_overproduction", "commoner_satisfaction", "effective_radicalism", "legitimacy",
		"population", "elites"]

signal recorded

var _last_year := -1
var _countries := {}  # country_id -> {"first_year": int, "phase": PackedByteArray, <series>: PackedFloat32Array}

## Records one sample per country the first time it sees a new year.
func record_if_new_year(world: SimWorld) -> void:
	var year := world.get_year()
	if year == _last_year:
		return
	_last_year = year

	var summaries: Dictionary = world.get_country_summaries()
	for id in summaries:
		var s: Dictionary = summaries[id]
		var h: Dictionary = _countries.get(id, {})
		if h.is_empty() or h.generation != s.generation:  # a reused slot is a new country
			h = {"first_year": year, "generation": s.generation, "phase": PackedByteArray()}
			for key in SERIES:
				h[key] = PackedFloat32Array()
			_countries[id] = h

		var trim: bool = h.phase.size() >= MAX_YEARS
		var phases: PackedByteArray = h.phase
		if trim:
			phases.remove_at(0)
			h.first_year += 1
		phases.append(s.phase)
		h.phase = phases
		for key in SERIES:
			var values: PackedFloat32Array = h[key]  # packed arrays copy on write, so store it back
			if trim:
				values.remove_at(0)
			values.append(float(s[key]))
			h[key] = values
	recorded.emit()

func _on_game_session_render_dirty(_mask: int) -> void:
	record_if_new_year(Game.world)

## The country's samples, oldest first, or an empty dictionary if none were recorded.
func get_country(country_id: int) -> Dictionary:
	return _countries.get(country_id, {})
