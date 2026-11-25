extends Control

func update_ui(selected_location: int):
	self.visible = selected_location >= 0

	if not self.visible:
		return

	var summary = Game.world.get_location_summary(selected_location)
	var owner_name := "none"
	if summary.owner_country >= 0:
		owner_name = Game.world.get_country_name(summary.owner_country)

	# Update UI Elements
	$PanelContainer/VBoxContainer/Owner.text = "Location %d, owner: %s" % [selected_location, owner_name]
	if summary.controller != summary.owner_country:
		$PanelContainer/VBoxContainer/Owner.text += ", held by %s" % Game.world.get_country_name(summary.controller)
	$PanelContainer/VBoxContainer/Population.text = "Population: %d (%d commoners)" % [summary.population, summary.commoners]
	$PanelContainer/VBoxContainer/Wealth.text = "Wealth: %.1f" % summary.wealth
	$PanelContainer/VBoxContainer/Unrest.text = "Unrest: %.2f stock, %.2f after suppression" % [summary.unrest, summary.effective_unrest]
	$"PanelContainer/VBoxContainer/Wealth per Capita".text = "Wealth per head: %.3f" % summary.wealth_per_capita
	$PanelContainer/VBoxContainer/Elites.text = "Elites: %d for %.0f positions (E %.2f)" % [summary.elites, summary.elite_positions, summary.elite_overproduction]
	$PanelContainer/VBoxContainer/Density.text = "Density: %.2f of carrying capacity" % summary.population_density
	$PanelContainer/VBoxContainer/Security.text = "Security: %.2f" % summary.security
	$PanelContainer/VBoxContainer/Control.text = "Control: %.2f" % summary.control


func _on_map_selection_changed(location_id: int) -> void:
	update_ui(location_id)


func _on_game_session_render_dirty(mask: Variant) -> void:
	update_ui(Game.map.selected_location_id)
