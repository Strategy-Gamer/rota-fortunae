extends Control

func update_ui(selected_location: int):
	self.visible = selected_location >= 0
	
	if not self.visible:
		return 
		
	var summary = Game.world.get_location_summary(selected_location)
	
	# Update UI Elements
	$PanelContainer/VBoxContainer/Owner.text = "Owner: " + str(summary.owner_country)
	$PanelContainer/VBoxContainer/Population.text = "Population: " + str(summary.population)
	$PanelContainer/VBoxContainer/Wealth.text = "Wealth: " + str(summary.wealth)
	$PanelContainer/VBoxContainer/Productivity.text = "Prod: " + str(summary.productivity)
	$"PanelContainer/VBoxContainer/Wealth per Capita".text = str(summary.wealth_per_capita)
	pass


func _on_map_selection_changed(location_id: int) -> void:
	update_ui(location_id)


func _on_game_session_render_dirty(mask: Variant) -> void:
	update_ui(Game.map.selected_location_id)
