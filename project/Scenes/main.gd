extends Node2D

const session_scene = preload("res://scenes//game_session.tscn")

func _ready():
	#var path = OS.get_system_dir(OS.SystemDir.SYSTEM_DIR_DOCUMENTS) + "/Paradox Interactive/Hearts of Iron IV/mod/EoaNB/toi/history/states/1-Corsica.txt"
	var session = session_scene.instantiate()
	add_child(session)




		

#Import JSON files and converts to lists or dictionary
func import_file(filepath):
	var file = FileAccess.open(filepath, FileAccess.READ)
	if file != null:
		#print(file.get_as_text())
		return JSON.parse_string(file.get_as_text())
	else:
		print("Failed to open file:", filepath)
		return null
