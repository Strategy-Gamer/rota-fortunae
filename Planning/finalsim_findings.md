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
- **State capacity** — see Increment 5b below for the corrected (non-binary) formulation.
- Supporting fix that stayed: the **E gauge uses BASELINE positions** (district+military, not
  patronage-funded) so raw overproduction stays visible during stagnation (proper strain phase) while
  patronage/`w0` use funded positions.

# Increment 5b: state capacity — fixing the binary S (buffer + structural health)

User pushback: the first S was **binary** (a "severe downgrade") and the instability-memory tail was a
bandaid. **Removed the memory.** Root-caused the fast crash instead of patching:

**Root cause (instrumented):** `S_target = min(1, FUNDS/desired)` with `funds = treasury + revenue`
put the *whole treasury in the numerator*, so S read **exactly 1.0 for the entire ~10 years the treasury
drained** (0.195→0.045) — no signal — then the instant it emptied, a decade of accumulated deficit
released at once and S went 1.0→0.6→0.35→0.08→0.004 in four ticks (amplified by the
unrest→collection→revenue cascade).

**Fix (per Grok's decomposition, corrected):** take the treasury OUT of the structural numerator and
split S into two signals (`S = w_buffer·buffer + (1−w_buffer)·structural`):
- `fiscal_buffer = treasury / max_treasury` — reserves; drain gradually while funding the deficit → S
  falls with **early warning**, not a cliff.
- `structural_health = min(1, revenue / desired)`, `desired = army_cost + desired_patronage` — can *this
  year's revenue* (not reserves) cover ongoing commitments? Declines as elites overproduce (commitments
  outgrow the tax base = Turchin's structural fiscal crisis) and craters when unrest wrecks collection →
  carries S→0 iff revenue→0 (the spec). **Note:** an earlier attempt used army-only ("essential") for
  structural, but that made structural ≈ 1 always (army is cheap) so the buffer did all the work and
  *leaning structural broke the cycle* (stuck in permanent strain). Putting patronage back into
  structural fixed that.

Now S=1.0 only while the state is in genuine surplus, then declines smoothly over the treasury-drain
period once the deficit begins (e.g. 1.0→0.51 over ~9 ticks), then the acute collection-collapse
finishes it. Gradual onset, gradual recovery, **no memory needed**.

**Extensive robustness sweep (the user asked to find where it breaks, not patch):** the cycle is robust
across tax_rate (0.15–0.35), k_patronage (1–4), army_base/army_unrest, treasury_years (2–12),
S_adjust (0.1–0.5), mil_positions, and **all of `w_buffer` 0.1–0.9**. Levers: higher `w_buffer` →
**deeper collapses** (S→0.07–0.12) & longer period (fits "preindustrial states mostly collapse"); lower
(structural-leaning) → shallower crashes, longer prosperity, shorter period. Default `w_buffer=0.5`.
Remaining sharpness is only the acute `collection = 1−U_e` tax-collapse (2–4 ticks) — realistic and
matches the spec; softenable via the collection curve if wanted.

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

# Increment 5c: state absorbs PART of elites + proper EMP (DONE)

User: the state paying for ALL excess elites is silly — it takes on a *shrinking fraction* as
overproduction rises. And EMP was still the placeholder `E·(1+w)`; it should be `E · ew⁻¹`
(ew = relative elite income).

- **Partial absorption**: `absorb_fraction = 1/(1 + k_absorb·(overproduction−1))`,
  `desired_patronage = excess·absorb_fraction·k_patronage`. The state employs a declining share of the
  excess. **Confirmed the user's prediction:** overproduction 2.75×→**3.0×** (E_max 0.72→**0.85**), and
  state capacity lasts longer (period 86→**125**, strain 14%→**21%** — a real stagnation phase). Phase
  54/21/25, ordering P→E→U_e→S intact, severe collapses kept.
- **EMP fixed**: `elite_income_pc = We/Ne`, `ew = elite_income_pc/gdp_pc`, `emp = k_emp·E·ew⁻¹`
  (`k_emp≈17` to reach firing scale; `ew⁻¹` runs 0.009–0.166 since elites are 6–110× richer per capita
  than average). Better shape than `(1+w)`: `ew⁻¹` rises with overproduction, so EMP amplifies exactly
  when elites are overproduced (intra-elite competition), and the crisis is now driven by elite
  overproduction/EMP (frustrated unabsorbed elites) rather than pure fiscal exhaustion — proper Turchin.

**Robustness / where it breaks (extensive test):** `k_emp` robust 10–28. `k_absorb` works 0.3–~1.5;
**≥2 BREAKS** → stuck in permanent strain, overproduction 6.5×, E=1.0, but **U_e≈0.03 and S≈0.52**.
Mechanism: absorbing too little makes patronage cost plateau *below* revenue → the state never runs a
deficit → S stays 1.0 → and since `psi ∝ (1−S)`, S=1 is an ABSOLUTE suppressor → no crisis however
overproduced. **Structural implication:** in this model elite overproduction can only trigger a crisis
by bankrupting the state; a permanently-solvent state suppresses everything. To let overproduction
threaten a solvent state (coups/organizing regardless of treasury), the `(1−S)` term would need
revisiting (open design question, not patched). Default `k_absorb=1.0`.

# Increment 5d: suppression as expenditure + S=1 leak + violence/population shape (DONE)

Addressed the solvent-state question (5c) and violence/population feedback, per the user's design:
- **Suppression is now an EXPENDITURE keyed off mobilization POTENTIAL, not active unrest.** Computed
  `mobilization_potential = mmp·emp` *before* the fiscal block; `army_cost = army_base + k_suppress·
  potential`. A strong state must pay to hold down overproduced elites, so overproduction drains the
  treasury *before* it erupts → **fixes the forever-solvent break**: the old "stuck in strain, no crisis
  at high k_absorb" mode is gone (k_absorb 1–5 all cycle; strain grows 18%→26% with k_absorb).
- **S=1 no longer negates all unrest**: `psi = potential·(1 − max_suppression·S)` (`max_suppression=0.9`),
  so a strong state leaks 10% of potential.
- **EMP** = `k_emp·E·ew⁻¹` (from 5c) now feeds `potential` cleanly (moved above fiscal, computed once).
- **Violence is now a WAVE, not a plateau**: ramps ~8 ticks, peaks ~0.95 for ~6 ticks, decays ~12 ticks
  (was pinned at 1.0 for 30–40). The leak + earlier crisis firing did it; no instability-memory bandaid.
- **Population decline gentler**: `k_war` 0.05→**0.035** (~38% crash, ~3.4%/tick vs 4.2%); trough is held
  ~0.60 of carrying capacity by the security floor regardless of k_war ("suppressed at the bottom").

Default (k_absorb=2, k_suppress=0.006, max_suppression=0.9, k_war=0.035): **~32 cyc, period ~94, phase
54/20/26**, ordering P→E→U_e→S, E→0.53, U_e→0.98, S→0.15.

**Tradeoffs / where it breaks (tested):** (1) the suppression cost fires the crisis when potential hits
the affordability threshold, so it **caps overproduction ~2×** (vs 3× in 5c) — lower `k_suppress` allows
more but risks the next break. (2) New break mode: **extreme `k_absorb`≥6 at low `k_suppress` → chronic
low-level fracture** (stuck in phase 2, U_e simmering); raising `k_suppress` to 0.01 extends the safe
range to k_absorb 8+. So k_suppress trades overproduction-ceiling against robustness.

# Increment 5e: lengthen the fracture — unrest-gated state recovery (DONE)

User: the fracture is too short because S recovers too fast ("dies then gets resurrected quickly"). Key
insight: a state that is weak AND still contested can't consolidate — S should not rebuild until unrest
is low. Goals: (1) keep S low longer [priority], (2) prolong unrest tail, (3) higher E to start, (4) E
drops further before recovery.

- **Asymmetric, unrest-gated S recovery**: collapse is ungated; rebuilding is throttled by
  `rebuild_gate = max(0, 1 − U_e/rebuild_thresh)` (`rebuild_thresh=0.2`), so S can only climb once unrest
  falls below the threshold. Self-limiting: low S sustains unrest → elite attrition keeps pruning →
  potential drops → unrest eventually dips below threshold → S recovers → new expansion. No stuck state.
  Achieves #1 (S sits ~0.16 for ~24 ticks), #2 (U_e decays slowly over ~40 ticks), and #4 (E → ~0.10
  before recovery) together.
