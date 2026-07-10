"""
Rota Fortunae — economy objects (OOP scaffolding).

This is the object model for the game's district/pop economy, per economy_design.md. It is written
in OOP for now for clarity and experimentation; the shipping engine will be data-oriented (DoD) in
C++, so keep the *data* on these objects clean and the *behaviour* in plain methods that a DoD port
can turn into systems over arrays.

Districts are Stellaris-like: each District is an individual building instance whose SIZE scales its
jobs / output / land. A district's TIER is really a SUB-TYPE: each tier has its own distribution of
OUTPUTS and INPUTS (e.g. Basic Farming makes bare wealth + enough food; Intensive Agriculture makes
much more food and a ton more wealth; Mechanized Agriculture additionally consumes materials as an
input). A DistrictType is the shared definition; District is the instance.

Subsistence is NOT an elite-owned district but is modelled as a hardcoded worker-owned district type
(commoners self-employing on unused arable/pastoral capacity, keeping their own output).

Wealth for pops is a MONTHLY RUNNING TOTAL, not a stock: it is recomputed and overwritten every tick
(commoner_income / elite_income ARE the pops' wealth). Only treasuries (state, civ) and elite
investment projects accumulate.

All the wage/output/wealth math lives in extra_functions.py and is reused here.
"""

from extra_functions import (
    get_wealth_level, get_shortage_modifier, get_land_productivity,
    get_wage_share, get_job_capacity, get_output, get_job_pull,
    wealth_req, food_req, goods_req,
)


# ---------------------------------------------------------------------------
# Tier (a sub-type of a district: its own outputs & inputs)
# ---------------------------------------------------------------------------
class Tier:
    """One step in a district's upgrade chain. A tier is effectively a sub-type: it defines its own
    per-worker OUTPUT distribution and INPUT distribution. Higher tiers usually make more (and shift
    the mix, e.g. from food toward wealth) and may require material/goods inputs."""

    def __init__(self, name, outputs, inputs=None, labor_factor=1.0):
        self.name = name
        self.outputs = dict(outputs)          # good -> base productivity per worker
        self.inputs = dict(inputs or {})      # good -> base input demand per worker
        self.labor_factor = labor_factor      # optional: tier changes labor efficiency

    def __repr__(self):
        ins = f" +in{list(self.inputs)}" if self.inputs else ""
        return f"Tier({self.name}: out{list(self.outputs)}{ins})"


# ---------------------------------------------------------------------------
# District type (shared definition/template)
# ---------------------------------------------------------------------------
class DistrictType:
    """Definition of a kind of district. Shared by every District of that type (the static/archetype
    data). Instances live in District."""

    def __init__(self, name, category, tiers,
                 labor_per_size=1.0, land_per_size=1.0,
                 saturation=1.0, exp=1.0,
                 elite_opps_per_size=0.0, security_exposure=1.0,
                 worker_type="Commoner", worker_owned=False):
        self.name = name
        self.category = category                 # 'arable','pastoral','plantation','mining','urban',...
        self.tiers = list(tiers)                 # [Tier, ...] upgrade chain; each tier is a sub-type
        self.labor_per_size = labor_per_size     # job capacity per unit size
        self.land_per_size = land_per_size       # land area consumed per unit size
        self.saturation = saturation             # get_output saturation param
        self.exp = exp                           # get_output exponent
        self.elite_opps_per_size = elite_opps_per_size   # elite opportunities (ownership/mgmt) per size
        self.security_exposure = security_exposure       # value lost when security drops
        self.worker_type = worker_type           # which pop type staffs it
        self.worker_owned = worker_owned         # True => workers keep output (subsistence); no elite owner

    def tier_name(self, tier): return self.tiers[tier].name
    def max_tier(self):        return len(self.tiers) - 1

    def __repr__(self):
        return f"DistrictType({self.name}, {self.category}, tiers={len(self.tiers)})"


