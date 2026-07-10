import math
import numpy as np
import matplotlib as plt
import random

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
        carrying_cap = self.land_area
        birth_rate = self.get_birth_rate(self.population, carrying_cap, security=max(0.5,self.S-self.U_e))
        death_rate = self.get_death_rate(self.population, carrying_cap, birth_rate, famine_severity=0.0, disease_severity=0.0, war_severity=self.U_e)
        self.population += (birth_rate - death_rate) * self.population
        dP = (birth_rate - death_rate)

        # Effective wages
        # TODO: Implement a more complex wage model that takes into account wealth, labor supply/demand, and cultural factors.
        w = 1.0 - self.P  # Inverse of population pressure (high population = low wages, low population = high wages)
        w_inverse = w ** -1  # Inverse relative wage
        
        # Elite overproduction increases with population pressure (so long as the state exists)
        # How elite numbers themselves grow:
        # de/dt ~= u0 * e * (w0 - w ) / w
        # u0 = mobility responsiveness (how quickly elites respond to changes in wages)
        # w0 = point where social mobility is zero (wage level where elites are neither gaining nor losing members, wages above w0 reduce elites, below increase elites)
        # w = commoner wages
        # When commoner wages are low, more people try to climb into the elite, so elite numbers expand. Key feedback that links popular & elite departments
        # Elites are also teh target in instability, so they are pruned when instability is high. This is a slow attrition, not a sharp crash, so elites persist into the depression.
        elite_positions = (1 + self.S) * self.population * 0.01 # 2% of the population can be elites
        elite_wage = 0.5
        if self.elites < elite_positions:
            elite_wage = 0.5 + 0.5 * (self.elites / elite_positions) # When elites are below the number of positions, their relative income is higher, so they are less likely to be pruned
        else:
            elite_wage = 0.5 - 0.1 * ((self.elites - elite_positions) / elite_positions) # When elites are above the number of positions, their relative income is lower, so they are more likely to be pruned

        e_social_mobility = (self.elites) * 0.02 * (elite_wage - w) / w
        if e_social_mobility < 0:
            e_social_mobility *= 0.5 # Social mobility is slower when elites are losing members
        e_attrition = self.elites * 0.05 * self.U_e
        self.elites += e_social_mobility - e_attrition
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

        # SFD (state fiscal distress) = Y/G * (1-T) - Y is national debt, G is GDP, T is trust/legitimacy. Debt Interest/State Revenue can be used instead. SFD is roughly inverse of state capacity.
        # dS/dt = p * surplus - elite demands - military costs - patronage
        revenue = self.population * 0.1
        expenses = 6.0 * (self.elites - elite_positions) + self.U_e * (self.elites * 8 + self.population * 0.1)
        
        dS = (revenue - expenses) / self.population
        if dS > 0:
            dS *= (1.5 - self.S)/1.5
            dS *= self.dS_mult
        else:
            dS *= (self.S+0.2)/1.2
            dS *= self.dS_nmult

        # Update variables with some smoothing
        #self.P = max(0.0, min(1.0, self.P + dP * 0.1))
        rel = self.population / max(carrying_cap, 1e-8)
        pressure = 1.0 / (1.0 + math.exp(-7.2 * (rel - 1.0))) # Logistic smoothing for population pressure
        self.P = max(0.0, min(1.0, pressure)) # Logistic smoothing for population pressure

        rel = (self.elites) / max(elite_positions, 1e-8)
        pressure = 1.0 / (1.0 + math.exp(-1.8 * (rel - 2.0))) # Logistic smoothing for elite overproduction pressure
        self.E = max(0.0, min(1.0, pressure)) # Logistic smoothing for elite overproduction pressure
        self.U = max(0.0, min(1.0, self.U))
        self.S = max(0.0, min(1.0, self.S + dS * 0.1))

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