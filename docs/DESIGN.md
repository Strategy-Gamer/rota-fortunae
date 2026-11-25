# Rota Fortunae — Design

> The architecture's decisions, the reasons for them, and the traps that are hard to see from the
> source. It is not a map of the code (read the code), and it is not a changelog (read git). When a
> decision changes, rewrite the section here rather than appending a second account.
>
> Raw research lives in [`design-notes/`](design-notes/) and the economy model in
> [`../Planning/`](../Planning/). Where a decision departs from a note, it says so.

---

## 0. The decisions this doc is built on

1. **The authoritative simulation lives in C++; networking stays in GDScript.** GDScript owns
   *when* (scheduling, ordering, transport); C++ owns *what* (state and math). Singleplayer is a
   one-player multiplayer game where you are your own host.
2. **Systematization is deferred.** Adopt the integer **ID registry** now (§8); *design* the
   stat/modifier engine (§7) but don't build it until real mechanics demand it.
3. **No prediction** (§4.3). Use input latency plus immediate feedback instead.

---

## 1. Core principles (the non-negotiables)

- **Authority, not language, is the real boundary.** GDScript may *describe* a change (build a
  command), but it must never *apply* one to authoritative state.
- **Determinism is mandatory.** Lockstep requires bit-identical results on every machine:
  - All **simulation** math uses `Fixed64`/`Fixed32` ([`FixedDecimal.h`](../src/utility/FixedDecimal.h))
    and [`FixedMath`](../src/utility/FixedMath.h). **Never `float`/`double`.**
  - Iteration order is fixed everywhere state is touched. Never iterate in hash order.
  - Only seeded integer RNG.
  - The **desync checksum is computed in C++** over authoritative state.
  - Geometry and rendering data (pixels, colors, centroids) may be integer or float. It isn't
    authoritative state (§6).
- **Data-oriented storage.** Entities are integer IDs indexing parallel arrays (struct-of-arrays).
  Simulation data never lives in per-entity Godot objects.
- **One source of truth.** Each fact has exactly one authoritative array (for example,
  `owner_country[location]`). Reverse indexes, palettes, ledgers and territory counts are
  *derived* and must be rebuildable.
- **Coarse boundary calls.** Cross the C++/GDScript line with a few big calls ("build this map",
  "give me this location summary", "execute this command"), never thousands of per-entity calls.

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
  owns one  rota::core::World
        │
   ┌────┴─────────────┬───────────────────┐
   ▼                  ▼                   ▼
 Stores            Systems             Queries
 (own memory)      (mutate state)      (read + aggregate + format)
   └──────────────── rota::core::World ─┘
```

**Layer responsibilities (note 02):**
- **Stores** hold plain C++ SoA data plus invariant helpers. They contain no Godot types and no
  cross-store logic.
- **Systems** are free functions over `World&` that mutate state, especially across stores (for
  example `create_country` and `set_location_owner`).
- **Queries** are free functions that read, possibly across stores, and produce answers or
  presentation data. Map modes are queries, **not** store methods.
- **Bridge (`SimWorld`)** translates Godot types to plain C++ and back, dispatches commands and
  exposes queries. It contains no simulation logic.

Keep these directory seams, but don't pre-build empty files. A system or query can start as one
free function next to its store; split it out when it earns it.

---

## 3. Rendering: ID texture + palette

Two stacked sprites share one **ID texture**: each pixel encodes its `LocationID` as RGB. The fill
shader reads a pixel's ID, uses it as an X index into a **1-pixel-tall palette texture**, and paints
that color.

- **Changing map mode** regenerates the small palette row in C++ and updates one texture. No
  geometry is touched, so it's instant.
- **Hover and select** pass `hovered_id` / `selected_id` as shader uniforms. They cost no CPU per
  frame.
- **So any new visualization is just a new palette builder** in `map_modes.cpp`. The rendering path
  is already general.

---

## 4. The command pipeline

Every mechanic the player or AI can trigger is "add another handler" on this path:

```
UI / AI  →  command dict (type_id, player_id, local_seq, payload)  →  Game.submit
Host: validate submission → assign exec_tick = current + input_delay → broadcast
On exec_tick, every peer, in deterministic order (player_id, local_seq):
   sim_world.execute_command(type_id, actor_civ, payload)
   → C++ dispatch table → handler → system fn mutates stores → render_dirty bits set
