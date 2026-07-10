class_name LocationData
extends Resource

var id: int
var name: String
var hex_color: Color
var owner_id: int = -1

# geometry / map info
var area: int = 0
var centroid: Vector2i = Vector2i.ZERO
var border_pixels
var neighbor_ids: PackedInt32Array = PackedInt32Array()
var pixels

# gameplay data
var development: int = 0
var tax_income: float = 0.0
var gdp: float = 0.0
var population: int = 0

func _init(_id: int, _hex_color: Color) -> void:
	id = _id
	hex_color = _hex_color

# func to_string() -> String:
#     return "LocationData(id=%d, name=%s, color=%s)" % [id, name, hex_color]
