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

---

## 0. The three decisions this doc is built on

1. **The authoritative simulation lives in C++; networking stays in GDScript.**
2. **First milestone = route one real command end-to-end** (GDScript → tick loop →
   C++ execution → checksum), proving the whole seam on a vertical slice.
3. **Systematization is deferred.** Adopt the integer **ID registry** now; *design*
   the stat/modifier engine (§7) but do not build it until real mechanics demand it.

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
  DeterministicSession (netcode: scheduling, ordering, transport, tick frontier)
  Command factory · submission validation · prediction
        │
        │  coarse calls: execute_command(type_id, payload) · queries · get_state_hash()
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

## 3. Naming & the `SimWorld` transition

- The C++ node currently named **`MapState`** becomes **`SimWorld`** — it has
  outgrown "map state" (it already owns countries + politics + map modes).
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

## 4. The command pipeline (the milestone target)

This is the vertical slice we build first. The netcode half already exists in
[`deterministic_session.gd`](../project/Scripts/deterministic_session.gd); the new
work is redirecting execution into C++.

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

**Definition of done for Milestone 1:** clicking a location issues an
`ASSIGN_OWNERSHIP` command that travels the full path above (not the current direct
`set_location_owner` call from `main.gd`), executes inside C++ on its scheduled tick,
updates the political map, and contributes to a C++-side checksum. Everything after
that (more command types, economy, pops) is "add another handler."

### 4.1 Command handler split (the part the notes don't resolve)

Because execution moved to C++, the note-01 "handler" concept splits by concern:

| Concern | Lives in | Why |
|---|---|---|
| Command construction (factory) | GDScript | UI-facing, changes often |
| Submission validation (format/permission/spoof) | GDScript | Cheap, pre-network |
| **Authoritative execution** | **C++** | Mutates the authoritative world |
| Execution validation (is this legal *against world state*?) | **C++** | Needs world state |
| Prediction (immediate UI effect + reconciliation) | GDScript | Presentation only |

Registry pattern applies on **both** sides: GDScript has a `type_id → {factory,
predictor}` table; C++ has a `type_id → execute_fn` dispatch. Neither side uses a
giant `match`.

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

### 4.3 Prediction (note 01, adopted as-is)

Predicted UI value = `confirmed value + effects of ordered unresolved local commands`,
**recalculated** from confirmed state whenever anything changes — never incrementally
undone. A prediction is removed when the command **executes** (not when merely
accepted/scheduled), so the UI never jumps. Confirmed values come from cheap C++
getters. Invariant: *with zero pending commands, displayed == confirmed.*

---

## 5. The simulation-backend seam

`DeterministicSession` talks to the world through a narrow interface, so the netcode
never hard-codes C++ specifics and the loop stays independently testable:

```
SimulationBackend (conceptual)
  execute_command(type_id, payload) -> bool
  step_tick()                          # advance one sim tick (calendar, systems)
  get_state_hash() -> int              # C++ checksum of authoritative state
  # + read-only query getters for UI/prediction
```

Implementation is `SimWorld` (C++). The seam exists so that: (a) the command loop can
be unit-tested against a trivial fake, and (b) `DeterministicSession` depends on an
interface, not on C++ internals. This is note 01's "SimulationBackend" and note 02's
"bridge" — **they are the same seam**; don't build two.

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

From [`ARCHITECTURE.md`](ARCHITECTURE.md) §7 — fix these as the relevant code is
touched during the `SimWorld` reorg, not as a separate sweep:

1. `map.gd` calls `map_state.get_location_by_id(...)` — **not a bound method** (latent crash).
2. `Geography.owner_id` — dead duplicate of `LocationPolitics.owner_country`; **delete** (violates §1 "one source of truth").
3. Two conflicting `CountryID` typedefs (`int32_t` in geography vs `uint32_t` in countries) — consolidate.
4. Dead `create_data_arrays`/`rgb_key` in `map.gd` (GDScript reimplementation of `MapBuilder`) — delete.
5. Border shader never wired to `id_tex` — borders unrendered.
6. `default_delay_ticks = 0` — set to ≥1 so the SP host player uses the same scheduled path (note 01).

---

## 10. Roadmap

- **Milestone 1 — vertical slice (current focus).** Rename `MapState`→`SimWorld`;
  add `execute_command` + `get_state_hash`; route `ASSIGN_OWNERSHIP` through the full
  GDScript→C++ path; retire `world_state.gd` into the backend seam. Clear the §9 debt
  that these files touch.
- **Milestone 2 — command infrastructure.** Command factory + dual registries +
  explicit type-ID contract; port `SET_PAUSED`/`SET_SPEED` through the new path;
  rigorous prediction on a scalar test value (note 01 §prediction test cases).
- **Milestone 3 — first real system.** Introduce an economy/population store
  (`Fixed64`) and a `step_tick` system; ground it against the Secular Cycles model in
  `Planning/`, one section at a time.
- **Milestone 4 — a sim-driven map mode** (e.g. wealth-per-capita) to prove
  sim→query→render end to end.
- **Later, only when earned:** stat/modifier engine (§7); reverse-index caches
  (country→locations, civ→countries) via rebuild, not maintained lists (note 02);
  neighbor/adjacency graph; free-list entity deletion; the rest of note 04.

## 11. Explicitly NOT now (guard against scope creep)

Stat/modifier/effect/trigger/scope engines · event bus · flow model · free-list
deletion + generation counters · neighbor/border extraction · location→pixel reverse
index · multiple Godot API objects (`world.map`, `world.countries`, …) · dedicated
server mode. Each is noted so it isn't forgotten — none is a prerequisite for the
milestones above.
```
