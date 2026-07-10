extends Camera2D

@export var move_speed: float = 1000.0
@export var zoom_step: float = 0.1
@export var min_zoom: float = 0.1
@export var max_zoom: float = 100.0

func _process(delta: float) -> void:
	_handle_move(delta)


func _unhandled_input(_event: InputEvent) -> void:
	_handle_zoom()

func _handle_move(delta: float) -> void:
	var dir = Vector2.ZERO
	
	if Input.is_action_pressed("cam_up"):
		dir.y -= 1.0
	if Input.is_action_pressed("cam_down"):
		dir.y += 1.0
	if Input.is_action_pressed("cam_left"):
		dir.x -= 1.0
	if Input.is_action_pressed("cam_right"):
		dir.x += 1.0
	
	if dir != Vector2.ZERO:
		dir = dir.normalized()
		
		var zoom_factor = zoom.x
		position += dir * move_speed * (1.0 / zoom_factor) * delta

func _handle_zoom():
	var changed = false
	var new_zoom = zoom
	
	if Input.is_action_pressed("cam_zoom_in"):
		new_zoom *= 1.0 + zoom_step
		changed = true
	if Input.is_action_pressed("cam_zoom_out"):
		new_zoom *= 1.0 - zoom_step
		changed = true
	
	if changed:
		new_zoom.x = clamp(new_zoom.x, min_zoom, max_zoom)
		new_zoom.y = new_zoom.x
		zoom = new_zoom
