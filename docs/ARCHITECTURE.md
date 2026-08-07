# Rota Fortunae — Architecture

> Purpose of this doc: a **map for reading the code**. It explains how the pieces
> connect, how each subsystem works, what was left half-done in the C++ port, and
> what needs building next. When something here drifts from the code, fix the code
> or fix this doc — don't let them disagree.

Rota Fortunae is an empire-building "god game": you play the spirit of a
civilization, raise and lose empires, and accumulate legacy/tech/culture. The
long-term simulation model is Turchin-style **Secular Cycles** (see `Planning/`).

---

## 1. The big picture: two worlds, one target

The codebase currently contains **two subsystems that barely talk to each other**:

- **World A — the Map.** C++-backed geography, countries, ownership, and the
  rendering data. This works today.
- **World B — the Netcode.** A GDScript deterministic-lockstep command loop
  (`deterministic_session.gd` → `world_state.gd`). The plumbing is solid but the
  "game state" it drives is placeholder counters (`test_value`).

They are linked only by a single unused reference (`session.map = map` in
`main.gd`). **The actual game simulation does not exist yet** — building it, in
C++, and wiring it into the netcode loop is the central work ahead.

### Target division of responsibility

```
GDScript                          C++
--------                          ---
Netcode: when & in what order      Simulation: what happens + holds state
things happen (scheduling,         (economy, pops, war — deterministic)
ordering, transport, host/client)
        │  execute_command(cmd)            uses Fixed64 for ALL math
        ▼  on the scheduled tick           (no floats — see §6)
   C++ SimState  ───────────────►  map_modes palettes ──► shader (already built)
```

Rule of thumb: **GDScript owns timing and coordination; C++ owns state and math.**
Networking stays in GDScript on purpose — that's where Godot's multiplayer APIs
live, and singleplayer is modelled as "a one-player multiplayer game where you are
your own host."

---

## 2. Directory map

```
src/                        C++ backend (the real one; ignore godot-cpp/ — vendored bindings)
  register_types.cpp        GDExtension entry point. Registers exactly ONE class: MapState.
  godot/
    map_state.{h,cpp}       THE C++↔GDScript bridge. Every backend call crosses here.
  map/
    geography.{h,cpp}       Geography struct: SoA (struct-of-arrays) map data.
    map_builder.{h,cpp}     Scans an RGB image → assigns LocationIDs, area, centroids.
    map_images.{h,cpp}      Builds the ID image + palette image for the shader.
    map_modes.{h,cpp}       Builds per-map-mode palette rows (location color / political).
  countries/
    countries.{h,cpp}       CountryStore: names + colors, indexed by CountryID.
    location_politics.{h,cpp}  LocationPolitics: location → owning country.
  utility/
    FixedDecimal.h          Fixed32/Fixed64 deterministic fixed-point math. UNUSED so far.

project/                    Godot project (GDScript + scenes + shaders + map images)
  Scenes/
    main.gd                 Startup: loads map, creates the session, wires UI.
    topbar.gd, menu.gd, map_camera.gd, province_area.gd
  Scripts/
    map.gd (GameMap)        Controller for everything map-related. Holds MapState + renderer.
    map_renderer.gd         Turns MapState images into textures + shader materials.
    deterministic_session.gd  The lockstep command loop (host/client, transport, prediction).
    world_state.gd          "Authoritative" per-peer state. Currently placeholder counters.
    calendar.gd             In-game date (pauses, has speed). Integer accumulator.
    synch_clock.gd          Netcode heartbeat tick (never pauses).
    command.gd              Command dictionary factory + validation.
    country_data.gd, command.gd, world_state.gd, ...
  Shaders/
    map.gdshader            Province fill shader (ID → palette lookup).
    map_mode.gdshader       (map-mode variant)

Planning/                   Design docs + the Secular Cycles economy model (Python prototypes).
docs/                       This doc + design-notes/ (pasted AI conversations, research).
```

---

## 3. How it connects (data flow)

### Startup (`project/Scenes/main.gd`)
1. `map.load_map(locationMapPath)` — builds World A from a PNG.
2. `session = DeterministicSession.new()`; set `is_host`, `my_player_id`, `session.map = map`.
3. Wire UI: `Menu.session`, `Topbar.session`, `Topbar.mapstate = map.map_state`.

