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
        self.k_war = 0.035  # war mortality coefficient (lower => more gradual population decline; trough is held ~0.6 of
                            #   carrying capacity by the security floor regardless -> "suppressed at the bottom")
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
        self.k_suppress = 0.006       # cost to suppress one unit of mobilization POTENTIAL (not active unrest) ->
                                      #   overproduction is fiscally costly even before it erupts (breaks a solvent state).
                                      #   robust ~0.006-0.008; too low+high k_absorb -> stuck in chronic fracture
        self.max_suppression = 0.9    # at S=1 the state suppresses this fraction of potential; the rest leaks (S=1 != 0 unrest)
        self.mil_positions = 0.01     # standing officer corps -> baseline elite positions (with districts)
        self.k_patronage = 2.0        # cost to employ one excess elite as a patronage position
        self.k_absorb = 2.0           # the state absorbs a SHRINKING fraction of excess elites as overproduction
                                      #   rises (it can't bear the whole class) -> higher overproduction, longer S
        self.k_emp = 10.0             # EMP scale: elite overproduction x inverse relative elite income. Lower =>
                                      #   crisis fires later => elites accumulate more => higher E_peak & longer strain
        self.treasury_years = 5.0     # treasury caps at this many years of (gross) revenue
        self.w_buffer = 0.5           # weight of treasury buffer vs structural budget health in the fiscal signal.
                                      #   Higher -> deeper collapses & longer period; lower (structural) -> shallower.
        self.treasury = 0.0           # state savings (stock; capped, floored at 0 -> no debt)

        # ---- Fracture / attrition params. ----
        self.k_attrition = 0.20       # rate violence prunes elites (x cull_violence). Each father-son spike is a
                                      #   burst of killing that clears a chunk of elites (a civil war prunes them even
                                      #   BELOW the peacetime baseline). Tuned so a SEQUENCE of ~2-3 flare-ups steps
                                      #   the overproduction down, the last one finishing them off.
        self.k_attrition_fracture = 0.0   # extra pruning in fracture (0 => the waves clear E on their own)
        self.u_cull_floor = 0.15      # violence below this barely culls elites; set just BELOW the endemic hum so a
                                      #   SMALL continuous cull always clears E (guarantees the phase exits -- no
                                      #   frozen limbo) while the above-hum flare-ups do the bulk of the culling.
        self.E_exit_thresh = 0.15     # fracture ends once overproduction is cleared. The E gauge floors ~0.14 at
                                      #   elites==baseline (sigma(1.8*(1-2))), so this triggers just as elites reach
                                      #   the peacetime level -- i.e. the crisis has consumed the surplus elites.

        # ---- Radicalization dynamics (Turchin's SIR/SIRS "father-son" model): the mobilizable population is
        #      split into NAIVE (disengaged, susceptible), RADICAL (drives violence), and MODERATE (de-radicalised,
        #      "immune"). Radicals recruit naives (contagion), moderates form once radicals are numerous and
        #      suppress them, and immunity WANES (moderate -> naive) over ~a generation. This produces relaxation
        #      OSCILLATIONS -- violence bursts, burns out, a refractory lull while immunity is high, then rebuilds:
        #      the small "father-son" pendulum. Recruitment is gated by `conditions` (immiseration + overproduction,
        #      damped by the state) so waves are naturally MUTED in prosperity and fire only when things are bad.
        #      U_e (violence) = the RADICAL fraction, so a 1.0 spike needs bad conditions AND a released burst. ----
        # The rest state (R~0) is EXCITABLE, not just stable: contagion only self-amplifies once R crosses an
        # ignition threshold (steep gate), so bursts are all-or-nothing and separated by refractory lulls -- a
        # relaxation oscillator (limit cycle), not a smooth endemic equilibrium. The naive pool slowly charges
        # via the seed until it trips the threshold ("pressure valve releases"); the burst builds immunity that
        # locks out re-ignition until it wanes ("next release needs immunity low").
        self.rad_N = 1.0              # naive fraction (susceptible)   -- pool of stored "pressure"
        self.rad_R = 0.0              # radical fraction (infected)    -- drives U_e
        self.rad_M = 0.0              # moderate fraction (recovered/immune) -- the refractory clock
        self.rad_alpha = 8.0          # contagion: how fast radicals recruit naives past ignition (x conditions)
        self.rad_seed = 0.003         # spontaneous radicalisation seed (x conditions) -> slowly charges toward ignition
        self.rad_burnout = 0.15       # radicals burning out into moderates on their own (rho)
        self.rad_suppress = 2.0       # moderates de-radicalising radicals (delta, x R*M) -> ends a burst & locks refractory
        self.rad_wane = 0.08          # immunity waning rate (moderate -> naive); ~1/this sets the father-son sub-period
        self.rad_k_ig = 40.0          # ignition-gate steepness (excitability): high => sharp all-or-nothing bursts
        self.rad_trig = 0.08          # ignition threshold: R must exceed this for contagion to take off
        self.cond_steepness = 8.0     # `conditions` logistic steepness (maps raw emp-badness -> [0,1] recruitment gate)
        self.cond_midpoint = 0.15     # raw emp-badness at which conditions = 0.5. Low so conditions stay ABOVE the
                                      #   sustained-oscillation level until elites are actually cleared -> the bursts
                                      #   keep firing to the end (final clearing spike) instead of petering out early.
                                      #   Still ~0 in prosperity/strain because a strong state (high S) suppresses it.
        # ---- Observed violence U_e = an ENDEMIC hum + the excitable spikes, then asymmetric-smoothed. The raw
        #      radical fraction R spikes sharply for ~2 ticks; on its own that made U_e a train of thin spikes
        #      with dead lulls. Real crises have a sustained low-level of violence (banditry/riots) all through
        #      the depression PLUS bigger father-son flare-ups. So: `endemic = k_hum*conditions` gives a ~0.25
        #      hum that fades only as the crisis resolves, and a fast-rise/slow-decay low-pass widens each spike
        #      to ~5-10 ticks with a gradual tail. As conditions ease (E clears) both the hum and the spikes shrink.
        self.k_hum = 0.26             # endemic baseline unrest at full conditions (the steady "hum" ~0.2-0.25)
        self.u_rise = 0.6             # U_e low-pass rate when violence is climbing (fast rise -> keeps peaks high)
        self.u_decay = 0.18           # U_e low-pass rate when violence is falling (slow decay -> 5-10 tick spikes)

        # ---- State capacity S = logistic readout of a slow "state health" stock (smooth, like P). ----
        self.state_health = 1.0       # slow stock the state's capacity is read off of (integrates the fiscal signal)
        self.health_adjust = 0.06     # recovery rate of state_health (small => smooth, non-sawtooth recovery)
        self.health_adjust_down = 0.5 # collapse rate (asymmetric: sharp drop at fracture onset -- user OK with sharp DOWN)
        self.fracture_floor = 0.1     # in fracture the health target is capped this low, easing up as E clears
        self.E_clear = 0.12           # E level at which the fracture cap fully lifts. Low so S stays suppressed
                                      #   (=> conditions stay high => bursts keep firing) until elites are cleared
        self.k_S = 5.0                # steepness of the S logistic readout (gentler than P's 7.2 -> visible ramp)
        self.x0_S = 0.4               # midpoint of the S logistic readout

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

        # ---- Mobilization POTENTIAL (before the state suppresses it) = mass x elite mobilization. ----
        # MMP = inverse relative wage (immiserated commoners). EMP = elite overproduction amplified by the
        # inverse of relative elite income (ew = elite income per capita / GDP per capita): as the elite pie
        # splits among more elites, relative elite income falls -> ew_inverse rises -> fiercer intra-elite
        # competition. Potential builds as elites overproduce even while a strong state keeps ACTIVE unrest low
        # -- and the state must SPEND to hold it down (see the army/suppression cost in the fiscal block).
        mmp = w_inverse
        elite_income_pc = We / max(Ne, 1e-9)
        ew_relative = elite_income_pc / max(gdp_pc, 1e-9)
        ew_inverse = 1.0 / max(ew_relative, 1e-6)
        emp = self.k_emp * self.E * ew_inverse
        mobilization_potential = mmp * emp    # raw pressure; drives both the suppression COST and `conditions` below

        # ---- State fiscal (treasury-buffered): taxes surplus, funds an army + patronage to absorb excess
        #      elites, runs a treasury capped at ~5 years of revenue. A survival-seeking state opens
        #      patronage positions for overproduced elites for as long as it can afford to. Debt ignored. ----
        total_income = commoner_wealth + elite_income          # wealth generated this tick (running totals)
        collection = max(0.0, 1.0 - self.U_e)                  # unrest wrecks the tax base; control ~1 for now
        revenue = self.tax_rate * total_income * collection    # taxes a portion of commoner + elite surplus alike

        district_jobs = sum(d.elite_opportunities() for d in self.location.districts)  # ownership/mgmt slots
        baseline_positions = district_jobs + self.mil_positions      # + standing officer corps (military)
        excess_elites = max(0.0, self.elites - baseline_positions)
        # The state takes on only a PART of the excess elites, a smaller share the more overproduced they
        # are (it cannot bear the whole overproduced class). -> overproduction runs higher, treasury lasts longer.
        overproduction = self.elites / max(baseline_positions, 1e-9)
        absorb_fraction = 1.0 / (1.0 + self.k_absorb * max(0.0, overproduction - 1.0))
        desired_patronage = excess_elites * absorb_fraction * self.k_patronage   # cost of the part it takes on
        # Army = baseline upkeep + the cost of suppressing the mobilization POTENTIAL (not active unrest).
        # A strong state must pay to hold down overproduced elites, so overproduction drains the treasury
        # even before it erupts -> this is what lets overproduction break even a permanently-solvent state.
        suppression_cost = self.k_suppress * mobilization_potential
        army_cost = self.army_base + suppression_cost

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
        # Root cause of the old snap: S_target = min(1, FUNDS/desired) put the whole treasury in the
        # numerator, so S read 1.0 until the treasury emptied then released a decade of deficit at once.
        # Fix: take the treasury OUT of the structural signal and track it separately as a buffer.
        #   structural_health = can this year's REVENUE (not reserves) cover the state's ongoing commitments
        #                       (army + the elite patronage it has taken on)? Declines as elites overproduce
        #                       (commitments outgrow the tax base = Turchin's structural fiscal crisis) AND
        #                       craters when unrest wrecks collection -> carries S toward 0 iff revenue -> 0.
        #   fiscal_buffer     = reserves (treasury / cap); drains gradually as it funds the deficit -> S falls
        #                       with early warning, not a cliff.
        desired = army_cost + desired_patronage         # ongoing commitments the state is trying to fund
        fiscal_buffer = self.treasury / max(max_treasury, 1e-9)
        structural_health = min(1.0, revenue / max(desired, 1e-9))
        fiscal_signal = self.w_buffer * fiscal_buffer + (1.0 - self.w_buffer) * structural_health

        # State capacity is a logistic read-out of a SLOW "state health" stock (mirrors how population
        # pressure P is read off the slow population stock) -> S is smooth, not a sawtooth.
        # In FRACTURE the collapsed, contested state is deliberately held down: the health target is capped
        # low and only eases up as elite overproduction (E) clears -> S stays low through the depression and
        # recovers smoothly as the elites are pruned. (Replaces the old unrest-rebuild gate.)
        target = fiscal_signal
        if self.phase == 2:
            frac_ceiling = self.fracture_floor + (1.0 - self.fracture_floor) * max(0.0, 1.0 - self.E / self.E_clear)
            target = min(target, frac_ceiling)
        # Asymmetric: collapse is sharp (state fails fast at the crisis onset), recovery is slow & smooth.
        rate = self.health_adjust_down if target < self.state_health else self.health_adjust
        self.state_health += (target - self.state_health) * rate
        self.state_health = max(0.0, min(1.0, self.state_health))
        self.S = 1.0 / (1.0 + math.exp(-self.k_S * (self.state_health - self.x0_S)))   # logistic readout, like P

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
        if self.phase == 2:
            # During the crisis NO one climbs in, and the non-violent bleed is frozen: elites clear ONLY by
            # violence (attrition). This holds E (=> `conditions`) high between radical bursts, so the waves
            # keep recurring -- each burst prunes a chunk of elites -- instead of the crisis fizzling after one.
            e_social_mobility = 0.0
        # Elite culling is SPIKE-driven: only violence ABOVE the endemic hum (revolts/civil wars, not the
        # background banditry that just grinds down the commoner population) prunes the elite. This keeps the
        # low-level hum from clearing overproduction on its own -> the crisis still needs a SEQUENCE of
        # father-son flare-ups to consume the surplus elites (a single wave no longer ends it).
        attrition_rate = self.k_attrition + (self.k_attrition_fracture if self.phase == 2 else 0.0)
        cull_violence = max(0.0, self.U_e - self.u_cull_floor)
        e_attrition = elite_count * attrition_rate * cull_violence
        elite_count += e_social_mobility - e_attrition
        self._elite_owner.amount = max(elite_count, 1e-6)
        self.elites = self._elite_owner.amount   # mirror for the E readout (and, next increment, the fiscal side)
        dE = (e_social_mobility - e_attrition) / max(self.elites, 1e-8)

        # ---- Radicalization dynamics -> active unrest (the father-son sub-pendulum). ----
        # `conditions` = the crisis pressure the state fails to suppress, squashed to a [0,1] recruitment gate.
        # It is driven by ELITE OVERPRODUCTION (emp), NOT the mass term (mmp): in the crisis phase Turchin's
        # driver is intra-elite conflict, and emp stays high until elites are actually pruned -- whereas mmp
        # collapses when the population crashes (wages recover), which would falsely end the crisis. The
        # `(1 - max_suppression*S)` gate keeps conditions low while the state is strong (no bursts in
        # prosperity/strain) and lets them rise as overproduction drains the state and S collapses -- that
        # collapse is what fires the FIRST burst and tips strain -> fracture. Calm -> ~0, crisis -> ~1.
        raw_conditions = emp * (1.0 - self.max_suppression * self.S)
        conditions = 1.0 / (1.0 + math.exp(-self.cond_steepness * (raw_conditions - self.cond_midpoint)))

        # SIR/SIRS flows on the naive/radical/moderate fractions (sum ~= 1):
        #   naive -> radical : (contagion*R + seed) * N * conditions   (radicals recruit; bad times seed unrest)
        #   radical -> moderate: burnout*R + suppress*R*M              (radicals tire out; moderates de-radicalise them)
        #   moderate -> naive : wane*M                                 (immunity fades over ~a generation)
        N, R, M = self.rad_N, self.rad_R, self.rad_M
        activation  = 1.0 / (1.0 + math.exp(-self.rad_k_ig * (R - self.rad_trig)))  # excitable ignition gate
        to_radical  = (self.rad_alpha * R * activation + self.rad_seed) * N * conditions
        to_moderate = self.rad_burnout * R + self.rad_suppress * R * M
        to_naive    = self.rad_wane * M
        N += to_naive - to_radical
        R += to_radical - to_moderate
        M += to_moderate - to_naive
        # Clamp & renormalise to a simplex (guards numerical drift; keeps N+R+M=1).
        N, R, M = max(0.0, N), max(0.0, R), max(0.0, M)
        tot = N + R + M
        if tot > 1e-9:
            N, R, M = N / tot, R / tot, M / tot
        self.rad_N, self.rad_R, self.rad_M = N, R, M
        # Observed violence = endemic hum (scales with conditions) + the excitable spike, low-passed so spikes
        # last ~5-10 ticks (fast rise, slow decay) and there is a continuous ~0.25 simmer through the crisis.
        endemic = self.k_hum * conditions
        violence_target = endemic + (1.0 - endemic) * R
        u_rate = self.u_rise if violence_target > self.U_e else self.u_decay
        self.U_e += (violence_target - self.U_e) * u_rate
        psi = self.U_e

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
        elif self.phase == 2 and (self.E < self.E_exit_thresh and dP > 0):
            self.phase = 0 # Prosperity (Expansion) -- once overproduction is (near-)cleared & population
                           # recovers. NOT gated on U_e<0.1: the endemic hum legitimately keeps U_e above
                           # that during the crisis, and it fades on its own in prosperity (conditions->0);
                           # gating on it caused a frozen limbo when the hum settled just below the cull floor.

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