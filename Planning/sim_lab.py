"""
sim_lab.py -- an inspection / testing harness for FinalSim (new_model.py).

Usage:
    PYTHONUTF8=1 python sim_lab.py        # runs the __main__ block at the bottom -- EDIT IT
or import into a notebook:
    from sim_lab import simulate, summary, trace, trace_first_crisis, sweep, hum_check

Everything funnels through simulate(), which returns (sim, hist):
    sim   -- the FinalSim object (so you can also read its built-in series:
             sim.P_history, E_history, U_history, U_e_history, S_history, phase_history,
             sim.wage_history, population_history, carrying_capacity_history,
             sim.elites_history, positions_history, e_reproduction_history,
             sim.e_mobility_up_history, e_mobility_down_history, e_deaths_history,
             sim.legitimacy_history, revenue_history, expenses_history,
             sim.suppression_history, treasury_history, ...)
    hist  -- {attr: [value per tick]} for the scalar attributes in TRACK (add your own).

Override ANY FinalSim attribute as a kwarg, e.g. simulate(6000, k_attrition=0.05, tax_rate=0.3).
"""
from new_model import FinalSim

# Scalar sim attributes recorded every tick. Add/remove freely -- any attribute FinalSim
# sets on itself works (phase, P, E, U, U_e, S, legitimacy, security, treasury, elites, population,
# overprod_ratio, felt_overproduction, birth_rate, death_rate, w, w0, wage_share, ew_inverse,
# revenue, expenses, suppression, suppression_cost, patronage_paid, army_shortfall, elite_*).
TRACK = ["phase", "P", "E", "U", "U_e", "S", "legitimacy", "security", "treasury",
         "elites", "population", "overprod_ratio", "felt_overproduction", "birth_rate", "death_rate",
         # instruments for the causal-chain verifier (exposed by new_model.py):
         "w", "w0", "wage_share", "ew_inverse", "immis",
         "revenue", "expenses", "suppression", "suppression_cost", "patronage_paid", "army_shortfall",
         "elite_births", "elite_up", "elite_down", "elite_cull", "elite_net",
         # crises/events layer:
         "crisis_load", "crisis_pop_deaths", "crisis_elite_deaths"]


def simulate(steps=6000, record=None, **overrides):
    """Run a fresh sim for `steps` ticks. Returns (sim, hist)."""
    record = record or TRACK
    s = FinalSim()
    for k, v in overrides.items():
        if not hasattr(s, k):
            raise KeyError(f"FinalSim has no attribute '{k}' -- typo?")
        setattr(s, k, v)
    hist = {a: [] for a in record}
    for t in range(steps):
        s.step(t)
        for a in record:
            hist[a].append(getattr(s, a))
    return s, hist


# ---------------------------------------------------------------- metrics
def phase_split(hist):
    ph = hist["phase"]; n = len(ph)
    return {nm: round(100 * sum(1 for p in ph if p == k) / n)
            for k, nm in [(0, "pros"), (1, "strain"), (2, "frac")]}

def secular_periods(hist):
    """Ticks between successive prosperity onsets (one full cycle each)."""
    ph = hist["phase"]
    on = [i for i in range(1, len(ph)) if ph[i] == 0 and ph[i-1] != 0]
    return [on[i+1] - on[i] for i in range(len(on) - 1)]

def crises(hist):
    """[(start,end)] index pairs for each fracture (phase==2) episode."""
    ph = hist["phase"]; out = []; i = 0; n = len(ph)
    while i < n:
        if ph[i] == 2:
            j = i
            while j < n and ph[j] == 2:
                j += 1
            out.append((i, j)); i = j
        else:
            i += 1
    return out

def is_stuck(hist, window=1200):
    """True if the last `window` ticks never change phase (a frozen limit cycle / dead state)."""
    return len(set(hist["phase"][-window:])) == 1

