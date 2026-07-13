# res://simulation/deterministic_session.gd
class_name DeterministicSession
extends Node

## Orchestrates the full command-and-tick loop for both SP and MP.
## In singleplayer: local client + local host with 1-tick in-memory delay.
## Every peer has exactly one my_world. The host's my_world *is* the authoritative state.
## Commands always go through the full scheduling path (even for the host player).

@export var default_delay_ticks: int = 0  # 1 for SP, increase (e.g. 4-8) for MP latency buffer
@export var my_player_id: int = 1
@export var is_host: bool = true  # true for the host peer (SP or MP host)

var clock: SynchClock = SynchClock.new()
var ticks_unpaused: bool = true

var my_world: WorldState = WorldState.new()  # The one and only world state for this peer
var map: GameMap

# Command queuing (shared for simplicity in skeleton; in real MP each peer has its own pending)
var pending_confirmed: Dictionary = {}  # tick -> Array[Dictionary] (sorted on execution)
var command_log: Array[Dictionary] = []  # For replay/debug

# Prediction layer (UI/presentation only - never mutates my_world)
var predicted_test_value: int = 0
var pending_issued: Dictionary = {}  # local_seq -> cmd

# Transport simulation
var _inbox_host: Array[Dictionary] = []      # {delivery_tick, cmd}
var _outbox_clients: Dictionary = {}         # peer_id -> Array[{delivery_tick, cmd}]

var next_local_seq: int = 0
var last_executed_tick: int = -1

signal command_executed(tick: int, cmd: Dictionary)
signal state_hash_reported(tick: int, hash_value: int)
signal desync_detected(tick: int, expected: int, actual: int)

func _ready() -> void:
	clock.ticked.connect(_on_tick)
	# Initialize prediction to match world
	predicted_test_value = my_world.test_value


func _physics_process(_delta: float) -> void:
	"""Drive the synch clock at fixed rate."""
	if ticks_unpaused:
		clock.step()

func _on_tick(tick: int) -> void:
	"""Fixed timestep heartbeat."""
	process_incoming_messages(tick)
	
	if is_host:
		_host_process_tick(tick)
	else:
		_client_process_tick(tick)
	
	# Periodic checksum
	if tick % 30 == 0:
		_report_hash(tick)

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
			my_world.execute_command(cmd)
			command_log.append({"tick": tick, "cmd": cmd.duplicate()})
			command_executed.emit(tick, cmd)
			_reconcile_prediction(cmd)  # Reconcile our own prediction
		
		pending_confirmed.erase(tick)
	
	my_world.step_on_tick()
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
			my_world.execute_command(cmd)
			command_executed.emit(tick, cmd)
			_reconcile_prediction(cmd)
		
		pending_confirmed.erase(tick)
		my_world.step_on_tick()
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

func submit_command(type: Command.Type, payload = null) -> void:
	"""Every peer (including host) issues commands this way. Predict immediately."""
	var cmd: Dictionary = Command.create(my_player_id, next_local_seq, type, payload)
	cmd.submission_tick = clock.get_current_tick()
	next_local_seq += 1
	
	pending_issued[cmd.local_seq] = cmd
	_apply_prediction(cmd)
	
	# Send to host via simulated transport (delay even for host player in SP)
	var delivery_delay: int = 0 if is_host else default_delay_ticks
	var delivery_tick: int = clock.get_current_tick() + delivery_delay
	_inbox_host.append({
		"delivery_tick": delivery_tick,
		"cmd": cmd.duplicate()
	})

func _host_receive_command(cmd: Dictionary) -> void:
	"""Host validates and schedules (called for everyone's commands, including host player's)."""
	if not Command.is_valid_submission(cmd, cmd.player_id):
		print("Host rejected invalid submission from player %d" % cmd.player_id)
		return
	
	var exec_tick: int = clock.get_current_tick() + default_delay_ticks
	cmd.exec_tick = exec_tick
	
	if not pending_confirmed.has(exec_tick):
		pending_confirmed[exec_tick] = []
	pending_confirmed[exec_tick].append(cmd)
	
	_broadcast_confirmed_to_clients(cmd)

func _broadcast_confirmed_to_clients(cmd: Dictionary) -> void:
	"""Broadcast confirmed command (loopback to self for host player)."""
	var delivery: int = clock.get_current_tick() + 0
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

func _apply_prediction(cmd: Dictionary) -> void:
	"""Immediate UI feedback only."""
	match cmd.type:
		Command.Type.SET_TEST_VALUE:
			predicted_test_value = int(cmd.payload)
		Command.Type.INCREMENT_TEST:
			predicted_test_value += int(cmd.payload if cmd.payload != null else 1)

func _reconcile_prediction(cmd: Dictionary) -> void:
	"""Snap prediction to reality after execution."""
	if cmd.type == Command.Type.SET_TEST_VALUE or cmd.type == Command.Type.INCREMENT_TEST:
		predicted_test_value = my_world.test_value

func _report_hash(tick: int) -> void:
	var h: int = my_world.get_state_hash()
	state_hash_reported.emit(tick, h)
	# In real MP clients would send their hash to host for comparison

func get_my_world_debug() -> String:
	return my_world.get_debug_string() + " | Tick: %d | Predicted Test: %d" % [
		clock.current_tick, predicted_test_value
	]

# Save / load (authoritative when host)
func save_snapshot() -> Dictionary:
	return {
		"synch_tick": clock.current_tick,
		"calendar": my_world.calendar.get_state_for_hash(),
		"test_value": my_world.test_value,
		"player_resources": my_world.player_resources.duplicate(),
		"command_log_size": command_log.size(),
		"next_local_seq": next_local_seq
	}

func load_snapshot(data: Dictionary) -> void:
	clock.reset(data.get("synch_tick", 0))
	my_world.test_value = data.get("test_value", 0)
	my_world.player_resources = data.get("player_resources", {}).duplicate()
	var cal_data = data.get("calendar", {})
	my_world.calendar.total_days = cal_data.get("total_days", 0)
	my_world.calendar.speed = cal_data.get("speed", 1)
	my_world.calendar.paused = cal_data.get("paused", false)
	my_world.calendar.accumulation = cal_data.get("accumulation", 0)
	predicted_test_value = my_world.test_value
