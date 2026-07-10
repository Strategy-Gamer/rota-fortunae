# SecularSim design notes (survives context compression)

The working model lives in `secular_cycles.ipynb` (the `SecularSim` class cell). This file records
its architecture, every tunable parameter, the design decisions behind them, and the known gaps, so
work can continue after context is compressed. Companion: `ode_dynamics.md` (the SimpleSim reference
dynamics the economy is meant to reproduce).

## Architecture

- **Four normalized gauges** `P, E, U_e, S ∈ [0,1]` (population pressure, elite overproduction,
  instability, state capacity). Each has a **mode flag** `"ode"` (proven SimpleSim rule) or `"econ"`
  (derived from the economy). Built up one flag at a time (Stages 0-5 in the notebook); all four
  `"econ"` = the full economy-driven model.
- **Stage 0 invariant:** with all flags `"ode"`, `SecularSim` reproduces `SimpleSim`'s gauges exactly
  (memory decay is mode-gated: SimpleSim's 0.95/0.02 while U is "ode", tuned 0.97/0.01 when "econ").
- **Economy (new-model):** elites own **districts** (take their surplus); commoners own the
  **subsistence** land they work (plus district wages); the state **only taxes**; **security** gates
  usable land. Per-tick: `_run_economy` (labor/wealth/food → immiseration → elite opps → fiscal if
  mode_S econ → district investment → population → derive candidates) → gauge update by mode →
  instability memory → war-weariness → phase classify → record.

## Phases — classified by ECONOMIC MEANING, not tuned to a quota

(Per user: "Strain/Fracture/Prosperity should depend on what's going on the ground, not be changed to
fit a quota.") Thresholds mark real transitions:
- **Prosperity/Expansion**: commoners well-off (immiseration low), population growing.
- **Strain/Stagnation**: commoners struggling (`imm > ph_strain_on`), state still contains unrest.
- **Fracture/Crisis+Depression**: `U_e > ph_crisis_on`; lasts until violence AND elite overproduction
  genuinely resolve (`U_e < ph_recover_U and dP > 0 and E < ph_recover_E`).

## The crisis mechanism (this IS the intended dynamic — the "bimodal trap" is the point)

Strain: state suppresses unrest + pays overproduced elites → expenses climb. Elites **drain** state
capacity (`fiscal_target ×= 1 − k_elite_drain·E`). As S falls, unrest rises; **collection falls with
unrest** (`collection = (1−U_e)·(0.6+0.4·security)`) → revenue craters → deeper collapse (the spiral).
Fracture/Depression: violence is **elite-led** (needs surviving elites), damped into waves by
**war-weariness**; elites are pruned by **gradual linear attrition** (`k_elite_attrition·U_e`) so they
persist and clear only at the end — which is what lets S recover and a new expansion begin.
Population declines via **disease** (worse with unrest+immiseration) toward the security-reduced
carrying capacity (moderate ~20-40% drop), not a birth collapse.

### Design tension (documented, not resolved)
Collection is tied mainly to **unrest** (user's preference — avoids the circular
S→security→collection→S). A *pure* `(1−U_e)` collection makes the fiscal state **bimodal**: it latches
permanently solvent (stuck in strain) or permanently collapsed (stuck in fracture) depending on `tax`,
instead of oscillating. The minor `0.6+0.4·security` term restores enough continuous coupling to carry
the system across the tipping point in both directions. Revisit if a cleaner de-circularised trigger
is found (e.g. make the crisis trigger purely expense-driven with more responsive S).

## Tunable parameters (current calibrated defaults, in `SecularSim.__init__`)

Population: `birth_base=0.020` (SoL-driven, NOT ×(1+S) so births recover in depression);
`k_food_mort=0.03`; `k_disease=0.06` (deaths = 0.01 + k_disease·U_e·(0.5+0.5·imm) + famine);
`p_pressure_scale=1.1`. Births taper via a density check near carrying capacity (stagnation plateau).
Immiseration: `imm_k=10, imm_x0=0.85` (≈0 in expansion, rises near carrying capacity).
Elites: `elite_opp_slot=0.03` (positions ∝ districts), `k_promotion=1.2`, `k_elite_attrition=0.05`
(gradual), `k_demotion=0.03` (crisis), `k_aspirant_demotion=0.0` (OFF — it clears elites before they
overproduce and kills the crisis), `k_elite_drain=0.6`.
Security: `sec_base=0.6, sec_S=0.4, sec_U=0.15, sec_floor=0.6` → `security = clamp(sec_base + sec_S·S −
sec_U·U_e, floor, 1)`; the floor bounds the crisis carrying-capacity (hence population) drop.
Unrest: `w_pop_unrest=1.0, w_elite_unrest=1.3` (additive; higher weights let the crisis fire from
mobilization at moderate S → wider robustness window), each ×(1−`elite_unrest_S_supp`·S) with
`elite_unrest_S_supp=1.0` (lowering it to fire elite conflict directly destabilised — kept at 1.0);
`mem_decay=0.97`.
War-weariness (father-son waves): `k_weariness=0.7, weariness_up=0.06, weariness_down=0.03`.
Fiscal: `tax=0.15, admin_frac=0.03, repr_frac=0.12, fiscal_inertia=0.8`.
Phases: `ph_strain_on=0.16, ph_strain_off=0.10, ph_crisis_on=0.35, ph_recover_U=0.08, ph_recover_E=0.25`.

## Current behaviour (full model, seed-invariant)

Period ~127; phase ≈ 42% prosperity / 25% strain / 34% fracture (EMERGENT, not tuned); population
drop ~43%; **`E` and `U_e` genuinely return to ~0.002/0.001 in prosperity** (fixed the earlier floor —
failed-aspirant demotion was NOT the fix; higher unrest weights + the compromise collection were);
elites persist through fracture and clear at its end; `U_e` peaks ~0.4 (no 100% plateau).

## ROBUSTNESS (the key open question) — partial

Tested by varying one parameter at a time (see the notebook's robustness cell). The cycle is:
- **Robust** across the *elite* parameters the user most wanted to vary: `k_elite_attrition` (0.03–0.08),
  `elite_opp_slot` (0.025–0.04), `k_promotion` (≈0.8–1.5), and moderate `tax`/`birth`/`k_disease`.
  Distribution shifts (as it should); the cycle persists.
- **Fragile** at the extremes of the *fiscal* parameters — notably `tax` (≳0.17 → stuck in strain, the
  state stays solvent so no crisis; too low → permanent collapse) and large `elite_opp_slot` swings.
  This is the **bimodal fiscal trap** the user identified as "the point": a bimodal crisis mechanism
  gives an inherently *narrow* oscillation window. Making it robust across the full fiscal range is the
  main open problem — it likely needs the crisis trigger to come reliably from elite overproduction
  (which builds inexorably in stagnation) rather than from the fragile fiscal balance, WITHOUT
  destabilising the calm prosperity phase (attempts to decouple elite conflict from `S` broke that).
- **Density check vs overshoot:** the birth density-check gives a clean stagnation plateau but limits
  the population overshoot that, in SimpleSim, reliably drives immiseration high enough to fire the
  crisis — a likely contributor to the fragility. Weakening it (more overshoot) is worth trying.

## Known gaps / next steps
- **Father-son waves**: only a single damped hump, not 2-3 distinct spikes per crisis. True multi-wave
  needs intra-elite conflict partly decoupled from S without destabilising prosperity.
- **Period**: ~99; user wants ~200. Lengthening needs birth AND elite rates slowed *together* — slowing
  births alone tips it into a static "stuck in strain" state.
- **Collection circularity**: see design tension above.
- **Carrying capacity** is land/security-based; per design it should later mean something different for
  industrial societies (relative SoL vs recent generations).

## How to run / tune
- Notebook: set `STEPS` (harness cell) low (e.g. 400) for quick graphs; gate checks are soft
  (`check()` prints, never raises). The final analysis cell runs the full model + phase-distribution.
- Fast scratch loop: extract the class to a `.py` and use a `runfull(seed, steps, **kwargs)` helper
  (kwargs override any parameter as an instance attribute) — that's how the calibration above was done.
