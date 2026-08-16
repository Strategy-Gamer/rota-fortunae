extends Node

var _session: GameSession = null
var world: SimWorld
func bind(session: GameSession, sim_world: SimWorld) -> void: 
	_session = session
	world = sim_world
func submit(cmd: Dictionary) -> void:
	if _session != null:
		cmd["player_id"] = _session.my_player_id
		_session.submit_command(cmd)
