extends SceneTree

## Headless playtest runner. Generates a seeded scenario, advances whole years in C++, and records
## each country's trajectory plus the state hash every year. runs > 1 repeats the same seed and
## fails if any year's hash differs.
##
## godot --headless --path project -s res://Tools/playtest.gd -- seed=0 countries=8 years=300 runs=2 out=C:/tmp/run.csv

const CSV_HEADER := "year,hash,country_id,name,locations,population,wealth,wealth_per_capita,elites,phase,P,E,U,U_e,legitimacy,treasury,war_weariness,crises"

func _init() -> void:
	var args := _parse_args(OS.get_cmdline_user_args())
	var seed: int = args.get("seed", "0").to_int()
	var countries: int = args.get("countries", "8").to_int()
	var years: int = args.get("years", "300").to_int()
	var runs: int = args.get("runs", "1").to_int()
	var out_path: String = args.get("out", "")

	var map_path: String = args.get("map", GameMap.DEFAULT_MAP_PATH)
	var image := GameMap.load_map_image(map_path)
	if image.is_empty():
		quit(1)
		return

	print("seed=%d countries=%d years=%d runs=%d" % [seed, countries, years, runs])

	var first_hashes: Array = []
	var exit_code := 0
	for run in runs:
		var t0 := Time.get_ticks_usec()
		var hashes := _run(image, map_path, seed, countries, years, out_path if run == 0 else "", run == 0)
		print("run %d: %d ms, final hash %d" % [run, (Time.get_ticks_usec() - t0) / 1000, hashes.back() if not hashes.is_empty() else 0])

		if hashes.is_empty():
			exit_code = 1
			break
		if run == 0:
			first_hashes = hashes
		elif hashes != first_hashes:
			var year := 0
			while hashes[year] == first_hashes[year]:
				year += 1
			print("NONDETERMINISTIC: run %d diverges from run 0 at year offset %d" % [run, year])
			exit_code = 1

	if runs > 1 and exit_code == 0:
		print("deterministic across %d runs" % runs)
	quit(exit_code)

# Returns the state hash for the start and after every year; empty if the scenario couldn't be built.
func _run(image: Image, map_path: String, seed: int, countries: int, years: int, out_path: String, report: bool) -> Array:
	var world := SimWorld.new()
	world.build_from_image(image, GameMap.read_sidecar(map_path, "_locations.txt"), GameMap.read_sidecar(map_path, "_straits.txt"))
	if not world.generate_random_scenario(seed, countries):
		push_error("Could not generate a scenario with %d countries" % countries)
		world.free()
		return []

	var file: FileAccess = null
	if out_path != "":
		file = FileAccess.open(out_path, FileAccess.WRITE)
		if file == null:
			push_error("Could not open %s for writing" % out_path)
			world.free()
			return []
		file.store_line(CSV_HEADER)

	var hashes: Array = []
	for i in years + 1:
		if i > 0:
			world.advance_years(1)
		var state_hash: int = world.get_state_hash()
		hashes.append(state_hash)

		var is_last := i == years
		if file == null and not (report and is_last):
			continue
		var summaries: Dictionary = world.get_country_summaries()
		for id in world.get_country_ids():
			var s: Dictionary = summaries[id]
			var row := "%d,%d,%d,%s,%d,%d,%.3f,%.6f,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.6f,%d" % [
					world.get_year(), state_hash, id, s.name, s.location_count, s.population, s.wealth,
					s.wealth_per_capita, s.elites, s.phase, s.population_pressure, s.elite_overproduction,
					s.unrest, s.effective_unrest, s.legitimacy, s.treasury, s.war_weariness, s.active_crises]
			if file != null:
				file.store_line(row)
			if report and is_last:
				print(row)

	if file != null:
		file.close()
	world.free()
	return hashes

func _parse_args(raw: PackedStringArray) -> Dictionary:
	var args := {}
	for arg in raw:
		var parts := arg.split("=", true, 1)
		if parts.size() == 2:
			args[parts[0]] = parts[1]
	return args
