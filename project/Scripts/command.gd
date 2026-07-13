# res://simulation/command.gd
class_name Command
extends RefCounted

## Command definition and helpers.
## Keep payload simple for skeleton. Use specific fields or a Variant payload.

enum Type {
	NONE = 0,
	PAUSE = 1,
	SET_SPEED = 2,
	SET_TEST_VALUE = 3,
	INCREMENT_TEST = 4,
	# Future examples:
	# REQUEST_SET_TAX_RATE = 10,
	# ISSUE_ARMY_ORDER = 20,
}

static func create(player_id: int, local_seq: int, type: Type, payload = null) -> Dictionary:
	"""Factory for command dictionaries (easy to serialize/send)."""
	return {
		"type": type,
		"player_id": player_id,
		"local_seq": local_seq,
		"exec_tick": 0,           # Assigned by host later
		"payload": payload,       # int, bool, or small Dictionary for complex later
		"submission_tick": 0      # Optional: when client issued it
	}

static func is_valid_submission(cmd: Dictionary, current_player_id: int) -> bool:
	"""Basic submission validation (format + permission). Separate from execution validation."""
	if not cmd.has_all(["type", "player_id", "local_seq"]):
		return false
	if cmd.player_id != current_player_id:
		return false  # Spoof check, in real use cryptographic or session token
	if cmd.type <= Type.NONE or cmd.type > Type.INCREMENT_TEST:  # expand range later
		return false
	return true

static func get_type_name(type: Type) -> String:
	match type:
		Type.PAUSE: return "PAUSE"
		Type.SET_SPEED: return "SET_SPEED"
		Type.SET_TEST_VALUE: return "SET_TEST_VALUE"
		Type.INCREMENT_TEST: return "INCREMENT_TEST"
		_: return "UNKNOWN"
