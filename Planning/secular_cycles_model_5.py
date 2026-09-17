import math
import random
import numpy as np
import matplotlib.pyplot as plt

from economy import Location, Pop, Parcel, CULTIVATED


# Crises fire stochastically in strain/fracture only (hazard scales with a driver + phase). Config per type:
#   base = per-tick ignition rate at driver=1 in strain   dmin/dmax = duration range   cool = refractory ticks
# The VIOLENT set shares a father-son LULL (see _step_crises war-weariness hysteresis): after a burst of
# violence they're suppressed for a generation, so ignition rates can be high (they fire, then a lull enforces spacing).
CRISIS_CFG = {
    "famine":     dict(base=0.0, dmin=3, dmax=8,  cool=6),    # driver: immiseration
    "epidemic":   dict(base=0.0, dmin=4, dmax=10, cool=8),    # driver: immiseration + density
    "revolt":     dict(base=0.12, dmin=4, dmax=10, cool=10),   # driver: immiseration + unrest + low legitimacy
    "civil_war":  dict(base=0.10, dmin=8, dmax=20, cool=15),   # driver: overproduction + unrest
    "coup":       dict(base=0.08, dmin=1, dmax=3,  cool=12),   # driver: overproduction + low legitimacy
    "bankruptcy": dict(base=0.0, dmin=1, dmax=3,  cool=15),   # driver: unpaid army (army_shortfall)
}
VIOLENT = {"revolt", "civil_war", "coup"}   # share the war-weariness lull


# Secular-cycle causal loop (Turchin): carrying capacity -> population growth -> low wages ->
# upward mobility + elite reproduction -> elite overproduction -> fiscal stress -> falling legitimacy ->
# rising instability -> elite & population mortality -> wages recover + downward mobility ->
# instability falls -> state recovers -> population grows again.


def plot_phase_space(phase_history):
    # shade the background by phase: prosperity green, strain yellow, fracture red
    starts, ends, kinds = [0], [], []
    for x in range(len(phase_history) - 1):
        if phase_history[x] != phase_history[x + 1]:
            kinds.append(phase_history[x]); ends.append(x); starts.append(x + 1)
    kinds.append(phase_history[-1]); ends.append(len(phase_history))
    colors = {0: 'green', 1: 'yellow', 2: 'red'}
    for k, a, b in zip(kinds, starts, ends):
        plt.axvspan(a, b, color=colors[k], alpha=0.1)


def plot_simulation(P_history, E_history, U_history, S_history, phase_history):
    plt.figure(figsize=(24, 6))
    plt.yticks([i / 10 for i in range(11)])
    plt.plot(P_history, label="Population (P)", color='blue')
    plt.plot(E_history, label="Elite Overproduction (E)", color='orange')
    plt.plot(U_history, label="Instability (U)", color='red')
    plt.plot(S_history, label="State Capacity (S)", color='green')
    plot_phase_space(phase_history)
    plt.legend(); plt.title("Secular Cycle Simulation"); plt.show()


