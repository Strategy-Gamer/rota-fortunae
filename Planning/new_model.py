import math
import numpy as np
import matplotlib.pyplot as plt
import random

from economy import Location, Pop, District, FARM

# Carrying Capacity -> Increase in Population -> 
# Reduction of wages (population pressure is highly correlated to inverse wages) -> 
# increase of social mobility into elites + MMP from low wages -> elite numbers -> 
# elite overproduction -> state income stress for supporting elites -> 
# falling elite income + rise in conspicuous consumption (so the barrier to being considered an elite is higher) -> 
# elite fragmentation and EMP -> falling state legitimacy as it fails to pay elites -> fall in state capacity -> 
# increase in PSI -> Instability -> Elite, Population mortality, erosion of state capacity -> more violence -> 
# drop in population & elites -> increase of wages -> decrease of elite incomes + downward mobility -> lower sustained violence -> 
# State comes back -> Elite overproduction stops -> Elite fragmentation stops -> violence stops -> population allowed to grow again.

def plot_phase_space(phase_history=[]):
    # build phase space plot to color the background based on the phase of the system
    # Phase 0 = prosperity (green), Phase 1 = strain (yellow), Phase 2 = fracture (red)
    phase_types = []
    phase_starts = [0]
    phase_ends = []
    for x in range(0, len(phase_history)-1):
        if phase_history[x] != phase_history[x+1]:
            phase_types.append(phase_history[x])
            phase_ends.append(x)
            phase_starts.append(x+1)
    phase_types.append(phase_history[-1])
    phase_ends.append(len(phase_history))
    
    for x in range(len(phase_types)):
        if phase_types[x] == 0:
            plt.axvspan(phase_starts[x], phase_ends[x], color='green', alpha=0.1)
        elif phase_types[x] == 1:
            plt.axvspan(phase_starts[x], phase_ends[x], color='yellow', alpha=0.1)
        elif phase_types[x] == 2:
            plt.axvspan(phase_starts[x], phase_ends[x], color='red', alpha=0.1)

def plot_simulation(
    P_history, E_history, U_history, S_history, phase_history
):
    # Implementation for plotting the simulation results
    plt.figure(figsize=(24, 6))
    plt.yticks([0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0])
    plt.plot(P_history, label="Population (P)", color='blue')
    plt.plot(E_history, label="Elite Overproduction (E)", color='orange')
    plt.plot(U_history, label="Sociopolitical Instability (U)", color='red')
    plt.plot(S_history, label="State Capacity (S)", color='green')
    plot_phase_space(phase_history)
    plt.legend()
    plt.title("Secular Cycle Simulation")
    plt.show()
    pass