- **Higher E (#3)**: lowered `k_emp` 17→**10** — weaker EMP delays the crisis so elites accumulate more →
  E_peak 0.53→**0.68**, and strain lengthens (15%→23%). (User's own suggestion: "lower the multipliers
  into violence to delay it until more elites come around.")

Default (rebuild_thresh=0.2, k_emp=10, k_absorb=2, k_suppress=0.006, max_suppression=0.9, k_war=0.035):
**26 cyc, period ~115, phase 46/23/31**, E→0.68, U_e→0.97, S→0.16, avg depression (S<0.3) ~24 ticks.
Robust: gate × k_absorb(1–4) × rebuild_thresh(0.15–0.25) all cycle (18–25), fracture tunable 29–37%,
no permanent-stuck states. `rebuild_thresh` is the primary fracture-length knob (lower = longer).

# Increment 6: fracture rework — reliably-high U, full E clearing, smooth (non-sawtooth) S (DONE)

User sanctioned phase-aware behaviour ("video game, attitudes differ by phase"). Three fracture fixes;
father-son waves deferred to a follow-up (`instability_memory` reserved).

- **Fracture EMP floor** (keeps U high as E clears): `emp = k_emp·(emp_floor + E·ew⁻¹)` in fracture, with
  `emp_floor = emp_floor_fracture · floor_scale`, `floor_scale` full while E≥`emp_floor_hi` (0.1) then
  LERPs to 0 by `E_exit_thresh` (0.05). **Must fade with E** — a constant floor pins U_e=1 → collection→0
  → no revenue → permanent deadlock (found & fixed).
- **Fracture exit gated on E clearing**: `2→0` now requires `E < E_exit_thresh (0.05)` (+ U_e<0.1, dP>0).
  E's natural floor is ~0.027 (elites→0), so 0.05 is reachable.
- **No upward elite mobility in fracture**: `if phase==2: e_social_mobility = min(0, ...)`. Without this,
  once E≈0.05 and S recovered, elites *regrow* and the system settles in a limbo (E~0.063, U~0.158) that
  never satisfies the exit → stuck in fracture. This makes E clear **monotonically** → robust exit.
- **S = logistic readout of a slow `state_health` stock** (replaces the 5e unrest-gate; mirrors P off the
  population stock): `fiscal_signal = w_buffer·buffer + (1−w_buffer)·structural`; in fracture the target is
  capped by `frac_ceiling = fracture_floor + (1−fracture_floor)·max(0,1−E/E_clear)` (low while elites
  remain, rises as E clears); `state_health += (target−state_health)·health_adjust` (slow = smoothing);
  `S = σ(k_S·(state_health−x0_S))`. Deliberate phase suppression + slow stock → **S is a smooth wave**
  (rounded ramps between plateaus), not a sawtooth. `S_adjust`/`rebuild_thresh` removed.

Self-limiting loop: fracture → S held low (ceiling) + U high (floor) → attrition clears E monotonically →
E<0.05 → floor fades, U drops, ceiling lifts → S recovers smoothly → phase ends.

Default (emp_floor_fracture=0.5, emp_floor_hi=0.1, k_attrition=0.05, k_attrition_fracture=0.0,
E_exit_thresh=0.05, health_adjust=0.06, fracture_floor=0.1, E_clear=0.3, k_S=5, x0_S=0.4):
**18 cyc, period ~167, phase 44/26/29**, ordering P→E→U→S; **E clears to 0.050 at exit** (was ~0.17),
**U_e mean 0.73 in fracture**, **S max Δ/tick 0.040 (smoother than P's 0.070)**, S range 0.19–0.95.

**Robustness (full sweep, no stuck states):** emp_floor_fracture 0.2–1.0 all cycle (also a fracture-length
knob: lower→longer, 0.2→67% / 0.5→29% / 1.0→22%); k_absorb 1–4, health_adjust 0.03–0.1, E_clear 0.2–0.4,
k_emp 8–14, w_buffer 0.3–0.7, fracture_floor, k_S all OK. `k_attrition_fracture` left 0 (the floor alone
clears E; a boost just shortens the fracture). The old emp_floor=0.3 stuck-in-fracture is fixed by the
no-climb rule. Note: U_e is a flat plateau (~1.0) through fracture — the deferred father-son pass adds the
oscillation/waves.

# Increment 6b: shape the fracture U_e curve (de-plateau) + war-weariness + sharp S collapse (DONE, partial)

User feedback: U_e was a flat plateau at 1.0 through fracture; wanted it to *decline through ~0.3*
before zero; population crashed too hard (because U stuck at 1); S collapsed near the *end* of fracture
not the start; E<0.05 exit maybe too strict. Changes:
- **War-weariness stock** (`war_weariness`): accrues from U_e (above `wear_thresh`), fades (`wear_decay`),
  multiplicatively damps `mobilization_potential` (`1/(1+k_weariness·weariness)`). Repurposed the reserved
  memory slot.
- **Gentler unrest logistic with a high midpoint**: `psi = σ(psi_steepness·(potential−psi_midpoint))`,
  `psi_steepness=3`, `psi_midpoint=1.5`. Key: the high midpoint keeps *calm* potential (~0.03) → U_e≈0
  while weariness-damped *fracture* potential (~1–2) lands mid-range → U_e passes through ~0.3. (A gentle
  logistic with the old 0.5 midpoint raised the baseline and stalled the whole cycle in prosperity.)
- **Asymmetric state_health**: `health_adjust_down=0.5` (sharp collapse at crisis onset — user OK with a
  sharp DOWN), `health_adjust=0.06` (smooth recovery).
- **Attrition up** `k_attrition` 0.05→0.10 (each violence spike kills more elites, since unrest is now
  lower/shaped). **Exit relaxed** `E_exit_thresh` 0.05→0.08.

Default now: **21 cyc, period ~143, phase 36/30/34**, U calm 0.012, **U fracture mean 0.33** (was 1.0),
E exit 0.08, S(0.13,0.95) sharp collapse, ordering P→E→U→S. Robust (k_absorb 1–4, emp_floor 0.3–0.8,
k_weariness 3–8 all cycle, no stuck states). Population crash is gentler now that U_e isn't pinned at 1.

**KNOWN LIMITATION (RESOLVED in Increment 7 below):** this weariness model gave a **damped spike→decline**
(U_e spikes ~1.0 for ~4 ticks then declines through ~0.3), **NOT true father-son oscillation**
(spike/lull/resurge). The negative feedback stabilized to a fixed point, not a limit cycle. As predicted,
the fix needed a **second slow variable** — the moderate/immune pool of an SIR/SIRS radicalization model,
made **excitable** so it becomes a relaxation oscillator. See Increment 7.


## Increment 7 - TRUE father-son oscillation (Turchin's SIR/SIRS radicalization model) - SOLVED

The 6b weariness kludge gave a damped spike->decline, not real waves. **Solved** by replacing it with
Turchin's actual mechanism from *Ages of Discord*: political violence as an **epidemic (SIR/SIRS)** on a
3-compartment "political mood" of the mobilizable population - **NAIVE** (disengaged/susceptible),
**RADICAL** (drives violence), **MODERATE** (de-radicalised/immune). The user supplied the design; the
names are Turchin's naive/radical/moderate. Chosen scope (AskUserQuestion): **one abstract pool** (not
split by class) and **waves DRIVE the phases** (no phase-flag gating of the waves).

**Mechanism** (fractions N+R+M~=1, in `step()` where the old psi-logistic was):
- `conditions` = crisis pressure the state fails to suppress, squashed to [0,1], **driven by ELITE
  overproduction `emp`, NOT the mass term `mmp`**: `raw = emp*(1 - max_suppression*S)`, then
  `conditions = sigma(cond_steepness*(raw - cond_midpoint))`. Why emp not mmp: after the population
  crashes, wages recover so `mmp` collapses and would falsely end the crisis; `emp` stays high until
  elites are actually pruned. The `(1 - max_suppression*S)` gate keeps conditions ~0 while the state is
  strong (no bursts in prosperity/strain) and lets them rise as overproduction drains the state and S
  collapses - that collapse fires the FIRST burst and tips strain->fracture (waves drive phases).
- Flows: `N->R = (rad_alpha*R*activation + rad_seed)*N*conditions`; `R->M = rad_burnout*R +
  rad_suppress*R*M`; `M->N = rad_wane*M`. **EXCITABILITY is the key**:
  `activation = sigma(rad_k_ig*(R - rad_trig))` - contagion only self-amplifies once R crosses an
  ignition threshold, so bursts are **all-or-nothing** separated by refractory lulls (a relaxation
  oscillator / limit cycle), NOT a smooth endemic equilibrium (constant forcing -> damped fixed point).
- `U_e = R` (violence = the radical share). A 1.0 spike needs bad conditions AND a released burst.
- Defaults: rad_alpha=8, rad_seed=0.003, rad_burnout=0.15, rad_suppress=2.0, rad_wane=0.08, rad_k_ig=40,
  rad_trig=0.08, cond_steepness=8, cond_midpoint=0.15.

**Two supporting fixes (both essential - without them it stalls):**
1. **Elites clear ONLY by violence in fracture**: `e_social_mobility = 0` in phase 2 (was `min(0,...)`).
   This freezes the non-violent bleed so E (=> conditions) HOLDS between bursts - otherwise E leaks away,
   conditions decay, and re-ignition dies after the first burst.
2. **conditions must stay high until elites are truly cleared** so bursts keep firing to the end (the
   final clearing spike). Achieved with **low cond_midpoint=0.15** AND **low E_clear=0.12** (the fracture
   S-ceiling stays down => S stays suppressed => conditions stay high). Early tries failed because a big
   burst dropped E below E_clear, S recovered, conditions collapsed => a 335-tick dead tail. Also
   **k_attrition=0.15** (moderate) so a SEQUENCE of ~5 bursts steps E down instead of one giant burst;
   and **E_exit_thresh=0.15** because the E gauge floors ~0.14 at elites==baseline (sigma(1.8*(1-2))), so
   exit must trigger as elites reach the peacetime level (the crisis has consumed the surplus elites).

**Result - a clean, perfectly regular limit cycle:** default **10 cyc, period ~400, phase 15/9/76,
~5 father-son bursts per crisis.** One fracture reads: opening 3-tick spike (U 0.91->0.61->0.08, E
0.87->0.67, S 0.65->0.20) -> ~30-tick lull -> resurge (U->0.81) -> lull -> resurge -> ... -> **final
clearing spike** (U->0.78) drives E 0.12->0.085 below exit -> S recovers -> crisis ends. Each burst is a
3-4 tick spike; violence is RARE (long lulls) but each spike prunes a chunk of elites. Exactly the user's
"3-4 tick spike, drops heavily, resurges, ... one last spike that clears out the rest." Waves are
naturally **muted in prosperity** (conditions ~0 while S high) - no phase flag needed.

**Robustness:** 14-way param sweep (k_absorb 1-4, k_emp 6-16, rad_alpha 5-12, rad_suppress 1-4,
rad_wane 0.05-0.12, k_attrition 0.10-0.25, w_buffer 0.2-0.8) - **all cycle, zero stuck states** (8-20
cyc). Higher rad_alpha/rad_wane => more/faster bursts & shorter period; rad_suppress trades burst count
for length; k_attrition=0.10 => ~6 small bursts, 0.25 => ~2 big ones. Module-default 500-step run OK.

**Removed:** the whole 6b war-weariness stock (war_weariness / wear_build / wear_thresh / wear_decay /
k_weariness), the fracture EMP floor (emp_floor_fracture / emp_floor_hi), and psi_steepness / psi_midpoint
(replaced by cond_steepness / cond_midpoint).

**Open / tuning:** fracture is LONG (~76%) - a crisis+depression hosting 5 violence waves plus the
depopulation trough. If a less crisis-dominated civilisation is wanted, that is phase-balance TUNING
(prosperity is cut short by the `P>0.7` strain trigger; genuine "good times" length is set by how fast
elites overproduce and how fast population recovers). Mass immiseration currently only shapes the
suppression cost, not `conditions` - could be added back as an additive (not multiplicative) term if
popular radicalism should matter independently of elite overproduction.



## Next

- **Phase balance (optional tuning):** fracture is ~76%. If a less crisis-dominated civilisation is
  wanted, tune how fast elites overproduce (strain length) and how fast population recovers (prosperity
  length); the P>0.7 strain trigger also cuts prosperity short.
- **Urban & slave pops** (user's stated next major addition).
- Food/famine mortality (famine_severity already computed); goods/materials; control vs security.
  (Elites building districts deemed unnecessary for the secular-cycle focus - it just shifts the
  starting districts.)
- Optional: add mass immiseration back into conditions as an additive term (independent popular radicalism).


## Increment 8 - Endemic hum + wider spikes (unrest shape)

User feedback on Increment 7: the pure excitable `U_e = R` gave thin 2-3 tick spikes with dead lulls.
Wanted instead (a) a **sustained low-level hum ~0.2-0.3** through the whole crisis (real crises have
constant banditry/riots, and it should grind down population continuously - not only at spikes),
(b) **wider spikes (5-10 ticks)**, and (c) spikes that **shrink over time**.

**Design:** decouple the *observed* violence from the raw radical fraction. Keep the sharp excitable `R`
as the trigger, but read out `U_e` = an endemic hum + the spike, **asymmetric-smoothed**:
- `endemic = k_hum * conditions` -> a ~0.2-0.25 hum that scales with how bad things are and fades as the
  crisis resolves (0 in prosperity).
- `violence_target = endemic + (1-endemic)*R`; then `U_e += (target - U_e) * rate` with `rate = u_rise`
  when climbing (fast, keeps peaks high) and `u_decay` when falling (slow -> 5-10 tick spikes with a
  gradual tail). Widths tunable: u_decay 0.12->~11 ticks, 0.25->~6.
- Defaults: `k_hum=0.26, u_rise=0.6, u_decay=0.18`.

**Supporting change - elite culling is now SPIKE-driven, not hum-driven.** The endemic hum grinds down
POPULATION (war mortality stays linear in U_e) but must NOT cull elites on its own, or the continuous
attrition clears overproduction in a single wave (fracture collapsed to 23%, father-son waves gone). So
`e_attrition = elite * k_attrition * max(0, U_e - u_cull_floor)`: only above-hum flare-ups (revolts/civil
wars) prune the elite, so a SEQUENCE of spikes is still needed. `u_cull_floor=0.15` is set just BELOW the
hum so a SMALL continuous cull always creeps E down (prevents a frozen-E stall) while spikes do the bulk.
`k_attrition` 0.15->0.20 to compensate for the floor subtraction.

**Robustness fix - exit no longer gated on `U_e < 0.1`.** The hum legitimately holds U_e ~0.15 during the
crisis; the old exit `E<thresh AND U_e<0.1 AND dP>0` caused a **frozen limbo** (rad_suppress=4: spikes too
weak, E stuck at 0.115, hum settled at 0.148 just under the cull floor -> no culling, and U_e>0.1 -> never
exits). Changed to `E < E_exit_thresh AND dP > 0` - once elites are cleared and population recovers, exit;
the residual hum fades on its own in prosperity (conditions->0). This restored full robustness.

**Result:** default **19 cyc, period ~211, phase 46/19/35** (prosperity now the longest phase), **~2-3
wide father-son waves per crisis**. One fracture: opening flare (peak ~0.72, ~7 ticks >0.4, ~15-tick
bump) -> ~40-tick hum lull at U_e~0.15 (population grinding down all through it) -> final clearing spike
(peak ~0.64) drives E below exit -> crisis ends. Peaks shrink modestly (0.72->0.64). **Robust:** 21-way
sweep (k_absorb/k_emp/rad_alpha/rad_wane/rad_suppress 1-6/k_hum 0.20-0.35/u_decay/u_cull_floor 0.10-0.20/
k_attrition/w_buffer) - ALL cycle, ZERO stuck states. Module-default 500-step run OK.

**Still open (user's stated next items, not yet done):**
- Hum dips a bit low (~0.14) in deep lulls; raising k_hum risks stalls (0.26 is the robust sweet spot).
- Spike amplitude shrinks only modestly - stronger shrink needs `conditions` to decay across the fracture
  (trades against the dead-tail risk that low cond_midpoint was set to avoid).
- Wave count is ~2-3 (wider spikes fit fewer per fracture); rad_wane up (0.12) gives more, shorter waves.
- **Downward social mobility** was removed in fracture (Increment 7) - user flagged this as a reason
  fracture runs long; restoring a little would also harden the exit. NEXT.
- **State capacity** pinned low (~0.18) through fracture bothers the user - wants it to ebb/flow with
  violence rather than sit floored. Separate redesign.
- **Population pressure** should drop FIRST and stay low (and be part of why E falls), not track E down.


## Increment 9 - Downward mobility restored + exit hardened + carrying-cap-averaged pressure

User: "restore downward mobility while preserving the (glorious) cycle", plus two population notes.

**Downward mobility restored in fracture.** Increment 7 froze `e_social_mobility = 0` in phase 2 because
the non-violent bleed used to decay `conditions` and stall the waves. That guard is now redundant (the
endemic hum sustains unrest independently of E; the exit no longer depends on re-ignition). Changed back to
`e_social_mobility = min(0.0, e_social_mobility)` -- no one climbs into a collapsing elite, but overproduced
elites slide DOWN to commoner as wages recover (a non-violent drain alongside the violent culling). Cycle
preserved (baseline essentially unchanged: 20 cyc, 47/19/33).

**Exit hardened (general robustness win).** Restoring mobility re-exposed a frozen limbo at extreme
`rad_suppress` (spikes too weak to fully clear E): E settles just BELOW the exit threshold (~0.117 < 0.15)
but the endemic hum's war mortality pins the population's `dP` at ~0, so the old `dP > 0` exit never fires.
Fix: exit on `E < E_exit_thresh AND dP > -0.001` (elites cleared AND population has STOPPED FALLING, not
strictly growing). This also fixed the low-`k_war` stalls (same population-recovery-blocked-by-hum cause).
Now robust across `rad_suppress` 1-8 and `k_war` 0.02-0.05 (previously several stuck). 20-way sweep: ALL
cycle, ZERO stuck.

**2b - population pressure off a sticky carrying capacity.** `P` now reads `pop / max(current_cc, mean of
last cc_window=100 ticks)` instead of `pop / current_cc`. A RISING cap relieves pressure immediately; a
FALLING one doesn't instantly spike pressure (recent-capacity memory cushions it). Births/deaths still use
the real instantaneous cap -- only the pressure GAUGE is smoothed. INERT for now (cap is constant 1.0);
wired for when districts/tech make cap dynamic. Verified a clean no-op (baseline identical).

**2a - war mortality: investigated, LEFT AS-IS (user's call).** Finding: lowering `k_war` does NOT reduce
the death toll -- the population crash depth is ~45% regardless (0.020->46%, 0.035->45%), because the
population falls until births rebalance deaths at the crisis mortality level. `k_war` only changes the
SPEED (lower => longer fracture: 50% at 0.020 vs 33% at 0.035), not the depth. The real crash is already
realistic: **pop -45%, elites -93%** (elites clear ~2x harder, exactly the ratio the user wanted). The
"way too many dying" impression is a READOUT artifact: the steep `sigma(7.2*(pop/cc-1))` maps pop=0.6*cc
to P~0.05, so a 45% crash LOOKS like ~95%. User chose to leave mortality as-is (realistic) rather than
gentle the gauge or shorten the crisis. `k_war` stays 0.035.

Result unchanged from Increment 8 shape: **20 cyc, ~200 period, 47/19/33**, ~2-3 wide father-son waves,
pop crash ~45% / elites ~93%, fully robust. Module-default 500-step run OK.

**Still open:** State Capacity redesign (user's next, larger project) -- it is pinned ~0.18 through
fracture; user wants it to ebb & flow with the violence rather than sit floored. Then: gentler P readout
if the gauge's steepness becomes annoying; urban & slave pops; famine/disease mortality (would let the
Malthusian crash come from overshoot instead of war, if a shallower/among-driver crash is ever wanted).


## Increment 10 - State capacity as an independent slow variable driven by LEGITIMACY

User: S was pinned flat (~0.18) through fracture by the `frac_ceiling`; wanted it to be its own slow
variable that EBBS AND FLOWS with the violence. Chose (AskUserQuestion) to introduce **legitimacy** now as
the driver, and folded ELITE COHESION into legitimacy (legitimacy = Mandate of Heaven + elite cooperation).

**Legitimacy** (`self.legitimacy`, [0,1] slow stock, starts 1.0): heals toward 1 in calm, eroded by
active unrest ABOVE a floor (popular illegitimacy) and by elite overproduction above a tolerance (elites
withdrawing cooperation):
```
legit_erosion = legit_unrest*max(0, U_e - legit_unrest_floor) + legit_frag*max(0, E - legit_frag_ok)
legitimacy += legit_recover*(1 - legitimacy) - legit_erosion   ; clamp [0,1]
```
Event-adjustable later (plain public attr; `legitimacy_history` tracked). Defaults `legit_recover=0.03`,
`legit_unrest=0.35`, `legit_unrest_floor=0.09`, `legit_frag=0.05`, `legit_frag_ok=0.30`.

**S** is now an independent slow-drifting variable (replaced the `state_health` logistic read-out + the
`frac_ceiling` pin). It integrates toward a blend of legitimacy (dominant) and fiscal health, asymmetric
(fast collapse / slow heal), reusing the old `dS_mult`/`dS_nmult`:
```
S_target = w_leg*legitimacy + w_fisc*fiscal_signal          # w_leg=0.65, w_fisc=0.35
S += (S_target - S) * (health_adjust_down*dS_nmult if S_target<S else health_adjust*dS_mult)  ; clamp
```
Removed: `state_health`, `fracture_floor`, `E_clear`, `k_S`, `x0_S`. Kept: `health_adjust`(0.06),
`health_adjust_down`(0.5), `w_buffer`, fiscal/treasury block (still produces `fiscal_signal`). Because E
reaches S only through the SLOW legitimacy stock, S doesn't instantly mirror 1-E.

**Two calibration traps found & fixed (key learnings):**
1. **Crisis aborted / S stayed high (~0.6).** A weak crisis doesn't crater `fiscal_signal`, which then
   PROPS S up regardless of legitimacy; and slow legitimacy drift couldn't force S down in time, so E
   dipped below the exit threshold via downward mobility and the crisis exited before developing (E stuck
   in a 0.14-0.29 limbo). Fixed by making legitimacy erosion key off **unrest above a floor** so the
   VIOLENCE SPIKES crash legitimacy hard (the forcing the old pin used to provide).
2. **Ebb/flow vs multi-wave tension.** With the floor ABOVE the fracture hum (0.15), S recovered fully in
   the lulls -> suppressed re-ignition -> only 1 wave/cycle (lost the father-son structure). Fixed by
   dropping the floor BELOW the fracture hum (~0.13) but ABOVE the prosperity hum (~0.07): the sustained
   crisis hum keeps legitimacy (and S) in a LOW BAND through fracture -> waves keep firing -> while spikes
   dip it further (ebb/flow) and only prosperity calm lets it fully heal. This reconciled both.

**Result:** default **16 cyc, period ~250, phase 55/13/33, 2.0 father-son waves/cycle, ordering P->E->U
preserved (15/15 cycles).** S now EBBS AND FLOWS: one fracture reads pre-crisis S~0.75 -> wave-1 crashes it
to 0.13 (legitimacy 0.55->0.00) -> lull heals S back to ~0.60 (legitimacy ->0.50) -> wave-2 resurgence
dips S to ~0.40 -> final clearing -> recovery. Legitimacy visibly crashes to ~0 (Mandate lost) at each
wave and heals between; prosperity S~0.76, legitimacy~0.74. Sharp collapse at spikes (max dS/tick ~0.16,
user OK with sharp DOWN), smooth recovery. **Robust: 22-way sweep (legit params, weights, health_adjust,
rad_suppress 1-8, k_hum, k_war, k_absorb, k_emp) - ALL cycle, ZERO stuck states.** Module-default 500-step
run OK. "High legitimacy tanks unrest" comes for free via the existing S->conditions coupling.

**Still open:** legitimacy EVENTS (endgame hook exists); gentler P read-out if wanted; urban & slave pops;
famine/disease mortality.


## Increment 11 - Tuning father-son waves: closer spacing, gradual clearing, clean exit

User feedback after a break, three points: (1) father-son waves too far apart (~80 ticks; want 20-30),
(2) fracture "disappears too late" / a violence spike leaks into early prosperity -- prosperity shouldn't
start until unrest is below the trigger, (3) elites killed too fast in the FIRST wave (E -> ~15% in one
shot); want it gradual, helped by more-frequent + less-lethal waves.

**#1 Closer spacing:** `rad_wane` 0.08 -> 0.18 (faster immunity waning => shorter refractory). Sub-period
dropped ~69 -> ~24-26.

**#3 Gradual clearing:** `k_attrition` 0.20 -> 0.08 (each wave culls less). Now a SEQUENCE of ~3 waves
steps E down (e.g. 0.68 -> 0.41 -> 0.31 -> 0.18 -> 0.14 -> 0.10) instead of the first wave clearing
0.73->0.21. New param `frac_mobility_scale=0.25` throttles non-violent downward mobility in fracture so the
WAVES (not the quiet bleed) do the clearing (turned out to matter less than expected, but kept as a knob).

**#2 No leak into prosperity -- three coordinated pieces:**
- Exit now also requires `rad_R < rad_exit_thresh` (0.05): don't exit mid-wave, only in a lull. Gating on
  the RADICAL fraction (not U_e) avoids the endemic-hum frozen-limbo. (`rad_R_history` now tracked.)
- **Radical-pool RESET on the 2->0 transition**: `rad_N,rad_R,rad_M = 0.2,0,0.8` -- the crisis is over, the
  movement has burned out, populace exhausted (moderate). Restarts the refractory so no leftover wave
  fires in early prosperity. (An M-gate on the exit was tried first but its window never overlaps the
  dP>=0 window -- population is still crashing right after the final wave when M is high.)
- **Prosperity radicalization damp**: `conditions *= prosperity_damp (0.25)` in phase 0 -- "good times
  don't breed revolt." The KEY robustness fix: after exit S is still low (legitimacy crashed) so
  `conditions` stays high (~0.76) for a while; a hot oscillator (high rad_alpha/seed, low rad_suppress)
  would fire in prosperity and cull elites so E never rebuilds (stall). Damping phase-0 recruitment stops
  that. The crisis still initiates in STRAIN (E>0.3, undamped), so onset isn't blocked. BONUS: this also
  stopped chronic prosperity unrest from nibbling E, so E rebuilds faster -> **period dropped 400 -> ~198
  and phase balance went 78/7/15 -> 57/14/29** (much healthier).

**Result:** default **20 cyc, period ~198, phase 57/14/29, ~2-3 father-son waves/cyc ~24 ticks apart,
ordering P->E->U 19/19, ZERO leaks.** E clears gradually over the waves; S still ebbs/flows with the
violence (crashes ~0.08 at waves, recovers in lulls & fully in prosperity). **Robust: 26-way sweep
(rad_alpha/seed/suppress/wane, k_attrition, u_cull, frac_mobility, legit params, w_leg, prosperity_damp,
k_hum/war/emp/absorb, w_buffer) -- ALL cycle, ALL leak-free.** rad_suppress=1.0 now works too (was going
to be out-of-range) thanks to the prosperity damp. Module-default 500-step run OK.

**Note / open:** the FIRST wave is still the biggest (drops E ~0.27) since the initial explosion is worst;
that's realistic and much gentler than before, but `k_attrition` lower / more waves could soften further
if wanted. S ebbs less BETWEEN waves now (they're only ~24 apart, less recovery time) -- still dips per
wave and fully recovers in prosperity. Other open items unchanged: legitimacy events, gentler P readout,
urban & slave pops, famine/disease.


## Increment 12 - Phase rebalance: fracture-dominant, wider/spaced father-son waves (Plantagenet-calibrated)

User: prosperity too long vs fracture. From Turchin/Plantagenat England: a father-son cycle is ~50 years
(~15-20yr of clustered violence + ~20-30yr lull), and the disintegrative (fracture) phase holds 2-4 of
them. Chosen targets (AskUserQuestion): **phase split ~40/15/45**, and **wider waves (~15-20 tick violence
periods, ~45-50 apart, 3-4 per fracture)**. Also flagged: population recovers slowly, prolonging prosperity.

**Population recovery (shortens prosperity):** new `security_floor` param (0.5 -> 0.62) on
`security = max(security_floor, S - U_e)`. The crashed population (~0.6) was pinned BELOW its own
security-limited birth cap (0.5) -> minimum births -> ~50 ticks of stagnation before S recovered enough to
lift the cap. Raising the floor lets it recover immediately. (Prosperity is ultimately gated by pop growing
to carrying capacity ~1.1, ~100 ticks, so this trims the dead start; the big ratio shift is the longer
fracture.)

**Wider + spaced waves:** `rad_wane` 0.18 -> 0.09 (slower waning -> ~40-tick spacing, a ~50yr father-son
cycle); `u_decay` 0.18 -> 0.14 (slower U_e decay -> ~15-20 tick "heightened instability" periods).

**3-4 waves + longer fracture:** `u_cull_floor` 0.15 -> 0.22 (ABOVE the fracture hum, so the hum no longer
clears E -- only the wave flare-ups do -> the lulls hold E steady -> a SEQUENCE of ~3-4 waves is needed);
`k_attrition` 0.08 -> 0.05 (gentler per-wave cull). E now steps down over 3-4 waves
(0.69->0.52->0.43->0.30->0.25->0.19->0.17->0.14).

**Two robustness mechanisms this regime NEEDED (the wide-spacing + cull-above-hum regime is fragile):**
1. **Crisis wind-down forced bleed** (anti frozen-limbo): with the hum no longer clearing E, a weak-wave
   crisis (e.g. k_emp=6, rad_suppress=8, security_floor=0.72 -- where population barely crashes so downward
   mobility is too weak to bleed E) would freeze E above the exit threshold forever. Fix: track
   `fracture_age`; once it exceeds `frac_ramp_start=160`, a forced elite bleed ramps up
   (`elite_count -= elite_count*ramp*frac_forced_bleed(0.03)`), guaranteeing E clears and the phase exits.
   Normal ~110-tick fractures never reach 160, so it's inert for them.
2. **U_e exit gate** (clean prosperity start): the exit gated on `rad_R<0.05` (wave truly over) but the
   SLOW-decaying smoothed `U_e` still read ~0.35 into early prosperity -- the last wave's tail bleeding into
   the green zone ("fracture disappears too late"). Added `U_e < u_exit_thresh(0.20)` to the exit so
   prosperity starts only once violence has actually decayed to ~the hum. (Diagnostic lesson: the earlier
   "leaks" my detector flagged were this decay TAIL, not re-ignitions -- `rad_R` was 0.0 in prosperity.)

**Result:** default **~19-23 cyc, period ~261, phase 41/11/48, 3-4 father-son waves ~39 ticks apart with
~20-tick violence periods, gradual E clearing, clean prosperity start, ordering P->E->U 18/18, S still
ebbs/flows 0.06-0.67.** **Robust: 26-way sweep -- ALL cycle, ZERO real leaks (rad_R never re-ignites in
prosperity), ZERO tail-bleed.** Module-default run OK. New params: security_floor, fracture_age/
frac_ramp_start/frac_ramp_len/frac_forced_bleed, u_exit_thresh; `rad_R_history` already tracked.

**Note:** strain came out ~11% (target 15%) -- minor. The game will layer discrete events (coups, revolts)
on top of the elevated-violence periods. Open items unchanged: legitimacy events, gentler P readout, urban
& slave pops, famine/disease.

---

## Increment 13: honest E curve (0 = no overproduction) + binary trigger + severity-scaled amplitude

Four user complaints, all rooted in read-out curves rather than dynamics:
1. **"E sticks too high."** The E gauge was a sigmoid `sigma(1.8*(rel-2))` on `rel = elites/positions`. At
   `rel=1` (elites exactly fill positions -- ZERO overproduction) it read **0.14**, and it never reached 0.
   So "E ~= 0.18 residual" wasn't "some overproduction", it was the FLOOR = "elites ~= positions". The user's
   key realization: **if there's no elite overproduction, E should be 0.**
2. **Trigger not binary.** At strain onset instability jumped up then CLIMBED gradually (the endemic hum
   `k_hum*conditions` rising as `conditions` rose through strain) before the first burst. Wanted: unrest
   stays low/ACCUMULATES through strain, then SPIKES at the strain->fracture onset.
3. **"Spikes always jump to the same point and the period changes."** Wanted the opposite: **period
   consistent, AMPLITUDE scales with the scale of the problem** (bigger overproduction -> bigger flare-ups).

**Fix 1 -- honest E curve (linear in the surplus, zeroed below rel=1).** Replaced the sigmoid with
`E = clamp((rel-1)/E_span, 0, 1)` (`E_span=2.0`). Now `E=0` exactly when `elites<=positions`, and E reads
the real overproduction FRACTION, ~linearly (severity reads proportionally). E is not a pure read-out -- it
feeds `emp = k_emp*E*ew_inverse` -- so this changed the loop: with E->0 in prosperity, `emp->0` -> zero
unrest pressure in good times (more correct). `E_exit_thresh` 0.15 -> **0.12** (was a hack pinned just above
the 0.14 sigmoid floor; now a genuine "surplus mostly consumed" threshold -- E keeps falling to 0 through
early prosperity as elites slide down anyway). Peak overproduction now `rel~3.2` (E clips at 1.0 only ~0.1%
of ticks -- a brief peak touch, no plateau). **Trap:** exiting at near-zero E (0.04-0.08) balloons the
fracture to 70-80% -- clearing the last sliver is slow (weak tail waves); keep exit ~0.12 above that cliff.

**Fix 2 -- binary trigger (quiet strain, onset spike).** The endemic hum is now SUPPRESSED outside fracture
(`hum_gate = 1.0 if phase==2 else hum_quiet(0.15)`). Strain stays visibly calm (U_e ~0.02-0.05) while
unrest ACCUMULATES (R charging toward ignition), then the first burst SPIKES at the onset (~0.02 -> ~0.68 in
one tick) and marks the transition. The ignition spike itself is NOT gated (only the hum), so the
transition-marking burst still fires at full size. (Before: hum climbed 0.05->0.17 through strain.)

**Fix 3 -- severity-scaled amplitude (E-driven), consistent period.** Violence now scales with
`severity = E**sev_exp` (`sev_exp=0.55`, concave): `violence_target = hum + (severity-hum)*spike_pulse`,
hum `= severity*k_hum*hum_gate`. So amplitude tracks the SCALE OF THE PROBLEM -- the worst overproduction
gives the biggest flare-ups, and waves TAPER as the crisis clears (per-fracture peaks now
**0.78 -> 0.54 -> 0.42** with rock-steady ~32-34 gaps, vs the old flat 0.69-0.72). Between regimes too:
milder crises give smaller spikes (k_emp 8/10/14 -> mean top-spike 0.82/0.78/0.77). Why `conditions` can't
carry magnitude: it saturates to ~1 in every crisis by design (low `cond_midpoint`) so waves keep firing
until E clears -- it gates WHEN, not HOW BIG. E is the clean unsaturated [0,1] magnitude now.

**The trap Fix 3 sprang, and the decoupling that fixed it.** Tying the DISPLAY violence to `E**exp` also
tied the CULLING to it (`cull ~ U_e - floor`), so late (small) waves culled too little -> E decayed in a
long slow tail -> fracture doubled to ~210 ticks (73%) and showed a KNIFE-EDGE in `u_cull_floor` (0.12->60%,
0.15->36%, 0.18->68%) -- a robustness red flag. Fix: **decouple the cull from the display.** Elite culling
is driven by a low-passed envelope of the (severity-INDEPENDENT) burst pulse -- `cull_env` (fast-rise/
slow-decay of `spike_component`), `cull_wave = max(0, cull_env - cull_env_floor(0.10))`. The envelope is a
wide "a wave is happening" window that does NOT taper, so each father-son wave clears a ~consistent fraction
of the surplus cohort -> E decays ~geometrically over a BOUNDED number of waves (bounded fracture), while
the displayed U_e still tapers for the graph / war-mortality / legitimacy. `k_attrition` 0.05 -> **0.06**.
(Intermediate dead-ends: culling off raw `R` gave too little integrated cull -- R is a sharp 2-tick spike,
not a wide low-passed window; boosting `k_attrition` under E-driven cull BACKFIRED -- a steep early E-crash
makes later waves tiny and uncullable, lengthening the tail.)

**New params:** `E_span=2.0`, `spike_gain=1.4`, `hum_quiet=0.15`, `sev_exp=0.55`, `cull_env=0.0`,
`cull_env_floor=0.10`; `self.overprod_ratio` exposed for introspection. **Removed:** `u_cull_floor` (the
old cull driver). Changed: `E_exit_thresh` 0.15->0.12, `k_attrition` 0.05->0.06, E read-out formula.

**Result:** default **~19-20 cyc, phase 42/13/44, 3 father-son waves ~32-34 apart, amplitude tapering
0.78->0.54->0.42, E now spans 0.000-1.000 (0 = no overproduction), quiet strain -> onset spike, ordering
P->E->U 19/19, S ebbs/flows 0.09-0.67.** **Robust: 46-way one-at-a-time sweep -- ALL cycle, ZERO stuck
(longest single-phase run <=233 ticks), ZERO prosperity leaks; plus 6 combined-stress configs all cycle.**
Module-default run OK. `sev_exp` and `E_exit` are the balance knobs (both robust across their range: even
`sev_exp=1.0` cycles, just fracture-heavy at 81%). Open items unchanged: legitimacy events, gentler P
readout, urban & slave pops, famine/disease.

---

## Increment 14: unrest shape (calm prosperity, smooth strain climb) + gentler/shallower population crash

Follow-up to Inc 13. The binary trigger from Inc 13 OVERSHOT -- the user does want unrest DURING strain
(it should climb), just not (a) any unrest in prosperity, nor (b) the little jump/plateau at the strain
onset. Plus a population complaint: the crash is too deep (~36%) and too PRECIPITOUS -- it should be
shallower (~1/3) and ROUNDED (like Turchin's pressure metric falling ~linearly 1610->1750, and Tudor
population stagnating 1650-1750), not a cliff.

**Fix A -- unrest shape (smooth phase-ramped hum gate).** Replaced the binary `hum_gate = 1 if fracture
else hum_quiet(0.15)` with a SMOOTH, boundary-continuous gate: **prosperity -> 0** (no unrest in good
times); **strain -> smoothstep((E - strain_E_thresh) / strain_hum_span)** ramping 0->1 as overproduction
builds (so unrest starts at ~0 at the strain onset -- continuous with prosperity, NO bump -- and CLIMBS
through strain); **fracture -> 1**. Verified: mid-prosperity U mean 0.0014 (~0), strain U climbs smoothly
0.001->0.008->0.036->0.092->0.162->0.315 with no onset jump. (The only prosperity U is the exit-threshold
DECAY TAIL in the first ~15 ticks -- the last wave fading as order returns, `u_exit_thresh=0.20`; kept
because it can't go below the ~0.13 late-fracture hum without risking the frozen-limbo trap.) New params
`strain_E_thresh=0.30` (also now drives the strain trigger, replacing the hard-coded 0.3), `strain_hum_span
=0.50`. REMOVED `hum_quiet`.

**Fix B -- population crash (gentler gauge + gentler war deaths).** Two parts:
1. **P read-out was the real culprit for the "precipitous" LOOK.** The actual per-cycle population drop was
   only ~36%, but the gauge `P = sigma(7.2*(pop/cap - 1))` amplified it into an ~83% GAUGE cliff (0.73->0.13).
   Dropped the steepness to `p_steepness=3.0` -> the gauge is ~linear over the operating band and swings in a
   Turchin-like **0.36-0.62** mid-range (his metric ~0.35-0.57), tracking the real population instead of
   craters. (The P gauge only feeds the `P>0.7` strain backup [now never fires -- E>0.3 leads] + is a
   read-out, so reshaping is safe.)
2. **War deaths lowered** `k_war` 0.035 -> **0.015**: direct war mortality was a big FRONT-LOADED spike at
   the first (biggest) father-son wave. Gentler war deaths -> shallower (**~30%**) and more rounded decline,
   driven more by suppressed births. **Why still convex (user diagnosed it, accepted):** the population
   asymptotes to a birth/death equilibrium under the security floor (a fixed-K crash is inherently convex),
   AND the RAPID SECURITY drop at onset (`security=max(floor, S-U_e)` craters 0.84->0.62 in ~8 ticks as S
   collapses and U_e spikes) crashes births to the floor (0.023->0.015) right at the onset. A truly LINEAR
   decline needs K itself to rise (pop stagnant, pressure falls) -- deferred until districts/tech make
   carrying_cap dynamic. User is fine with "somewhat convex"; the security->births cliff is the lever if a
   gentler slope is wanted later.

**Also exposed** `self.conditions / security / birth_rate / death_rate` as instance attributes (game
read-outs + introspection). **New params:** `strain_E_thresh`, `strain_hum_span`, `p_steepness`. Changed:
`k_war` 0.035->0.015, strain trigger uses `strain_E_thresh`. **Result:** default **~20-21 cyc, phase
44/11/44, population drop ~30% (peak ~1.15 -> trough ~0.80), P gauge 0.36-0.62, unrest 0 in prosperity &
climbing smoothly through strain, ordering P->E->U 20/20.** **Robust: 42-way one-at-a-time sweep -- ALL
cycle, ZERO stuck (longest <=230t), ZERO leaks, mid-prosperity unrest ~0 everywhere.** Module-default OK.
Open items unchanged: legitimacy events, DYNAMIC carrying capacity (would give the true linear pop decline
+ Tudor-style stagnation), urban & slave pops, famine/disease.

---

## Increment 15: father-son waves must ALWAYS fire (never cut off) -- unconditional seed + M-gate exit

User: the LAST father-son wave sometimes never fires (blatant in the first/transient cycle -- a ~58-tick
dead tail with no spike), and the fracture "should have one more spike". Clarified: **the period doesn't
have to be constant, but a wave should NEVER *not* fire after enough time** (I had implemented the
amplitude-by-conditions part of Inc 13 but NOT a firing guarantee). Diagnosis: (1) the recharge `seed` was
gated by `conditions`, so as E cleared and `conditions` fell the oscillator recharged ever slower and could
STALL; (2) the exit could fire during a pre-ignition RECHARGE lull (R still <rad_exit, U_e in a lull, but
immunity M already waned -> a wave IMMINENT) -> it cut that wave off. Mature cycles happened to exit right
after a wave (M=0.55-0.67), but the first cycle exited mid-recharge (M=0.07).

**Fix 1 -- unconditional seed (firing guarantee).** `to_radical = (rad_alpha*R*activation + rad_seed)*N*
conditions` -> `(rad_alpha*R*activation*conditions + rad_seed)*N`. The generational pressure valve now
ALWAYS recharges (seed not gated by conditions), so once immunity wanes a wave WILL fire "after enough
time". CONTAGION is still `*conditions`, so the burst's SIZE still tracks conditions (amplitude by
conditions, firing guaranteed). In prosperity `conditions`~0 so contagion can't amplify and the tiny
seed-fed R plateaus at ~seed/burnout=0.02 << ignition -> no wave in good times (prosperity R "leak" rose
0.001->~0.015, still invisible: hum_gate=0 in prosperity so it shows ~0 violence).

**Fix 2 -- M-gate exit (no cut-off).** Exit now also requires `rad_M > m_exit_thresh(0.40)` -- i.e. we are
in the lull JUST AFTER a wave burned out (moderates high), NOT mid-recharge with a wave pending. Guarantees
a building wave fires before the crisis resolves. Verified: first cycle now fires its 4th wave (amps 0.71,
0.65, 0.44, **0.19** -- the small final aftershock, amplitude tracking the low late-crisis conditions),
exits at M=0.84; mature cycles keep 3 waves, exit at M=0.53-0.66 (also post-wave, never cut off).

**The trap Fix 2 sprang + the bypass.** A bare M-gate DEFEATED THE FORCED-BLEED SAFETY NET: in weak/dead-
oscillator regimes (rad_suppress>=4, rad_alpha=4, rad_wane=0.06, ...) M never reaches 0.40, so the exit
locked the crisis open FOREVER (sweep showed ~15 STUCK configs, longest ~3900) -- the forced-bleed cleared
E but couldn't exit past the M-gate. Fix: **safety bypass** -- `(rad_M > m_exit_thresh OR fracture_age >
frac_ramp_start(160))`. Normal fractures (incl. the first cycle, ~137t) stay under 160 and use the M-gate
(no cut-off); a pathologically long fracture exits regardless of M so a dead oscillator can't lock it.

**New param:** `m_exit_thresh=0.40`. Changed: seed term unconditional, exit gains the M-gate+bypass.
**Result:** default **~21 cyc, phase 42/8/50, FIRST cycle 4 waves + mature 3 (last wave ALWAYS fires, never
cut off), pop drop ~28%, P band 0.37-0.61, ordering P->E->U 20/20, prosperity clean.** **Robust: 37-way
sweep -- ALL cycle, ZERO stuck (longest <=239t), ZERO leaks.** Module-default OK. (Note: strain thinned to
~8% as the unconditional seed lets the first burst ignite a touch earlier; fracture 50% / prosperity 42%
dominate, fine. If MATURE fractures should also carry a 4th wave, lower `k_attrition` -> ~4.3 waves/frac at
0.03, still robust, but a longer fracture.) Open items unchanged: legitimacy events, DYNAMIC carrying
capacity, urban & slave pops, famine/disease.

---

## Increment 16: DYNAMICS over CATCHES -- de-restrict the fracture exit, fix E-reset, rebalance ~40/20/40

User edited `security = max(floor, floor + (S - U_e))` (was `max(floor, S - U_e)`) -> births much higher in
good times -> population OVERSHOOTS far more (peak ~1.6-1.9 vs ~1.15) -> bigger, MORE VARIED crashes (user
likes this -- less perfectly-repeating, more realistic). But it exposed real problems + a philosophy note:
**the user wants DYNAMICS to dominate, not a stack of exit CATCHES** ("things enabling/disabling per phase
is fine, but these catches irk me"). Diagnosed live: (1) **E plateaued at ~0.133 just above E_exit_thresh
(0.12) and stuck** -- late waves were weak (contagion gated by `conditions`, which collapses as E falls) so
they stopped culling; the fracture only escaped via the age>160 bypass -> 80% fracture. (2) The 5-condition
exit (E<thr AND dP>-.001 AND R<.05 AND U_e<.20 AND (M>.4 OR age>160)) TRAPPED valid exits -- clear "crisis
over" states (dP>0, R low, U low) were blocked by the E-plateau + M-gate. (3) Strain far too short (buildup
popped it early). (4) "Not as robust as claimed" -- the M-gate stall regimes.

**Redesign (dynamics-first):**
- **Phase-dependent oscillator drive `crisis_drive`:** FRACTURE=1.0 (waves ignite at FULL strength on their
  own generational clock REGARDLESS of conditions -> every wave culls decisively -> **E always resets to ~0**,
  no plateau); STRAIN=`conditions` (the FIRST burst's timing is conditions-driven -> a real, longer buildup,
  per the user: "'always every 50yr' is only true in fracture; strain buildup should play into conditions");
  PROSPERITY=`conditions*prosperity_damp` (no waves). Replaced the unconditional-seed of Inc 15.
- **Exit is now PURE DYNAMICS, two conditions:** `E < E_exit_thresh AND U_e < u_exit_thresh` (overproduction
  cleared + violence in a lull). Dropped the dP / rad_R / M-gate / age-bypass catches AND removed the
  age-timed forced-bleed entirely. Removed dead params (rad_exit_thresh, m_exit_thresh, frac_ramp_*,
  frac_forced_bleed).
- **DOWNWARD MOBILITY clears E (the user's suggestion), two parts:** `frac_mobility_scale` 0.25->1.0 (full
  wage-driven downward slide as pop crashes) PLUS a new `frac_elite_drain=0.012` -- a steady surplus-
  proportional drain (`(elites-positions)*drain`) representing overproduced elites that simply CAN'T be
  sustained through a crisis (lost offices/estates), independent of violence/wages. This is the DYNAMIC (not
  a catch) that guarantees E clears even when the oscillator is dead (high rad_suppress, where violence is
  too weak to crash the population and trigger wage-driven mobility) -> fixes the last stuck regimes.
- **Rebalance to ~40/20/40:** `elite_mobility` (new param, was hard-coded 0.02) -> **0.009** (slower elite
  REBUILD -> longer prosperity/strain expansion); `k_attrition` 0.06->0.09; `strain_E_thresh` 0.30->0.40;
  new `strain_P_thresh=0.85` (was a hard-coded `P>0.7` firing strain early and capping prosperity -- raising
  it lets prosperity run near peak pressure).

**Result:** default **~41 cyc, split 40/18/42, period ~97, E resets to 0.000 every cycle, population varies
0.61-1.94 cycle-to-cycle (NOT perfectly repeating), strain shows a smooth unrest buildup 0->~0.23, big first
burst ~0.7 then a 2nd wave, pop crash ~25%.** **Robust: 20-way sweep incl. the user's security change -- ALL
cycle, ZERO stuck (rad_suppress 6/8 go fracture-heavy ~80% but still cycle), E resets everywhere, ZERO
leaks.** Far fewer catches (exit went 5 conditions -> 2; forced-bleed gone). Module-default OK. NOTE: at
period ~97 a fracture holds ~2 waves (~33t spacing); 3-4 waves/fracture needs a LONGER period (~200-250,
Plantagenet) -> slow the whole clock (mobility/growth) if wanted. Open items unchanged: legitimacy events,
DYNAMIC carrying capacity, urban & slave pops, famine/disease.