class FinalSim:
    """One abstract location running a Turchin secular cycle.
    phase: 0 = prosperity, 1 = strain, 2 = fracture (labels over continuous dynamics)."""

    def __init__(self):
        # ---- population & vital rates ----
        self.population = 0.6
        self.elites = 0.02
        self.land_area = 1.0
        self.land_productivity = 1.0          # raises carrying capacity as tech/land improve (static for now)
        self.max_birth = 0.030
        self.min_birth = 0.015
        self.death_base = 0.01
        self.child_mortality = 0.4
        self.cc_window = 100                  # ticks the pressure gauge averages carrying capacity over
        self.p_steepness = 3.0                # pressure-gauge slope; gentle so it stays in a Turchin-like band

        self.k_war = 0.0
        self.security = 1.0

        # attached economy: one elite-owned cultivated parcel; commoners work it and the wage emerges
        self.location = Location(land_area=self.land_area, security=1.0, wage_d_crit=0.9, wage_k=-10)
        self._commoner = Pop("Commoner", self.population)
        self._elite_owner = Pop("Elite", self.elites)
        self.location.add_pop(self._commoner)
        self.location.add_pop(self._elite_owner)
        self.location.add_parcel(Parcel(CULTIVATED, area=1.0, owner=self._elite_owner))
        self.wage_share = 0

        # ---- state fiscal (treasury, no debt) ----
        self.tax_rate = 0.25
        self.collection_floor = 1.0          # a delegitimized state still extracts some tax; without a floor,
                                              #   legitimacy 0 -> revenue 0 -> unpaid army -> legitimacy pinned at 0
        self.army_base = 0.006
        self.k_suppress = 0.2               # cost of holding down mobilization POTENTIAL (not active unrest):
                                              #   as elite overproduction mobilizes the populace, the cost of coercion
                                              #   balloons and DRAINS the treasury through strain toward the fiscal
                                              #   break -- this is the "immense cost -> state failure" chain link.
                                              #   (>0.032 bifurcates: strain balloons to dominate the cycle)
        self.suppress_reach = 5.0             # coercive reach per unit revenue (caps crisis intensity)
        self.mil_positions = 0.01             # officer corps -> baseline elite positions
        self.k_patronage = 5.0                # cost per patronage position
        self.k_absorb = 2.0                   # patronage absorbs a shrinking share of the surplus as it grows
        self.treasury_years = 5.0             # treasury cap, in years of gross revenue
        self.treasury = 0.0

        # ---- unrest ----
        self.R, self.M = 0.0, 0.0
        
        self.stress = 0.0
        self.hot = 0.0

        self.ibn_margin = 2.0
        self.u_fracture_on = 0.25             # strain -> fracture once violence erupts past this
        self.u_exit_thresh = 0.1             # fracture ends once violence lulls
        self.overprod_exit = 1.2              # ... and overproduction has cleared

        # ---- elite ledger (Turchin dE = r_e*E + mu_0*(w0-w)/w * N) ----
        self.r_e_premium = 0.02               # elite reproductive surplus over commoners in orderly times (main driver)
        self.stab_u_ref = 0.30                # violence at which reproduction & upward climbing are fully choked
        self.w0_target = 0.5                  # wage where elite up/down mobility balances (constant; needs tuning)
        self.mu_0 = 0.002                    # upward-mobility rate (x commoner pool) when wages sit below w0
        self.mu_down = 0.05                    # downward-mobility rate (x elite pool) when wages recover above w0
        self.k_attrition = 0.01               # rebellion culling rate (x U^2); gentle so a crisis doesn't wipe elites
        self.E_span = 2.0                     # surplus ratio that reads as E=1 on the display gauge
        
        # ---- legitimacy (relaxes toward a target = 100% minus insecurity, unrest, unpaid obligations, overproduction) ----
        self.legitimacy = 1.0
        self.sec_k_full = 1.0
        self.security = 1.0                   # order stock (= 1 - unrest); set at tick end, read at the next tick top
        self.legit_speed = 0.05               # relaxation rate toward the target
        self.legit_floor = 0.0               # target floor: only an event pushes legitimacy below this toward zero
        self.legit_insecurity = 0.15          # target drop from disorder (low security)
        self.legit_unrest = 0.30              # target drop from active violence (U_e)
        self.legit_unpaid_army = 0.50         # target drop from an unpaid army: the fiscal break's teeth
        self.legit_unpaid_patron = 0.25       # target drop from cut patronage
        self.legit_frag = 0.30                # target drop from elite overproduction (felt_overprod > 1.5)
        self.k_occupation = 0.0               # how much unrest degrades control, hence tax collection
        self.control_legit_floor = 1.0        # tax control retained at zero legitimacy (1.0 = revenue ignores legitimacy)
        
        # ---- crises / events (stochastic, seeded; only fire in strain/fracture, can overlap) ----
        self.seed = 12345
        self.rng = random.Random(self.seed)
        self.crisis_frac_mult = 4.0           # fracture makes crises ~this much more likely than strain
        self.crisis_hazard_cap = 0.5          # max per-tick ignition probability
        self.w_immis_thresh = 0.5             # wage share below which immiseration bites
        self.k_famine = 0.06                  # famine pop-death fraction per tick at intensity 1
        self.k_epidemic = 0.05                # epidemic pop-death fraction per tick at intensity 1
        self.revolt_unrest = 0.30             # unrest a revolt injects per tick at intensity 1
        self.civilwar_cull = 0.010            # elite-death fraction from civil war PER TICK (compounds over its duration)
        self.coup_cull = 0.04                 # elite-death fraction from a coup (targeted, short) per tick at intensity 1
        self.coup_legit_shock = 0.40          # legitimacy hit from a coup/regicide at intensity 1
        self.bankruptcy_legit_shock = 0.30    # legitimacy hit from fiscal bankruptcy at intensity 1
        # father-son lull: violent crises build war-weariness; past weary_high a generation-long lull begins
        # (active violence ends, none can ignite) until it bleeds back below weary_low -> the next wave can fire.
        self.weary_gain = 0.06                # war-weariness added per active violent crisis-tick (x intensity)
        self.weary_relax = 0.02               # war-weariness that bleeds off each tick (lull length = band/relax)
        self.weary_high = 0.7                 # weariness that triggers the lull (violence exhausts itself)
        self.weary_low = 0.4                  # weariness at which the lull lifts and violence can flare again
        self.war_weariness = 0.0
        self.in_lull = False
        self.crises = {name: {"active": False, "age": 0, "intensity": 0.0, "dur": 0, "cd": 0}
                       for name in CRISIS_CFG}
        self.immis = 0.0
        self.crisis_load = 0.0                # sum of active crisis intensities (quick gauge)
        self.crisis_pop_deaths = self.crisis_elite_deaths = 0.0

        # ---- gauges & read-outs (labels only; nothing feeds back off them) ----
        self.phase = 0
        self.P = self.E = self.U = self.U_e = 0.0
        self.S = self.legitimacy = 0.0
        self.birth_rate = self.death_rate = 0.0

        # ---- histories ----
        self.cc_history = []
        self.P_history, self.E_history, self.U_history = [], [], []
        self.U_e_history, self.S_history, self.phase_history = [], [], []
        self.wage_history, self.population_history, self.carrying_capacity_history, self.immiseration_history = [], [], [], []

        self.elites_history, self.positions_history = [], []
        self.e_reproduction_history, self.e_mobility_up_history, self.e_mobility_down_history, self.e_deaths_history = [], [], [], []

        self.legitimacy_history, self.revenue_history, self.expenses_history = [], [], []
        self.suppression_history, self.treasury_history = [], []

        self.crisis_history = {name: [] for name in CRISIS_CFG}   # per tick: intensity if active else 0
        self.war_weariness_history = []
        self.crisis_elite_deaths_history, self.crisis_pop_deaths_history = [], []


    # ------------------------------------------------------------------ vital rates
    def get_birth_rate(self, pop, cap, security):
        # high fertility well under cap, smoothstep decline as density climbs; low security shrinks the effective cap
        rel = pop / max(cap * security, 1e-8)
        t = np.clip((rel - 0.5) / (1.5 - 0.5), 0.0, 1.0)
        t = t * t * (3 - 2 * t)
        #t = min(min(t, self.U_e*10), 1.0)
        return max(self.max_birth - (self.max_birth - self.min_birth) * t, self.min_birth)

    def get_death_rate(self, pop, cap, birth_rate, war_severity):
        # baseline mortality (incl. child mortality) + war deaths; famine/disease deferred
        rel = pop / max(cap, 1e-8)
        base = self.death_base + self.child_mortality * birth_rate
        return base + rel ** 1.2 * war_severity * self.k_war

    # ------------------------------------------------------------------ tick
    def step(self, t):
        if t == 0:
            self.rng = random.Random(self.seed)          # reseed so simulate(seed=...) overrides take effect
        cc_eff = self._step_population()
        econ = self._step_economy()
        f = self._step_fiscal(econ, 0)
        self._step_legitimacy(f)
        self._step_unrest(econ, f)
        self._step_crises(f)                             # stochastic events layer on top of the continuous cycle
        w0 = self._step_elites(econ["w"], f)
        self._step_gauges(cc_eff, f)
        self._step_phase(econ["w"], w0, f)
        self._record(cc_eff, f)

    def _step_population(self):
        self.security = min(self.security + 0.02, 1.0 - self.U_e)
        self.security = max(self.security, 0.5)
        #self.security = 1.0 - self.U_e * 0.5

        cap = self.land_area * self.land_productivity
        self.cc_history.append(cap)
        if len(self.cc_history) > self.cc_window:
            self.cc_history.pop(0)
        cc_eff = max(cap, sum(self.cc_history) / len(self.cc_history))   # sticky cap for the pressure gauge
        self.birth_rate = self.get_birth_rate(self.population, cap, self.security)
        self.death_rate = self.get_death_rate(self.population, cap, self.birth_rate, war_severity=self.U_e)
        self.population += (self.birth_rate - self.death_rate) * self.population
        return cc_eff
    
    def _step_economy(self):
        # commoners work the land; read out Turchin's relative wage w and the inverse relative elite income
        self._commoner.amount = max(self.population, 1e-8)
        self.location.security = self.security
        econ = self.location.tick()
        self.wage_share = econ["loc_wage_share"]
        Wc, Nc = self._commoner.wealth, self._commoner.amount
        We, Ne = self._elite_owner.wealth, self._elite_owner.amount
        gdp_pc = (Wc + We) / max(Nc + Ne, 1e-8)
        w = min(max((Wc / max(Nc, 1e-8)) / max(gdp_pc, 1e-8), 1e-4), 1.0)
        elite_income_pc = We / max(Ne, 1e-9)
        ew_inverse = 1.0 / max(elite_income_pc / max(gdp_pc, 1e-9), 1e-6)   # rises as the elite pie splits thin
        # immiseration = how far commoners fall below a decent living: low wage share + food shortfall
        wage_penalty = max(0.0, (self.w_immis_thresh - self.wage_share) / self.w_immis_thresh)
        food_penalty = max(0.0, 1.0 - self._commoner.food_access)
        self.immis = min(1.0, wage_penalty + food_penalty)
        self.w = w                                       # instrument: relative wage (falling wages -> immiseration)
        self.ew_inverse = ew_inverse                     # instrument: inverse relative elite income (thin pie)
        self.elite_income_pc = elite_income_pc           # instrument: elite income per head
        return {"w": w, "ew_inverse": ew_inverse, "elite_income": econ["elite_income"],
                "commoner_wealth": self._commoner.wealth,
                "wealth_pc": self._commoner.wealth_per_capita(), "food_access": self._commoner.food_access}

    def _step_fiscal(self, econ, stress):
        # tax the wealth base (collection gated by control), fund army then patronage, run a capped treasury
        total_income = econ["commoner_wealth"] + econ["elite_income"]
        collection = 1.0 - self.U_e * 0.5
        revenue = self.tax_rate * total_income * collection

        baseline_positions = self.location.elite_opportunities() + self.mil_positions
        overproduction = self.elites / max(baseline_positions, 1e-9)
        excess_elites = max(0.0, self.elites - baseline_positions)
        absorb_fraction = 1.0 / (1.0 + self.k_absorb * max(0.0, overproduction - 1.0))  # patronage can't keep pace
        desired_patronage = excess_elites * absorb_fraction * self.k_patronage
        # the state pays to hold down mobilization POTENTIAL, so overproduction drains even a solvent treasury
        army_cost = self.army_base + self.k_suppress * self.U_e * self.population

        funds = self.treasury + revenue
        army_paid = min(army_cost, funds)                                   # army is paid before patronage
        patronage_paid = max(0.0, min(desired_patronage, funds - army_paid))
        patronage_jobs = patronage_paid / max(self.k_patronage, 1e-9)
        army_shortfall = max(0.0, army_cost - army_paid) / max(army_cost, 1e-9)
        patronage_shortfall = max(0.0, desired_patronage - patronage_paid) / max(desired_patronage, 1e-9)
        # Coercive reach = the army the state can actually PAY. While the treasury holds reserves it funds a
        # suppression bill bigger than current revenue; once empty, army_paid caps at revenue and any shortfall
        # collapses coercion -> unrest erupts (the fiscal break, on the ground). Treasury depth = how long it holds.
        suppression_capacity = self.suppress_reach * army_paid

        max_treasury = self.treasury_years * self.tax_rate * total_income
        self.treasury = min(max(self.treasury + revenue - army_paid - patronage_paid, 0.0), max_treasury)

        self.revenue = revenue                           # instrument: state income
        self.expenses = army_paid + patronage_paid       # instrument: total outlay (army + patronage)
        self.suppression = suppression_capacity          # instrument: coercive reach (subtracted from U)
        self.suppression_cost = self.k_suppress * self.U_e * self.population  # cost of holding down potential
        self.patronage_paid = patronage_paid             # instrument: patronage burden actually funded
        self.army_shortfall = army_shortfall             # instrument: unpaid army (state failure)

        # felt overproduction is vs FUNDED positions: patronage placates the surplus but collapses to baseline
        # the moment it goes unfunded -> the fiscal trap
        elite_positions = max(baseline_positions + patronage_jobs, 1e-6)
        #elite_positions = baseline_positions
        self.felt_overproduction = self.elites / elite_positions            # stored for next tick's alpha
        return {"revenue": revenue, "baseline_positions": baseline_positions, "overproduction": overproduction,
                "elite_positions": elite_positions, "felt_overproduction": self.felt_overproduction,
                "suppression_capacity": suppression_capacity, "army_shortfall": army_shortfall,
                "patronage_shortfall": patronage_shortfall}
    
    def _step_legitimacy(self, f):
        # Legitimacy relaxes toward a target that starts at 100% and is pulled down by insecurity, active
        # unrest, unpaid obligations (army + patronage), and elite overproduction. Floored above zero by
        # legit_floor -- only an event pushes it toward zero. Recovers on its own in prosperity as stressors clear.
        insecurity = max(0.0, (self.sec_k_full - self.security) / self.sec_k_full)
        target = (1.0
                  - self.legit_insecurity    * insecurity
                  - self.legit_unrest        * self.U_e
                  - self.legit_unpaid_army   * f["army_shortfall"]
                  - self.legit_unpaid_patron * f["patronage_shortfall"]
                  - self.legit_frag          * max(0.0, f["felt_overproduction"] - 1.5))
        target = max(self.legit_floor, min(1.0, target))
        self.legitimacy += self.legit_speed * (target - self.legitimacy)
        self.legitimacy = max(0.0, min(1.0, self.legitimacy))
        
    def _step_unrest(self, econ, f):
        #alpha = alpha_0 + alpha_w * (self.w0_target - w) + alpha_e * (self.elites - f["elite_positions"])

        if self.phase != 2:
            self.U += 0.1 * (self.w0_target - econ["w"]) + 0.4 * (self.elites - 2.0 * f["elite_positions"])
        else:
            self.U += 0.05 + 0.1 * (self.w0_target - econ["w"]) + 1.0 * (self.elites - (2 * self.U_e + 1.0) * f["elite_positions"])
            if self.felt_overproduction < 1.1:
                self.U -= 0.1
            #self.U = min(self.U, 0.5)
        # unrest_target = 0

        # mmp = 1.0 / max(econ["w"], 1e-4)
        # emp = 5.0 * (self.elites - f["elite_positions"]) / f["elite_positions"] * econ["ew_inverse"]

        self.U = min(1.0, max(0.0, self.U))

        self.U_e = self.U - 1.0 * f["suppression_capacity"]
        self.U_e = min(1.0, max(0.0, self.U_e))

    # ------------------------------------------------------------------ crises / events
    def _crisis_drivers(self, f):
        # each driver is a 0-1(ish) pressure; hazard = base * driver * phase_mult
        op = max(0.0, f["felt_overproduction"] - 1.0)                 # elite overproduction pressure
        density = max(0.0, self.population / max(self.land_area, 1e-9) - 0.8)
        low_legit = 1.0 - self.legitimacy
        return {
            "famine":     self.immis,
            "epidemic":   0.5 * self.immis + 0.5 * density,
            "revolt":     0.4 * self.immis + 0.4 * self.U_e + 0.3 * low_legit,
            "civil_war":  0.5 * op + 0.5 * self.U_e,
            "coup":       0.5 * op + 0.5 * low_legit,
            "bankruptcy": f["army_shortfall"],
        }

    def _step_crises(self, f):
        self.crisis_pop_deaths = self.crisis_elite_deaths = 0.0
        if self.phase == 0:                                          # no crises in prosperity; wind everything down
            for cr in self.crises.values():
                cr["active"] = False
                cr["cd"] = max(0, cr["cd"] - 1)
            self.war_weariness = max(0.0, self.war_weariness - self.weary_relax)
            self.in_lull = self.in_lull and self.war_weariness > self.weary_low
            self.crisis_load = 0.0
            return
        pop0, el0 = self.population, self.elites
        drivers = self._crisis_drivers(f)
        phase_mult = 1.0 if self.phase == 1 else self.crisis_frac_mult   # strain survivable, fracture fatal

        # father-son hysteresis: past weary_high a LULL starts (active violence ends, none can ignite);
        # it lifts once weariness bleeds back below weary_low -> the next generation's wave can fire.
        if not self.in_lull and self.war_weariness >= self.weary_high:
            self.in_lull = True
            for name in VIOLENT:
                cr = self.crises[name]
                if cr["active"]:
                    cr["active"] = False; cr["cd"] = CRISIS_CFG[name]["cool"]
        elif self.in_lull and self.war_weariness <= self.weary_low:
            self.in_lull = False

        for name, cr in self.crises.items():
            cfg = CRISIS_CFG[name]
            if cr["active"]:
                cr["age"] += 1
                self._apply_crisis(name, cr["intensity"])
                if cr["age"] >= cr["dur"]:
                    cr["active"] = False; cr["cd"] = cfg["cool"]
            elif cr["cd"] > 0:
                cr["cd"] -= 1
            elif name in VIOLENT and self.in_lull:
                continue                                            # lull suppresses new violent crises
            else:
                drive = drivers[name]
                p = min(self.crisis_hazard_cap, cfg["base"] * drive * phase_mult)
                if drive > 0.0 and self.rng.random() < p:
                    cr["active"] = True; cr["age"] = 0
                    cr["intensity"] = min(1.0, drive * (0.5 if self.phase == 1 else 1.0))  # milder in strain
                    cr["dur"] = self.rng.randint(cfg["dmin"], cfg["dmax"])
                    self._apply_crisis(name, cr["intensity"])

        violent_load = sum(cr["intensity"] for n, cr in self.crises.items() if n in VIOLENT and cr["active"])
        self.war_weariness = max(0.0, self.war_weariness - self.weary_relax) + self.weary_gain * violent_load
        self.crisis_load = sum(cr["intensity"] for cr in self.crises.values() if cr["active"])
        self.crisis_pop_deaths = max(0.0, pop0 - self.population)
        self.crisis_elite_deaths = max(0.0, el0 - self.elites)

    def _apply_crisis(self, name, x):
        # x = intensity in (0,1]; effects are per active tick. Revolt/civil-war unrest lands in next tick's U_e.
        if name == "famine":
            self.population *= (1.0 - self.k_famine * x)
        elif name == "epidemic":
            self.population *= (1.0 - self.k_epidemic * x)
        elif name == "revolt":
            self.U = min(1.0, self.U + self.revolt_unrest * x)
            self.population *= (1.0 - 0.1 * self.k_famine * x)
            self.elites = max(1e-6, self.elites * (1.0 - 0.3 * self.civilwar_cull * x))
        elif name == "civil_war":
            self.elites = max(1e-6, self.elites * (1.0 - self.civilwar_cull * x))
            self.population *= (1.0 - 0.2 * self.k_famine * x)
            self.treasury *= (1.0 - 0.1 * x)
            self.U = min(1.0, self.U + 0.5 * self.revolt_unrest * x)
        elif name == "coup":
            self.elites = max(1e-6, self.elites * (1.0 - self.coup_cull * x))
            self.legitimacy = max(0.0, self.legitimacy - self.coup_legit_shock * x)
        elif name == "bankruptcy":
            self.treasury = 0.0
            self.legitimacy = max(0.0, self.legitimacy - self.bankruptcy_legit_shock * x)

    def _step_elites(self, w, f):
        # build by reproduction + upward mobility (both need order), clear by rebellion culling + downward mobility
        elite_count = self.elites
        commoner_N = self._commoner.amount
        positions = f["elite_positions"]

        # w0: wage at which up/down mobility balances
        w0 = self.w0_target

        # only elites WITH positions found surplus lineages (surplus cadets fail -- primogeniture, monastic
        # dumping); the premium is ~0 without order/prosperity. Elite r ~ commoner r; the buildup is mobility-led.
        #reproducing = min(elite_count, positions)
        reproducing = elite_count * self.legitimacy
        e_biological = reproducing * self.r_e_premium

        misery = (w0 - w) / max(w, 1e-9)
        if misery < 0.0:
            underprod = min(1.0, elite_count / max(positions, 1e-9))  # few elites left -> curb demotion
            e_mobility = elite_count * self.mu_down * misery * underprod   # wages recovered -> elites demote
        else:
            e_mobility = commoner_N * self.mu_0 * misery

        # civil-war/purge culling
        e_attrition = elite_count * self.k_attrition * self.U_e ** 1.5

        self.elites = max(elite_count + e_biological + e_mobility - e_attrition, 1e-6)
        self._elite_owner.amount = self.elites
        self.w0 = w0                                   # readout: the mobility zero-point (w<w0 => climbing in)
        # instrument the elite ledger flows (signed, per tick) so the chain is measurable
        self.elite_births = e_biological               # reproduction inflow
        self.elite_up = max(0.0, e_mobility)           # upward mobility inflow (commoners -> elites)
        self.elite_down = max(0.0, -e_mobility)        # downward mobility outflow (elites -> commoners)
        self.elite_cull = e_attrition                  # rebellion/purge mortality outflow
        self.elite_net = e_biological + e_mobility - e_attrition
        return w0
    
    def _step_gauges(self, cc_eff, f):
        # display gauges (nothing feeds off them) + the order stock read at the next tick top
        rel = self.population / max(cc_eff, 1e-8)
        self.P = 1.0 / (1.0 + math.exp(-self.p_steepness * (rel - 1.0)))
        
        self.overprod_ratio = self.elites / max(f["elite_positions"], 1e-8)
        self.E = max(0.0, min(1.0, max(0.0, self.overprod_ratio - 1.0) / self.E_span))

        self.S = self.legitimacy

    def _step_phase(self, w, w0, f):
        # concrete transitions on how society is actually functioning (labels over the dynamics; no resets)
        if self.phase == 0:
            traditional = (w < w0) and (self.elites > f["baseline_positions"])
            ibn_khaldun = f["overproduction"] > self.ibn_margin
            if traditional or ibn_khaldun:
                self.phase = 1
        elif self.phase == 1:
            fiscal_break = (self.treasury <= 1e-9) and (f["army_shortfall"] > 0.0)
            if self.U_e > self.u_fracture_on or fiscal_break:          # violence erupting IS the strain->fracture tip
                self.phase = 2
        elif self.phase == 2:
            if f["overproduction"] < self.overprod_exit and self.U_e < self.u_exit_thresh:
                self.phase = 0

    def _record(self, cc_eff, f):
        self.phase_history.append(self.phase)
        self.P_history.append(self.P)
        self.E_history.append(self.E)
        self.U_history.append(self.U)
        self.U_e_history.append(self.U_e)
        self.S_history.append(self.S)

        self.population_history.append(self.population)
        self.wage_history.append(self.wage_share)
        self.carrying_capacity_history.append(cc_eff)
        self.immiseration_history.append(self.immis)

        self.elites_history.append(self.elites)
        self.positions_history.append(f["elite_positions"])
        self.e_reproduction_history.append(self.elite_births)
        self.e_mobility_up_history.append(self.elite_up)
        self.e_mobility_down_history.append(self.elite_down)
        self.e_deaths_history.append(self.elite_cull + self.crisis_elite_deaths)  # continuous + crisis (waves show here)
        self.crisis_elite_deaths_history.append(self.crisis_elite_deaths)
        self.crisis_pop_deaths_history.append(self.crisis_pop_deaths)

        self.legitimacy_history.append(self.legitimacy)
        self.revenue_history.append(self.revenue)
        self.expenses_history.append(self.expenses)
        self.suppression_history.append(self.suppression)
        self.treasury_history.append(self.treasury)

        for name, cr in self.crises.items():
            self.crisis_history[name].append(cr["intensity"] if cr["active"] else 0.0)
        self.war_weariness_history.append(self.war_weariness)
        