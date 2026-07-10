class_name CountryData
extends Resource

var id: int
var name: String
var color: Color
var capital_location_id: int = -1

func _init(_id: int, _name: String, _color: Color) -> void:
	id = _id
	name = _name
	color = _color