def elite_cull_depth(hist):
    """Per crisis, min(elites during) / max(elites just before). 1.0=no cull, 0.0=wiped."""
    out = []
    for a, b in crises(hist):
        pre = max(hist["elites"][max(0, a-25):a] or [1e-9])
        lo = min(hist["elites"][a:b] or [0])
        if pre > 1e-9:
            out.append(round(float(lo / pre), 2))
    return out

def pop_crash_depth(hist):
    """Per crisis, 1 - min(pop during)/max(pop just before)."""
    out = []
    for a, b in crises(hist):
        pre = max(hist["population"][max(0, a-25):a] or [1e-9])
        lo = min(hist["population"][a:b] or [0])
        if pre > 1e-9:
            out.append(round(float(1 - lo / pre), 2))
    return out


# ---------------------------------------------------------------- causal-chain verifier
# The secular cycle the model MUST reproduce, as a per-phase set of directional links (the user's chain).
# Each link: (variable, expected_direction, human label). Directions:
#   up   = net rise over the phase        down = net fall
#   pos  = flow active (mean > 0)         dip  = interior minimum then recovers (down-up)
#   peakfall = interior maximum then falls (up-down)
CHAIN = {
    0: [  # PROSPERITY: low pop/elites, state recovers -> pop grows -> wages fall -> immiseration
        ("population",     "up",   "population rising (pressure builds)"),
        ("w",             "down",  "wages falling (immiseration)"),
        ("security",       "up",   "state power (security) recovering"),
        ("legitimacy",     "up",   "state legitimacy recovering"),
    ],
    1: [  # STRAIN: mobility -> more elites -> overproduction -> unrest; state drained toward the fiscal break
        ("elite_up",       "pos",  "upward mobility into elites active"),
        ("elites",         "up",   "elite numbers rising"),
        ("overprod_ratio", "up",   "elite overproduction (E/pos) rising"),
        ("U_e",            "up",   "unrest rising"),
        ("treasury",      "down",  "state drained (suppression + patronage)"),
        ("security",      "down",  "order eroding as unrest rises"),
    ],
    2: [  # FRACTURE: elite mortality, overprod falls, slow pop decline, wages rise, state revives
        ("elite_cull",     "pos",  "elite mortality active"),
        ("elites",        "down",  "elite numbers falling"),
        ("overprod_ratio","down",  "overproduction falling"),
        ("population",     "down", "population declining"),
        ("w",              "up",   "wages rising"),
        ("U_e",        "peakfall", "unrest peaks then falls"),
        ("security",       "up",   "security recovering (overproduction clearing)"),
        ("legitimacy",    "down",  "legitimacy dips (drains, then revives in prosperity -- should NOT hit zero)"),
    ],
}
PHASE_NAMES = {0: "PROSPERITY", 1: "STRAIN", 2: "FRACTURE"}


def cycle_bounds(hist):
    """[(start,end)] for each COMPLETE cycle: prosperity onset to the next prosperity onset."""
    ph = hist["phase"]
    on = [i for i in range(1, len(ph)) if ph[i] == 0 and ph[i-1] != 0]
    return [(on[i], on[i+1]) for i in range(len(on) - 1)]

def phase_runs(hist, a, b):
    """Contiguous phase runs within [a,b): list of (phase, start, end)."""
    ph = hist["phase"]; runs = []; i = a
    while i < b:
        j = i
        while j < b and ph[j] == ph[i]:
            j += 1
        runs.append((ph[i], i, j)); i = j
    return runs

def phase_segment(hist, a, b, phase):
    """The longest contiguous run of `phase` within cycle [a,b) -- or None if that phase never occurs."""
    runs = [(s, e) for p, s, e in phase_runs(hist, a, b) if p == phase]
    return max(runs, key=lambda se: se[1] - se[0]) if runs else None