# ---------------------------------------------------------------------------
# District (instance: one building, upgradable in size and tier)
# ---------------------------------------------------------------------------
class District:
    """A single district building. Upgradable in SIZE (scale) and TIER (sub-type / tech). Owned by an
    Elite pop (or None for subsistence). `workers` is the labor assigned this tick."""

    def __init__(self, dtype: DistrictType, size=1.0, tier=0, owner=None):
        self.dtype = dtype
        self.size = float(size)
        self.tier = int(tier)
        self.owner = owner            # Pop (Elite) that owns this district; None for subsistence
        self.workers = 0.0            # labor assigned this tick (set by the location's allocator)

    def cur_tier(self):              return self.dtype.tiers[self.tier]

    # --- capacity / footprint -------------------------------------------------
    def job_capacity(self):        return get_job_capacity(self.dtype.labor_per_size * self.size
                                                           * self.cur_tier().labor_factor)
    def land_use(self):            return self.dtype.land_per_size * self.size
    def elite_opportunities(self): return self.dtype.elite_opps_per_size * self.size

    # --- labor market ---------------------------------------------------------
    def wage_share(self, worker_owned_count, d_crit=1.0):
        """Share of this district's wealth paid to its workers (labor-market driven: falls as workers crowd
        the jobs). Subsistence keeps everything (1.0). `d_crit` is the labor-pressure falling point."""
        if self.dtype.worker_owned:
            return 1.0
        return get_wage_share(self.workers, self.job_capacity(), worker_owned_count, d_crit=d_crit)

    def job_pull(self, wage_share, pull_factor=1.0):
        """How strongly this district pulls workers (weighted by pay). Summed across districts to split labor."""
        return get_job_pull(self.job_capacity(), wage_share, pull_factor)

    # --- production -----------------------------------------------------------
    def _flow(self, table):
        """Scale a per-worker {good: base} table by the current labor (saturating with jobs/worker)."""
        jobs = self.job_capacity()
        return {g: get_output(jobs, self.workers, base, self.dtype.saturation, self.dtype.exp)
                for g, base in table.items()}

    def produce(self):       return self._flow(self.cur_tier().outputs)   # {good: amount produced}
    def input_demand(self):  return self._flow(self.cur_tier().inputs)    # {good: amount consumed}

    # --- upgrades -------------------------------------------------------------
    def expand(self, amount=1.0):
        """Grow the building (more jobs/output/land)."""
        self.size += amount

    def upgrade_tier(self):
        """Advance one tier up the chain (a different sub-type), if possible."""
        if self.tier < self.dtype.max_tier():
            self.tier += 1
            return True
        return False

    def __repr__(self):
        return (f"District({self.dtype.name} t{self.tier}:{self.dtype.tier_name(self.tier)} "
                f"size={self.size:.1f} workers={self.workers:.2f})")


# ---------------------------------------------------------------------------
# Concrete district types (per economy_design.md; each tier has its own output/input mix)
# ---------------------------------------------------------------------------
# Arable farming: shifts from bare-subsistence toward heavy food + wealth; mechanization adds materials in.
FARM = DistrictType(
    "Farm", "arable",
    tiers=[
        Tier("Basic Farming",          {"food": 1.5, "wealth": 0.3}),
        Tier("Intensive Agriculture",  {"food": 3.0, "wealth": 1.5}),
        Tier("Mechanized Agriculture", {"food": 5.0, "wealth": 3.0}, inputs={"materials": 0.5}),
        Tier("Precision Agriculture",  {"food": 7.0, "wealth": 5.0}, inputs={"materials": 1.0}),
    ],
    labor_per_size=1.0, land_per_size=1.0, elite_opps_per_size=0.02,
)

# Pastoral: less food/wealth per land, cheaper; factory farms consume materials.
PASTURE = DistrictType(
    "Pasture", "pastoral",
    tiers=[
        Tier("Open Grazing",    {"food": 1.0, "wealth": 0.3}),
        Tier("Managed Pasture", {"food": 1.8, "wealth": 0.8}),
        Tier("Factory Farms",   {"food": 3.5, "wealth": 1.5}, inputs={"materials": 0.4}),
    ],
    labor_per_size=1.0, land_per_size=1.5, elite_opps_per_size=0.015,
)

# Plantation: cash crops -> goods + wealth; mechanization adds materials in.
PLANTATION = DistrictType(
    "Plantation", "plantation",
    tiers=[
        Tier("Basic Planting",          {"goods": 1.0, "wealth": 0.3}),
        Tier("Cash Crops",              {"goods": 2.0, "wealth": 1.2}),
        Tier("Mechanized Plantations",  {"goods": 3.5, "wealth": 2.5}, inputs={"materials": 0.5}),
        Tier("Precision Plantations",   {"goods": 5.0, "wealth": 4.0}, inputs={"materials": 1.0}),
    ],
    labor_per_size=1.0, land_per_size=1.0, elite_opps_per_size=0.02,
)

