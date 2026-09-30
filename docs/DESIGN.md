# Rota Fortunae — Target Architecture & Design

> **What this doc is:** the architecture we are *building toward*, distilled from the
> design conversations in [`design-notes/`](design-notes/) and the decisions made on
> top of them. [`ARCHITECTURE.md`](ARCHITECTURE.md) describes the code as it exists
> **today**; this describes where it is **going** and why. When a decision here is
> implemented, update `ARCHITECTURE.md` to match reality.
>
> **Source notes:** `01` (lockstep/commands), `02` (data-oriented design + the C++
> bridge), `03` (map design), `04` (systematizing variables). Where a decision
> departs from a note, it's called out — usually because note `01`/`04` predate the
> decision to put the authoritative simulation in C++.
>
> **Status — Milestone 2 in progress.** Milestone 1 (the command spine) is done: the
> `MapState`→`SimWorld` transition, C++ FNV-1a checksum (`get_state_hash`), scene/ownership
> reorg (`GameSession` is the in-game root, `SimWorld` + `Map` its children), the `Game`
> autoload command bus, and retirement of the GDScript sim
> (`world_state`/`calendar`/`synch_clock`/`deterministic_session`) are all complete. The
> command path is proven twice over — first via **time-control commands** (pause/speed),
> and ownership now travels the full pipeline too (`make_set_location_owner` → `Game.submit`
> → scheduled → C++); the direct `set_location_owner` mutation is retired and the prediction
> stubs are deleted. Two decisions changed during implementation: **prediction is cut**
> (§4.3) and a **dirty-flag refresh** pattern was adopted (§6.5). **Milestone 2** adds the
> first real simulation systems (population/productivity/wealth), day/year tick cadence, real
> speed presets, a query layer, and a location UI panel — broken into five ordered steps in
> §10. The only M1 leftover is the unwired border shader, deferred to map work (§9).

---

## 0. The decisions this doc is built on

1. **The authoritative simulation lives in C++; networking stays in GDScript.**
2. **Prove it on a vertical slice first** (GDScript → tick loop → C++ execution →
   checksum) before broadening. ✓ done (Milestone 1).
3. **Systematization is deferred.** Adopt the integer **ID registry** now; *design*
   the stat/modifier engine (§7) but do not build it until real mechanics demand it.
4. **No prediction** (§4.3) — input latency + immediate feedback instead; add bespoke
   prediction per-action only if one ever feels laggy.

---

## 1. Core principles (the non-negotiables)

- **Authority, not language, is the real boundary.** GDScript may *describe* a change
  (build a command); it must never *apply* one to authoritative state. C++ owns the
  world; GDScript owns presentation, input, and network coordination.
- **Determinism is mandatory** (lockstep requires bit-identical results everywhere):
  - All **simulation** math uses `Fixed64`/`Fixed32` ([`FixedDecimal.h`](../src/utility/FixedDecimal.h)) — **never `float`/`double`.**
  - Fixed iteration order everywhere state is touched (no hash-order iteration).
  - Seeded integer RNG only.
  - The **desync checksum is computed in C++** over authoritative state.
  - Geometry/rendering data (pixels, colors, centroids) may stay integer/float — it
    is not authoritative simulation state. Keep that line clean (see §6).
- **Data-oriented storage.** Entities are integer IDs indexing parallel arrays
  (struct-of-arrays). No per-entity Godot objects for simulation data.
- **One source of truth.** For each fact there is exactly one authoritative array
  (e.g. `owner_country[location]`). Everything else — reverse indexes, palettes,
  ledgers, territory counts — is *derived* and rebuildable. Never two authoritative
  copies. *(The current `Geography.owner_id` vs `LocationPolitics.owner_country`
  duplication violates this and is scheduled for removal — see §9.)*
- **Coarse boundary calls.** Cross the C++/GDScript line with a few big calls
  ("build this map", "give me this location summary", "execute this command"), never
  thousands of tiny per-entity calls.

---

## 2. The layered architecture

