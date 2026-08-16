extends Control

func _process(delta: float):
	if Game.world != null:
		$PanelContainer/MarginContainer/VBoxContainer/TickLabel.text = "Tick: " + str(Game.world.get_tick())
		$PanelContainer/MarginContainer/VBoxContainer/DateLabel.text = ""
		$PanelContainer/MarginContainer/VBoxContainer/TimeIncLabel.text = "NextDay: " 
		$PanelContainer/MarginContainer/VBoxContainer/ValueLabel.text = "Value: "

func _unhandled_input(_event: InputEvent) -> void:
	if Input.is_key_pressed(KEY_ESCAPE):
		show()

func set_menu_visible(is_open: bool = false):
	if(is_open):
		show()
	else:
		hide()
		
func _on_continue_button_pressed() -> void:
	hide()

func _on_exit_button_pressed() -> void:
	get_tree().quit()

func _on_increment_tick_button_pressed() -> void:
	Game.submit(Command.make_step_clock())

func _on_toggle_ticks_button_pressed() -> void:
	Game._session.ticks_paused = !Game._session.ticks_paused
