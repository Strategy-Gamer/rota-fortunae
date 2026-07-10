extends Area2D
class_name StateArea

signal hover_over_state(state: StateArea)
signal hover_off_state(state: StateArea)
signal click_state(state: StateArea)

var state_name = ""
var id = 0
var owner_tag = null
var terrain_type = null
var region = null

# Instantiation
func _on_child_entered_tree(node: Node) -> void:
	pass
	
func _on_base_map_child_entered_tree(node: Node) -> void:
	node.color = getColor(false)

func _on_overlay_map_child_entered_tree(node: Node) -> void:
	node.color = Color(1,1,1,1)
	node.modulate = Color(1,1,1,0);

func getColor(hover: bool):
	var r = 0.6
	var g = 0.25
	var b = 0.25
	var hover_intensity = 0.2
	
	if hover:
		r += (1 - r) * hover_intensity
		g += (1 - g) * hover_intensity
		b += (1 - b) * hover_intensity
		
	return Color(r,g,b,1)

# Hover
func _on_mouse_entered() -> void:
	for node in $"Base Map".get_children():
		node.color = getColor(true)
	#for node in $"Overlay Map".get_children():
		#node.modulate = Color(1,1,1,0.5);
	hover_over_state.emit(self)

func _on_mouse_exited() -> void:
	for node in $"Base Map".get_children():
		node.color = getColor(false)
	#for node in $"Overlay Map".get_children():
		#node.modulate = Color(1,1,1,0);
	hover_off_state.emit(self)

# Click
func _on_input_event(viewport: Node, event: InputEvent, shape_idx: int) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.is_pressed():
		click_state.emit(self)