```
GDScript ─ presentation & coordination
  UI · input · camera · map rendering glue
  GameSession (netcode: scheduling, ordering, transport, tick frontier)
  Command factory · submission validation   ·   Game autoload (command bus + world read accessor)
        │
        │  coarse calls: execute_command(type_id, actor_civ, payload) · step_tick() · queries · get_state_hash()
        ▼
C++ ─ SimWorld  (godot::Node, the ONLY registered bridge class)
  owns one  rota::World
  bindings split across domain .cpp files (world node stays thin)
        │
   ┌────┴─────────────┬───────────────────┐
   ▼                  ▼                   ▼
 Stores            Systems             Queries
 (own memory)      (mutate state)      (read + aggregate + format)
   │                                        │
   └──────────────── rota::World ───────────┘
        Geography · CountryStore · LocationPolitics · (future: Population, Economy…)
```

**Layer responsibilities (note 02):**
- **Stores** — plain C++ SoA data + invariant helpers only. No Godot types. No
  cross-store logic.
- **Systems** — free functions that mutate the world, especially across stores
  (`create_country(World&, ...)`, `assign_owner(World&, loc, country)`).
- **Queries** — free functions that read (possibly across stores) and produce
  answers or presentation data (`build_political_palette(World&)`,
  `build_location_summary(World&, loc)`). Map modes are queries, **not** store methods.
- **Bridge (`SimWorld`)** — translates Godot types ↔ plain C++, dispatches commands,
  exposes queries. Contains no simulation logic itself.

> **Pragmatic note for a solo dev learning C++:** keep the *directory seams* so the
> layers can grow, but don't pre-build dozens of empty files. Early on, a "system"
> or "query" can be a single free function next to its store. Split when it earns it.

---

## 3. Naming & the `SimWorld` transition — ✓ done (Milestone 1)

*This section records a completed transition; kept for context.*

- The C++ node formerly named **`MapState`** is now **`SimWorld`** — it had
  outgrown "map state" (it owns countries + politics + map modes).
- `SimWorld` owns one `rota::World` struct aggregating all stores:
  ```cpp
  struct World {
      map::Geography          geography;
      countries::CountryStore countries;
      countries::LocationPolitics politics;
      // future: Population, Economy, SecularCycle, ...
      std::uint64_t tick = 0;
  };
  ```
- `register_types.cpp` still registers **exactly one** class (`SimWorld`). Splitting
  its implementation across `world_state_map.cpp`, `_countries.cpp`, `_bindings.cpp`,
  etc. is a *file* organization choice; SCons compiles files, `register_types`
  registers classes.
- **GDScript `world_state.gd` is retired.** Its role ("authoritative per-peer state")
  moves into `SimWorld`. What remains GDScript-side is a **thin backend seam** (§5)
  so `DeterministicSession` doesn't hard-code C++ calls and the pipeline stays
  testable — but it holds no authoritative state.

---

## 4. The command pipeline — ✓ built (Milestone 1)

This was Milestone 1's vertical slice, now built. The netcode half lives in
[`game_session.gd`](../project/Scripts/game_session.gd); execution is redirected into C++
via `SimWorld.execute_command`. It stays documented here because every future mechanic is
"add another handler" on this exact path.

```
UI / AI
   │  CommandFactory.make_assign_owner(player, location, country)
   ▼
Command (dict: type_id, player_id, local_seq, payload)  ──►  predict locally (GDScript)
   ▼  submit to host via (simulated) transport
Host: validate submission → assign exec_tick = current + input_delay → broadcast
   ▼  every peer queues it at exec_tick
On exec_tick, in deterministic order (player_id, local_seq):
   ▼
sim_world.execute_command(type_id, payload)          ← the ONE new bridge call
   ▼  C++ dispatches type_id → handler → mutates stores (via a system fn)
World state changes · derived palette marked dirty
   ▼  every 30 ticks
sim_world.get_state_hash()  → desync check   ← checksum computed in C++
```

**Milestone 1 outcome (achieved):** clicking a location issues a `set_location_owner`
command that travels the full path above (no longer a direct C++ mutation), executes
inside C++ on its scheduled tick, updates the political map, and contributes to the
C++-side checksum. Everything after this (more command types, economy, pops) is "add
another handler."

### 4.1 Command handler split (the part the notes don't resolve)