def trend(hist, var, s, e):
    """Directional summary of hist[var] over [s,e): endpoints, interior extrema (+timing), slope."""
    seg = hist[var][s:e]
    if len(seg) < 2:
        return None
    lo, hi = min(seg), max(seg); n = len(seg)
    fmin = seg.index(lo) / n; fmax = seg.index(hi) / n
    xs = list(range(n)); mx = (n - 1) / 2; my = sum(seg) / n
    denom = sum((x - mx) ** 2 for x in xs) or 1e-9
    slope = sum((x - mx) * (y - my) for x, y in zip(xs, seg)) / denom
    return {"start": seg[0], "end": seg[-1], "lo": lo, "hi": hi,
            "fmin": fmin, "fmax": fmax, "slope": slope, "mean": my}

def _check(direction, tr):
    """(passed_bool, detail_str) for one link's expected direction against a measured trend."""
    if tr is None:
        return (None, "no data")
    s, e, lo, hi = tr["start"], tr["end"], tr["lo"], tr["hi"]
    eps = 1e-6 + 0.02 * max(abs(s), abs(e), 1e-6)          # ignore trivial wiggles
    if direction == "up":
        return (e > s + eps, f"{s:.3f}->{e:.3f} (d{e-s:+.3f})")
    if direction == "down":
        return (e < s - eps, f"{s:.3f}->{e:.3f} (d{e-s:+.3f})")
    if direction == "pos":
        return (tr["mean"] > eps, f"mean {tr['mean']:.4f}")
    if direction == "dip":
        interior = 0.05 < tr["fmin"] < 0.95
        real = lo < s - eps and lo < e - eps
        return (interior and real, f"min {lo:.3f}@{tr['fmin']*100:.0f}% (ends {s:.3f}->{e:.3f})")
    if direction == "peakfall":
        interior = 0.05 < tr["fmax"] < 0.98
        fell = e < hi - eps
        return (interior and fell, f"peak {hi:.3f}@{tr['fmax']*100:.0f}% ->end {e:.3f}")
    return (False, "unknown dir")

def _sustained_rise(seg, min_run=8, min_gain=0.05):
    """True if the series climbs > min_gain above a running low (a real regrowth, not noise)."""
    if len(seg) < min_run + 1:
        return False
    lo = seg[0]
    for v in seg[1:]:
        if v < lo:
            lo = v
        elif (v - lo) / max(lo, 1e-9) > min_gain:
            return True
    return False

def pop_decline_shape(hist):
    """Per fracture: is the population decline slow/steady (good) or a front-loaded cliff (bad)?"""
    out = []
    for a, b in crises(hist):
        seg = hist["population"][a:b]
        if len(seg) < 5:
            continue
        peak, trough = max(seg), min(seg); drop = peak - trough
        if drop <= 1e-9:
            out.append({"front_frac": 0.0, "max_tick_drop": 0.0, "cliff": False}); continue
        k = max(1, int(0.10 * len(seg)))
        front = (peak - min(seg[:k+1])) / drop                # share of the crash done in first 10%
        mtd = max((seg[i] - seg[i+1]) / max(seg[i], 1e-9) for i in range(len(seg)-1))
        out.append({"front_frac": round(front, 2), "max_tick_drop": round(mtd, 4),
                    "cliff": front > 0.4 or mtd > 0.05})
    return out

def elite_regrowth_check(hist, overprod_exit=1.3):
    """Per fracture: do elites regrow BEFORE overproduction clears? (they must not.)"""
    out = []
    for a, b in crises(hist):
        op = hist["overprod_ratio"][a:b]; el = hist["elites"][a:b]
        clear = next((i for i, v in enumerate(op) if v < overprod_exit), None)
        window = el[:clear] if clear is not None else el
        out.append({"cleared": clear is not None, "clear_at": clear,
                    "regrew_before_clear": _sustained_rise(window)})
    return out