# Subsistence: NOT elite-owned. Commoners self-employ unused capacity and keep output. One tier.
SUBSISTENCE = DistrictType(
    "Subsistence", "arable",
    tiers=[Tier("Subsistence Farming", {"food": 1.2, "wealth": 0.2})],
    labor_per_size=1.0, land_per_size=1.0, elite_opps_per_size=0.0, worker_owned=True,
)

DISTRICT_TYPES = {dt.name: dt for dt in (FARM, PASTURE, PLANTATION, SUBSISTENCE)}


# ---------------------------------------------------------------------------
# Pop (a group of people of one type & civilization in a location)
# ---------------------------------------------------------------------------
class Pop:
    TYPES = ("Slave", "Commoner", "Urbanite", "Elite")

    def __init__(self, pop_type, amount, civ="default", wealth=0.0):
        assert pop_type in self.TYPES, pop_type
        self.pop_type = pop_type
        self.civ = civ
        self.amount = float(amount)
        # Wealth is a MONTHLY RUNNING TOTAL (this tick's income), not an accumulating stock. >= 0.
        self.wealth = float(wealth)
        # Standard-of-living inputs, refreshed by the location each tick:
        self.food_access = 1.0          # fraction of food need met
        self.goods_access = 0.0         # fraction of (wealth-scaled) goods need met
        self.security = 1.0

    # --- wealth (running total) ----------------------------------------------
    def wealth_per_capita(self):
        return self.wealth / self.amount if self.amount > 0 else 0.0

    def wealth_level(self):
        return get_wealth_level(self.amount, self.wealth)

    def set_wealth(self, amount):
        """Overwrite this tick's wealth (running total). Clamped >= 0; pops never accumulate."""
        self.wealth = max(0.0, amount)

    # --- needs (scale with wealth level: richer pops want & consume more goods) ---
    def food_need(self):   return food_req[self.wealth_level()] * self.amount
    def goods_need(self):  return goods_req[self.wealth_level()] * self.amount

    # --- standard of living ---------------------------------------------------
    def standard_of_living(self):
        """SoL from wealth level (mainly), food access, goods access, security. Bounded ~[0,1].
        Drives population growth, unrest, migration, class mobility, treasury contribution."""
        wl = self.wealth_level() / 50.0
        return 0.55 * wl + 0.25 * min(1.0, self.food_access) \
             + 0.10 * min(1.0, self.goods_access) + 0.10 * self.security

    def __repr__(self):
        return (f"Pop({self.pop_type} n={self.amount:.2f} w/c={self.wealth_per_capita():.2f} "
                f"lvl={self.wealth_level()} SoL={self.standard_of_living():.2f})")