### Map load (`map.gd::load_map`)
1. Load PNG, force `FORMAT_RGB8`.
2. `map_state.build_from_image(img)` → C++ `MapBuilder::build_from_rgb8`.
   - **Each unique pixel color becomes one LocationID.** Fills `Geography`:
     `pixel_location` (pixel→id lookup), `display_color_rgb`, `area`, `centroid_x/y`.
   - `politics_.initialize(location_count)` sets every location to unowned.
3. `map_renderer.prepare_rendering()` builds the textures (below).
4. `create_test_countries()` — debug: makes England/Scotland, checkerboards ownership.

### Rendering (`map_renderer.gd` + shaders) — the clever part
Two stacked sprites share one **ID texture** (each pixel encodes its LocationID as
RGB). The fill shader reads a pixel's ID, uses it as an **X index into a
1-pixel-tall palette texture**, and paints that color.

- **Change map mode** = regenerate the tiny palette row in C++
  (`create_map_mode_palette`) and call `palette_texture.update()`. No geometry
  touched → instant.
- **Hover / select** = pass `hovered_id` / `selected_id` as shader uniforms. The
  shader tints that one ID. Zero per-frame CPU cost.
- **Consequence:** any new visualization (unrest, population, wealth-per-capita)
  is just "add a `MapMode` enum value + a palette builder in `map_modes.cpp`."
  The rendering path is already general.

### The command loop (`deterministic_session.gd`) — World B
1. Player action → `Command.create(...)` dictionary (`command.gd`), gets a `local_seq`.
2. **Predicted immediately** for snappy UI (`_apply_prediction`), never touches
   authoritative state.
3. Sent to the host through a **simulated transport** (`_inbox_host` /
   `_outbox_clients` arrays fake network delay even in singleplayer).
4. Host validates, assigns an `exec_tick`, schedules into `pending_confirmed[tick]`.
5. On that tick, **all** commands run in deterministic order
   (sort by `player_id`, then `local_seq`), then prediction is reconciled to reality.
6. Every 30 ticks, `get_state_hash()` produces a checksum for desync detection.

This is a correct textbook lockstep skeleton — it just drives `test_value` instead
of a real world. Completing it = replacing `world_state.gd`'s placeholder state
with calls into the C++ sim, and computing the desync hash **in C++** over real state.

---

## 4. The C++ ↔ GDScript boundary (the contract)

`MapState` (`src/godot/map_state.h`) is the **only** class exposed to Godot. Bound
methods (see `_bind_methods` in `map_state.cpp`) — this is the whole API surface:

| Method | Purpose |
|---|---|
| `build_from_image(image)` | Build geography from an RGB8 image |
| `clear()` / `is_loaded()` | Reset / check loaded |
| `get_width/height/location_count()` | Map dimensions |
| `get_location_id_at_pixel(px)` | Pixel → LocationID (−1 if none) |
| `get_location_color(id)` | Location's source color |
| `get_location_area(id)` / `get_location_centroid(id)` | Per-location geometry |
| `create_country(name, color)` → id | Add a country |
| `get_country_count/name/color(id)` | Country lookups |
| `set_location_owner(loc, country)` / `get_location_owner(loc)` | Ownership |
| `create_map_mode_palette(mode)` | 1px palette image for the shader |
| `create_id_image()` / `create_palette_image()` | Textures for rendering |

Everything crossing the boundary is a Godot type (`String`, `Color`, `Vector2i`,
`Ref<Image>`). Colors are packed to `uint32` RGB internally (`pack_color`/`unpack_color`).

---

## 5. Subsystem quick-reference (for reading the code)

- **`Geography` (`map/geography.h`)** — Struct-of-arrays: parallel `std::vector`s all
  indexed by `LocationID`. Cache-friendly; keep this pattern. `location_at(x,y)`
  does the pixel→id lookup with bounds checking.
- **`MapBuilder` (`map/map_builder.cpp`)** — Single pass over pixels. Uses an
  `unordered_map<color, LocationID>` to dedupe colors; accumulates area + centroid
  sums, then `finalize_centroids` divides (integer division — deterministic).
- **`CountryStore` (`countries/countries.h`)** — SoA of `alive`, `names`,
  `display_color_rgb`. `create_country` appends and returns the new id.
- **`LocationPolitics` (`countries/location_politics.h`)** — one array:
  `owner_country[LocationID]`. **This is the real ownership store** (see debt §7.2).