def unrest_lag_check(hist, overprod_exit=1.3, u_exit=0.20):
    """Per fracture: does unrest track its cause? Reports how many ticks unrest stays elevated AFTER
    elite overproduction has already cleared -- i.e. how long violence lingers past what's driving it.
    lag >> 0 means unrest has its own clock (bad); lag ~ 0 means unrest is derived from overproduction."""
    out = []
    for a, b in crises(hist):
        op = hist["overprod_ratio"][a:b]; ue = hist["U_e"][a:b]
        op_clear = next((i for i, v in enumerate(op) if v < overprod_exit), None)
        u_clear = next((i for i, v in enumerate(ue) if v < u_exit), None)
        if op_clear is None:
            out.append({"lag": None, "note": "overprod never cleared"}); continue
        if u_clear is None:
            out.append({"lag": (b - a) - op_clear, "note": "unrest never cleared"}); continue
        out.append({"lag": u_clear - op_clear,
                    "op_clear": op_clear, "u_clear": u_clear, "frac_len": b - a})
    return out


def verify_chain(hist):
    """Check every link of the causal chain in every complete cycle. PASS = holds in all cycles."""
    cyc = cycle_bounds(hist)
    print(f"  complete cycles: {len(cyc)}   split={phase_split(hist)}   stuck={is_stuck(hist)}")
    if not cyc:
        ph = hist["phase"]
        print(f"  -> NOT CYCLING (stuck in phase {ph[-1]} = {PHASE_NAMES.get(ph[-1])}). No cycle to verify.")
        print("     Per-phase link directions can't be checked without a cycle; fix cycling first.")
        return
    for phase in (0, 1, 2):
        print(f"\n  --- {PHASE_NAMES[phase]} ---")
        for var, dr, label in CHAIN[phase]:
            res = []
            for a, b in cyc:
                seg = phase_segment(hist, a, b, phase)
                res.append(_check(dr, trend(hist, var, *seg)) if seg else (None, "phase absent"))
            oks = [ok for ok, _ in res if ok is not None]
            npass = sum(1 for ok in oks if ok)
            mark = "PASS" if oks and npass == len(oks) else ("FAIL" if oks else "----")
            print(f"    [{mark}] {npass}/{len(oks)}  {label:40s} last: {res[-1][1]}")

def verify_rules(hist, overprod_exit=1.3):
    """The two qualitative rules: no pop cliff, no elite regrowth before clearance."""
    print("\n  --- QUALITATIVE RULES (per fracture) ---")
    ps = pop_decline_shape(hist)
    if not ps:
        print("    (no fractures)")
    for i, d in enumerate(ps):
        print(f"    frac {i}: pop decline {'CLIFF' if d['cliff'] else 'ok   '}  "
              f"front10%={d['front_frac']}  max_tick_drop={d['max_tick_drop']}")
    for i, d in enumerate(elite_regrowth_check(hist, overprod_exit)):
        tag = "REGREW-EARLY" if d["regrew_before_clear"] else "ok"
        cl = f"cleared@+{d['clear_at']}" if d["cleared"] else "never cleared"
        print(f"    frac {i}: elites {tag:12s}  ({cl})")


# ---------------------------------------------------------------- reports
def summary(hist):
    sp = secular_periods(hist); cr = crises(hist)
    rng = lambda a: (round(float(min(hist[a])), 3), round(float(max(hist[a])), 3))
    print(f"  split P/S/F %:    {phase_split(hist)}   stuck={is_stuck(hist)}")
    print(f"  periods:          {sp[:10]}  mean={round(sum(sp)/len(sp),1) if sp else 0}")
    print(f"  crises:           {len(cr)}  lengths={[b-a for a,b in cr][:10]}")
    print(f"  elite cull depth: {[round(x,2) for x in elite_cull_depth(hist)][:10]}  (0=wiped,1=none)")
    print(f"  pop crash depth:  {pop_crash_depth(hist)[:10]}")
    for a in ("E", "U_e", "legitimacy", "security", "elites", "population", "treasury", "psi", "alpha"):
        if a in hist:
            print(f"  {a:11s} range: {rng(a)}")
    up = [u for u in hist["U_e"] if u > 0.02]
    if up:
        print(f"  unrest: {len(up)} active ticks | {round(100*sum(1 for u in up if u<0.9)/len(up))}% partial(<0.9) | "
              f"mean-active {round(sum(up)/len(up),2)}")

