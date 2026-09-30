# Rota Fortunae — 28-Day Prototype Plan

> **Target:** a playable prototype to demo to a friend on **~2026-10-06** (28 days from
> 2026-09-08). This doc is the *delivery* plan (what to build, in what order, by when).
> [`DESIGN.md`](DESIGN.md) remains the *architecture*; this compresses its M2/M3 roadmap
> toward one deadline.
>
> **Status (~mid Week 2):** the Python model is **complete** — the plan's #1 risk (porting a
> moving target) is retired. **Behind on the calendar** (model tuning overran Week 1 into
> mid-Week 2), but the remaining Week-1+Week-2 work (finish M2 + port the model) is ~3 days and
> fully tractable — the hard/uncertain part is done, what's left is engineering you understand.
> The port is mechanical Python→C++ (see the two traps in the risk table). New critical path is
> **Week 3** (collapse + multi-civ + on-map visibility). Terrain + better AI stay stretch
> pull-ins, now less certain given the slip.

## North star (the one thing the demo must show)

**Empires visibly rise and fall.** Everything below is prioritized by whether it serves
that. A viewer should sit down, press play, and within a few minutes *watch* civilizations
grow, strain, hit a crisis, and collapse — on the map, not just in a debug log.

**This reframes scope:** "rise & fall" is only *visible* if states change on-screen when
they collapse. So a **minimal cohesion/collapse mechanic is promoted from stretch to core**
(Week 3). The internal secular cycle (pop/wealth/unrest) is the *engine*; a visible collapse
event is the *payoff*.

## Definition of done (the demo script)

1. Load a scenario: several civilizations/countries placed on the existing map.
2. Press play; time advances (speeds 1–5). Pacing is tuned so a full cycle plays out in
   **minutes of watching**, not simulated centuries of real time.
3. Over that time: populations grow, wealth accumulates, elites overproduce, unrest climbs,
   states hit fiscal/social crises and **visibly collapse** (fragment / lose cohesion /
   reset elites), then recover.
4. The viewer can *see* it: map modes for population / wealth / unrest / political, updating
   live, plus a country panel showing the selected state's trajectory (numbers, ideally a
   simple graph).
5. The player can select countries/provinces and issue **2–3 real commands** that visibly
   nudge the cycle. (Interaction exists; it is not the focus.)

Anything not required for that script is **cut to stretch** (see bottom).

---

## Reality check on scope

At a realistic **20–30 h/week** (~90–110 h total), this is *tight but achievable* **only
with disciplined scope**. The full front-facing list you wrote is more than 28 days solo
while learning C++. The single biggest lever is **Week 2 (the model port)** — if it goes
smoothly, Weeks 3–4 have room; if it slips, cut multi-civ breadth and AI, and demo fewer
states going through the cycle.

**Momentum rule (given you get distracted):** every week ends with something *runnable and
visibly better than last week*. The exciting payoff (the cycle running) is deliberately in
**Week 2**, not saved for the end — so the fun part pulls you forward instead of a wall of
plumbing before any reward.

---

## Week 1 (Sep 8–14) — Tame the model + finish Milestone 2

**Two tracks; the Python model is the bulk of the week.**

**Track A — tune the secular-cycle model (most of the week).** It already cycles but is
"frustrating to work with." Goal: get its *dynamics* stable and legible enough to **freeze
for porting** at week's end. Tune structure/ordering here; numeric parameters can keep
moving after the freeze (see Week 2). This — not M2 — is Week 1's real cost and its risk.

**Track B — finish Milestone 2 (≈1–3 days).** Small; mostly plumbing you already understand:
- Stores: population (`uint64`) + economy (`Fixed32`/`Fixed64`) SoA *(in progress)*.
- Tick-driver + day/year rollover (M2 step 2): calendar *reports*, driver *orders*.
- Speeds 1–5 as `tick_multiplier` presets (M2 step 3).
- Query layer + `build_location_summary` (M2 step 4) — first read path out of C++.
- Location/country UI panel reading that query (M2 step 5).

**Milestone at week end:** the model is stable enough to freeze, **and** the C++ engine turns
(play → placeholder pop/wealth ticking on a panel at adjustable speed).

## Week 2 (Sep 15–21) — The secular cycle lives  ⚠️ **the crux**

**Goal:** the real secular-cycle dynamics run per-country in C++, matching the Python
FinalSim's *behaviour* (ordering, not exact numbers — see [[rf-secular-cycle-model]]).

- **FREEZE the model's *structure* (update rules + ordering) and port that.** Numeric
  parameters may keep moving: because you port *ordering not numbers*
  ([[rf-secular-cycle-model]]), later Python tuning transfers to C++ as plain value copies.
  What you must NOT chase mid-port is changing *dynamics*. Snapshot `new_model.py` at the
  Week-1→2 boundary.
