extends Node2D
class_name GameMap

# Node that holds all logic pertaining to the map, including rendering, data management, etc.
# This class functions as a controller for map-related functionality.

# Path to the location hex map image
@export var sim_world: SimWorld
@onready var map_renderer: MapRenderer = $MapRenderer
@onready var location_map: Sprite2D = $MapRenderer/LocationMap

func _ready():
	map_renderer.sim_world = sim_world
	pass

func _process(_delta: float) -> void:
	pass

func _unhandled_input(_event: InputEvent) -> void:
	pass

# Scenario initialization functions
func create_random_scenario(seed: int = 0) -> void:
	# Creates random scenario on the map
	# Map must already be loaded

	# Create Countries

	# Assign Capital Locations to Countries

	# Assign Locations to Countries based on proximity to Capitals

	pass

# Location selection functions

func set_hovered_location(location_id: int) -> void:
	if map_renderer.hovered_location_id == location_id:
		return

	map_renderer.set_hovered_location(location_id)
func set_selected_location(location_id: int) -> void:
	if map_renderer.selected_location_id == location_id:
		return
	map_renderer.set_selected_location(location_id)

func get_location_at_px(pos: Vector2i) -> int:
	return sim_world.get_location_id_at_pixel(pos)

func get_location_at_mouse() -> int:
	var mouse_pos = location_map.get_global_mouse_position()
	var local_pos = location_map.to_local(mouse_pos)

	var x = int(floor(local_pos.x))
	var y = int(floor(local_pos.y))

	return get_location_at_px(Vector2i(x, y))

## TO BE MOVED TO MAPLOADER LATER ##

func load_map(hex_map_path) -> void:

	var t0 = Time.get_ticks_usec()
	print("Loading map from: %s" % hex_map_path)
	var hex_image: Image = _load_image(hex_map_path)
	if hex_image.is_empty():
		push_error("Failed to load location hex map image: %s" % hex_map_path)
		return
	print("Loaded hex map image: %s x %s" % [hex_image.get_width(), hex_image.get_height()])

	if hex_image.is_compressed():
		hex_image.decompress()
	if hex_image.get_format() != Image.FORMAT_RGB8:
		hex_image.convert(Image.FORMAT_RGB8)

	var t1 = Time.get_ticks_usec()
	print(
		"Loaded map image: %d x %d in %d us"
		% [
			hex_image.get_width(),
			hex_image.get_height(),
			t1 - t0
		]
	)

	# Process hex_image
	t0 = Time.get_ticks_usec()
	sim_world.build_from_image(hex_image)
	
	t1 = Time.get_ticks_usec()
	print(
		"Built map state with %d locations in %d us"
		% [
			sim_world.get_location_count(),
			t1 - t0
		]
	)
	
	map_renderer.prepare_rendering()
	print("Map rendering prepared.")
	create_test_countries()
	
func create_test_countries() -> void:
	var england: int = sim_world.create_country(
		"England",
		Color(0.8, 0.15, 0.15)
	)

	var scotland: int = sim_world.create_country(
		"Scotland",
		Color(0.15, 0.3, 0.85)
	)

	map_renderer.set_map_mode(
		MapRenderer.MapMode.POLITICAL
	)

func _load_image(path: String) -> Image:
	var tex = load(path) as Texture2D
	if tex == null:
		push_error("Failed to load texture: %s" % path)
		return Image.new()

	return tex.get_image()

func on_render_dirty(mask: int):
	if mask != 0:
		map_renderer.set_map_mode(
			MapRenderer.MapMode.POLITICAL
		)
