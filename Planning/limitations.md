# FinalSim — known limitations & design notes

## Ahistorically strong states can permanently suppress the secular cycle
If a state can suppress unrest **too effectively and too cheaply**, mobilization potential never overcomes
suppression, so the crisis never fires and the secular cycle is permanently suppressed (the sim reads this
as a "stuck" strain/fracture that oscillates underneath but never phase-changes).

This is a **feature, not a bug** — it mirrors reality: a state sitting atop a "gunpowder mine" of elite
overproduction stays intact only until an external shock (invasion, bankruptcy, succession crisis) removes
its ability to keep paying for suppression. It just means the internal model alone won't break such a state;
that break has to come from outside the single-location cycle.

**Design intent to make it self-limiting (not yet fully implemented / needs balancing):**
- Suppression should get **more costly the more it's used** — ideally *superlinear* in mobilization
  potential (`cost ~ mob_pot^gamma`, gamma > 1), so holding down a large overproduced elite drains the
  treasury fast → the strong state becomes fiscally **brittle** rather than permanently stable.
- Suppression **effectiveness** scales with legitimacy (e.g. 0% legitimacy → ~50% effectiveness, not 1:1),
  so a delegitimized-but-solvent state still can't suppress cheaply.
- Optionally, the unrest pressure gauge could "explode" past some mobilization threshold — a hard ceiling
  where accumulated grievance overwhelms any suppression.

The point: deliberately engineer ways to break ahistorically strong states, rather than letting cheap
suppression flat-line the cycle. Exact limits to be found in later robust testing.
