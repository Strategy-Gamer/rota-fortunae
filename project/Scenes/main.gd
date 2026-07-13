extends Node2D

@export var locationMapPath: String = "res://MapData/locations.png"

@onready var map: GameMap = $Map
@onready var session = DeterministicSession.new()

func _ready():
	#var path = OS.get_system_dir(OS.SystemDir.SYSTEM_DIR_DOCUMENTS) + "/Paradox Interactive/Hearts of Iron IV/mod/EoaNB/toi/history/states/1-Corsica.txt"
	map.load_map(locationMapPath)
	session.is_host = true
	session.my_player_id = 1
	session.map = map
	add_child(session)
	$CanvasLayer/Menu.session = session


func _unhandled_input(event):
	if event is InputEventMouseMotion:
		# Update hovered ID in map
		map.set_hovered_location(map.get_location_at_mouse())
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			# Update selected ID in map
			map.set_selected_location(map.get_location_at_mouse())


		

#Import JSON files and converts to lists or dictionary
func import_file(filepath):
	var file = FileAccess.open(filepath, FileAccess.READ)
	if file != null:
		#print(file.get_as_text())
		return JSON.parse_string(file.get_as_text())
	else:
		print("Failed to open file:", filepath)
		return null
