extends Control

@export var session: DeterministicSession
@export var mapstate: MapState
@export var map: GameMap

var set_ownership_toggle = false
var remove_ownership_toggle = false
var mapmode = 1

func _process(delta: float):
	if session != null:
		$PanelContainer/Rightside/PauseButton.text = "Date: " +  str(session.my_world.calendar.get_date_string()) + "\nSpeed: " + str(session.my_world.calendar.speed)

func _on_increase_speed_button_pressed() -> void:
	session.my_world.calendar.set_speed(session.my_world.calendar.speed + 1)

func _on_decrease_speed_button_pressed() -> void:
	session.my_world.calendar.set_speed(session.my_world.calendar.speed - 1)

func _on_pause_button_pressed() -> void:
	session.my_world.calendar.set_paused(!session.my_world.calendar.paused)


func _on_set_owner_button_pressed() -> void:
	set_ownership_toggle = !set_ownership_toggle
	remove_ownership_toggle = false

func _on_remove_owner_button_pressed() -> void:
	remove_ownership_toggle = !remove_ownership_toggle
	set_ownership_toggle = false
	


func _on_toggle_mapmodes_button_pressed() -> void:
	if mapmode == 0:
		map.map_renderer.set_map_mode(MapRenderer.MapMode.POLITICAL)
		mapmode = 1
	elif mapmode == 1:
		map.map_renderer.set_map_mode(MapRenderer.MapMode.LOCATION_COLOR)
		mapmode = 0