def waves_per_fracture(hist):
    """Distinct crisis episodes (rising edges of crisis_active) inside each fracture -- the seesaw count."""
    if "crisis_active" not in hist:
        return []                              # crises removed for now (unrest is continuous from PSI)
    ca = hist["crisis_active"]
    out = []
    for a, b in crises(hist):
        waves = sum(1 for i in range(a, b) if ca[i] and (i == 0 or not ca[i-1]))
        out.append(waves)
    return out

def hum_check(hist):
    """Seesaw check: how many distinct crisis waves fire per fracture (want 2-4, not one giant wave)."""
    w = waves_per_fracture(hist)
    if not w:
        print("  (no fractures)"); return
    fr = [u for p, u in zip(hist["phase"], hist["U_e"]) if p == 2]
    lull = sum(1 for p, c in zip(hist["phase"], hist["crisis_active"]) if p == 2 and not c)
    print(f"  waves/fracture:  {w[:12]}  mean={round(sum(w)/len(w),1)}  [want 2-4: the father-son seesaw]")
    print(f"  fracture time:   {round(100*lull/len(fr))}% in lull (hum only) / {100-round(100*lull/len(fr))}% mid-wave")

def trace(hist, start, end, cols=None, bar="U_e"):
    """Tick-by-tick table for [start,end), with an ASCII bar for `bar`."""
    cols = cols or ["phase", "elites", "U", "U_e", "overprod_ratio", "legitimacy", "security", "population", "treasury", "w"]
    print("t     " + "  ".join(f"{c[:8]:>8}" for c in cols) + f"   {bar}")
    for t in range(max(0, start), min(end, len(hist[cols[0]]))):
        cells = []
        for c in cols:
            v = hist[c][t]
            cells.append(f"{v:8d}" if isinstance(v, int) else f"{v:8.3f}")
        b = "#" * int(max(0.0, min(1.0, hist[bar][t])) * 30) if bar in hist else ""
        print(f"{t:5d} " + "  ".join(cells) + "   " + b)

def trace_first_crisis(hist, pre=6, post=70):
    for a, b in crises(hist):
        if a > 50:                       # skip the warm-up transient
            trace(hist, a - pre, a + post); return
    print("  (no post-warmup crisis found)")

def sweep(param_grid, steps=4000, seed=31, n=None, metric=None, show=15):
    """Random grid sweep. param_grid={name:[values]}. metric(hist)->str; default flags stuck."""
    import itertools, random
    keys = list(param_grid); combos = list(itertools.product(*param_grid.values()))
    random.seed(seed); random.shuffle(combos)
    if n:
        combos = combos[:n]
    metric = metric or (lambda h: "STUCK" if is_stuck(h) else "ok")
    bad = []
    for vals in combos:
        params = dict(zip(keys, vals))
        _, h = simulate(steps, **params)
        if metric(h) != "ok":
            bad.append((params, metric(h)))
    print(f"sweep: {len(combos)} configs | {len(bad)} flagged ({round(100*len(bad)/len(combos),1)}%)")
    for p, r in bad[:show]:
        print(f"   {r}: {p}")
    return bad


if __name__ == "__main__":
    # ============================== EDIT ME ==============================
    sim, h = simulate(6000)                       # e.g. simulate(6000, k_attrition=0.25, gamma=4.0)

    print("=== SUMMARY ===");            summary(h)
    print("\n=== CAUSAL-CHAIN VERIFICATION (does each link fire, per phase, every cycle?) ===")
    verify_chain(h)
    verify_rules(h, sim.overprod_exit)

    # print("\n=== ROBUSTNESS SWEEP ===")
    # sweep({"rad_burnout": [0.2, 0.3, 0.4], "overprod_tol": [1.2, 1.4, 1.6],
    #        "k_attrition": [0.15, 0.25, 0.4], "r_e_premium": [0.012, 0.02, 0.03]}, n=100)
    # ====================================================================