Because execution moved to C++, the note-01 "handler" concept splits by concern:

| Concern | Lives in | Why |
|---|---|---|
| Command construction (factory) | GDScript | UI-facing, changes often |
| Submission validation (format/permission/spoof) | GDScript | Cheap, pre-network |
| **Authoritative execution** | **C++** | Mutates the authoritative world |
| Execution validation (is this legal *against world state*?) | **C++** | Needs world state |

**Dispatch lives in C++ only.** `rota::core::command::execute_command` looks up
`type_id` in a file-local `unordered_map<int, ExecuteFn>` (anonymous namespace in
`command.cpp`) and calls the handler — no giant `match`. GDScript just *builds* commands
via a factory (`command.gd`); with prediction cut (§4.3) there is **no** GDScript
predictor registry, so the "dual registries" idea is dropped — one C++ dispatch table
is all that's needed.

### 4.2 Command type-IDs are a cross-language contract (a real pitfall)

The `type_id` enum is now shared by two languages. If they drift, you get **silent
desyncs**, the worst kind of bug. Rules:

- **C++ is the source of truth** for the numeric IDs. GDScript mirrors them.
- **Explicit numbers with domain gaps**, never implicit ordering (note 01):
  ```
  NONE = 0
  SET_PAUSED = 1, SET_SPEED = 2          # time control
  ASSIGN_OWNERSHIP = 100                  # territory
  CREATE_COUNTRY = 101
  # economy = 200+, military = 300+, ...
  ```
- **Never renumber or reuse a retired ID** once saves/replays/networking depend on
  it. Mark dead IDs deprecated; leave a gap.
- Keep payloads simple and serializable: integers and IDs. No Godot object state in
  a command payload (it has to survive the network and a save file).

### 4.3 No prediction (decision — supersedes note 01's prediction section)

**Prediction is cut.** For a pausable grand-strategy game, commands land in 1–2 ticks
(tens of ms) — imperceptible — so the complexity and the "looked-like-it-worked-then-
didn't" jank of optimistic prediction aren't worth it. Factorio and Paradox both use
plain *input latency*, not prediction.

- **Default:** UI reads confirmed state via `Game.world.get_X()`; it catches up within a
  couple ticks and nobody notices.
- **Immediate feedback ≠ prediction:** acknowledge the *input* (button depress, "queued"
  marker, ghost outline) without faking the *result*. Simple, never wrong.
- **Bespoke only:** add prediction for a *single* action (unit move, building ghost)
  only if that specific interaction ever feels laggy — note 01's "bespoke presentation."

This removes the `displayed = confirmed + pending` facade, the GDScript predictor
registry, and the whole prediction subsystem. ✓ The dead prediction stubs in
`game_session.gd` (`_apply_prediction`, `predicted_test_value`, `pending_issued`) have
been deleted.

---

## 5. The simulation-backend seam

`GameSession` talks to the world through a narrow interface, so the netcode
never hard-codes C++ specifics and the loop stays independently testable:

```
SimulationBackend (conceptual) — implemented by SimWorld
  execute_command(type_id, actor_civ, payload) -> bool
  step_tick()                          # advance one sim tick (clock heartbeat + systems)
  get_state_hash() -> int              # C++ FNV-1a checksum of authoritative state
  take_render_dirty() -> int           # presentation refresh hints (§6.5)
  # + read-only query getters for UI
```

Implementation is `SimWorld` (C++), fronted GDScript-side by the `Game` autoload.
`GameSession` drives *when* (`_physics_process` → `step_tick`) and owns command
ordering; `SimWorld` owns *what happens* and holds state. This is note 01's
"SimulationBackend" and note 02's "bridge" — **the same seam**; don't build two.

---

## 6. Determinism & the Fixed-point line

- **Authoritative simulation quantities** (population, wealth, treasury, prices,
  productivity, anything that feeds the checksum or the economy) → `Fixed64`/`Fixed32`.
- **Geometry & rendering data** (pixel→ID map, display colors, area, centroids,
  palette bytes) → ordinary `int`/`float`. Not authoritative, not hashed.
- Do **not** promote geometry to fixed-point just because it shares a `LocationID`
  with simulation data. Keep the stores separate (Geography vs Economy) exactly so
  this line stays crisp (note 02 §9).
