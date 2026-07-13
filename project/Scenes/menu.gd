extends Control

@export var session: DeterministicSession

func _process(delta: float):
	if session != null:
		$PanelContainer/MarginContainer/VBoxContainer/TickLabel.text = "Tick: " + str(session.clock.current_tick)
		$PanelContainer/MarginContainer/VBoxContainer/DateLabel.text = str(session.my_world.calendar.get_date_string())
		$PanelContainer/MarginContainer/VBoxContainer/TimeIncLabel.text = "NextDay: " + str(session.my_world.calendar.accumulation)
		$PanelContainer/MarginContainer/VBoxContainer/ValueLabel.text = "Value: " + str(session.my_world.test_value) + " | " + str(session.predicted_test_value)

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

func _on_increment_button_pressed() -> void:
	session.submit_command(Command.Type.INCREMENT_TEST, 1)

func _on_zero_button_pressed() -> void:
	session.submit_command(Command.Type.SET_TEST_VALUE, 0)

func _on_increment_tick_button_pressed() -> void:
	session.clock.step()

func _on_toggle_ticks_button_pressed() -> void:
	session.ticks_unpaused = !session.ticks_unpaused