Periodically: sim_world.get_state_hash() → desync check
```

### 4.1 Command handler split

| Concern | Lives in | Why |
|---|---|---|
| Command construction (factory) | GDScript | UI-facing and changes often |
| Submission validation (format, permission, spoofing) | GDScript | Cheap, and happens before the network |
| **Authoritative execution** | **C++** | Mutates the authoritative world |
| Execution validation (is this legal *against world state*?) | **C++** | Needs world state |

Dispatch lives in C++ only: a file-local table in `command.cpp` maps `type_id` to a handler.
Because prediction is cut, there is no GDScript predictor registry. One table is all that's needed.

### 4.2 Command type-IDs are a cross-language contract

The `CommandType` enum in [`command.h`](../src/core/command.h) is shared by two languages:
`SimWorld` binds each ID as a `CMD_*` constant. If the two sides drift, you get **silent desyncs**.

- **C++ is the source of truth.** GDScript reads the bound constants and never hard-codes numbers.
- **Explicit numbers with domain gaps.** The ranges are documented on the enum itself.
- **Never renumber or reuse an ID.** Saves, replays and networking depend on them. Retire an ID
  by leaving a gap.
- **Keep payloads to integers and IDs.** No Godot object state, since a payload has to survive the
  network and a save file. `SimWorld` decodes the payload into plain named integers before any
  handler sees it, so the sim compiles without godot-cpp.

### 4.3 No prediction (supersedes note 01's prediction section)

In a pausable grand-strategy game, commands land within 1–2 ticks (tens of ms), which nobody
notices. Optimistic prediction isn't worth its complexity or the "looked like it worked, then
didn't" jank. Factorio and Paradox both use plain input latency.

- **Default:** the UI reads confirmed state through `Game.world.get_X()`.
- **Immediate feedback is not prediction:** acknowledge the *input* (button press, "queued"
  marker, ghost outline) without faking the *result*.
- **Bespoke only:** add prediction for a single action only if that interaction ever feels laggy.

---

## 5. The simulation-backend seam

`GameSession` talks to the world through a narrow interface implemented by `SimWorld` and fronted
by the `Game` autoload: `execute_command`, `step_tick`, `get_state_hash`, `take_render_dirty`, and
read-only queries. `GameSession` drives *when*; `SimWorld` owns *what happens*. This is note 01's
"SimulationBackend" and note 02's "bridge". They are **the same seam**, so don't build two.

**The playtest runner shares the game's sim path.** `project/Tools/playtest.gd` builds a
`SimWorld` from the same map image and the same seeded scenario generator. It then skips the clock
with `advance_years`, which keeps the day loop in C++. The game ticks through `on_tick`. Both
step through `TimeProgression::advance_day`, so **all per-day sim logic belongs in
`advance_day`**. Anything placed in `on_tick` around it would run in the game but not in the
runner. The game and the runner can't be compared by hash, because the runner never advances the
sync clock; they agree on the sim's outcome by date.

---

## 6. Determinism & the fixed-point line

- **Authoritative simulation quantities** use `Fixed64`/`Fixed32`. That means anything that feeds
  the checksum or the economy.
- **Geometry and rendering data** use ordinary `int`/`float`. It's not authoritative and not
  hashed. Don't promote geometry to fixed-point just because it shares a `LocationID` with sim
  data; keep the stores separate so this line stays crisp (note 02 §9).
- The checksum covers *all and only* authoritative state, in a fixed order. **Each store has its
  own `feed_hash(Hasher&)`**, composed in order by `SimWorld::get_state_hash`, which is what makes
  "which store desynced?" debuggable later. Feed **fixed-width integers only**: bools as a byte,
  never `int`/`size_t`, never floats.
- **Derived data is not hashed**, for example `location_count`. Neither are presentation state
  such as render-dirty flags, or cold archives such as dead-country records.

**One integer code path on every compiler.** The game DLL builds with MSVC, which has no
`__int128`, while the standalone tests build with g++. `Fixed64` multiply and divide (and
`fmath::sqrt`) therefore use `detail::U128`, a 128-bit type built from 64-bit halves, on every
compiler. **Never add `__int128`, compiler-specific branches or floats to fixed-point math**: two
paths means two answers, and the tests would only prove one of them. Both test files print a
digest of their results; after any fixed-point change, run them under g++ *and* MSVC and confirm
the digests match.

**Keep hot-path products narrow.** Multiply and divide take a plain int64 path when the product
fits in 64 bits, and the U128 path (with a 64-step long division) otherwise; both give the same
exact result, so the choice is only about speed. Fine per country, ruinous per location: in the
first cycle port, multiplying two headcounts as `Fixed64` sent ~6 operations per location-year down
the long division and cost more than everything else combined. Keep headcounts as integers
(`Fixed64 × int64` is one multiply), and form ratios before scaling. Likewise, don't evaluate a
series (`exp`, `sqrt`) per location: sample the curve once with `fmath::SampledCurve`, and build
that on first use, never during static init, because `fmath`'s constants live in another
translation unit.

---

## 6.5. Presentation refresh & map-mode ownership

The sim mutates state in C++, but the palette is rebuilt in Godot, which won't know it's stale. The
bridge is a **dirty flag**, not signals:

- A system marks what it changed: `world.render_dirty |= DIRTY_POLITICAL`.
- After the tick, `GameSession` calls `take_render_dirty()` (which returns the mask and clears it).
  The renderer rebuilds only if its active mode's bit is set.
- **Dirty flags are presentation state and are never hashed.** Peers may refresh at different real
  times without desyncing.

**Map-mode ownership:** palette generation is a C++ query (`map_modes.cpp`), so C++ owns *what the
palette is*. Active-mode selection, texture and shader belong to the GDScript `MapRenderer`, which
owns *when and how to show it*. Mode IDs are defined once, in `rota::map::MapMode`; SimWorld binds
them as its `MAP_*` constants, and GDScript never hard-codes the numbers. The renderer rebuilds the
active mode's palette on any dirty bit, since most cycle modes change every year.

**History for graphs is presentation state.** `CycleHistory` (GDScript) samples every country once
per new year through the batch `get_country_summaries()` query. The sim doesn't need the past, so
the history stays out of `World`: it is never hashed, and it starts empty after a load.

---

## 7. Stats & modifiers — designed now, built later

Note 04's stat/modifier engine is the right *eventual* shape, but building it now would be
premature generalization, since there are no mechanics to validate it against. Once about five real
stats exist and adding the next one hurts, extract the engine from the working code.

**The agreed shape:**
- A **stat** is data: `id, scope (location/country/civ), kind (stored/derived/accumulating), clamp,
  cadence, rule`.
- One **`get_stat(entity, stat_id)`** is the single read path: base (or derived formula), then
  modifiers in fixed order, then clamp.
- **Modifiers** are first-class: `(target_stat, op ∈ {ADD,MUL,MIN,MAX,OVERRIDE}, value, source,
  condition, duration)`. Laws, techs and events *emit modifiers* rather than hand-writing math.
- **Base evolution** (per-tick drift and decay) is kept separate from the **final value** (after
  modifiers).
- Every stat value that feeds the sim is **`Fixed64`**. Note 04 used float; this corrects it for
  determinism.

**Until then:** hardcode the handful of stats a mechanic needs, as plain SoA arrays in their store,
updated by an explicit system.

---

## 8. The ID registry

Every "type" of thing (command types, stat IDs, goods, buildings, pop classes) gets a **stable
integer ID**, and runtime code uses ints, not strings. Data-authoring tools may stay string-based,
with a load step mapping strings to ints once. It costs little to adopt early and is painful to
retrofit. It also makes the boundary trivial (everything crossing is already an int) and makes the
command contract (§4.2) well-defined.

---

## 9. Entity lifetimes

Choose the lightest storage that fits each store's churn (design-notes/05):

- **Locations: stable, never deleted.** A plain SoA vector indexed by `LocationID`.
- **Countries: low churn, stable IDs.** Countries rise, fall and fragment, but they are few, their
  rows are wide, and they are referenced **by ID-as-index** (`owner_country[loc]` indexes the
  country arrays). So:
  - The SoA stays indexed by `CountryID`, with a **free list** so array size tracks peak
    simultaneous countries, not cumulative ones.
  - An `alive` tombstone means "this slot is occupied". *Having territory* is derived from
    `owner_country`; it is never stored as truth.
  - A **generation** per slot is bumped on free. `{id, generation}` is a country's permanent
    historical key and guards against stale references.
  - Dead slots are reset to defaults, so the checksum doesn't depend on history.
  - **Never dense-pack or swap-remove.** Reordering would break every ID-as-index reference.
- **Lifecycle is a set of world-level systems** (`country_systems.h`). `CountryStore` is the sole ID
  authority. Sibling per-country stores keep no free list of their own; the create and destroy
  systems reset them in lockstep.
  - **Destruction is deferred:** systems *mark* a country, and the tick driver sweeps marked
    countries after each day's systems have run. No system ever sees a slot freed mid-loop.
    Destroying a country unowns its land and archives it in an append-only, unhashed
    `DeadCountryRecord` list.
  - **Reviving** allocates a *fresh* slot from an archived record, since the old ID may already be
    reused. The old record stays in the archive as history.
- **The first high-churn store** (pops, units, transient orders) is where the full kit earns its
  place: sparse-set IDs, swap-remove and batched deletes. Those aren't referenced by ID-as-index,
  so dense reordering is fine.

---

## 10. The secular-cycle simulation

The model is `FinalSim` in [`Planning/new_model.py`](../Planning/new_model.py) (with `economy.py`
and `extra_functions.py`), ported to
[`secular_cycle_system.cpp`](../src/economy/secular_cycle_system.cpp). The port kept the update
rules, their *ordering* and the parameter values. **The port is finished:** the C++ is now the
model, and new rules are designed here, not in the Python first. One FinalSim tick is one game year
(`on_yearly`).

**The location/country split.** FinalSim fuses a location's economy and its state into one unit.
The game separates them (per `Planning/Rota Fortunae_ Civilizations.txt`):
- **Per location (the default):** commoners, elites, this year's commoner and elite income,
  security, and unrest (the `U` stock and the suppressed `U_e`).
- **Per country (only the state):** legitimacy, treasury, phase, this year's state positions, the
  crises and war-weariness.
- **Aggregation connects them.** Budget inputs (income, elites, positions, the suppression bill)
  are sums over the locations. The phase and the crisis drivers read *commoner-weighted means* of
  the relative wage, immiseration, security and `U_e`, so a country's phase follows its typical
  location, not its worst.
- **Two country→location couplings**, both flowing forward within the year as in FinalSim:
  suppression is spent **per capita** (each location's `U_e` is its `U` minus the country's
  suppression ÷ the country's commoners), and **state positions** (military plus funded patronage)
  are split by each location's share of the country's elites, because they are nationally
  competitive.
- **Crises are per country.** One set of crises and one war-weariness lull per country; each
  crisis hits every owned location by the same fraction. They are **load-bearing**: with the crisis
  rates at zero, FinalSim never leaves fracture.
- **Unowned land sits out** the cycle entirely until a country owns it.

**Step order** keeps FinalSim's sequence as linear passes. Each location pass streams through the
location arrays in `LocationID` order and reads or sums into its owner's row of a per-year
`CountryYear` table. That table is small and hot, so a location's country fields sit together in
one row instead of one array per field. The country steps run between the location passes, in
ascending `CountryID`:

population and economy (locations) → fiscal and legitimacy → unrest (locations) → crises →
crisis effects and elites (locations) → phase.

A year's crises fold into one factor per country, so each location is touched once for all of
them. The one-year lags are the review-critical part: security, the army bill, collection and
legitimacy read *last* year's `U_e`; fracture unrest reads last year's phase.

**Units.** Population is in **whole people** at `LOCATION_CAPACITY` per location (FinalSim's cap of
1). Absolute parameters (`army_base`, `mil_positions`) scale per unit of capacity, which keeps the
model scale-invariant: a 100-location country behaves like a 1-location one. Fractional changes in
commoners and elites become whole people by **keyed stochastic rounding**: the fraction is added as
one person with that probability, drawn from `keyed_hash(seed, year, stream, id)`. So there is no
stored remainder and no RNG stream whose call order matters. Crisis rolls use the same keyed draws.

**Deliberate departures from the Python at the port:**
- New countries start at legitimacy 1. FinalSim's `__init__` resets it to 0 (line 165), which only
  changes its first ~20 years.
- Inactive paths were left out: war deaths (`k_war = 0`), famine, epidemic and bankruptcy (rate 0),
  and the carrying-capacity window (capacity is static). Add each when a mechanic needs it.
- The random scenario seeds each country's starting population (0.4–0.9 of capacity), or every
  country would enter strain in the same year.

**Porting traps:**
- Python's `1e-8`/`1e-9` division guards round to **zero** in `Fixed64`. Use a representable floor
  instead (one person, for example).
- Python's true division `/` and integer division `//` differ. `np.clip` becomes `std::clamp`.
- **Checking the port against Python:** set the crisis rates to zero on both sides. That leaves
  everything deterministic, and a country's yearly trajectory should then track FinalSim closely:
  population and elites to about four significant figures over the first two centuries, with
  identical phases. Unrest is close to bang-bang, so near a sharp transition it can flip a year
  early or late. With crises on, compare statistics over many seeds (phase split,
  period, population and elite ranges) instead, because the Python RNG differs.