class FinalSim:
    def __init__(
            self, 
            P=0.4, # Starting Population
            E=0.1, # Starting Elite Overproduction
            S=0.5, # Starting State Capacity
            dP_mult=1.0, # Growth multipliers
            dE_mult=1.0, 
            dS_mult=1.0, 
            dP_nmult=1.0, # Shrink multipliers (when growth is negative)
            dE_nmult=1.0, 
            dS_nmult=1.0
        ):
        self.P = P  # Population
        self.E = E  # Elite Overproduction
        self.U = 0.0  # Sociopolitical Instability
        self.U_e = 0.0  # Effective Instability (instability above state capacity)
        self.S = S  # State Capacity
        self.instability_memory = 0.0 # Memory of past instability to create inertia in the system
        
        self.phase = 0 # 0 = prosperity, 1 = strain, 2 = fracture
        
        # Random value multipliers that change every 10 steps since adjusting randomness every step cancels out the effect.
        self.P_rand = 0.0
        self.E_rand = 0.0
        self.U_rand = 0.0
        self.S_rand = 0.0

        self.P_history = []
        self.E_history = []
        self.U_history = []
        self.U_e_history = []
        self.S_history = []
        self.phase_history = []

        # Multipliers
        self.dP_mult = dP_mult
        self.dE_mult = dE_mult
        self.dS_mult = dS_mult
        self.dP_nmult = dP_nmult
        self.dE_nmult = dE_nmult
        self.dS_nmult = dS_nmult

        # Economy
        self.max_birth = 0.04
        self.min_birth = 0.015
        self.death_base = 0.01
        self.child_mortality = 0.4
        self.k_famine = 0.1
        self.k_disease = 0.1
        self.k_war = 0.05
        self.land_area = 1.0
        self.population = 0.6
        self.elites = 0.006

        # ---- Agrarian economy (districts + subsistence). Read-out layer for now: computed & tracked,
        #      but NOT yet fed back into P/E/U/S (that comes with elite income & district-driven capacity).
        #      NOTE: NO GDP in this model. Districts generate WEALTH (surplus value, tax base) which is
        #      NEVER negative; falling below subsistence is a FOOD shortage, tracked separately. See
        #      economy_design.md. ----
        self.land_productivity = 1.0   # yield per unit land (tech / district-tier hook; will raise K later)
        self.subsistence = 1.0         # per-capita subsistence need (food-equivalent units)
        self.base_yield = 2.0          # per-worker output at low density (subsistence units); sets surplus scale
        self.district_share = 0.5      # fraction of usable land organized into elite-owned districts
        self.wage_share = 0.5          # commoners' share of district wealth (the rest is elite surplus)

        # Economic history (measurement + wiring for later stages)
        self.wealth_pc_history = []      # WEALTH per capita (>=0), the main SoL driver
        self.food_ratio_history = []     # food output / need (>=1 fed, <1 shortage)
        self.wage_history = []           # commoner SoL / real-wage index driving mobilization
        self.elite_income_history = []   # district wealth surplus accruing to elites
        self.commoner_wealth_history = []# commoner wealth (self-employed surplus + district wages)

        # ---- Attached district/pop economy (economy.py). Only the COMMONER side is exposed to the
        #      cycle (it drives the wage). Elite district income is computed but discarded ("into the
        #      aether") for now; the old self.elites scalar dynamics below are untouched. This lets us
        #      confirm the cycle still works with real districts attached before rewiring the elites. ----
        self.econ_land = 1.0             # location land ~ carrying capacity, so district jobs ~ population and
                                         #   labor pressure actually bites (workers/jobs ~ 1 at carrying capacity)
        self.wage_d_crit = 0.9           # wage-SHARE falling point: commoners' share of district wealth crosses 50%
                                         #   when workers reach this fraction of carrying capacity (=> wage 50% at ~90%)
        self.location = Location(land_area=self.econ_land, security=1.0, wage_d_crit=self.wage_d_crit)
        self._commoner = Pop("Commoner", self.population)
        self._elite_owner = Pop("Elite", self.elites)   # the "new" elite pop; grown/shrunk by the elite dynamics
        self.location.add_pop(self._commoner)
        self.location.add_pop(self._elite_owner)
        # Districts fill the land so subsistence (fully commoner-owned) doesn't dilute the wage share, and so
        # commoner capture ~= the labor wage share (elite-independent). Elites building districts comes later.
        self.location.add_district(District(FARM, size=1.0, tier=0, owner=self._elite_owner))

        # ---- State fiscal (treasury-buffered, survival-seeking; debt ignored for now). ----
        self.tax_rate = 0.25          # constant tax on commoner + elite surplus alike
        self.army_base = 0.006        # baseline army upkeep
        self.army_unrest = 0.02       # extra army upkeep per unit unrest (suppression surge)
        self.mil_positions = 0.01     # standing officer corps -> baseline elite positions (with districts)
        self.k_patronage = 2.0        # cost to employ one excess elite as a patronage position
        self.treasury_years = 5.0     # treasury caps at this many years of (gross) revenue
        self.S_adjust = 0.2           # how fast state capacity tracks its target
        self.w_buffer = 0.55          # weight of the treasury buffer vs structural budget health in S (rest is structural)
        self.treasury = 0.0           # state savings (stock; capped, floored at 0 -> no debt)
        self.treasury_history = []
        self.revenue_history = []

    def get_birth_rate(self, P, carrying_cap, security=1.0, start_decline=0.75, end_decline=1.5):
        # High fertility when well under cap (resources abundant, early marriage etc.), gradual decline as density rises. Still significant births beyond carrying cap
        rel = P / max(carrying_cap * security, 1e-8) # Low security has the effect of reducing carrying capacity when it comes to births
        t = np.clip((rel - start_decline) / (end_decline - start_decline), 0.0, 1.0)
        t = t * t * (3 - 2 * t)  # Smoothstep interpolation for a gradual transition

        return max(self.max_birth - (self.max_birth - self.min_birth) * t, self.min_birth)
    
    def get_death_rate(self, P, carrying_cap, birth_rate, famine_severity=0.0, disease_severity=0.0, war_severity=0.0):
        rel = P / max(1e-8, carrying_cap)

        base_death = self.death_base + self.child_mortality * birth_rate

        famine_excess = rel ** 1.2 * famine_severity ** 1.25 * self.k_famine
        disease_excess = rel ** 1.2 * disease_severity * self.k_disease * (1 + famine_severity ** 1.25)  # Disease worsens with famine
        war_excess = rel ** 1.2 * war_severity * self.k_war  # War increases death rate
        excess = famine_excess + disease_excess + war_excess

        return base_death + excess
    
    def step(self, t, randomness=0.0):

        # Population growth is based on a logistic growth model with carrying capacity influenced by state capacity and effective instability.
        # carrying_cap = min(max(0.5 + self.S**2 - self.U_e * 1.0, 0.0), 1.0)
        # dP = (self.P + 0.1) * (carrying_cap - self.P)
        # if dP > 0:
        #     dP *= self.P * (1.1 - self.P) / 1.1 # Population grows slower as it approaches carrying capacity
        #     dP *= self.dP_mult
        # else:
        #     dP *= (self.P+0.1)/1.1 # Population shrinks slower as it approaches 0.0
        #     dP *= self.dP_nmult


        # Population Growth
        security = max(0.5, self.S - self.U_e)   # state order minus violence; gates districts (and, via births, the effective cap)
        carrying_cap = self.land_area * self.land_productivity
        birth_rate = self.get_birth_rate(self.population, carrying_cap, security=security)
        death_rate = self.get_death_rate(self.population, carrying_cap, birth_rate, famine_severity=0.0, disease_severity=0.0, war_severity=self.U_e)
        self.population += (birth_rate - death_rate) * self.population
        dP = (birth_rate - death_rate)

        # ---- Attached district/pop economy: commoners work districts + subsistence; the wage emerges. ----
        # Commoners work districts + subsistence; elites (self._elite_owner) receive the district surplus.
        self._commoner.amount = max(self.population, 1e-8)
        self.location.security = security
        econ = self.location.tick()
        elite_income = econ["elite_income"]                    # district surplus -> self._elite_owner.wealth (used below)
        food_access = self._commoner.food_access               # food produced / food need (=1 at carrying capacity)
        wealth_per_capita = self._commoner.wealth_per_capita() # commoner wealth (running total) per head
        commoner_wealth = self._commoner.wealth
        food_ratio = food_access
        famine_severity = max(0.0, 1.0 - food_access)          # tracked; feeds famine mortality in a later increment

        # Commoner wage that drives mass mobilization: Turchin RELATIVE WAGE = average commoner wage /
        # GDP-per-capita analog = (commoner wealth / commoners) / (total wealth / total population).
        # Elite-independent in practice (commoner capture ~= the labor wage share). Low w -> high MMP.
        Wc, Nc = self._commoner.wealth, self._commoner.amount
        We, Ne = self._elite_owner.wealth, self._elite_owner.amount
        gdp_pc = (Wc + We) / max(Nc + Ne, 1e-8)
        w = (Wc / max(Nc, 1e-8)) / max(gdp_pc, 1e-8)
        w = min(max(w, 1e-4), 1.0)
        w_inverse = w ** -1                                     # inverse relative wage -> mass mobilization potential
        
        # ---- State fiscal (treasury-buffered): taxes surplus, funds an army + patronage to absorb excess
        #      elites, runs a treasury capped at ~5 years of revenue. A survival-seeking state opens
        #      patronage positions for overproduced elites for as long as it can afford to. Debt ignored. ----
        total_income = commoner_wealth + elite_income          # wealth generated this tick (running totals)
        collection = max(0.0, 1.0 - self.U_e)                  # unrest wrecks the tax base; control ~1 for now
        revenue = self.tax_rate * total_income * collection    # taxes a portion of commoner + elite surplus alike

        district_jobs = sum(d.elite_opportunities() for d in self.location.districts)  # ownership/mgmt slots
        baseline_positions = district_jobs + self.mil_positions      # + standing officer corps (military)
        excess_elites = max(0.0, self.elites - baseline_positions)
        desired_patronage = excess_elites * self.k_patronage         # cost to employ every excess elite
        army_cost = self.army_base + self.army_unrest * self.U_e     # baseline upkeep + suppression surge

        # Fund from treasury + revenue; the army is paid before patronage.
        funds = self.treasury + revenue
        army_paid = min(army_cost, funds)
        patronage_paid = max(0.0, min(desired_patronage, funds - army_paid))
        patronage_jobs = patronage_paid / max(self.k_patronage, 1e-9)

        # Treasury: keep the surplus, capped at ~5 years of gross revenue, floored at 0 (no debt).
        max_treasury = self.treasury_years * self.tax_rate * total_income
        self.treasury = min(max(self.treasury + revenue - army_paid - patronage_paid, 0.0), max_treasury)

        # Funded elite positions = baseline + the patronage the state could actually pay for. Used for elite
        # mobility (w0): patronage keeps opening slots, so aspirants keep climbing. The E *gauge* below uses
        # baseline_positions instead, so RAW overproduction stays visible in stagnation even while the state
        # pacifies it (S suppresses the conflict) — the strain phase then shows up before the collapse.
        elite_positions = max(baseline_positions + patronage_jobs, 1e-6)

        # ---- State capacity = BUFFER + STRUCTURAL-HEALTH blend (NOT a binary funds/desired). ----
        # The old min(1, funds/desired) read 1.0 for as long as the treasury held anything, then snapped to
        # ~0 the instant it emptied (a decade of accumulated deficit released at once). Split into two signals:
        #   fiscal_buffer     = how full the reserves are (treasury / cap). Patronage (discretionary) drains it
        #                       gradually through stagnation -> S declines with early warning, not a cliff.
        #   structural_health = can current REVENUE fund the ESSENTIAL core (the army the state must pay)?
        #                       Stays ~1 through stagnation (revenue >> army), and craters only when unrest
        #                       wrecks tax collection -> this is what carries S toward 0 in the acute crisis.
        # So stagnation weakens S via the buffer; the deep collapse comes via structural when collection -> 0
        # (S can't reach 0 unless tax collection does, as specified).
        essential_cost = army_cost                      # army = must-fund core; patronage is discretionary (buffer)
        fiscal_buffer = self.treasury / max(max_treasury, 1e-9)
        structural_health = min(1.0, revenue / max(essential_cost, 1e-9))
        S_target = self.w_buffer * fiscal_buffer + (1.0 - self.w_buffer) * structural_health
        self.S = max(0.0, min(1.0, self.S + (S_target - self.S) * self.S_adjust))

        # ---- Elites: the "new" elite POP is grown/shrunk here; self.elites mirrors it for the readouts. ----
        # How elite numbers grow (Turchin):  de/dt ~= u0 * e * (w0 - w) / w
        #   u0 = mobility responsiveness; w0 = the commoner-wage level at which mobility is ZERO (the value
        #        the user calls "elite_wage" — NOT the elites' literal wage); w = commoner (relative) wage.
        #   When commoner wages fall below w0, more people climb into the elite, so elite numbers expand —
        #   the key feedback linking commoners & elites. Elites are also the target of instability, pruned
        #   slowly (linear in U_e) so they persist into the depression and clear only at its end.
        # Elite OPPORTUNITIES (elite_positions) come from the fiscal block above: district ownership/mgmt
        # slots + military officer corps + the patronage the state can currently afford.
        elite_count = self._elite_owner.amount

        # w0 = mobility zero point. Most attractive when elites ~ fill the positions, less so when
        # overproduced (too many elites for the slots). Multiplier chosen for scale (free per the design).
        if elite_count < elite_positions:
            elite_wage = 0.5 + 0.5 * (elite_count / elite_positions)
        else:
            elite_wage = 0.5 - 0.1 * ((elite_count - elite_positions) / elite_positions)

        e_social_mobility = elite_count * 0.02 * (elite_wage - w) / w
        if e_social_mobility < 0:
            e_social_mobility *= 0.5              # downward mobility is stickier than upward
        e_attrition = elite_count * 0.05 * self.U_e   # violence prunes elites (linear -> gradual)
        elite_count += e_social_mobility - e_attrition
        self._elite_owner.amount = max(elite_count, 1e-6)
        self.elites = self._elite_owner.amount   # mirror for the E readout (and, next increment, the fiscal side)
        dE = (e_social_mobility - e_attrition) / max(self.elites, 1e-8)

        # Sociopolitical instability increases with elite overproduction and population pressure, and decreases with state capacity. 
        # TODO:
        # MMP (Mass Mobilization Potential) = W^-1 * (N(urb) / N) * A(20-29)
        # W^-1 = Inverse relative wage (real worker wage / GDP per capita) -> high wages = low unrest, low wages = high unrest
        # N(urb) / N = Urbanization rate (urban population / total population) -> high urbanization = high unrest, low urbanization = low unrest. 
        #       Rural misery is much harder to turn into mass political action. High urbanization dramatically increases the potential for unrest.
        # A(20-29) = Age structure (population aged 20-29 / total population) -> high proportion of young adults = high unrest, low proportion of young adults = low unrest. 
        #       Young adults are more likely to be politically active and willing to take risks.
        # Additional proxy factors: Labor oversupply more broadly, Declining biological wellbeing, erosion of family & community structures, rising urban density within cities, reduction of cooperation
        mmp = w_inverse # Simplified MMP: Inverse relative wage (real worker wage / GDP per capita) -> high wages = low unrest, low wages = high unrest

        # EMP (Elite Mobilization Potential) = ew^-1 * E
        # ew^-1 = Relative elite income (elite income / GDP per capita).
        #       When elite pie is divided between too many people, relative elite incomes fall even if absolute incomes are high. Creates huge competition within elites
        # E = Elite overproduction (number of elites / number of elite positions) -> high elite overproduction = high unrest, low elite overproduction = low unrest.
        emp = self.E * (1+w) # Simplified EMP: Just Elite overproduction
        psi = mmp * emp * (1 - self.S) # Sociopolitical instability is a function of MMP, EMP, and state capacity. High state capacity suppresses instability.
        # Smooth the instability on the high end to avoid runaway instability. This is a simple logistic function that caps instability at 1.0.
        psi = 1.0 / (1.0 + math.exp(-8.0 * (psi - 0.5))) # Logistic smoothing for sociopolitical instability

        # (State capacity S is now set by the treasury-buffered fiscal block above, not a dS gauge rule.)

        # Update variables with some smoothing
        #self.P = max(0.0, min(1.0, self.P + dP * 0.1))
        rel = self.population / max(carrying_cap, 1e-8)
        pressure = 1.0 / (1.0 + math.exp(-7.2 * (rel - 1.0))) # Logistic smoothing for population pressure
        self.P = max(0.0, min(1.0, pressure)) # Logistic smoothing for population pressure

        rel = (self.elites) / max(baseline_positions, 1e-8)   # RAW overproduction (vs baseline, not patronage-funded)
        pressure = 1.0 / (1.0 + math.exp(-1.8 * (rel - 2.0))) # Logistic smoothing for elite overproduction pressure
        self.E = max(0.0, min(1.0, pressure)) # Logistic smoothing for elite overproduction pressure
        self.U = max(0.0, min(1.0, self.U))
        # (self.S already updated in the fiscal block.)

        self.U_e = max(0.0, min(psi, 1.0)) # Effective instability is instability above 0.5 * state capacity
        
        # Phase transitions
        if self.phase == 0 and (self.E > 0.3 or self.P > 0.7):
            self.phase = 1 # Strain (Stagflation)
        elif self.phase == 1 and (self.U_e > 0.5 or self.S < 0.5 or (dE < 0 and dP < 0)):
            self.phase = 2 # Fracture (Crisis+Depression)
        elif self.phase == 2 and (self.U_e < 0.1 and dP > 0 and self.S > 0.2):
            self.phase = 0 # Prosperity (Expansion)

        # Record history
        self.P_history.append(self.P)
        self.E_history.append(self.E)
        self.U_history.append(self.U)
        self.U_e_history.append(self.U_e)
        self.S_history.append(self.S)
        self.phase_history.append(self.phase)

        # Economic read-outs (tracked for measurement; not yet coupled into the cycle)
        self.wealth_pc_history.append(wealth_per_capita)
        self.food_ratio_history.append(food_ratio)
        self.wage_history.append(w)
        self.elite_income_history.append(elite_income)
        self.commoner_wealth_history.append(commoner_wealth)
        self.treasury_history.append(self.treasury)
        self.revenue_history.append(revenue)

random.seed(42)

final_sim = FinalSim(
    P=0.4,
    E=0.1,
    S=0.5,
    dP_mult=1.0,
    dE_mult=1.0,
    dS_mult=2.0,
    dP_nmult=1.0,
    dE_nmult=1.0,
    dS_nmult=1.0
)
for t in range(500):
    final_sim.step(t, randomness=0.0)

#plot_simulation(    
    #final_sim.P_history, final_sim.E_history, final_sim.U_e_history, final_sim.S_history, final_sim.phase_history
#)