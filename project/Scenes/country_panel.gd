extends PanelContainer

## Debug panel for the selected location's owner: phase, the cycle's gauges, the state's stocks,
## active crises, and the trajectory graphs. Built in code; the final UI will be hand-made.

const GAUGES := [
	["P  population pressure", "population_pressure", Color(0.35, 0.55, 1.0)],
	["E  elite overproduction", "elite_overproduction", Color(1.0, 0.6, 0.2)],
	["C  commoner satisfaction", "commoner_satisfaction", Color(0.9, 0.85, 0.4)],
	["R  radicalism", "effective_radicalism", Color(1.0, 0.3, 0.3)],
	["L  legitimacy", "legitimacy", Color(0.4, 0.85, 0.5)],
]
const CRISIS_NAMES := {
	SimWorld.CRISIS_REVOLT: "Revolt",
	SimWorld.CRISIS_CIVIL_WAR: "Civil war",
	SimWorld.CRISIS_COUP: "Coup",
}

@export var history: CycleHistory

var _country := -1
var _title: Label
var _phase: Label
var _bars := {}  # summary key -> ProgressBar
var _stats: Label
var _gauge_graph: TrajectoryGraph
var _stock_graph: TrajectoryGraph

func _ready() -> void:
	custom_minimum_size = Vector2(380, 0)
	theme = Theme.new()
	theme.default_font_size = 14
	var box := VBoxContainer.new()
	add_child(box)

	_title = Label.new()
	_title.add_theme_font_size_override("font_size", 18)
	box.add_child(_title)
	_phase = Label.new()
	box.add_child(_phase)

	var grid := GridContainer.new()
	grid.columns = 2
	box.add_child(grid)
	for g in GAUGES:
		var label := Label.new()
		label.text = g[0]
		label.add_theme_color_override("font_color", g[2])
		grid.add_child(label)
		var bar := ProgressBar.new()
		bar.max_value = 1.0
		bar.step = 0.001
		bar.custom_minimum_size = Vector2(150, 0)
		bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var fill := StyleBoxFlat.new()
		fill.bg_color = Color(g[2], 0.8)
		bar.add_theme_stylebox_override("fill", fill)
		grid.add_child(bar)
		_bars[g[1]] = bar

	_stats = Label.new()
	box.add_child(_stats)

	_gauge_graph = _add_graph(box, "Gauges")
	_stock_graph = _add_graph(box, "Stocks (each vs its own max)")

	if history != null:
		history.recorded.connect(_refresh)
	visible = false

func _add_graph(box: VBoxContainer, title: String) -> TrajectoryGraph:
	var graph := TrajectoryGraph.new()
	graph.title = title
	graph.custom_minimum_size = Vector2(0, 140)
	box.add_child(graph)
	return graph

func show_country(country_id: int) -> void:
	_country = country_id
	_refresh()

func _on_map_selection_changed(location_id: int) -> void:
	show_country(Game.world.get_location_owner(location_id) if location_id >= 0 else -1)

func _on_game_session_render_dirty(_mask: int) -> void:
	_on_map_selection_changed(Game.map.selected_location_id)

func _refresh() -> void:
	visible = _country >= 0
	if not visible:
		return
	var s: Dictionary = Game.world.get_country_summary(_country)
	if s.is_empty():
		visible = false
		return

	_title.text = s.name
	_title.add_theme_color_override("font_color", Game.world.get_country_color(_country))
	_phase.text = TrajectoryGraph.PHASE_NAMES[s.phase]
	_phase.add_theme_color_override("font_color", TrajectoryGraph.PHASE_COLORS[s.phase])
	for key in _bars:
		_bars[key].value = s[key]

	var crises: PackedStringArray = []
	for bit in CRISIS_NAMES:
		if s.active_crises & (1 << bit):
			crises.append(CRISIS_NAMES[bit])
	_stats.text = "\n".join([
		"Locations: %d    Population: %s" % [s.location_count, _group(s.population)],
		"Elites: %s    Wealth per head: %.3f" % [_group(s.elites), s.wealth_per_capita],
		"Treasury: %s    War-weariness: %.2f" % [_group(int(s.treasury)), s.war_weariness],
		"Elite satisfaction: %.2f    Radicalism stock: %.2f" % [s.elite_satisfaction, s.radicalism],
		"Crises: %s" % (", ".join(crises) if not crises.is_empty() else "none"),
		"Rebels hold: %d locations" % s.occupied,
	])
	_plot()

func _plot() -> void:
	var h: Dictionary = history.get_country(_country) if history != null else {}
	if h.is_empty():
		_gauge_graph.plot(0, PackedByteArray(), [])
		_stock_graph.plot(0, PackedByteArray(), [])
		return
	var gauges := []
	for g in GAUGES:
		gauges.append({"label": g[0].substr(0, 1), "color": g[2], "values": h[g[1]]})
	_gauge_graph.plot(h.first_year, h.phase, gauges)
	_stock_graph.plot(h.first_year, h.phase, [
		{"label": "population", "color": Color(0.35, 0.55, 1.0), "values": _normalized(h.population)},
		{"label": "elites", "color": Color(1.0, 0.6, 0.2), "values": _normalized(h.elites)},
	])

static func _normalized(values: PackedFloat32Array) -> PackedFloat32Array:
	var top := 0.0
	for v in values:
		top = max(top, v)
	var out := values.duplicate()
	if top > 0.0:
		for i in out.size():
			out[i] /= top
	return out

static func _group(n: int) -> String:
	var digits := str(abs(n))
	var out := ""
	while digits.length() > 3:
		out = "," + digits.substr(digits.length() - 3) + out
		digits = digits.substr(0, digits.length() - 3)
	return ("-" if n < 0 else "") + digits + out
