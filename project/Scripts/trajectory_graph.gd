class_name TrajectoryGraph
extends Control

## A debug line chart over years with each year's phase shaded behind it, like the Planning plots.

const PHASE_COLORS := {
	SimWorld.PHASE_PROSPERITY: Color(0.24, 0.63, 0.27),
	SimWorld.PHASE_STRAIN: Color(0.9, 0.78, 0.2),
	SimWorld.PHASE_FRACTURE: Color(0.78, 0.2, 0.16),
}
const PHASE_NAMES := {
	SimWorld.PHASE_PROSPERITY: "Prosperity",
	SimWorld.PHASE_STRAIN: "Strain",
	SimWorld.PHASE_FRACTURE: "Fracture",
}
const BAND_ALPHA := 0.2
const FONT_SIZE := 11

var title := ""
var _first_year := 0
var _phases := PackedByteArray()
var _series: Array = []  # [{"label": String, "color": Color, "values": PackedFloat32Array}]
var _y_max := 1.0

## Series values are drawn against [0, y_max]; values outside are clamped to the edges.
func plot(first_year: int, phases: PackedByteArray, series: Array, y_max: float = 1.0) -> void:
	_first_year = first_year
	_phases = phases
	_series = series
	_y_max = y_max
	queue_redraw()

func _draw() -> void:
	var font := get_theme_default_font()
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.08, 0.08, 0.1))

	var n := _phases.size()
	if n >= 2:
		var dx := size.x / float(n - 1)
		var start := 0
		for i in range(1, n + 1):
			if i == n or _phases[i] != _phases[start]:
				var x0: float = max(0.0, (start - 0.5) * dx)
				var x1: float = min(size.x, (i - 0.5) * dx)
				var c: Color = PHASE_COLORS[_phases[start]]
				draw_rect(Rect2(x0, 0, x1 - x0, size.y), Color(c, BAND_ALPHA))
				start = i

		for s in _series:
			var values: PackedFloat32Array = s.values
			var points := PackedVector2Array()
			for i in n:
				var y := clampf(values[i] / _y_max, 0.0, 1.0)
				points.append(Vector2(i * dx, size.y - y * (size.y - 2) - 1))
			draw_polyline(points, s.color, 1.5, true)

		var span := "years %d - %d" % [_first_year, _first_year + n - 1]
		var span_width := font.get_string_size(span, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x
		draw_string(font, Vector2(size.x - span_width - 4, size.y - 4), span, HORIZONTAL_ALIGNMENT_LEFT, -1,
				FONT_SIZE, Color(1, 1, 1, 0.6))

	var x := 4.0
	draw_string(font, Vector2(x, FONT_SIZE + 2), title, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE)
	x += font.get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x + 10
	for s in _series:
		draw_string(font, Vector2(x, FONT_SIZE + 2), s.label, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, s.color)
		x += font.get_string_size(s.label, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x + 8
