extends SceneTree

## Screenshot runner. Boots the real main scene, advances the sim one year at a time (so the history
## graphs fill as they do in play), selects a location, fits the map on screen, and saves a PNG per
## map mode. Needs a real window, not --headless, since it captures what the renderer drew. The game
## starts maximized; size=WxH captures at a fixed window size instead.
##
## godot --path project -s res://Tools/screenshot.gd -- out=C:/tmp size=1600x900 years=150 location=200 modes=phase,radicalism

const MODES := {
	"location_color": SimWorld.MAP_LOCATION_COLOR,
	"political": SimWorld.MAP_POLITICAL,
	"phase": SimWorld.MAP_PHASE,
	"satisfaction": SimWorld.MAP_SATISFACTION,
	"radicalism": SimWorld.MAP_RADICALISM,
	"population_pressure": SimWorld.MAP_POPULATION_PRESSURE,
	"elite_overproduction": SimWorld.MAP_ELITE_OVERPRODUCTION,
	"legitimacy": SimWorld.MAP_LEGITIMACY,
	"control": SimWorld.MAP_CONTROL,
	"war": SimWorld.MAP_WAR,
}

func _initialize() -> void:
	root.add_child(load("res://Scenes/main.tscn").instantiate())
	_run.call_deferred()

func _run() -> void:
	var args := _parse_args(OS.get_cmdline_user_args())
	var out: String = args.get("out", "")
	if out == "":
		push_error("Pass out=<directory>")
		quit(1)
		return
	var years: int = args.get("years", "150").to_int()
	var location: int = args.get("location", "-1").to_int()
	var modes: PackedStringArray = args.get("modes", "political").split(",")

	if args.has("size"):
		var dims: PackedStringArray = args["size"].split("x")
		root.get_window().mode = Window.MODE_WINDOWED
		root.get_window().size = Vector2i(dims[0].to_int(), dims[1].to_int())
	await _frames(10)  # let the session build the map and scenario, and the window settle
	var game = root.get_node("Game")  # autoload names aren't in scope for -s scripts
	_fit_camera(game)
	game.map.set_selected_location(location)
	for y in years:
		game.world.advance_years(1)
		var mask: int = game.world.take_render_dirty()
		game._session.render_dirty.emit(mask)
		game.map.on_render_dirty(mask)

	for name in modes:
		if not MODES.has(name):
			push_error("Unknown map mode %s; known: %s" % [name, ", ".join(MODES.keys())])
			continue
		game.map.map_renderer.set_map_mode(MODES[name])
		await _frames(3)
		var path := out.path_join("%s.png" % name)
		root.get_viewport().get_texture().get_image().save_png(path)
		print("saved ", path)
	quit()

func _fit_camera(game) -> void:
	var camera: Camera2D = game._session.get_node("Camera2D")
	var map_size := Vector2(game.world.get_width(), game.world.get_height())
	var view := Vector2(root.get_viewport().get_visible_rect().size)
	var zoom: float = min(view.x / map_size.x, view.y / map_size.y) * 0.95
	camera.position = map_size / 2
	camera.zoom = Vector2(zoom, zoom)

func _frames(n: int) -> void:
	for i in n:
		await process_frame

func _parse_args(raw: PackedStringArray) -> Dictionary:
	var args := {}
	for arg in raw:
		var parts := arg.split("=", true, 1)
		if parts.size() == 2:
			args[parts[0]] = parts[1]
	return args
