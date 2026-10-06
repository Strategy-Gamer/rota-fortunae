# Plan — Port the secular-cycle model to deterministic C++ (Prototype Week 2)

## Context

The Python secular-cycle model is finished (`Planning/new_model.py` `FinalSim` + `economy.py`
+ `extra_functions.py`), validated in `finalsim_findings.md`. Week 2 ports it into the
authoritative C++ sim so countries run Turchin cycles and **visibly rise & fall**. Translation,
not redesign: **preserve the update rules and their ordering; transfer parameter values, not
exact numbers** ([[rf-secular-cycle-model]], [[rf-fix-dynamics-not-definitions]],
[[rf-respect-original-intent]]). Deviations get a comment with why.

**The location↔country split (per `Planning/Rota Fortunae_ Civilizations.txt`) — the thing the
first draft got wrong.** FinalSim fuses a location's economy with a state into one unit; the game
separates them:
- **Per location:** pops (Slave/Commoner/Elite × civ), population, wealth (split elite/commoner by
  local wage share), land, productivity, security, control. Doc also puts **elites** and **unrest**
  per location.
- **Per country:** treasury, legitimacy, state capacity, resilience, strategic priorities — the
  fiscal-political state.
- **Aggregation connects them:** the country's fiscal/legitimacy/phase run on **sums of its
  locations** (control/security are weighted averages). `location_economy` is the authoritative
  per-location layer — **kept and extended, NOT retired** (the first draft's error).

**Decisions locked (user):**
1. **Fixed64 + a deterministic math lib** — ✅ DONE (Part A). No `double` in authoritative math.
2. **Per-country cycle, per-location economy.** State stocks (treasury/legitimacy/state-capacity/
   phase) live on the country; population/wealth/economy stay per-location and are aggregated up.
3. **Core cycle first.** Defer `_step_crises` (famine/revolt/civil-war/coup + war-weariness lull +
   seeded RNG) as a fast-follow.

**Confirmed split (user):** per-location is the default; the **only purely-country** quantities are
**state capacity and its variables** (treasury, revenue, suppression). **Elites and unrest are
per-location** (authoritative). **State elite opportunities** (patronage/bureaucracy/military) are
country-funded and added to each location's local opportunity pool. The country still keeps
**accumulators** (total elites, overproduction, aggregate instability, total income) — derived reads
over the per-location truth — because state decisions need them. Two country→location couplings:
**suppression budget** (reduces local unrest) and **state opportunities** (raise local elite
opportunities), both flowing forward within the tick exactly as FinalSim already does. Regions tier
(~100) skipped for now — aggregate location→country directly.

**De-risking sequence:** seed each country with a **single location** first → the per-location
machinery reduces exactly to FinalSim (N=1) → straight-port validation against Python → then scale to
multiple locations. Per-location data model from day one; first runnable cut is the one-location port.

---

## Part A — deterministic fixed-point math lib ✅ DONE

`src/utility/FixedMath.{h,cpp}` — `sqrt/exp/ln/pow/logistic` on `Fixed64`, 49/49 in
`tests/fixed_math_test.cpp`, MSVC-portable. **Underflow probe passed → no unit-rescaling needed**
(port quantities at natural Python scale). API mapping: `logistic`→wage-share curve + P/E/S gauges;
`pow(rel,1.2)`→deaths; `U_e·sqrt(U_e)`→`^1.5` attrition.

## Part B — country lifecycle & territory (build FIRST)

The cycle keys by `CountryID` and countries create/destroy (rise/fall/fragment), so get the
registry right before anything indexes into it (see [[rf-architecture]], design-notes/05 tier-2).

- **`CountryStore` as the sole ID authority.** Add `free_list` (reuse dead slots → size tracks
  peak-concurrent, not cumulative), `generation[]` (stale-ref guard + permanent historical key),
  keep `alive` as a **bool meaning "slot occupied / exists"** — NOT "on map".
- **On-map / has-territory is DERIVED** from `owner_country`, never stored (DESIGN §1 one-source).
  Keep a rebuildable `territory_count[country]` (updated on ownership change) for cheap "owns ≥1".
- **Enumerate a country's locations:** scan `location_politics.owner_country` in ascending
  LocationID order (cheap at yearly cadence); a reverse index is a later optimization.
- **`create_country` / `destroy_country` are World-level *systems*** (free functions over `World&`)
  — `CountryStore` allocates/releases the id; the system `ensure_slot`/`reset_slot`s every
  per-country store (`CountryState`, …) in lockstep. Sibling stores keep **no** free-list of their
  own. `destroy_country` also reassigns/clears `owner_country[loc]==id` and archives the country.
- **Dead-country archive:** `std::vector<DeadCountryRecord>` (append-only, cold, AoS) keyed by
  `{slot_id, generation}` — stub fields now (name, founded/died year), grow later. **Out of the
  per-tick checksum** (save/load only). Reset dead slots on destroy so the live hash is
  history-independent.

## Part C — aggregation system (location_economy → country totals)

**New `src/economy/aggregation.{h,cpp}`** — `aggregate_country(World&, CountryID)` builds the country
**accumulators** the state steps need: total population, total wealth, commoner/elite income, food,
**total elites & local elite opportunities**, derived **population pressure / elite overproduction /
aggregate instability**, control/security weighted averages. Fixed iteration order (ascending
LocationID). The reverse coupling — **country→location push of the suppression budget and state elite
opportunities** — is produced by the fiscal step and distributed per location before the per-location
unrest/elite steps. This replaces the placeholder `economy_system.cpp`; `location_economy` stays.

## Part D — stores

- **`location_economy` (extend, stays — the per-location truth):** `population` (`uint32`),
  `wealth`/`productivity` (`Fixed`), land, **`elites`**, **unrest `U`/`U_e`**, local security/control,
  carrying-cap window. `initialize(count)` (the M2 bug — never sized today), `feed_hash`. (Pops-by-civ,
  urban/consumables, capital = later increments.)
- **New `src/economy/country_state.{h,cpp}` (per-country, SoA by CountryID):** the purely-country
  state — `treasury`, `legitimacy`, `state_capacity`, `phase` (`uint8`), country-readout gauges
  (`P/E/U/S`, non-authoritative). Country accumulators (total elites, overproduction, instability) are
  derived by Part C each tick — store only what the readout/hash needs. `ensure_slot`/`reset_slot`
  (called by the create/destroy systems), `feed_hash`.
- **`CycleParams`** — FinalSim's ~40 `__init__` constants as `Fixed64` consts (one shared default;
  per-civ later). Params keep moving in Python, land here as value copies.

