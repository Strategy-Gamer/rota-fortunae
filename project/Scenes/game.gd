extends Node

var _session: GameSession = null
var world: SimWorld
var map: GameMap
func bind(session: GameSession, sim_world: SimWorld, game_map: GameMap) -> void: 
	_session = session
	world = sim_world
	map = game_map
func submit(cmd: Dictionary) -> void:
	if _session != null:
		cmd["player_id"] = _session.my_player_id
		_session.submit_command(cmd)
