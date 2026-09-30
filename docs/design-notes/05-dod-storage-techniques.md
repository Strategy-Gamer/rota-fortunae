# 05 — DoD storage techniques (swap-remove, sparse sets, out-of-band)

Source: a YouTube DoD walkthrough (watched 2026-08), + annotations for **our** C++/SoA
context. **Exploratory — not adopted yet.** Ties into `DESIGN.md` §10 "Later, only when
earned" (entity deletion, threading). Revisit when we have a store that actually churns
(pops, units, projectiles), **not** for countries/locations, which are few and long-lived.

## The video's benchmark (AoS → progressively DoD)

A "process all enemies" loop, mean time per pass:

| Change | Time |
|---|---|
| Original OOP enemy class | 303 ms |
| → plain struct (AoS) | 189 ms |
| → drop the string id | 153 ms |
| → more deletions, erased **in-loop** | **1654 ms** (regression!) |
| → batch all deletions **after** the loop | 547 ms |
| → **swap-remove** array instead of order-preserving erase | 137 ms |
| → tune deletion count back down | 14.2 ms |
| → **out-of-band** (membership implies state) | **7.9 ms** (≈39× vs original) |

Two lessons matter more than the numbers:
1. **Removing from the middle of a contiguous array in a loop is O(n²)** — every erase
   shifts the tail. That's the 1654 ms spike. The fix is *how* you delete, not *what*.
2. Each later win came from **removing work/branches from the hot loop**, not from cleverness.

---

## Technique 1 — swap-remove ("swapback array")

Remove without preserving order: overwrite the dead slot with the **last** element, then
shrink. O(1), no tail shift.

```cpp
// remove index i from a std::vector (SoA: do this to every parallel array at i)
arr[i] = std::move(arr.back());
arr.pop_back();
```

For our **SoA** stores it's the same idea applied to each parallel array at the same index
`i`. (The video used AoS ending as a bare `uint` id — that's just SoA with one array.)

**The catch you already spotted:** the element that *was* at the back now lives at `i`, so
anything that remembered "entity X is at index i" is now wrong. You need a **stable id that
is independent of array position**, plus a way to find an entity's *current* index.

### The structure for that: a sparse set / slot map

There's no `std::` container for this — you hand-roll it (or use EnTT, which is built on it).
Two pieces:

- `dense[]` — the packed SoA arrays you iterate (no gaps, order not meaningful).
- `sparse[]` (or an `unordered_map<Id,int>`) — **stable id → current dense index.**

Swap-remove then also fixes the map for the moved element:

```cpp
void remove(Id id) {
    int i = index_of[id];              // where it currently lives
    Id moved = ids[dense.size()-1];    // the entity we're about to move into slot i
    dense[i] = std::move(dense.back());
    ids[i]   = moved;
    dense.pop_back(); ids.pop_back();
    index_of[moved] = i;               // <-- the fix-up that keeps ids valid
    index_of.erase(id);
}
```

Stable ids never change; only positions do. External references (commands, save files,
the checksum) must key off **id**, never a raw dense index.

### Determinism note (important for us)

Swap-remove makes array order depend on the *history* of insertions/removals, not on id.
That is **fine for lockstep** — every peer runs the identical command stream in the identical
order, so every peer's arrays end up in the identical (unsorted) order. "Fixed iteration
order" then means "iterate the dense array as-is," which is bit-identical across peers.
**The rule stays:** never let a non-deterministic input (hash-map iteration order, wall-clock,
float compare) decide *what* gets removed or *in what order*. The removal must be driven by
deterministic sim events only.

---

## Technique 2 — out-of-band ("membership implies state")

Instead of a `bool alive` / `enum state` field read inside the hot loop, keep **separate
arrays per state** and let *which array an entity is in* encode the flag. e.g. an `alive`
set and a `dead` set; iterating `alive` needs no per-element branch and touches no dead data.

Biggest win when: **many** entities, **frequent** iteration of one subset, and the subset
churns. Weak win when: few, long-lived entities with lots of fields each (moving a country
between two partitions copies a fat row for almost no skipped work).

**Judgment for us:** apply out-of-band to high-churn stores (pops, units, events) *if/when
they exist*. **Skip it for countries and locations** — small counts, long lifetimes, wide
rows. Your instinct there was right.

---

## Technique 3 — batch deletions

Don't delete inside the iteration. Mark (or collect ids) during the pass, apply all removals
once at the end. Avoids repeatedly disturbing the array you're walking and plays nicely with
swap-remove.

---

## Beyond (the "…SIMD, parallelism, IPC…" tail)

The video gestured at SIMD, multithreading, and better instruction-level parallelism / cache
behavior as the next frontier. For us that's the **already-deferred** bucket
(`DESIGN.md` §10 / §11): SoA + stable ids is exactly the layout that makes those a drop-in
*later*, once a profiler shows a real freeze. Don't reach for them now.

## When this becomes real for RF

- **Now:** nothing. Countries/locations don't churn; a free-list isn't even needed yet.
- **First trigger:** the first store with lots of create/destroy per tick (likely pops, or
  transient events/orders). At that point: SoA + sparse-set ids + swap-remove + batched
  deletes, in that order. Out-of-band only if profiling says the alive/dead branch matters.
