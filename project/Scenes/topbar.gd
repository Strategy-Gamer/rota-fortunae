extends Control

var set_ownership_toggle = false
var remove_ownership_toggle = false
var mapmode = 1
var gamespeed = 0
var gamespeeds = [1, 5, 30, 90, 365]

func _process(delta: float):
	if Game.world != null:
		$PanelContainer/Rightside/PauseButton.text = "Date: " + str(Game.world.get_year()) + "-" + str(Game.world.get_day()) + "\nSpeed: " + str(Game.world.get_speed())

func _set_gamespeed(speed):
	if speed > 4:
		speed = 4
	elif speed < 0:
		speed = 0
	gamespeed = speed
	Game.submit(Command.make_set_game_speed(gamespeeds[gamespeed]))

func _get_gamespeed() -> int:
	for i in range(len(gamespeeds)):
		if gamespeeds[i] == Game.world.get_speed():
			return i;
	return 0;

func _on_increase_speed_button_pressed() -> void:
	_set_gamespeed(_get_gamespeed() + 1)

func _on_decrease_speed_button_pressed() -> void:
	_set_gamespeed(_get_gamespeed() - 1)

func _on_pause_button_pressed() -> void:
	Game.submit(Command.make_toggle_pause(!Game.world.is_paused()))


func _on_set_owner_button_pressed() -> void:
	set_ownership_toggle = !set_ownership_toggle
	remove_ownership_toggle = false

func _on_remove_owner_button_pressed() -> void:
	remove_ownership_toggle = !remove_ownership_toggle
	set_ownership_toggle = false
	


func _on_toggle_mapmodes_button_pressed() -> void:
	if mapmode == 0:
		Game._session.map.map_renderer.set_map_mode(MapRenderer.MapMode.POLITICAL)
		#map.map_renderer.set_map_mode(MapRenderer.MapMode.POLITICAL)
		mapmode = 1
	elif mapmode == 1:
		Game._session.map.map_renderer.set_map_mode(MapRenderer.MapMode.LOCATION_COLOR)
		#map.map_renderer.set_map_mode(MapRenderer.MapMode.LOCATION_COLOR)
		mapmode = 0
