extends Node2D
class_name GameMap

# Node that holds all logic pertaining to the map, including rendering, data management, etc.
# This class functions as a controller for map-related functionality.

# Path to the location hex map image
@onready var map_state: MapState = $"MapState"
@onready var map_renderer: MapRenderer = $MapRenderer
@onready var location_map: Sprite2D = $MapRenderer/LocationMap

func _ready():
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

func get_location_at_id(location_id: int) -> int:
	return map_state.get_location_by_id(location_id)
func get_location_at_px(pos: Vector2i) -> int:
	return map_state.get_location_id_at_pixel(pos)

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
	map_state.build_from_image(hex_image)
	
	t1 = Time.get_ticks_usec()
	print(
		"Built map state with %d locations in %d us"
		% [
			map_state.get_location_count(),
			t1 - t0
		]
	)
	
	map_renderer.prepare_rendering()
	print("Map rendering prepared.")
	create_test_countries()
	
func create_test_countries() -> void:
	var england: int = map_state.create_country(
		"England",
		Color(0.8, 0.15, 0.15)
	)

	var scotland: int = map_state.create_country(
		"Scotland",
		Color(0.15, 0.3, 0.85)
	)

	var location_count := (
		map_state.get_location_count()
	)

	for location_id in range(location_count):
		if location_id % 2 == 0:
			map_state.set_location_owner(
				location_id,
				england
			)
		else:
			map_state.set_location_owner(
				location_id,
				scotland
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

func create_data_arrays(map_hex_image: Image) -> Dictionary:

	var data: PackedByteArray = map_hex_image.get_data()
	# For FORMAT_RGB8: bytes per pixel = 3 (R, G, B)

	var w = map_hex_image.get_width()
	var h = map_hex_image.get_height()

	var id_bytes = PackedByteArray()
	id_bytes.resize(map_hex_image.get_width() * map_hex_image.get_height() * 3)

	var lut: PackedInt32Array = PackedInt32Array()  # Lookup table for colors to IDs
	lut.resize(1<<24)  # 24-bit colors

	var id = 0
	var pi = 0 # pixel index
	var di = 0 # data index
	var n = w * h

	var t0 = Time.get_ticks_usec()
	while pi < n:
		var r = data[di]
		var g = data[di + 1]
		var b = data[di + 2]
		var key = r | (g << 8) | (b << 16)
		
		var v = lut[key]
		if v == 0:
			id += 1
			lut[key] = id
			v = id
		
		var province_id = v - 1  # IDs start at 0
		id_bytes[di] = int(province_id & 0xFF)
		id_bytes[di + 1] = int((province_id >> 8) & 0xFF)
		id_bytes[di + 2] = int((province_id >> 16) & 0xFF)
		
		pi += 1
		di += 3
	var t1 = Time.get_ticks_usec()
	print("create_data_arrays loop took %d us" % (t1 - t0))

	# for y in range(h):
	# 	for x in range(w):
	# 		var idx = (y * w + x) * 3
	# 		var r = data[idx]
	# 		var g = data[idx + 1]
	# 		var b = data[idx + 2]
	# 		var key = rgb_key(r, g, b)
			
	# 		var v = lut[key]
	# 		if v == 0:
	# 			id += 1
	# 			lut[key] = id
	# 			v = id

	# 		var province_id = v - 1  # IDs start at 0
	# 		id_bytes[idx] = int(province_id & 0xFF)
	# 		id_bytes[idx + 1] = int((province_id >> 8) & 0xFF)
	# 		id_bytes[idx + 2] = int((province_id >> 16) & 0xFF)

	return { "id_bytes": id_bytes, "num_locations": id }

static func rgb_key(r:int, g:int, b:int) -> int:
	return r | (g << 8) | (b << 16)