- Two traps from `Planning/limitations.md` still apply: population must approach carrying
  capacity, and suppression must not be so cheap that the crisis never fires.

**Fixing the model:** when the sim misbehaves, fix the update rule that produces the wrong
trajectory. Never redefine phases or thresholds, and never add catches, bleeds or resets to force
the outcome. Phases are read-outs painted over the dynamics.

---

## 11. Roadmap

**North star: empires visibly rise and fall.** A viewer presses play and within minutes *watches*
civilizations grow, strain, hit crisis and collapse. That has to show on the map, not just in a log.
"Rise and fall" is only visible if states change on screen when they collapse, so a **minimal
collapse mechanic is core, not stretch**.

**Definition of done for the prototype:**
1. Load a scenario with several countries placed on the map.
2. Press play; time advances at speeds 1–5, paced so a full cycle plays out in **minutes**.
3. Populations grow, wealth accumulates, elites overproduce, unrest climbs, and states hit crisis
   and **visibly collapse** (fragment, lose cohesion or reset elites), then recover.
4. Map modes for population, wealth, unrest and political update live, along with a country panel
   showing the selected state's trajectory.
5. The player can issue **2–3 real commands** that visibly nudge the cycle.

**Next, in order** (the cycle (§10) runs in every country, and the map modes and trajectory panel
are in). Each step has to show on the map by itself:
1. **Geography:** adjacency, land, sea and impassable locations, and routes across the sea, on the
   EU4-based map; a stored capital per country; scenario countries that are contiguous.
