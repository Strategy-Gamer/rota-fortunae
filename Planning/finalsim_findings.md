# FinalSim — investigation & findings

`FinalSim` (in `new_model.py`) is a from-scratch rebuild that models Turchin's structural-demographic
theory the way it was actually meant to work. It is a clear improvement over the earlier `SecularSim`
on the two things that gave us the most trouble: **cycle robustness** and **period length**. This
documents how it works, how it behaves, why it's robust, and what still needs balancing.

## The causal loop it implements (from the header comment)

Carrying capacity → population ↑ → **wages ↓** (population pressure ≈ inverse wages) → social mobility
into elites + mass mobilization from low wages → elite numbers ↑ → **elite overproduction** → state
income stress from paying elites → falling elite income + conspicuous consumption (higher bar to stay
elite) → elite fragmentation & elite mobilization → falling state legitimacy as it can't pay elites →
**state capacity ↓** → political stress → **instability** → elite & population mortality + erosion of
state → violence → population & elites ↓ → wages ↑ → elite incomes ↑ + downward mobility → violence
subsides → state recovers → overproduction ends → violence ends → population grows again.

This is the full Turchin loop, and — importantly — the model wires up **each link explicitly** rather
than approximating the whole thing with hand-tuned ODEs.

## Architecture: real stocks, derived gauges (the key design shift)

Unlike `SecularSim` (where P/E/U/S were themselves the integrated state), `FinalSim` integrates two
**real stocks** and treats the four gauges as **read-outs**:

- **State**: `self.population` (real, ~0.45–1.26) and `self.elites` (real, ~0.004–0.065). `S` is still
  integrated as a gauge; `U`/`U_e` are computed each tick.
