extends Node2D
class_name MapRenderer

# Node that handles rendering the map using shaders
# It uses the MapState to get the necessary textures and data
# All functions here are meant to be called from outside, e.g. from a controller node
# It does not handle any input or logic itself, and assumes the caller knows what they are doing

@export var province_shader: Shader
@export var border_shader: Shader

@onready var map_state: MapState = $"../MapState"
@onready var map_sprite: Sprite2D = $LocationMap
@onready var border_sprite: Sprite2D = $BorderMap

var selected_location_id: int = -1
var hovered_location_id: int = -1

func prepare_rendering() -> void:
	
	# Base province map
	map_sprite.texture = map_state.id_texture
	map_sprite.centered = false
	map_sprite.position = Vector2.ZERO
	map_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	# Setup shader material
	var mat = ShaderMaterial.new()
	mat.shader = province_shader
	mat.set_shader_parameter("id_tex", map_state.id_texture)
	mat.set_shader_parameter("palette_tex", map_state.palette_texture)
	mat.set_shader_parameter("selected_id", selected_location_id)
	mat.set_shader_parameter("hovered_id", hovered_location_id)
	map_sprite.material = mat

	# Setup border map
	border_sprite.texture = map_state.id_texture
	border_sprite.centered = false
	border_sprite.position = Vector2.ZERO
	border_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	var bmat = ShaderMaterial.new()
	bmat.shader = border_shader
	border_sprite.material = bmat
	border_sprite.visible = true

func set_selected_location(location_id: int) -> void:
	selected_location_id = location_id
	var mat = map_sprite.material as ShaderMaterial
	mat.set_shader_parameter("selected_id", selected_location_id)

func set_hovered_location(location_id: int) -> void:
	hovered_location_id = location_id
	var mat = map_sprite.material as ShaderMaterial
	mat.set_shader_parameter("hovered_id", hovered_location_id)
