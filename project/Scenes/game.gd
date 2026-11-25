extends Node

var _session: GameSession = null
var world: SimWorld
var map: GameMap

func _enter_tree() -> void:
	get_tree().node_added.connect(_on_node_added)

# Buttons don't keep keyboard focus, so space/enter can't re-press the last one clicked.
# A button that should take focus sets focus_mode in its own _ready, which runs after this.
func _on_node_added(node: Node) -> void:
	if node is BaseButton:
		node.focus_mode = Control.FOCUS_NONE

func bind(session: GameSession, sim_world: SimWorld, game_map: GameMap) -> void: 
	_session = session
	world = sim_world
	map = game_map
func submit(cmd: Dictionary) -> void:
	if _session != null:
		cmd["player_id"] = _session.my_player_id
		_session.submit_command(cmd)