## Part E — the cycle system (port new_model.py core steps, split location/country)

**New `src/economy/secular_cycle_system.{h,cpp}`** — `SecularCycleSystem::step(World&, CountryID)`.
FinalSim's order `[pop, econ | fiscal, legit | unrest, elites | gauges, phase]`
([new_model.py:201-213](Planning/new_model.py)) splits cleanly by scope (**order preserved**):
1. **per location:** `_step_population`, `_step_economy` (wages/income via Part F).
2. **aggregate → per country:** `_step_fiscal`, `_step_legitimacy` → emit suppression budget + state
   opportunities + revenue; push the two couplings down to the locations.
3. **per location:** `_step_unrest` (minus local suppression share), `_step_elites` (local + state
   opportunities).
4. **aggregate accumulators → per country:** `_step_gauges`, `_step_phase`.
Omit `_step_crises`. Cite each sub-step's Python source; mark deviations. The interleaving + FinalSim's
existing one-tick lags (security, `felt_overproduction`, `U_e`) are the review-critical part. Called
from `TimeProgression::on_yearly` per country in **ascending CountryID order** (determinism).

## Part F — economy primitives (ports of economy.py + extra_functions.py)

**New `src/economy/economy_primitives.{h,cpp}`:** baked `const` `wealth_req/food_req/goods_req[51]`
tables (no runtime `pow`); `get_wealth_level`, `get_wage_share` (→ `fmath::logistic`, clamp
[0.1,0.9]), `get_output` (rational, `exp==1` so no `pow` yet), `get_job_capacity/get_job_pull`;
LandType constants; `tick_location_economy(...)` = `Location.tick()` guts (labor by job-pull,
production, income split — slaves earn 0, `elite_income = total − commoner`, food access).
**Trap:** Python's `1e-8/1e-9` div-guards round to **0** in Fixed64 — replace with a representable
floor (`from_raw(1)` or larger). No `np.clip` → manual min/max.

## Part G — wire hash, bridge, queries

- `get_state_hash`: feed `location_economy` + `CountryState` (raws, ascending id) + `CountryStore`
  (already partly there). Dead/free slots canonicalized.
- Queries: extend `build_location_summary` (pop/wealth) + new `build_country_summary` (phase,
  legitimacy, treasury, totals) for the Week-4 panel & sim-driven map modes. Bridge mirrors
  `get_location_summary` ([sim_world.cpp:467](src/godot/sim_world.cpp#L467)); `Fixed→double` only there.

## Critical files
New: `country_state.{h,cpp}`, `aggregation.{h,cpp}`, `economy_primitives.{h,cpp}`,
`secular_cycle_system.{h,cpp}`, `queries/country_summary.{h,cpp}`, `tests/secular_cycle_test.cpp`.
Modified: `countries/countries.{h,cpp}` (free-list/generation/archive + create/destroy systems),
`economy/location_economy.{h,cpp}` (initialize/feed_hash/extend), `core/world.{h,cpp}`,
`godot/sim_world.cpp` (init + hash + summary bindings), `time/time_progression.cpp`.
Globs already cover `src/economy`, `src/utility`, `src/queries`, `src/countries`.
Reuse: `fmath`, `Fixed64`, `Hasher`, `owner_country`, `geography.location_count()`.

## Verification
1. FixedMath — ✅ done.
2. Cycle arc (standalone `tests/secular_cycle_test.cpp`): one country with a few locations, ~2000-3000
   yearly steps → Turchin ordering (pop→elites→U_e→min legitimacy) and oscillation; match *shape*,
   not Python's numbers.
3. Not-stuck (`limitations.md`): pop must approach carrying capacity; suppression not so cheap the
   crisis never fires.
4. Determinism: two runs → identical `get_state_hash` sequence; cycle moves the hash.
5. In-game: load scenario, run at speed; country panel shows phase/legitimacy/population over years.

## Notes / deferred
- **Partial-map re-evaluation** (user): on intra-year territory change, recompute only affected
  countries/regions instead of looping all locations × countries. Deferred optimization — needs a
  dirty-region set; not now.
- Fast-follow: `_step_crises` + seeded deterministic RNG; then Week-3 **resilience + rebel movements +
  fragmentation** layered on the per-location unrest that already exists (the on-map collapse). Later:
  regions tier (~100), full economy (consumables/urban/capital/trade/migration), pops-by-civ.
