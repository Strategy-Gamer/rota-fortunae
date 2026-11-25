# Rota Fortunae

A god game where civilizations rise and fall through Turchin-style secular cycles. Godot 4.5 for
presentation and netcode (GDScript), a C++ GDExtension for the simulation. One developer; agents
write most of the code and the dev reads every diff.

## What matters

- **Empires visibly rise and fall.** Prioritize work by whether it serves that.
- Find the smallest model that makes the correct behavior unsurprising. Don't keep complexity just
  because it exists, and don't add machinery because it looks architectural. The roadmap and the
  "not now" list in `docs/DESIGN.md` say what is deliberately deferred.
- Honor the dev's intent. Before changing something the dev wrote, work out what it was meant to
  do. If your change departs from that, say so plainly.
- These are defaults, and the dev overrides them. If a rule fights the task, say so before you
  break it.

## What counts as slop here

- **Making a check pass by changing the check.** When the sim misbehaves, fix the update rule that
  produces the wrong trajectory. Don't redefine phases or thresholds, and don't add clamps, bleeds
  or resets to hide it. A passing verifier is a measurement, not the goal.
- **Tests that prove nothing:** tests of getters or wrappers, tests that restate the
  implementation, mocks of our own code. See [Verifying](#verifying).
- **Defensive code for states that can't happen.** Validate at the boundary (`SimWorld`) and trust
  data inside the sim.
- **Speculative structure:** an abstraction with one caller, a config knob nobody asked for, or a
  generic system where one concrete function would do.
- **Scope creep.** One request means one change. If you notice something else broken, mention it
  instead of fixing it in the same change.
- **Claiming it works without running it.** Report what you ran, what you saw, and what you didn't
  check.
- **Leftovers:** dead code, code you commented out, debug prints, `TODO`s with no context.

## Comments

- Default to no comment. Write one only to explain *why* (a constraint, an engine quirk, an
  invariant) or to sum up genuinely tricky code in a sentence or two.
- Match the surrounding density. That is usually one short trailing comment; never a multi-line
  rationale block. Explain reasoning in chat or in a design doc instead.
- Never repeat values in comments. `// grows 1% a year` goes stale the first time someone retunes
  the rate. Name the constant and let the code be the source of truth.
- No comments addressed to the reviewer, such as "changed to fix X" or "as requested". History
  belongs in commits.

## Ways to hurt the project

1. **Floats in simulation state.** Sim math uses `Fixed32`/`Fixed64` and `FixedMath`, so lockstep
   peers compute bit-identical results. Floats are fine for rendering and geometry only.
2. **Unstable iteration order.** Never let hash-map or hash-set order affect state. Loop over
   entities by ID.
3. **A store that isn't wired in.** Every authoritative store goes in `World`, gets cleared in
   `World::clear`, and is fed into `SimWorld::get_state_hash`. If it is left out of the hash, you
   get silent desyncs later. Render-dirty flags are presentation state and are never hashed.
4. **Renumbering command IDs.** `CommandType` in `src/godot/sim_world.h` is a contract with
   GDScript. Add new IDs inside their domain range and never reuse or renumber one.
5. **Two sources of truth.** Each fact has one authoritative array. Anything else (reverse
   indexes, palettes, totals) is derived and must be rebuildable.
6. **Editing `godot-cpp/`.** It's a vendored submodule. Never touch it.
7. **Editing `Planning/` models.** That's the dev's design workspace: read it freely, but edit the
   Python models only when asked.
8. **Killing the dev's Godot editor.** If the build can't overwrite the DLL, the editor probably
   has it locked. Tell the dev rather than killing the process.

## Hit every layer

Most features cross the whole stack. Before calling one done, say which of these applied:

- **Store:** in `World`, `clear`, initialization, and the state hash.
- **System:** called from the tick driver (`TimeProgression`) at an explicit point in the order.
  Never from GDScript.
- **Player actions:** a command (an ID plus a handler in `src/core/command.cpp`). GDScript
  describes a change and C++ applies it. There are no direct setters from GDScript.
- **Reads:** through a query in `src/queries/`. Cross the boundary with a few coarse calls, not
  per-entity loops.
- **Visibility:** if it's meant to be seen, it's on a panel or map mode, and the code that changes
  it sets `render_dirty`.
- **Docs:** if `docs/DESIGN.md` now says something false, fix it.

## Building and running

The tools work but aren't on PATH. Run these from the repo root:

- **Build:** `py -m SCons platform=windows target=template_debug` (MSVC). This installs the DLL into
  `project/bin/windows/`.
- **Run headless:**
  `"C:/Users/white/Documents/Godot/Godot_v4.5.1-stable_win64_console.exe" --headless --path project --quit-after <frames>`.
  Use the `_console` build so output reaches stdout.
- **Playtest runner:** run the headless command above with `-s res://Tools/playtest.gd -- seed=0
  countries=8 years=300 runs=2 out=<scratchpad>/run.csv` in place of `--quit-after`. It generates
  a seeded scenario and writes each country's yearly trajectory and the state hash to CSV. It
  prints the final year, and with `runs` above 1 it checks determinism (exit code 1 if runs
  diverge). A few hundred years takes milliseconds.
- **Standalone C++ tests:** each file in `tests/` has its compile line in its header (g++ from
  `C:/msys64/mingw64/bin`). Put binaries in the scratchpad, not the repo. g++ and the MSVC game
  build take different `Fixed64` code paths (`docs/DESIGN.md` §6), so a passing g++ test doesn't
  prove the game's math.

## Verifying

Prefer the checks that resemble play, in this order:

1. **Run the playtest runner** over simulated years and read the trajectories. Try several
   seeds, because behavior that only works on one seed isn't working.
2. **Determinism:** run the runner with `runs=2` or more. For anything that touches commands or
   the tick loop, also boot the game headless.
3. **Unit tests,** only for self-contained code with independently known answers. For example,
   `FixedMath` is checked against `<cmath>`.

Agents can't judge fun or feel. For UI and pacing changes, tell the dev exactly what to look at.
For sim behavior, diagnose with traces before you change a rule.

## Branches and commits

- `develop` is for everyday work, which may be unfinished or unfun. `main` holds what the dev has
  playtested and accepted.
- Work on `develop`. Commit there on your own once a change builds and is verified, in small
  commits that are easy to review.
- Never merge into or push to `main`, force-push, or rewrite history. The dev verifies and
  playtests before promoting anything to `main`.
- Commit messages are a short summary line in the existing style, plus a body when the why isn't
  obvious.
- Don't end your work with `develop` failing to build without saying so.

## Docs

- `docs/DESIGN.md` is authoritative. It records decisions and their reasons, constraints that span
  components, and traps that are hard to see from the source. Before adding a paragraph, ask what
  the dev would get wrong without it.
- Don't list fields, methods or constants, narrate control flow, or keep file catalogs. Those
  drift, and the code already records them. Link to the source instead.
- When a decision changes, rewrite the affected text. Don't append a second account.
- `docs/design-notes/` is raw research material, not authoritative.
- Don't commit plans, research notes or scratch files. Keep them outside the repo.

## Where code lives

- `src/core/`: `World` (all authoritative state), command dispatch, hashing.
- `src/godot/`: `SimWorld`, the only class GDScript sees.
- `src/time/`: calendar, sync clock, and `TimeProgression` (the tick driver).
- `src/economy/`, `src/countries/`, `src/map/`: stores and systems, as struct-of-arrays indexed by
  ID.
- `src/queries/`: read paths out of C++ for the UI.
- `src/utility/`: fixed-point types and math.
- `project/`: the Godot project. `Game` (autoload) is the command bus and world accessor;
  `GameSession` runs the tick loop.
- `Planning/`: the dev's design docs and Python secular-cycle prototypes.
