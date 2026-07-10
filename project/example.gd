extends Node


func _ready() -> void:
	var example := MapReader.new()
	var image_data = 0
	example.analyze_rgb8(image_data, 10, 10)