# ---------------------------------------------------------------------------
# Location — thin harness that wires pops + districts + land together for one tick.
# (Not the final design; just enough to exercise the objects and match the doc's flow.)
# ---------------------------------------------------------------------------
class Location:
    def __init__(self, land_area=10.0, security=1.0, control=1.0, wage_d_crit=1.0):
        self.land_area = land_area
        self.security = security
        self.control = control
        self.wage_d_crit = wage_d_crit # labor-pressure falling point for the wage share (lower => wages fall earlier)
        self.pops = {}                 # pop_type -> Pop
        self.districts = []            # elite-owned districts
        self.subsistence = District(SUBSISTENCE, size=0.0)   # grows to fill unused capacity
        self.output = {}               # last-tick produced goods
        self.inputs_used = {}          # last-tick consumed inputs

    def add_pop(self, pop):     self.pops[pop.pop_type] = pop
    def add_district(self, d):  self.districts.append(d)

    def rural_labor(self):
        return sum(self.pops[t].amount for t in ("Slave", "Commoner") if t in self.pops)

    def tick(self):
        commoners = self.pops.get("Commoner")
        elites = self.pops.get("Elite")
        workers = self.rural_labor()

        # --- land: elite districts consume land; the remainder is free for subsistence ---
        used_land = sum(d.land_use() for d in self.districts)
        free_land = max(0.0, self.land_area - used_land)
        self.subsistence.size = free_land / self.subsistence.dtype.land_per_size

        all_districts = self.districts + [self.subsistence]
        subsistence_cap = self.subsistence.job_capacity()   # worker-owned outside option

        # --- labor allocation: split workers across districts weighted by job pull (pay) ---
        # wage_share depends on assigned workers (circular), so use a location-level wage share as the
        # weight for elite districts; subsistence weight uses wage_share 1.0 (workers keep everything).
        elite_jobs = sum(d.job_capacity() for d in self.districts)
        loc_wage_share = get_wage_share(workers, elite_jobs, subsistence_cap, d_crit=self.wage_d_crit) if elite_jobs > 0 else 0.0
        pulls = [d.job_pull(1.0 if d.dtype.worker_owned else loc_wage_share) for d in all_districts]
        total_pull = sum(pulls)
        for d, p in zip(all_districts, pulls):
            d.workers = workers * (p / total_pull) if total_pull > 0 else 0.0

        # --- production, input demand, wealth distribution (all RUNNING TOTALS this tick) ---
        totals, ins = {}, {}
        commoner_income = elite_income = 0.0
        for d in all_districts:
            for g, amt in d.produce().items():
                totals[g] = totals.get(g, 0.0) + amt
            for g, amt in d.input_demand().items():
                ins[g] = ins.get(g, 0.0) + amt
            wealth = d.produce().get("wealth", 0.0)
            if d.dtype.worker_owned:
                commoner_income += wealth                      # subsistence: workers keep all
            else:
                ws = d.wage_share(subsistence_cap, d_crit=self.wage_d_crit)
                commoner_income += wealth * ws                 # wages
                elite_income += wealth * (1.0 - ws)            # surplus to elite owners
        self.output, self.inputs_used = totals, ins

        # Pops' wealth is this tick's income (running total), NOT accumulated.
        if commoners is not None: commoners.set_wealth(commoner_income)
        if elites is not None:    elites.set_wealth(elite_income)

        # --- food & goods access (needs scale with wealth level) ---
        for pop in self.pops.values():
            fneed, gneed = pop.food_need(), pop.goods_need()
            pop.food_access = (totals.get("food", 0.0) / fneed) if fneed > 0 else 1.0
            pop.goods_access = (totals.get("goods", 0.0) / gneed) if gneed > 0 else 0.0
            pop.security = self.security

        return {"output": totals, "inputs": ins, "commoner_income": commoner_income,
                "elite_income": elite_income, "loc_wage_share": loc_wage_share}


# ---------------------------------------------------------------------------
# Demo / smoke test
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    loc = Location(land_area=10.0, security=1.0)
    loc.add_pop(Pop("Commoner", amount=8.0))
    loc.add_pop(Pop("Elite", amount=0.1))
    loc.add_district(District(FARM, size=3.0, tier=0, owner=loc.pops["Elite"]))
    loc.add_district(District(FARM, size=2.0, tier=1, owner=loc.pops["Elite"]))
    loc.add_district(District(PASTURE, size=2.0, tier=0, owner=loc.pops["Elite"]))

    print("District types:", list(DISTRICT_TYPES))
    print("Tiers of FARM (each a sub-type):")
    for tr in FARM.tiers:
        print("   ", tr)

    print("\nDistricts:")
    for d in loc.districts:
        print("  ", d, "| jobs", round(d.job_capacity(), 2), "| land", round(d.land_use(), 2),
              "| elite_opps", round(d.elite_opportunities(), 3))

    print("\nRunning 3 ticks (wealth is a running total, should be steady — NOT accumulating):")
    for t in range(3):
        r = loc.tick()
        o = r["output"]
        print(f" t{t}: food {o.get('food',0):.2f} wealth {o.get('wealth',0):.2f} "
              f"goods {o.get('goods',0):.2f} mat_in {r['inputs'].get('materials',0):.2f} "
              f"| wage_share {r['loc_wage_share']:.2f} | commoner_w {r['commoner_income']:.2f} "
              f"elite_w {r['elite_income']:.2f}")
    print("\nPops after (wealth = this month's running total):")
    for p in loc.pops.values():
        print("  ", p)

    print("\nSub-type change: upgrade the first farm to Mechanized (needs materials in)")
    f = loc.districts[0]; f.upgrade_tier(); f.upgrade_tier()
    print("  ", f, "| produce", {k: round(v, 2) for k, v in f.produce().items()},
          "| inputs", {k: round(v, 2) for k, v in f.input_demand().items()})
