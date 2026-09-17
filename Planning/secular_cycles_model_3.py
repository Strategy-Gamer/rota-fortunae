import math
import numpy as np
import matplotlib.pyplot as plt

from economy import Location, Pop, Parcel, CULTIVATED


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
        self.elites = 0.006
        self.land_area = 1.0
        self.land_productivity = 1.0          # raises carrying capacity as tech/land improve (static for now)
        self.max_birth = 0.04
        self.min_birth = 0.015
        self.death_base = 0.01
        self.child_mortality = 0.4
        self.k_war = 0.08                     # war-death weight; sized for a ~1/3 population crash at peak crisis
        self.security_floor = 0.62            # order never fully collapses; also the post-crisis birth-capacity floor
        self.p_steepness = 3.0                # pressure-gauge slope; gentle so it stays in a Turchin-like band
        self.cc_window = 100                  # ticks the pressure gauge averages carrying capacity over

        # attached economy: one elite-owned cultivated parcel; commoners work it and the wage emerges
        self.location = Location(land_area=self.land_area, security=1.0, wage_d_crit=0.9)
        self._commoner = Pop("Commoner", self.population)
        self._elite_owner = Pop("Elite", self.elites)
        self.location.add_pop(self._commoner)
        self.location.add_pop(self._elite_owner)
        self.location.add_parcel(Parcel(CULTIVATED, area=1.0, owner=self._elite_owner))

        # ---- state fiscal (treasury, no debt) ----
        self.tax_rate = 0.25
        self.collection_floor = 0.25          # a delegitimized state still extracts some tax; without a floor,
                                              #   legitimacy 0 -> revenue 0 -> unpaid army -> legitimacy pinned at 0
        self.army_base = 0.006
        self.k_suppress = 0.05                # cost of policing rebellion; drains the treasury toward the fiscal break
        self.suppress_reach = 6.0             # coercive reach per unit revenue (see convex suppression)
        self.mil_positions = 0.01             # officer corps -> baseline elite positions
        self.k_patronage = 2.0                # cost per patronage position
        self.k_absorb = 2.0                   # patronage absorbs a shrinking share of the surplus as it grows
        self.treasury_years = 5.0             # treasury cap, in years of gross revenue
        self.treasury = 0.0

        # ---- elite ledger (Turchin dE = r_e*E + mu_0*(w0-w)/w * N) ----
        self.r_e_premium = 0.02               # elite reproductive surplus over commoners in orderly times (main driver)
        self.stab_u_ref = 0.30                # violence at which reproduction & upward climbing are fully choked
        self.mu_0 = 0.0004                    # upward-mobility rate (x commoner pool) when wages sit below w0
        self.mu_down = 0.006                  # downward-mobility rate (x elite pool) when wages recover above w0
        self.k_attrition = 0.25               # rebellion culling rate (x U^2); low so a sequence of waves, not a wipe
        self.E_span = 2.0                     # surplus ratio that reads as E=1 on the display gauge

        # ---- radicalization SIR (naive -> radical -> moderate -> naive): the father-son oscillator ----
        self.rad_N, self.rad_R, self.rad_M = 1.0, 0.0, 0.0
        self.sigma_0 = 0.003                  # spontaneous seed: a wave always eventually fires once immunity wanes
        self.gamma = 3.0                      # moderates gate recruitment; contagion runs only while alpha > gamma*M
        self.rad_burnout = 0.3                # radicals -> moderates; keeps the SIR oscillating rather than simmering
        self.rad_wane = 0.09                  # immunity waning; roughly sets the father-son period

        # ---- grievance kindling (alpha); overproduction-driven this pass ----
        self.alpha_0 = 0.0
        self.alpha_w = 0.0                    # mass-immiseration weight (off: needs its own wage reference)
        self.alpha_e = 4.0                    # overproduction weight on the signed gap, so alpha can go negative
        self.overprod_tol = 1.4               # e_0: below this alpha < 0 (social peace) so the surplus builds calmly
        self.ibn_margin = 2.0                 # Ibn-Khaldun strain entry: extreme overproduction even if wages are ok

        # ---- rebellion gate + legitimacy ----
        self.rebel_thresh_base = 0.85         # radical share needed to rebel at full legitimacy
        self.rebel_thresh_floor = 0.03        # ... at zero legitimacy: a riot turns revolutionary
        self.rebel_gain = 6.0                 # scales the legitimacy-gated radical excess into rebellion pressure
        self.u_exit_thresh = 0.20             # fracture ends only once violence decays to a lull

        self.legitimacy = 1.0
        self.security = 1.0                   # order stock (= 1 - unrest); set at tick end, read at the next tick top
        self.rebel_pressure = 0.0             # last tick's rebellion; the army budgets its suppression against it
        self.legit_recover = 0.03
        self.legit_cohere = 0.02              # extra healing when elites are under their positions (cooperation)
        self.legit_unpaid_army = 0.30         # erosion from an unpaid army: the fiscal break's teeth
        self.legit_unpaid_patron = 0.06       # erosion from cut patronage
        self.legit_insecurity = 0.20          # erosion from disorder (low security)
        self.legit_frag = 0.12                # erosion from felt overproduction (rival elites withdraw cooperation)
        self.k_occupation = 0.6               # how much unrest degrades control, hence tax collection

        # ---- gauges & read-outs (labels only; nothing feeds back off them) ----
        self.phase = 0
        self.P = self.E = self.U = self.U_e = 0.0
        self.S = self.legitimacy
        self.alpha = self.conditions = 0.0
        self.overprod_ratio = 0.0
        self.birth_rate = self.death_rate = 0.0

        # ---- histories ----
        self.cc_history = []
        self.P_history, self.E_history, self.U_history = [], [], []
        self.U_e_history, self.S_history, self.phase_history = [], [], []
        self.wage_history, self.wealth_pc_history, self.food_ratio_history = [], [], []
        self.elite_income_history, self.commoner_wealth_history = [], []
        self.treasury_history, self.revenue_history = [], []
        self.legitimacy_history, self.rad_R_history = [], []

    # ------------------------------------------------------------------ vital rates
    def get_birth_rate(self, pop, cap, security):
        # high fertility well under cap, smoothstep decline as density climbs; low security shrinks the effective cap
        rel = pop / max(cap * security, 1e-8)
        t = np.clip((rel - 0.75) / (1.5 - 0.75), 0.0, 1.0)
        t = t * t * (3 - 2 * t)
        return max(self.max_birth - (self.max_birth - self.min_birth) * t, self.min_birth)

    def get_death_rate(self, pop, cap, birth_rate, war_severity):
        # baseline mortality (incl. child mortality) + war deaths; famine/disease deferred
        rel = pop / max(cap, 1e-8)
        base = self.death_base + self.child_mortality * birth_rate
        return base + rel ** 1.2 * war_severity * self.k_war

    # ------------------------------------------------------------------ tick
    def step(self, t):
        cc_eff = self._step_population()
        econ = self._step_economy()
        f = self._step_fiscal(econ)
        self._step_legitimacy(f)
        w0 = self._step_elites(econ["w"], f)
        alpha = self._step_instability(econ["w"], w0, f)
        self._step_gauges(cc_eff, f, alpha)
        self._step_phase(econ["w"], w0, alpha, f)
        self._record(econ, f)

    def _step_population(self):
        # security (order) is last tick's value; low order suppresses births -> the crisis population crash
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
        # commoners work the land; read out Turchin's relative wage w = commoner wage / GDP-per-capita
        self._commoner.amount = max(self.population, 1e-8)
        self.location.security = self.security
        econ = self.location.tick()
        Wc, Nc = self._commoner.wealth, self._commoner.amount
        We, Ne = self._elite_owner.wealth, self._elite_owner.amount
        gdp_pc = (Wc + We) / max(Nc + Ne, 1e-8)
        w = min(max((Wc / max(Nc, 1e-8)) / max(gdp_pc, 1e-8), 1e-4), 1.0)
        return {"w": w, "elite_income": econ["elite_income"], "commoner_wealth": self._commoner.wealth,
                "wealth_pc": self._commoner.wealth_per_capita(), "food_access": self._commoner.food_access}

    def _step_fiscal(self, econ):
        # tax the wealth base (collection gated by control), fund army then patronage, run a capped treasury
        total_income = econ["commoner_wealth"] + econ["elite_income"]
        control = max(0.0, min(1.0, self.legitimacy * (1.0 - self.k_occupation * self.U_e)))  # a realm in revolt can't tax
        collection = self.collection_floor + (1.0 - self.collection_floor) * control
        revenue = self.tax_rate * total_income * collection

        baseline_positions = self.location.elite_opportunities() + self.mil_positions
        overproduction = self.elites / max(baseline_positions, 1e-9)
        excess_elites = max(0.0, self.elites - baseline_positions)
        absorb_fraction = 1.0 / (1.0 + self.k_absorb * max(0.0, overproduction - 1.0))  # patronage can't keep pace
        desired_patronage = excess_elites * absorb_fraction * self.k_patronage
        army_cost = self.army_base + self.k_suppress * self.rebel_pressure  # policing last tick's rebellion (one-tick lag)

        funds = self.treasury + revenue
        army_paid = min(army_cost, funds)                                   # army is paid before patronage
        patronage_paid = max(0.0, min(desired_patronage, funds - army_paid))
        patronage_jobs = patronage_paid / max(self.k_patronage, 1e-9)
        army_shortfall = max(0.0, army_cost - army_paid) / max(army_cost, 1e-9)
        patronage_shortfall = max(0.0, desired_patronage - patronage_paid) / max(desired_patronage, 1e-9)
        # coercive reach scales with sustainable revenue, not the treasury, so it shrinks smoothly during a crisis
        suppression_capacity = self.suppress_reach * revenue

        max_treasury = self.treasury_years * self.tax_rate * total_income
        self.treasury = min(max(self.treasury + revenue - army_paid - patronage_paid, 0.0), max_treasury)

        # felt overproduction is vs FUNDED positions: patronage placates the surplus but collapses to baseline
        # the moment it goes unfunded -> the fiscal trap
        elite_positions = max(baseline_positions + patronage_jobs, 1e-6)
        return {"revenue": revenue, "baseline_positions": baseline_positions, "overproduction": overproduction,
                "elite_positions": elite_positions, "felt_overproduction": self.elites / elite_positions,
                "suppression_capacity": suppression_capacity, "army_shortfall": army_shortfall,
                "patronage_shortfall": patronage_shortfall}

    def _step_legitimacy(self, f):
        # heals in calm and with elite under-production; erodes from unpaid obligations, disorder, overproduction
        cohere = self.legit_cohere * max(0.0, 1.0 - f["felt_overproduction"])
        erosion = (self.legit_unpaid_army * f["army_shortfall"]
                   + self.legit_unpaid_patron * f["patronage_shortfall"]
                   + self.legit_insecurity * max(0.0, 1.0 - self.security)
                   + self.legit_frag * max(0.0, f["felt_overproduction"] - self.overprod_tol))
        self.legitimacy += self.legit_recover * (1.0 - self.legitimacy) + cohere - erosion
        self.legitimacy = max(0.0, min(1.0, self.legitimacy))

    def _step_elites(self, w, f):
        # build by reproduction + upward mobility (both need order), clear by rebellion culling + downward mobility
        elite_count = self.elites
        commoner_N = self._commoner.amount
        positions = f["elite_positions"]

        # w0: wage at which up/down mobility balances (placeholder fill-ratio; Piece 1 ties it to elite living standard)
        if elite_count < positions:
            w0 = 0.5 + 0.5 * (elite_count / positions)
        else:
            w0 = 0.5 - 0.1 * ((elite_count - positions) / positions)

        stability = max(0.0, 1.0 - self.U_e / self.stab_u_ref)      # chokes reproduction & climbing amid violence
        e_biological = elite_count * self.r_e_premium * stability

        misery = (w0 - w) / max(w, 1e-9)
        if misery >= 0.0:
            e_mobility = commoner_N * self.mu_0 * misery * stability  # commoners climb (aspiration also needs order)
        else:
            underprod = min(1.0, elite_count / max(positions, 1e-9))  # few elites left -> curb demotion
            e_mobility = elite_count * self.mu_down * misery * underprod

        e_attrition = elite_count * self.k_attrition * self.U_e * self.U_e  # convex: civil war culls, banditry barely

        self.elites = max(elite_count + e_biological + e_mobility - e_attrition, 1e-6)
        self._elite_owner.amount = self.elites
        return w0

    def _step_instability(self, w, w0, f):
        # alpha = grievance kindling on the signed gap (felt_op - tol); goes negative in social peace -> no waves
        alpha = (self.alpha_0
                 + self.alpha_w * max(0.0, w0 - w) / max(w, 1e-9)
                 + self.alpha_e * (f["felt_overproduction"] - self.overprod_tol))

        # SIR flows; contagion runs only past the excitable gate alpha > gamma*M, giving separated father-son bursts
        N, R, M = self.rad_N, self.rad_R, self.rad_M
        contagion = max(0.0, alpha - self.gamma * M) * R
        to_radical = (self.sigma_0 + contagion) * N
        to_moderate = self.rad_burnout * R
        to_naive = self.rad_wane * M
        N += to_naive - to_radical
        R += to_radical - to_moderate
        M += to_moderate - to_naive
        N, R, M = max(0.0, N), max(0.0, R), max(0.0, M)
        tot = N + R + M
        if tot > 1e-9:
            N, R, M = N / tot, R / tot, M / tot
        self.rad_N, self.rad_R, self.rad_M = N, R, M

        # rebellion = radicals past the legitimacy-set bar; unrest = the rebellion the army fails to suppress.
        # Convex suppression U = P^2/(P+C): small rebellions crushed, large ones leak (relative grip falls with size)
        rebel_thresh = self.rebel_thresh_floor + (self.rebel_thresh_base - self.rebel_thresh_floor) * self.legitimacy
        self.rebel_pressure = max(0.0, R - rebel_thresh) * self.rebel_gain
        P = self.rebel_pressure
        self.U_e = min(1.0, P * P / (P + f["suppression_capacity"] + 1e-9))
        return alpha

    def _step_gauges(self, cc_eff, f, alpha):
        # display gauges (nothing feeds off them) + the order stock read at the next tick top
        rel = self.population / max(cc_eff, 1e-8)
        self.P = 1.0 / (1.0 + math.exp(-self.p_steepness * (rel - 1.0)))

        self.overprod_ratio = self.elites / max(f["baseline_positions"], 1e-8)
        self.E = max(0.0, min(1.0, max(0.0, self.overprod_ratio - 1.0) / self.E_span))
        self.U = self.U_e
        self.security = max(self.security_floor, 1.0 - self.U_e * 0.5)  # suppression already folded into U_e

        self.alpha = alpha
        self.conditions = min(1.0, alpha)
        self.S = self.legitimacy

    def _step_phase(self, w, w0, alpha, f):
        # concrete transitions on how society is actually functioning (labels over the dynamics; no resets)
        moderation = self.gamma * self.rad_M                          # waves take off only when alpha exceeds this
        if self.phase == 0:
            traditional = (w < w0) and (self.elites > f["baseline_positions"])
            ibn_khaldun = f["overproduction"] > self.ibn_margin
            if traditional or ibn_khaldun:
                self.phase = 1
        elif self.phase == 1:
            takeoff = alpha > moderation
            fiscal_break = (self.treasury <= 1e-9) and (f["army_shortfall"] > 0.0)
            if takeoff or fiscal_break:
                self.phase = 2
        elif self.phase == 2:
            if (f["felt_overproduction"] < self.overprod_tol
                    and self.U_e < self.u_exit_thresh
                    and alpha < moderation):
                self.phase = 0

    def _record(self, econ, f):
        self.P_history.append(self.P)
        self.E_history.append(self.E)
        self.U_history.append(self.U)
        self.U_e_history.append(self.U_e)
        self.S_history.append(self.S)
        self.phase_history.append(self.phase)
        self.wage_history.append(econ["w"])
        self.wealth_pc_history.append(econ["wealth_pc"])
        self.food_ratio_history.append(econ["food_access"])
        self.elite_income_history.append(econ["elite_income"])
        self.commoner_wealth_history.append(econ["commoner_wealth"])
        self.treasury_history.append(self.treasury)
        self.revenue_history.append(f["revenue"])
        self.legitimacy_history.append(self.legitimacy)
        self.rad_R_history.append(self.rad_R)


if __name__ == "__main__":
    sim = FinalSim()
    for t in range(500):
        sim.step(t)
    # plot_simulation(sim.P_history, sim.E_history, sim.U_e_history, sim.S_history, sim.phase_history)
