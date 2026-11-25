extends Node2D
class_name GameMap

# Node that holds all logic pertaining to the map, including rendering, data management, etc.
# This class functions as a controller for map-related functionality.

const DEFAULT_MAP_PATH := "res://MapData/uk_eu5.png"

@export var sim_world: SimWorld
@onready var map_renderer: MapRenderer = $MapRenderer
@onready var location_map: Sprite2D = $MapRenderer/LocationMap

signal hover_changed(location_id: int)
signal selection_changed(location_id: int)

var hovered_location_id: int = -1
var selected_location_id: int = -1

func _ready():
	map_renderer.sim_world = sim_world
	pass

func _process(_delta: float) -> void:
	pass

func _unhandled_input(event):
	if event is InputEventMouseMotion:
		# Update hovered ID in map
		set_hovered_location(get_location_at_mouse())
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			# Update selected ID in map
			var location_id = get_location_at_mouse()
			set_selected_location(location_id)
			#if $CanvasLayer/Topbar.set_ownership_toggle and location_id >= 0:
				#Game.submit(Command.make_set_location_owner(location_id, 0))
				#map_renderer.set_map_mode($CanvasLayer/Topbar.mapmode)
			#if $CanvasLayer/Topbar.remove_ownership_toggle and location_id >= 0:
				#Game.submit(Command.make_set_location_owner(location_id, 1))
				#map_renderer.set_map_mode($CanvasLayer/Topbar.mapmode)
# Location selection functions

func set_hovered_location(location_id: int) -> void:
	if hovered_location_id == location_id:
		return
	hovered_location_id = location_id
	map_renderer.set_hovered_location(location_id)
	hover_changed.emit(location_id)
	
func set_selected_location(location_id: int) -> void:
	if selected_location_id == location_id:
		return
	selected_location_id = location_id
	map_renderer.set_selected_location(location_id)
	selection_changed.emit(location_id)

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
	var hex_image: Image = load_map_image(hex_map_path)
	if hex_image.is_empty():
		return

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
	map_renderer.set_map_mode(SimWorld.MAP_POLITICAL)

# Returns the location image as RGB8, the format SimWorld.build_from_image requires. Empty on failure.
static func load_map_image(path: String) -> Image:
	var tex = load(path) as Texture2D
	if tex == null:
		push_error("Failed to load location hex map image: %s" % path)
		return Image.new()

	var image: Image = tex.get_image()
	if image.is_compressed():
		image.decompress()
	if image.get_format() != Image.FORMAT_RGB8:
		image.convert(Image.FORMAT_RGB8)
	return image

func on_render_dirty(mask: int):
	if mask != 0:
		map_renderer.refresh()
