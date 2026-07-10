import math

# Wealth Levels:
wealth_req = [0] * 51
food_req = [1] * 51
goods_req = [0] * 51
for level in range(51): # 0 to 50 inclusive
    wealth_req[level] = 2 ** (level * 0.3) - 1     # 2^0.3x - 1
    goods_req[level] = level * 0.1 + 0.02 * max(0, level - 4) * 2 ** (level * 0.3) # 0.02 * max(0, x - 4) * 2^0.3x + 0.1x
    
# Gets a wealth level based off per capita wealth.
# Returns an integer from 0 to 50 inclusive.
def get_wealth_level(pop: float, wealth: float) -> int:
    if wealth == 0:
        return 0
    
    per_capita = wealth / pop
    for level in range(50):
        if per_capita < wealth_req[level+1]:
            return level
        
    return 50

# Gets a shortage modifier based off wealth level.
# Higher wealth levels decreases shortage impact on the pop. It also applies a buffer.
# Eg. At wealth level 50, shortages only apply at a 50% shortage or higher. The effects of the shortage are also 50% less.
def get_shortage_modifier(wealth_level: int, shortage: float) -> float:
    buffer = wealth_level / 100.0
    effective_shortage = shortage - buffer

    if effective_shortage <= 0:
        return 0.0

    modifier = effective_shortage * (1.0 - (wealth_level / 100.0))
    return modifier

def get_land_productivity(land: float, workers: float, base_productivity: float = 2.0, saturation: float = 1.0, exp: float = 1.0) -> float:
    if land <= 0.0 or workers <= 0.0:
        return 0.0

    land_per_worker = land / workers
    productivity = base_productivity * ((land_per_worker / (land_per_worker + saturation)) ** exp)

    return productivity

# Calculates share of wages that goes to workers
# Workers count is the amount of pops that are working in that location of that type
# Jobs count includes all jobs (for that worker type) in the location
# Worker Owned Count is the amount considered owned by the workers. For all practical purposes that's subsistence workers only
def get_wage_share(workers_count: float, jobs_count: float, worker_owned_count: float, k: float = -8.0, d_crit: float = 1.0) -> float:
    if jobs_count <= 0.0:
        return 0.0
    
    pressure = (workers_count - 0.25 * worker_owned_count) / jobs_count if jobs_count > 0 else float('inf')
    wage_share_base = logistic_formula(pressure, k=k, x0=d_crit)
    wage_share = max(0.1, min(0.9, 0.75 * wage_share_base + 0.1)) # Ensure wage share is between 10% and 90%
    
    return wage_share

# This function calculates the job capacity based on a base capacity and factor
def get_job_capacity(base_capacity: float, jobs_factor: float = 1.0) -> float:
    if base_capacity <= 0.0 or jobs_factor <= 0.0:
        return 0.0
    return base_capacity * jobs_factor

# Output of industries
# Different industries may have different saturation/exponent values. Agriculture likely to just be 1, 1, but other sectors may be easier/harsher.
def get_output(jobs: float, workers: float, base_prod: float, saturation: float = 1.0, exp: float = 1.0, output_factor: float = 1.0) -> float:
    if jobs <= 0.0 or workers <= 0.0:
        return 0.0

    jobs_per_worker = jobs / workers
    productivity = base_prod * ((jobs_per_worker / (jobs_per_worker + saturation)) ** exp)
    return workers * productivity * output_factor

# Job pull for industries. Determines how workers get split. 
# Total job pull gets summed across all districts and workers are split accordingly.
# Basically just weighted so workers will go to districts that give them more money (which is essentially subsistence farming only)
def get_job_pull(jobs: float, wage_share: float = 1.0, pull_factor: float = 1.0) -> float:
    if wage_share < 0.01:
        wage_share = 0.01
    return jobs * wage_share * pull_factor

# Logistic formula for smooth transitions
# x - input value
# k - steepness of the curve
# x0 - the x value of the sigmoid's midpoint
def logistic_formula(x: float, k: float = 1.0, x0: float = 0.0) -> float:
    return 1 / (1 + math.exp(-k * (x - x0)))
    