extends Node2D
class_name MapRenderer

# Node that handles rendering the map using shaders
# It uses the SimWorld to get the necessary textures and data
# All functions here are meant to be called from outside, e.g. from a controller node
# It does not handle any input or logic itself, and assumes the caller knows what they are doing

@export var province_shader: Shader
@export var border_shader: Shader

@onready var sim_world: SimWorld
@onready var map_sprite: Sprite2D = $LocationMap
@onready var border_sprite: Sprite2D = $BorderMap

var id_texture: ImageTexture
var palette_texture: ImageTexture

var selected_location_id: int = -1
var hovered_location_id: int = -1

enum MapMode {
	LOCATION_COLOR = 0,
	POLITICAL = 1,
}

func prepare_rendering() -> void:
	var id_image: Image = sim_world.create_id_image()
	var palette_image: Image = sim_world.create_map_mode_palette(
		MapMode.LOCATION_COLOR
	)

	id_texture = ImageTexture.create_from_image(id_image)
	palette_texture = ImageTexture.create_from_image(
		palette_image
	)

	_prepare_location_map()
	_prepare_border_map()

func _prepare_location_map() -> void:
	map_sprite.texture = id_texture
	map_sprite.centered = false
	map_sprite.position = Vector2.ZERO
	map_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	var mat := ShaderMaterial.new()
	mat.shader = province_shader
	mat.set_shader_parameter("id_tex", id_texture)
	mat.set_shader_parameter("palette_tex", palette_texture)
	mat.set_shader_parameter("selected_id", selected_location_id)
	mat.set_shader_parameter("hovered_id", hovered_location_id)

	map_sprite.material = mat


func _prepare_border_map() -> void:
	border_sprite.texture = id_texture
	border_sprite.centered = false
	border_sprite.position = Vector2.ZERO
	border_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	var mat := ShaderMaterial.new()
	mat.shader = border_shader

	# Add this only if the border shader declares an id_tex uniform.
	# material.set_shader_parameter("id_tex", id_texture)

	border_sprite.material = mat
	border_sprite.visible = true

func set_map_mode(map_mode: int) -> void:
	var palette_image: Image = (
		sim_world.create_map_mode_palette(map_mode)
	)

	if palette_image == null or palette_image.is_empty():
		push_error("Invalid map-mode palette.")
		return

	if palette_texture == null:
		palette_texture = (
			ImageTexture.create_from_image(
				palette_image
			)
		)
	else:
		palette_texture.update(palette_image)

	var mat := (
		map_sprite.material as ShaderMaterial
	)

	if mat != null:
		mat.set_shader_parameter(
			"palette_tex",
			palette_texture
		)
	
func set_selected_location(location_id: int) -> void:
	selected_location_id = location_id
	var mat = map_sprite.material as ShaderMaterial
	mat.set_shader_parameter("selected_id", selected_location_id)

func set_hovered_location(location_id: int) -> void:
	hovered_location_id = location_id
	var mat = map_sprite.material as ShaderMaterial
	mat.set_shader_parameter("hovered_id", hovered_location_id)