- The checksum must cover *all and only* authoritative state, in a fixed order.
- **Hash per store** (`feed_hash(Hasher&)` on each store/component), composed in fixed
  order by `SimWorld::get_state_hash`. Per-store hashes are what make Paradox-style
  "which store desynced?" debugging possible later. Feed **fixed-width integers only**
  (`uint32_t`/`uint64_t`, never `int`/`size_t`); bools as a byte; never floats.

---

## 6.5. Presentation refresh & map-mode ownership

The sim mutates state in C++, but the map's palette/texture is rebuilt in **Godot** and
won't know it's stale. The bridge is a **dirty flag**, not signals or session-side
command inspection:

- A C++ command handler marks what it changed: `world.render_dirty |= DIRTY_POLITICAL`.
- After the tick, `GameSession` calls `sim_world.take_render_dirty()` (returns mask,
  clears it) and tells the renderer; the renderer rebuilds **only if its active mode's
  bit is set**. One coarse call per tick, no per-entity crossing.
- **Dirty flags are presentation state — never hashed.** Peers may refresh at different
  real-times without desyncing.

**Map-mode ownership:**
- **Palette generation = C++ query** (`map_modes.cpp`, reads whatever stores a mode
  needs). C++ owns *what the palette is*.
- **Active-mode selection + texture + shader = GDScript `MapRenderer`.** It owns *when
  to show a mode and how to texture it*, pulling palettes via
  `sim_world.create_map_mode_palette(mode)`.
- **Cleanup owed:** the map-mode enum currently exists three times (GDScript enum, C++
  enum, magic `if map_mode == 1`). Give it the same single-source-of-truth
  `BIND_ENUM_CONSTANT` treatment as `CommandType` (§4.2).

---

## 7. Stats & modifiers — designed now, built later

Note 04's stat/modifier engine is the right *eventual* shape, but building it now is
premature generalization (no mechanics to validate it against). **We commit to the
shape on paper and defer the engine.** When ~5 real stats exist and adding the next
one hurts, extract the engine from the working code — don't predict it.

**The agreed shape (for when we build it):**
- A **stat** is data: `id, scope (location/country/civ), kind (stored/derived/
  accumulating), clamp, cadence (daily/monthly/…), rule`. Definitions live in data,
  not bespoke member fields.
- One **`get_stat(entity, stat_id)`** is the single read path: base (or derived
  formula) → apply modifiers in fixed order → clamp. No scattered `+ stability_mod`.
- **Modifiers** are first-class: `(target_stat, op ∈ {ADD,MUL,MIN,MAX,OVERRIDE},
  value, source, condition, duration)`. Laws/techs/events *emit modifiers* rather
  than hand-writing math.
- **Base evolution** (tick drift/decay) is separate from **final value** (modifiers).
- All stat values that feed the sim are **`Fixed64`** (note 04 used float — corrected
  here for determinism).

**What we build now instead:** hardcode the handful of stats a mechanic actually
needs, as plain SoA arrays in their store, updated by an explicit system function.

---

## 8. The ID registry (the one systematization we adopt now)

Every "type" of thing — command types, stat ids, good ids, building ids, pop classes,
etc. — gets a **stable integer ID**, and all runtime code uses ints, not strings.
Data-authoring tools may still be string-based; a load step maps strings → ints once.

Why this one, now: it's the bridge between "data-driven" and "tight arrays," it makes
the language boundary trivial (everything crossing is already an int), and it costs
almost nothing to adopt early but is painful to retrofit. This is also what makes the
command type-ID contract (§4.2) well-defined.

---

## 9. Debt to clear while restructuring

Original list from [`ARCHITECTURE.md`](ARCHITECTURE.md) §7 — most cleared during the
`SimWorld` reorg:

1. ✓ `get_location_by_id` latent crash — removed.
2. ✓ `Geography.owner_id` dead duplicate — removed.
3. ✓ conflicting `CountryID` typedefs — resolved (dead `rota::map::CountryID` deleted).
4. ✓ dead `create_data_arrays`/`rgb_key` in `map.gd` — removed.
5. ⬜ **Border shader never wired to `id_tex`** — borders still unrendered.
   **Deferred** to the next round of map work (bundled with the §6.5 map-mode cleanup).
