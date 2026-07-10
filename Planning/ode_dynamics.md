# SimpleSim (ODE) reference dynamics — and how the economy model should match them

This documents the qualitative behaviour of the reliable 4-variable `SimpleSim`, then lists where
the economy-grounded `SecularSim` must reproduce those couplings. Gauges are all in `[0,1]`:
`P` population pressure (0.5 = carrying capacity), `E` elite overproduction, `U_e` instability,
`S` state capacity. `mem` = instability memory.

## The equations (per tick, ×0.1 integration step)

```
carrying_cap cc = clamp(0.5 + S^2 - U_e)                 # what the land+state can support
dP = (P+0.1)(cc - P)          [growth-shaped]           # logistic toward cc
immiseration = max(0, P - cc/2)*2 + E*0.2               # misery starts at half capacity; elites add
dE = P^2 * max(0, S - U_e)  - U_e^2  - max(0, 0.5-P)*0.25
dS = P*(1 - U_e)  - U_e^2  - E*(S + 0.1)
unrest = p_mob * e_mob * (1 - S)   ; p_mob=f(immiseration,mem), e_mob=f(E,mem)
U_e = unrest ;  mem = max(U_e, mem*0.95 - 0.02)         # memory ratchets up, decays slowly
```

## What each variable does

**P (population pressure)**
- *Rises* toward carrying capacity `cc = 0.5 + S² − U_e`; fastest when P is low (expansion),
  slowing as it nears `cc`.
- *Falls* when it overshoots `cc`, and hard when `cc` itself drops (i.e. when `U_e` spikes).
- High P alone does **not** collapse the society — it raises immiseration, which only matters once
  it combines with elites to produce unrest.

**E (elite overproduction)**
- *Grows* ∝ `P²` **but only while the state functions** (`S > U_e`): a large population throws off
  many aspirant elites. Growth is fastest at high P.
- *Shrinks* ∝ `U_e²` — violence prunes elites — and shrinks when `P < 0.5` (a small population
  can't support many elites; high wages squeeze them out).
- Therefore E **persists as long as P stays high and violence is moderate**, and only clears when
  sustained violence (`U_e`) grinds it down. **E reaching ~0 is what ends the crisis.**

**U_e (instability / violence)**
- `unrest = popular_mobilization × elite_mobilization × (1 − S)`. Crucially a **product**: it needs
  **both** immiserated commoners **and** surplus elites, and is suppressed by a strong state.
- Elites are the necessary leadership of violence — **no excess elites ⇒ e_mob→0 ⇒ no unrest.**
- `mem` ratchets up with U_e and decays slowly, giving the crisis inertia (father–son waves).

**S (state capacity)**
- *Grows* with `P·(1−U_e)` — a large, orderly population is a big tax base.
- *Falls* with `U_e²` (collapse/repression cost) and, importantly, with **`E·(S+0.1)`** —
  **overproduced elites drain the state** (offices, patronage, in-fighting).
- So S **stays low while E is high**, and can only recover once elites have been pruned.

## The cycle these produce

1. **Expansion** — P rises fast, S rises with it, E begins to grow (∝P²), U_e ~0.
2. **Stagnation** — P near `cc`; immiseration high; E large (elite golden age) and draining S;
   unrest still contained by a strong state.
3. **Crisis** — S can no longer contain it; `unrest` fires; `cc` drops so P falls.
4. **Depression** — violence (needs E) continues; U_e² slowly prunes E while E·(S+0.1) keeps S low;
   population is suppressed. Ends when **E → ~0** ⇒ e_mob→0 ⇒ U_e→0 ⇒ S recovers ⇒ new expansion.

## Requirements the economy model must satisfy (targets for the retune)

1. **P**: rises most in expansion, plateaus in stagnation, declines in fracture. The fracture-phase
   population drop should be **moderate (~10–40%, ~50% only for a Black-Death-scale shock)**, and it
   should be driven by *instability*, not by population pressure alone.
2. **E**: must **persist through the whole fracture phase and decline gradually**, reaching ~0 only
   at its end — because clearing elite overproduction is *the* reason the fracture ends. It must not
   crash to 0 mid-fracture.
3. **U_e**: sustained by elites — **no elites ⇒ no violence**. (The economy's additive
   `w_pop·pop_unrest + w_elite·elite_unrest` must not keep firing once `E≈0`.)
4. **S**: must **stay low for a sustained depression**, recovering only as E clears — i.e. reproduce
   SimpleSim's `−E·(S+0.1)` elite-drain coupling, not snap back the moment the budget balances.

## How the economy model now matches these (after the retune)

- **E persists and declines gradually.** Elite crisis mortality is now a **linear attrition**
  (`k_elite_attrition · U_e`, not a `U_e²` crash) with gentle demotion (`k_demotion`), so elites
  glide down over the whole fracture (e.g. ~11 → 4) and E hits ~0 only at the end — which is what
  triggers recovery. (Reproduces SimpleSim's slow `−U_e²` pruning.)
- **Unrest needs elites.** `unrest = w_pop·pop_unrest + w_elite·elite_unrest`, and because E now
  persists, elite-led violence continues through the depression and stops when E clears.
- **S stays low through the depression.** Added the explicit elite drain
  `fiscal_target ×= (1 − k_elite_drain·E)` (SimpleSim's `−E·(S+0.1)`), so S only recovers as E clears.
- **Population drop is moderate (~40%).** Deaths are now **disease-driven**
  (`k_disease · U_e · (0.5+0.5·imm)`, worse under unrest & immiseration), births are decoupled from S
  (so they recover in the depression), and a **security floor** keeps the carrying-capacity drop
  bounded — the population declines *toward the reduced carrying capacity* instead of crashing 80%.
- **Violence no longer pins at 100%.** A **war-weariness** term damps each wave and decays over a
  generation, so U_e rises and falls (peak ~0.6) rather than sitting at 1.0 for ~28 steps. (Full
  multi-wave father-son oscillation is only partial — a known area for further work, since the
  resurgence competes with the state's `(1−S)` suppression.)
- Period is now ~130–200 (tunable via `birth_base` and the mortality/attrition rates).

## Known remaining gaps
- **Father-son waves** are damped to a single broad hump, not 2–3 distinct violence spikes per
  crisis. Getting true multi-wave behaviour needs elite conflict partly decoupled from `S` without
  destabilising prosperity.
- **Carrying capacity** is still land/`security`-based; per the design it should later mean
  something different in industrial societies (relative SoL vs recent generations).