2. **Control:** effective distance from the capital (over land and across the sea) sets each
   location's control, which weakens collection, suppression and access to state positions toward
   the frontier. This deliberately ends the scale-invariance in §10.
3. **Civil war on the map:** rebels hold the frontier and high-unrest locations, and elite deaths
   come from devastation where the two sides meet, replacing the cycle's uniform civil-war cull.
4. **Breakaways:** a rebel side that wins with low control secedes as a new country (§9). This is
   the first border change the sim makes by itself.
5. **Conquest,** then **shallow AI** (a few automated reactions, such as raising taxes under
   fiscal stress) and 2–3 player commands. Pacing is tuned so a cycle reads in minutes.

**If ahead, pull in first:** (1) **terrain beyond land and sea** (vegetation, rivers,
elevation), because the map is the primary lens; (2) **better AI**, so states feel governed and not merely cycling.

**Cut to stretch:** pretty UI · curved labels and JFA-SDF borders (design-notes/06) · cultural
values and traits · deep country mechanics · cohesion modelling beyond the minimal collapse ·
multiplayer · regions tier · pops-by-civ · the full economy (consumables, urban, capital, trade,
migration) · partial-map re-evaluation on territory change.

---

## 12. Open debt

- **Border shader never wired to `id_tex`.** Borders are unrendered. Bundle this with the map-mode
  cleanup.
- **No land/sea distinction.** Sea zones are ordinary locations, so the random scenario hands them
  to countries and the economy grows population on them. This needs terrain data.
- **Scenarios are random only.** Authored scenarios (a fixed start date, chosen countries and
  borders) don't exist yet.

## 13. Explicitly NOT now (guard against scope creep)

Prediction (cut, §4.3) · threading or a job system (until a profiler shows a freeze; then
parallelize *within* a tick with a fixed merge order) · stat/modifier/effect/trigger/scope engines
· an event bus · a flow model · a location→pixel reverse index ·
multiple Godot API objects · dedicated server mode. Each is noted so it isn't forgotten; none is a
prerequisite for the roadmap.