- Port to deterministic C++ systems (Fixed math, fixed iteration order):
  - Economy (wealth, subsistence — recall: no GDP, wealth-per-capita, below-subsistence is a
    food problem, [[rf-economy-no-gdp]]).
  - Population: slaves / commoners / elites split, **pops split by civilization**.
  - Unrest dynamics.
  - Country mechanics (state fiscal balance, elite dynamics).
- Cross-check: same initial conditions → same qualitative arc (growth → strain → crisis) as
  Python.
- **Scenario setup v1:** load an initial state (countries, starting pops/wealth) — hardcoded
  or a simple data file is fine. Enough real data to run one full cycle.

**Demoable at week end:** watch a *single* country run a complete cycle on the panel — the
"aha." Highest-value, riskiest week; budget generously.

## Week 3 (Sep 22–28) — Rise AND fall, visibly  + interaction

**Goal:** states visibly collapse/recover on the political map; many civs run at once;
minimal AI; player can poke it.

- **Minimal collapse/cohesion mechanic (promoted to core):** cross an unrest/cohesion
  threshold → a *visible* collapse event (fragment territory / anarchy / elite purge /
  reset). Binary is fine for the demo — it just has to show on the map.
- Scale the sim: run **all** scenario countries simultaneously (watch determinism/perf).
- **Basic AI:** shallow — let the model drive, plus a couple of automated reactions (e.g.
  raise taxes under fiscal stress). Not clever; the *cycle* is the dynamism.
- **Player interactions:** 2–3 real commands through the existing command spine that visibly
  affect the cycle.

**Demoable at week end:** a mapful of states each on their own cycle, some collapsing, some
rising — and you can nudge one.

## Week 4 (Sep 29–Oct 5) — Make it watchable  + buffer

**Goal:** it reads clearly; the map shows the cycle; UI shows trajectories; bugs squashed;
demo rehearsed.

- **Map-mode refactor** (§6.5 single-source enum) + **sim-driven map modes**: population,
  wealth, unrest, political (collapse-aware). Wire the **dirty-flag refresh** so the map
  updates live as the sim runs.
- Country/pop UI panel showing the selected state's cycle (numbers; a simple line graph if
  time permits).
- Functional UI pass (not pretty): time controls, selection, panels laid out.
- **Integration + bug-fix + determinism sanity + 1–2 day buffer.**
- **Rehearse the demo.** Critically: **tune tick pacing** so a cycle is watchable in demo
  time — a secular cycle spans "centuries," so pick a speed where rise→fall reads in minutes.

**Demoable at week end:** the full script above, end to end.

---

## If ahead of schedule — pull these in first

If the Week-4 goals are done by end of Week 3, the priority pulls (in order):
1. **Terrain (map data: terrain, rivers, elevation).** Deemed *incredibly important* — the map
   is the primary lens for the whole game, so this is the biggest believability upgrade.
2. **Better AI.** Makes states feel *governed*, not merely cycling — the next believability jump.

## Cut to stretch (NOT in the base prototype)

Pretty/laid-out UI · curved labels & JFA-SDF borders ([[rf-opengs-reference]]) · cultural
values / traits · deep country-mechanics breadth · cohesion modelling beyond the minimal
collapse event · multiplayer/netcode · the FixedDecimal cross-platform `__int128` fix. Each is
real and wanted — none is needed to show empires rising and falling. (Terrain & better AI are
promoted to the pull-in list above.)

## Risk register

| Risk | Mitigation |
|---|---|
| **Week-2 port slips** (the crux) | Freeze the Python target; port *ordering not numbers*; if late, cut Week-3 multi-civ/AI and demo fewer states. |
| **Model tuning drags** (it's "frustrating") | Hard-freeze the *structure* at end of Week 1 and port that; parameters may keep moving (they transfer as value copies, ordering-not-numbers). Don't let open-ended tuning eat the port. *(Retired — model complete.)* |
| **Python→C++ port isn't 1:1** (trap 1) | Python floats → `Fixed32`/`Fixed64` with **fixed iteration order**; it's a re-expression, not a literal translation. Watch Python true-division `/` vs integer `//`. |
| **Validating the port** (trap 2) | Check the cycle *qualitatively* (same rise→strain→collapse arc) — **not** by bit-matching Python numbers; Fixed rounding ≠ float ([[rf-secular-cycle-model]], ordering-not-numbers). |
| **Motivation dips / distraction** | Each week ends runnable; the payoff (cycle running) is Week 2, not the end. |
| **"Rise & fall" not visible** | Minimal collapse mechanic promoted to core (Week 3). |
| **Demo pacing** (cycle too slow to watch) | Dedicated pacing-tune task in Week 4; adjustable speeds already in Week 1. |
| **Determinism regressions at scale** | Keep the C++ checksum on; add cycle logic behind the existing per-store hashing. |