- **`map_modes.cpp`** — `create_location_color_palette` (raw colors) and
  `create_political_palette` (owner's color, else grey `UNOWNED_COLOR`). Add new
  modes here.
- **`SynchClock` vs `GameCalendar`** — two clocks. SynchClock = netcode heartbeat,
  60Hz, **never pauses**. GameCalendar = in-game date, pauses, speed multiplier via
  an integer accumulator crossing `ADVANCE_THRESHOLD`. Both integer-only by design.
- **`FixedDecimal.h`** — `Fixed32` (3 decimals) / `Fixed64` (6 decimals),
  pure-integer fixed-point. Exists so the future C++ sim can be **bit-identical
  across machines** (lockstep requirement). Currently referenced nowhere.

---

## 6. Determinism rules (do not break these)

Lockstep multiplayer means every machine must compute **bit-identical** results
from the same commands. Therefore:

1. **All simulation math goes through `Fixed64` — never `float`/`double`.** Floats
   round differently across compilers/CPUs → desync. This is the entire reason
   `FixedDecimal.h` exists.
2. **Fixed iteration order.** Commands already sort by `(player_id, local_seq)`.
   Any loop over game entities that affects state must have a stable order (avoid
   iterating hash maps by hash order).
3. **Seeded, deterministic RNG only** (integer, reproducible) — no `randf()`.
4. **The desync hash must cover the real state.** Once the sim moves to C++, the
   hash in `world_state.gd::get_state_hash` must be replaced by a C++ hash over the
   authoritative C++ state.

---

## 7. Known debt / what the C++ port left behind

Small and specific — these are migration seams, not "bad code":

1. **Latent crash.** `map.gd::get_location_at_id` (line ~47) calls
   `map_state.get_location_by_id(...)`, which **is not a bound method** on
   `MapState`. Nothing calls it yet, so it hasn't fired. Fix or remove.
2. **Duplicated ownership.** `Geography.owner_id` (`geography.h`) is written at
   build (`INVALID`) and cleared, but **never read**. Real ownership lives in
   `LocationPolitics.owner_country`. Drop `Geography.owner_id` to avoid two sources
   of truth.
3. **Two `CountryID` types, same name.** `map/geography.h` → `int32_t` (invalid
   `-1`); `countries/countries.h` → `uint32_t` (invalid `UINT_MAX`). Different
   signedness + sentinel = sign-conversion footgun. Consolidate on one definition.
4. **Dead GDScript.** `map.gd::create_data_arrays` + `rgb_key` reimplement in
   GDScript what `MapBuilder` now does in C++. Unused leftover — delete.
5. **Border shader stubbed.** `map_renderer.gd::_prepare_border_map` never wires
   `id_tex`; borders aren't actually drawn yet.
6. **Map loading mixed into the controller.** `map.gd` marks a "TO BE MOVED TO
   MAPLOADER LATER" section; loading wants to be its own unit eventually.

---

## 8. Roadmap — what to build next

Ordered roughly by dependency:

1. **Settle the sim's home in C++.** Decide whether the simulation lives inside
   `MapState` or a new `SimState`/`World` C++ class registered alongside it.
   (Leaning: a dedicated sim class, so map/geometry stays separate from economy.)
2. **Design the netcode ↔ C++ sim bridge.** `world_state.gd` shrinks to: forward
   `execute_command` into C++, read state back, hash in C++. Define the command
   set the sim understands (this is where the "how to structure commands to scale
   to hundreds" research goes — see `docs/design-notes/`).
3. **Port a first real system** (candidate: the Secular Cycles economy from
   `Planning/`) into C++ using `Fixed64`. Ground it one section at a time.
4. **Pay down the §7 debt** as you touch each area (cheap now, annoying later).
5. **Add a real map mode** driven by sim state (e.g. wealth-per-capita) to prove
   the sim→render path end to end.

---

## 9. Open design questions

- **Command scaling.** How to structure commands so the loop handles dozens–hundreds
  per tick without choking (batching, bundles — `_create_and_distribute_bundle` is
  a placeholder for this). Research notes live in `docs/design-notes/`.
- **Where the desync hash is computed** once state is in C++ (see §6.4).
- **Save/load** currently serializes only the placeholder state; needs to cover the
  real C++ sim state (snapshot the authoritative world).
```
