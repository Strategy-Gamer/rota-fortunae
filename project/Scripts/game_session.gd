class_name GameSession
extends Node2D

## Orchestrates the full command-and-tick loop for both SP and MP.
## In singleplayer: local client + local host with 1-tick in-memory delay.
## Every peer has exactly one my_world. The host's my_world *is* the authoritative state.
## Commands always go through the full scheduling path (even for the host player).

@export var default_delay_ticks: int = 1  # 1 for SP, increase (e.g. 4-8) for MP latency buffer
@export var my_player_id: int = 1
@export var is_host: bool = true  # true for the host peer (SP or MP host)

@onready var sim_world: SimWorld = $SimWorld
@onready var map: GameMap = $Map

@export var locationMapPath: String = "res://MapData/uk_eu5.png"

# Command queuing (shared for simplicity in skeleton; in real MP each peer has its own pending)
var pending_confirmed: Dictionary = {}  # tick -> Array[Dictionary] (sorted on execution)
var command_log: Array[Dictionary] = []  # For replay/debug

# Transport simulation
var _inbox_host: Array[Dictionary] = []      # {delivery_tick, cmd}
var _outbox_clients: Dictionary = {}         # peer_id -> Array[{delivery_tick, cmd}]

var next_local_seq: int = 0
var last_executed_tick: int = -1

var ticks_paused: bool = false

signal command_executed(tick: int, cmd: Dictionary)
signal state_hash_reported(tick: int, hash_value: int)
signal desync_detected(tick: int, expected: int, actual: int)

func _ready() -> void:
	Game.bind(self, sim_world)
	map.load_map(locationMapPath) 

func _physics_process(_delta: float) -> void:
	"""Drive the synch clock at fixed rate."""
	if ticks_paused:
		return
	_on_tick(sim_world.get_tick())

func _unhandled_input(event):
	if event is InputEventMouseMotion:
		# Update hovered ID in map
		map.set_hovered_location(map.get_location_at_mouse())
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			# Update selected ID in map
			var location_id = map.get_location_at_mouse()
			map.set_selected_location(location_id)
			if $CanvasLayer/Topbar.set_ownership_toggle and location_id >= 0:
				Game.submit(Command.make_set_location_owner(location_id, 0))
				map.map_renderer.set_map_mode($CanvasLayer/Topbar.mapmode)
			if $CanvasLayer/Topbar.remove_ownership_toggle and location_id >= 0:
				Game.submit(Command.make_set_location_owner(location_id, 1))
				map.map_renderer.set_map_mode($CanvasLayer/Topbar.mapmode)

func _on_tick(tick: int) -> void:
	"""Fixed timestep heartbeat."""

	# Process & Execute Commands
	process_incoming_messages(tick)
	
	if is_host:
		_host_process_tick(tick)
	else:
		_client_process_tick(tick)
	
	var dirty = sim_world.take_render_dirty()
	if dirty != 0: map.on_render_dirty(dirty)

func _host_process_tick(tick: int) -> void:
	# Execute scheduled commands on our (authoritative) world
	if pending_confirmed.has(tick):
		var cmds: Array = pending_confirmed[tick]
		cmds.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
			if a.player_id != b.player_id:
				return a.player_id < b.player_id
			return a.local_seq < b.local_seq
		)
		
		for cmd in cmds:
			Game.world.execute_command(cmd.type_id, cmd.actor_civ, cmd.payload)
			command_log.append({"tick": tick, "cmd": cmd.duplicate()})
			command_executed.emit(tick, cmd)
		
		pending_confirmed.erase(tick)
	
	# Process tick simulation side
	sim_world.step_tick()
	last_executed_tick = tick
	
	_create_and_distribute_bundle(tick)

func _client_process_tick(tick: int) -> void:
	# Clients only advance if they have the bundle
	if pending_confirmed.has(tick):
		var cmds: Array = pending_confirmed[tick]
		cmds.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
			if a.player_id != b.player_id:
				return a.player_id < b.player_id
			return a.local_seq < b.local_seq
		)
		
		for cmd in cmds:
			#my_world.execute_command(cmd)
			command_executed.emit(tick, cmd)
		
		pending_confirmed.erase(tick)
		# Process tick simulation side
		sim_world.step_tick()
		last_executed_tick = tick

func process_incoming_messages(current_tick: int) -> void:
	"""Simulated transport. Handles delivery to host inbox and local outbox."""
	# Deliver to host
	var still_pending_host: Array[Dictionary] = []
	for msg in _inbox_host:
		if msg.delivery_tick <= current_tick:
			_host_receive_command(msg.cmd)
		else:
			still_pending_host.append(msg)
	_inbox_host = still_pending_host
	
	# Deliver confirmed commands to this peer
	if _outbox_clients.has(my_player_id):
		var still_pending: Array[Dictionary] = []
		for msg in _outbox_clients[my_player_id]:
			if msg.delivery_tick <= current_tick:
				_client_receive_confirmed(msg.cmd)
			else:
				still_pending.append(msg)
		_outbox_clients[my_player_id] = still_pending

func submit_command(cmd = null) -> void:
	"""Every peer (including host) issues commands this way. Predict immediately."""
	next_local_seq += 1
	
	if cmd.type_id == SimWorld.CMD_STEP_CLOCK:
		_on_tick(Game.world.get_tick())
		return
	
	# Send to host via simulated transport (delay even for host player in SP)
	var delivery_delay: int = 0 if is_host else default_delay_ticks
	var delivery_tick: int = sim_world.get_tick() + delivery_delay
	_inbox_host.append({
		"delivery_tick": delivery_tick,
		"cmd": cmd.duplicate()
	})

func _host_receive_command(cmd: Dictionary) -> void:
	"""Host validates and schedules (called for everyone's commands, including host player's)."""
	if not Command.is_valid_submission(cmd, cmd.player_id):
		print("Host rejected invalid submission from player %d" % cmd.player_id)
		return
	
	var exec_tick: int = sim_world.get_tick() + default_delay_ticks
	cmd.exec_tick = exec_tick
	
	if not pending_confirmed.has(exec_tick):
		pending_confirmed[exec_tick] = []
	pending_confirmed[exec_tick].append(cmd)
	
	_broadcast_confirmed_to_clients(cmd)

func _broadcast_confirmed_to_clients(cmd: Dictionary) -> void:
	"""Broadcast confirmed command (loopback to self for host player)."""
	var delivery: int = sim_world.get_tick() + 0
	for peer_id in [1, 2]:  # Skeleton: hardcoded. Expand for real peers.
		if is_host: # Skip if host
			continue
		if not _outbox_clients.has(peer_id):
			_outbox_clients[peer_id] = []
		_outbox_clients[peer_id].append({
			"delivery_tick": delivery,
			"cmd": cmd.duplicate()
		})

func _client_receive_confirmed(cmd: Dictionary) -> void:
	"""Queue confirmed command for future execution on this peer's my_world."""
	var et: int = cmd.exec_tick
	if et <= last_executed_tick:
		return
	
	if not pending_confirmed.has(et):
		pending_confirmed[et] = []
	pending_confirmed[et].append(cmd)

func _create_and_distribute_bundle(tick: int) -> void:
	"""Placeholder for full tick bundles (future optimization)."""
	pass

func get_my_world_debug() -> String:
	return "debug"

# Save / load (authoritative when host)
func save_snapshot() -> Dictionary:
	return {}

func load_snapshot(data: Dictionary) -> void:
	pass