6. ✓ `default_delay_ticks` now ≥1.

Cleared during the Milestone 1 tidy:
- ✓ **Ownership now travels the command path** (`make_set_location_owner` → `Game.submit`
  → scheduled → C++); the direct `set_location_owner` mutation is retired, so commands are
  the only write path.
- ✓ Dead prediction stubs in `game_session.gd` deleted (§4.3).

Known latent determinism risk (not blocking, no cross-platform MP yet):
- ⬜ **`Fixed64 × Fixed64` / `÷ Fixed64` fall back to a `double` computation** when
  `__int128` is unavailable (pure MSVC `cl.exe`). mingw/clang provide `__int128` and take
  the exact integer path, so the current toolchain is fine and
  [`tests/fixed_decimal_test.cpp`](../tests/fixed_decimal_test.cpp) passes. But a
  Windows-MSVC peer could desync against a Linux/mingw peer. Fix before any cross-platform
  MP: a portable 128-bit multiply in `FixedDecimal.h`. (The `Fixed × int` overloads were
  separately found dividing by `SCALE` and corrected — the source of the wealth/growth being
  off by 1e6; the test file guards against regressions.)

---

## 10. Roadmap

- **Milestone 1 — vertical slice. ✓ COMPLETE.** `MapState`→`SimWorld`;
  `execute_command` + `get_state_hash`; command spine proven via **time-control
  commands**; scene/ownership reorg (`GameSession` root, `Game` autoload bus); GDScript
  sim retired; debt §9 items 1–4,6 cleared. (Leftover tidy: ownership→command, unbind
  mutator, delete prediction stubs — §9.)
- **Milestone 2 — first real systems + location UI (IN PROGRESS).** Five ordered steps:
  1. **Stores + start of the economy system.** New store(s) in a separate file, DoD/SoA:
     `population` (`std::uint64_t`), `productivity` (`Fixed32`), `wealth` (`Fixed64`), per
     location. An **economy system** (its own file, free functions over `World&`) computes
     `wealth = productivity × population`. These *are* §7's "hardcode a handful of stats as
     plain SoA arrays" — **not** the stat engine.
  2. **Day/year rollover detection & execution.** *One* source of action for time
     advancement that detects calendar rollovers and drives system execution **in a defined
     order** — without intertwining the calendar with the systems it triggers. The calendar
     reports "a day/year rolled over"; a separate tick-driver decides *what runs and when*.
     A minimal ordered dispatch hook, **not** note 04's generic cadence engine.
  3. **Tick-multiplier presets.** Real speeds 1–5 as `tick_multiplier` presets wired into
     the time controls.
  4. **Query layer + `build_location_summary`.** Stand up the query seam (free functions
     that read across stores and format for UI) and the first real query,
     `build_location_summary(World&, loc)`. First real read path *out* of C++.
  5. **Location UI (Godot).** A location panel that reads through the `build_location_summary`
     query via `Game.world`. First real presentation of live sim state.
- **Milestone 3 — map modes + queries.** Consolidate the map-mode enum to one source of
  truth (§6.5); wire the dirty-flag refresh; add a **sim-driven map mode** (wealth/pop)
  to prove sim→query→render; stand up a proper query layer (location summary, ledger
  page) as reads grow.
- **Later, only when earned:** threading / within-tick job-splitting for expensive ticks
  (deferred until a profiler shows a freeze; DOD structure makes it a drop-in);
  stat/modifier engine (§7); reverse-index caches via rebuild (note 02); neighbor graph;
  free-list entity deletion; the rest of note 04.

## 11. Explicitly NOT now (guard against scope creep)

Prediction (cut, §4.3) · threading/job system (until profiled) · stat/modifier/effect/
trigger/scope engines · event bus · flow model · free-list deletion + generation
counters · neighbor/border extraction · location→pixel reverse index · multiple Godot
API objects (`world.map`, `world.countries`, …) · dedicated server mode. Each is noted
so it isn't forgotten — none is a prerequisite for the milestones above.
```