- **Read-outs** (logistic maps of the stocks, so they're always in [0,1] and interpretable):
  - `P = σ(7.2·(population/carrying_cap − 1))` → 0.5 at carrying capacity, steep.
  - `E = σ(1.8·(elites/elite_positions − 2))` → 0.5 when elites are 2× the available positions.
  - `U_e = psi` (see below); `S` integrated from fiscal balance.

This avoids the double-integration ambiguity we fought in `SecularSim` and makes each gauge a clean
function of the underlying economy.

## Component analysis

**Population** (`get_birth_rate`, `get_death_rate`, step lines 143–147)
- Births: high (`max_birth=0.04`) well under capacity, smoothstep decline to `min_birth=0.015` as
  density rises from 0.75× to 1.5× capacity — **still significant births past carrying capacity** (good,
  realistic; allows overshoot). Low security shrinks the *effective* capacity for births.
- Deaths: `death_base + child_mortality·birth_rate + excess`, where excess = famine+disease+war, each
  scaled by `rel^1.2` (density) × severity × k. War severity = `U_e`. Disease worsens with famine.
- Net `population += (birth−death)·population`; `dP = birth − death`.

**Wages** (lines 151–152) — `w = 1 − P`, `w_inverse = w^−1`. Wages are the **central coupling**: they
fall as population pressure rises and drive *both* mass mobilization and elite social climbing. This is
the Turchin insight `SecularSim` lacked (there, immiseration was a separate pressure term).

**Elites** (lines 162–174)
- `elite_positions = (1+S)·population·0.01` (~1–2% of population, scaled by state capacity).
- `elite_wage` rises when elites < positions, falls when elites > positions (relative elite income
  falling under overproduction — the "smaller slice of pie" effect).
- `e_social_mobility = elites·0.02·(elite_wage − w)/w` — **Turchin's elite mobility equation**: when
  commoner wages `w` are low relative to elite income, commoners climb into the elite; down-mobility
  (negative) is halved (sticky elite status).
- `e_attrition = elites·0.05·U_e` — **linear** pruning by instability → elites persist into the
  depression and clear gradually (exactly the behaviour we spent many iterations chasing).

**Instability / PSI** (lines 185–194)
- `MMP = w_inverse` (mass mobilization from low wages).
- `EMP = E·(1+w)` (elite mobilization from overproduction).
- `psi = MMP · EMP · (1−S)` — the proper Turchin **product** form (needs both mass and elite
  mobilization, suppressed by the state), then logistic-smoothed `σ(8·(psi−0.5))` to cap at 1.

**State capacity** (lines 198–207) — `revenue = population·0.1`;
`expenses = 6·(elites − elite_positions) + U_e·(elites·8 + population·0.1)` — i.e. the cost of the
**elite surplus** plus **repression** (scaling with U_e). `dS = (revenue − expenses)/population`, with
the SimpleSim growth-shaping. This is the fiscal death-spiral: overproduced elites + repression
outrun revenue → S falls → more instability → less revenue.

**Phases** (lines 224–229): same thresholds as SimpleSim (0→1: E>0.3 or P>0.7; 1→2: U_e>0.5 or S<0.5
or both falling; 2→0: U_e<0.1 and P recovering and S>0.2).

## Observed behaviour (2000 steps, deterministic)

- **~9 cycles, period ≈ 203** — right at the ~200-year secular target we couldn't reach in SecularSim.
- **Phase ≈ 41% prosperity / 30% strain / 29% fracture** — well balanced and emergent.
- `P` 0.02–0.87, `E` 0.04–0.94, `U_e` 0.02–1.00, `S` 0.04–1.00 — **all gauges use their full range**;
  E and U_e return to ~0.02–0.04 in prosperity (genuinely calm) and peak near 1.0 in crisis.
- Real stocks: population 0.455–1.26 (overshoots capacity, then crashes), elites grow ~15× and clear.
- **E persists through fracture (mean ≈ 0.59, min 0.11)** and **S stays low in fracture
  (mean ≈ 0.23)** — the depression dynamics we wanted, achieved structurally rather than by tuning.

## Robustness (the standout result)

Varying one parameter at a time over 2000 steps, **the cycle persists** and only the distribution
shifts (the goal):
- `dS_mult` 1.0 and 2.0 both cycle (8–9). `k_war` 0.03–0.1 → 10/9/7 cycles. `death_base` 0.008–0.015 →
  11/9/5. `child_mortality` 0.3–0.5 → 11/9/7. `max_birth` sweeps also hold.
This is a genuine structural-demographic **relaxation oscillator**, so the limit cycle is intrinsic and
survives parameter changes — exactly what `SecularSim`'s bimodal-fiscal crisis failed to do.

## Issues & balancing opportunities

1. **Population crash is deep (~64% peak-to-trough).** Population overshoots to ~1.26× capacity then
   crashes to ~0.46. That's beyond the ~20–40% "normal / up to 50% Black-Death" range discussed
   earlier. Driver: sustained war mortality (`k_war·U_e·rel^1.2`) over a long fracture. Softening
   `k_war`, shortening the fracture, or capping per-step mortality would moderate it.
2. **Famine & disease are dead knobs.** `step()` passes `famine_severity=0.0, disease_severity=0.0`
   (only `war_severity=U_e` is live), so `k_famine`/`k_disease` currently do nothing. They're wired and
   ready — hook them to real food balance / a disease process to activate (disease already worsens with
   famine in the formula).
3. **`import matplotlib as plt` (line 3) is wrong** — should be `import matplotlib.pyplot as plt`.
   Harmless now (plot calls are commented out) but `plot_simulation`/`plot_phase_space` will crash.
4. **`w = 1 − P` and `w_inverse = w^−1` can blow up as P→1** (w→0). In practice max P≈0.867 → w_inverse≈7.5,
   fine, but a clamp/floor on `w` would harden it against extreme runs.
5. **`EMP = E·(1+w)`** raises elite mobilization at *high* wages, which is slightly backwards vs Turchin
   (EMP should rise as relative elite income falls). `E` already encodes overproduction, so it works,
   but consider `EMP = E · (elite_wage^−1)` or similar if refining.
6. **`randomness` is accepted but never applied** — the model is fully deterministic. Add per-variable
   noise (as SimpleSim did) if stochastic runs are wanted.
7. **`elite_positions` uses `population·0.01`**, i.e. tied to raw population not to jobs/districts; fine
   for this abstraction but note it when connecting to the game's district economy.

## How it compares to SecularSim / SimpleSim

FinalSim keeps SimpleSim's gauge interface and phase logic, but replaces the hand-tuned gauge ODEs with
an explicit structural-demographic economy (real stocks + wages + Turchin's elite-mobility and PSI
equations). Versus `SecularSim` it is **simpler, more faithful, more robust, and hits the target
period** — at the cost of a currently-too-deep population crash and a couple of unwired/rough edges
(famine/disease, EMP form, the import bug). It is the better foundation to build the game economy on.

---

# Economy build — Increment 1: agriculture & districts (DONE)

Goal (user): de-abstract the economy onto FinalSim one section at a time, preserving the *qualitative*
cycle (P↑ → E↑ → U↑ → P/S/E↓), not the exact numbers. First section = agriculture & districts, so
population has a place to work, **wages emerge from the labor market**, and GDP/wealth-per-capita are
measurable. Two deferrals (user's call): districts do **not** yet feed carrying capacity or elite income.

## What changed in `new_model.py`

- **`security` hoisted** to the top of `step()` (`max(0.5, S − U_e)`) — same value as before, now reused
  by both births and the district economy.
- **`carrying_cap = land_area · land_productivity`** (was just `land_area`) — `land_productivity` is the
  tech/district-tier hook; default 1.0 so nothing changes yet.
- **Wage is now live**, replacing `w = 1 − P`:
  - `rel_econ = population / (land_area·land_productivity)` (labor-to-land ratio).
  - `w = 1/(1 + exp(7.2·(rel_econ − 1)))` — the **normalized marginal product of labor**. This is
    algebraically identical to the old `1 − P` at default calibration (since `1 − σ(x) = 1/(1+eˣ)`), so
    the wage is now genuinely economic yet the cycle is preserved. It responds to `land_productivity`/
    `land_area` (tech, conquest) going forward. `w_inverse = w⁻¹` still feeds MMP unchanged.
  - Minor: `w` now uses the *current* population (removes a one-step lag vs the old pre-P-update read).
- **Output accounting — NO GDP** (corrected per user; see `economy_design.md` / [[rf-economy-no-gdp]]):
  `output_pc = base_yield/(1+rel_econ)` (per-worker output, food-equivalent units), which splits into two
  separate tracks:
  - **Food**: `food_ratio = output_pc/subsistence` (≥1 fed, <1 shortage); `famine_severity = max(0,
    1−food_ratio)` (tracked, feeds famine mortality in a later increment).
  - **Wealth** (surplus value, the tax base): `wealth_per_capita = max(0, output_pc − subsistence)` —
    **clamped ≥ 0, never negative.** Falling below subsistence is a *food* shortage, not negative wealth.
  With `base_yield=2, subsistence=1`, wealth/capita = 0 at carrying capacity and food shortage appears
  beyond it.
- **Districts vs subsistence split** (of the wealth pool): `active_district_share = district_share ·
  security`; `total_wealth = wealth_per_capita · population`; `district_wealth`/`subsistence_wealth`;
  `wage_bill = district_wealth·wage_share`; `elite_income = district_wealth − wage_bill` (district surplus
  to elites, **tracked, not fed back**); `commoner_wealth = subsistence_wealth + wage_bill`. All ≥ 0.
- New params: `land_productivity=1.0, subsistence=1.0, base_yield=2.0, district_share=0.5, wage_share=0.5`.
- New histories: `wealth_pc_history, food_ratio_history, wage_history, elite_income_history,
  commoner_wealth_history`.

`w` = the commoner SoL / real-wage *index* driving mobilization; `wage_share` = the institutional
labor share of district wealth. Two distinct economic objects (kept separate on purpose).

## Verification (2000 steps, dS_mult=2.0)

- Cycle preserved: **10 cycles, period ≈200, phase 42/29/30** (was 41/30/29). Gauges use full range.
- **Ordering correct within a cycle**: argmax P (921) → argmax E (942) → argmax U_e (951) → argmin S
  (974). Exactly the intended Turchin sequence.
- Economy sane: **wealth/capita 0.0–0.372** (never negative — pinned at 0 through the crowded
  stagnation/crisis, positive in prosperity); **food_ratio 0.885–1.372** (dips below 1 = food shortage,
  bottoming exactly at peak population); elite income & commoner wealth both ≥ 0.
- **Read-out knobs are inert on the cycle** (as designed): `district_share`, `wage_share` → cycle
  byte-identical; `base_yield` → only rescales wealth/food levels; `land_productivity` → raises/lowers K,
  population re-equilibrates, cycle persists (9–10 cyc) with wealth/food ranges invariant (correct
  Malthusian scaling).

## Next increments (planned, not started)

2. **Elite income explicit** — feed `elite_income` (district surplus) into elite maintenance/mobility &
   the state's fiscal base, and tie `elite_positions` to district/state slots instead of raw population.
   This is the second thing that can break the whole system — do it carefully, gate the ordering.
3. **Districts → carrying capacity** — let district land raise `land_productivity`/K (intensive vs
   subsistence agriculture), the growth engine that lets population overshoot.
4. **Food/famine** — `famine_severity` is now computed (from `food_ratio`); wire it into
   `get_death_rate` (currently still passed as `0.0`) and add **disease** separately. Gate the crash
   depth (may finally let the ~64% crash become food/disease-driven and more moderate).
5. State fiscal numbers (already ~works, "plug in different numbers"), then unrest wiring.

---

# Economy build — Increment 2: attach the district/pop economy (DONE)

Goal (user): integrate the `economy.py` object economy (Location/District/Pop) into FinalSim, but
**expose only the commoner/pop side** — the "new" elites (district owners) are kept SEPARATE from the
old `self.elites` scalar, so district → elite income just "disappears into the aether" for now. Purpose:
confirm the cycle still works with real districts attached, before rewiring the elite dynamics (next)
and then state capacity (after).

## What changed in `new_model.py`

- Imports `Location, Pop, District, FARM` from `economy.py`.
- `__init__` builds an attached economy: `self.location = Location(land_area=2.5)`, a commoner `Pop`
  (`self._commoner`), a placeholder elite owner `Pop` (`self._elite_owner`, the "new" elite — kept
  separate from `self.elites`), and one `FARM` district (size 1.0). `econ_land=2.5` is scaled so the
  district+subsistence food roughly feeds FinalSim's population range. Wage knobs: `w_steep=12`,
  `w_mid=1.0`.
- `step()` — the old inline wealth/food block is **replaced by a real economy tick**:
  `self._commoner.amount = population`; `self.location.security = security`; `econ = location.tick()`.
  Then read `food_access`, `wealth_per_capita`, `commoner_wealth` off the commoner pop, and
  `elite_income` off `econ` (**discarded — aether**).
- **Wage is now economy-derived**: `rel_econ = 1/food_access` (food need / food produced, = 1 at
  carrying capacity); `w = 1/(1+exp(w_steep·(rel_econ − w_mid)))`, floored at 1e-4. Drives MMP
  (`w_inverse`) and elite mobility exactly as before. The population block (births/deaths, `carrying_cap`)
  and the entire elite/PSI/fiscal chain are **unchanged** — only the *source* of `w` moved to the pops.

## Verification (2000 steps, dS_mult=2.0)

- Cycle persists with districts attached: **11 cycles, period ≈182, phase 47/20/33** (was 42/29/30 —
  strain shorter because the real wage swings less than the old abstract one; rebalanceable via
  `w_steep`/`w_mid`). Gauges use full range.
- **Ordering intact**: argmax P (827) → E (846) → U_e (855) → argmin S (880).
- Real economy signals: wage `w` 0.136–0.767, commoner wealth/capita 0.146–0.187, food_ratio
  0.867–1.11; elite income (aether) 0.006–0.012.
- No elite blow-up: `w` floored so `(elite_wage−w)/w` stays bounded; E peaks 0.94 (not pinned at 1).

# Economy build — Increment 3: Turchin relative wage as the driver (DONE, works well)

Switched the mobilization wage to Turchin's proper **relative wage**: `w = (commoner wealth / commoners)
/ (total wealth / total pop)` (avg commoner wage / GDP-per-capita analog), behind `wage_mode`
("relative" default, "food" fallback).

**First attempt flatlined** (0 cycles, parked in strain, `w`~0.95): commoners captured ~93–95% of all
wealth, so their per-capita wage ≈ GDP per capita → `w`≈1, never dropping to where elites grow. **User's
diagnosis (correct):** commoners *own too much land* (subsistence, fully commoner-owned, dilutes the
share) AND their *wage share falls too late* (`d_crit` too high). Note: the wage SHARE is a labor-market
function (workers vs jobs) and must stay **elite-independent** — it just needed retuning, not the elite
rewire I'd wrongly assumed.

**Fix (two levers, both the user's):**
1. **Districts fill the land** (`econ_land=1.0`, FARM size 1.0 → subsistence ≈ 0), so commoner capture ≈
   the labor wage share (no subsistence floor propping `w` up), and district jobs ≈ population so labor
   pressure actually bites (workers/jobs ≈ 1 at carrying capacity).
2. **`wage_d_crit=0.9`** — the wage share (and thus `w`) crosses 50% when workers reach 90% of carrying
   capacity, exactly as specified. Threaded a tunable `d_crit` through `economy.py`
   (`Location(wage_d_crit=…)` → `get_wage_share`).

Result: `w` swings **0.87→0.15**, crossing 0.5 at pop 0.9. Full cycle (3000 steps, relative mode):
**15 cycles, period ≈200, phase 45/25/30**, gauges full-range, ordering **P(864)→E(883)→U_e(893)→S
bottom(917)**. This is the best balance/period yet, and it's driven by the real Turchin relative wage.
(Food mode is now a poorly-scaled fallback — its `w_steep`/`w_mid` were tuned for the old land=2.5.)

Note the elite side is still the old `self.elites` scalar and `elite_income` still goes to the aether;
`w` works because it's ≈ the (elite-independent) wage share while the elite pop is a tiny fixed
placeholder. When elites are properly wired, `w` will pick up real inequality dynamics on top.

# Economy build — Increment 4: rewire elites onto the economy (DONE)

Removed the food-mode wage fallback (relative wage is the sole driver now). Rewired the elite dynamics
onto the attached economy:
- **Elite count is now the "new" elite pop** `self._elite_owner.amount`, grown/shrunk in place by the
  Turchin mobility eq `de = e·u0·(w0−w)/w` minus `e·k·U_e` attrition. `self.elites` mirrors it for the
  E readout (and next the fiscal side). Started aligned at 0.006.
- **Elite opportunities from REAL districts**: `elite_positions = Σ district.elite_opportunities()`
  (currently 0.02, one fixed FARM) instead of the abstract `(1+S)·pop·0.01`.
- **Elite income is no longer discarded**: the district surplus lands in `self._elite_owner.wealth` and
  enters the cycle through the relative wage's GDP-per-capita term (`We` in `gdp_pc`).
- `elite_wage` (= w0, the mobility zero point, *not* a literal wage — user's framing) kept on the
  overproduction structure (peaks near full positions, falls when overproduced). Tying w0 directly to
  elite income/capita was rejected: that quantity is volatile (0.9–19) and spikes when elites are few,
  which would wrongly grow elites in recovery.

Result (3000 steps): **18 cycles, period ≈167, phase 48/20/32**, ordering **P(740)→E(748)→U_e(754)→S
bottom(784)**. Crisis character is good: population crash ~47% (moderate), elites overproduce in strain
(0.02→0.052) and **clear gradually** through fracture (→0.02), E declines gradually (0.74→0.15, no
cliff), **U_e in a wave** (peak 0.62, subsides) rather than pinned at 1.0, S stays low (~0.44) and
recovers as elites clear.

**Change vs pre-rewire** (was 200 period, U_e→1.0, S→0.05): crises are **milder** because district
positions are now *fixed* and no longer collapse as S falls (the old `(1+S)·pop·0.01` shrank in crises,
spiking overproduction). This actually improves the pop-crash depth and the U_e-wave shape; restoring
severe state-collapse crises belongs to the state increment (positions shrinking as the state fails).

# Economy build — Increment 5: state capacity (treasury-buffered fiscal state) (DONE)

Replaced the abstract `dS` gauge rule with a real, survival-seeking fiscal state (debt ignored, per user):
- **Taxation**: `revenue = tax_rate · (commoner_income + elite_income) · collection`, `collection = 1−U_e`
  (unrest wrecks the tax base). Flat `tax_rate=0.25`.
- **Army**: `army_cost = army_base + army_unrest·U_e` (baseline upkeep + suppression surge).
- **Elite positions = district jobs + military officer corps + PATRONAGE**. The state, wanting to
  survive, opens patronage jobs to employ every excess elite it can afford (`excess·k_patronage`),
  funding the army first, then patronage, from `treasury + revenue`.
- **Treasury**: a stock, capped at `treasury_years·gross_revenue` (5 yr), floored at 0 (no debt). Builds
  in expansion, drains via patronage in stagnation.
- **State capacity** `S ← S + (S_target−S)·S_adjust`, `S_target = min(1, funds/desired)` where desired =
  army + full patronage want. So S=1 while the treasury buffers, and only falls once the treasury is
  drained AND revenue can't cover army+patronage → **never 0 unless collection (revenue) → 0**, exactly
  as specified.
- Two supporting fixes: the **E gauge uses BASELINE positions** (district+military, not patronage-funded)
  so raw overproduction stays visible during stagnation (proper strain phase) while patronage/`w0` use
  funded positions; and the dormant **instability_memory** is now active (`mmp ·= 1+mem_weight·mem`,
  ratchets with U_e, decays ~a generation) giving the crisis a long tail / father-son character.

**Mechanism (verified over a cycle):** expansion builds the treasury (→0.20) & S→1; strain = elites
overproduce visibly (E 0.2→0.63) while the state patronizes them and the **treasury drains** (0.20→0),
S still 1 (holding on); at treasury=0 **S collapses → U_e spikes to 1.0**; depression = S→0.04, elites
clear, pop crashes ~49%, **U_e holds ~15 ticks then subsides** (memory tail); recovery refills treasury
& clears elites. Result (3000 steps): **28 cycles, period ≈107, phase 61/17/22**, ordering
**P(511)→E(512)→U_e(514)→S bottom(528)** now spread over ~17 ticks (was 4). Severe preindustrial-style
collapses restored (S→0.03, U_e→1.0).

**Tuning levers (user: period/prosperity length are tuning, not structural):** expansion (hence the 61%
prosperity) is just the demographic recovery rate (birth/death); period via elite mobility rate + treasury
size; crisis depth via `army_unrest`, `k_patronage`, collection curve. `dS_mult/dS_nmult` are now unused.

## Next (open, per earlier notes)

- Tune period/prosperity balance if desired (birth rate, elite mobility, treasury size).
- Elites building districts (district-driven carrying capacity growth); food/famine mortality
  (`famine_severity` already computed); goods/materials; control vs security.
