# FinalSim — known limitations & design notes

## Ahistorically strong states can permanently suppress the secular cycle
If a state can suppress unrest **too effectively and too cheaply**, mobilization potential never overcomes
suppression, so the crisis never fires and the secular cycle is permanently suppressed (the sim reads this
as a "stuck" strain/fracture that oscillates underneath but never phase-changes).

This is a **feature, not a bug** — it mirrors reality: a state sitting atop a "gunpowder mine" of elite
overproduction stays intact only until an external shock (invasion, bankruptcy, succession crisis) removes
its ability to keep paying for suppression. It just means the internal model alone won't break such a state;
that break has to come from outside the single-location cycle.

**Design intent to make it self-limiting:**
- **DONE:** Suppression **effectiveness scales with legitimacy** (`suppress_legit_floor=0.5`: a fully
  delegitimized state suppresses at half strength). Suppression is also now **per-capita** (`/population`)
  so a big/rich realm's coercion no longer dwarfs the bounded unrest.
- **Still open:** Suppression should get **more costly the more it's used** — *superlinear* in mobilization
  (`cost ~ mob_pot^gamma`, gamma > 1) — so a **cheap** army (`k_suppress` low) can't hold down a large
  overproduced elite indefinitely. This is the remaining driver of the stuck-fracture corner (a cheap,
  effective, well-funded army caps U_e below what's needed to clear elites → overproduction parks on the
  exit gate). Left unfixed by choice: an external shock (invasion) disrupts such a state's suppression.

The point: deliberately engineer ways to break ahistorically strong states, rather than letting cheap
suppression flat-line the cycle.

## Insufficient population growth stalls the cycle (permanent prosperity / "golden age")
If population **can't grow fast enough to approach carrying capacity**, wages never fall → no immiseration →
no elite overproduction crisis → the realm sits in **permanent prosperity** (`STUCK-pros`).

Net population growth is slow by construction: `net ≈ birth·(1 − child_mortality) − death_base`, and it
**decays toward 0 as pop → cap**. With defaults that's only ~0.8%/yr at best (the user's illustration: 2.5%
birth × 50% child mortality = 1.25% net births − 1% death = **0.25%** net), so the approach to cap is
asymptotic and slow. Any of these can outpace it and stall the cycle:
- **Fast/continuous expansion** — carrying capacity (land) grows faster than population can chase it (the
  Roman/Rurikid case: `+0.6%/tick` land growth → permanent golden age in testing).
- **High `land_productivity`** raising cap faster than pop fills it.
- **Low birth / high child mortality** settings — the growth engine is too weak to ever reach cap.

**Design fix (deferred):** a **"cultural optimism"** modifier — the longer a realm stays in prosperity, the
higher its birth rate — would let population chase a rising cap and keep the cycle alive under sustained
expansion (and is historically plausible: settled prosperous eras had baby booms). Until then: **too little
population growth breaks the sim** — keep net growth high enough (relative to any cap growth) that pop
actually reaches the density that drives immiseration.
