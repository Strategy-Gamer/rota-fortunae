#include "economy/secular_cycle_system.h"

#include "core/rng.h"
#include "core/world.h"
#include "economy/economy_primitives.h"
#include "utility/FixedMath.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace rota::economy {
namespace {

using rota::countries::CountryID;
using rota::map::LocationID;
namespace rc = rota::countries;

Fixed64 milli(std::int64_t m) { return Fixed64::from_raw(m * 1000); }

const Fixed64 ZERO;
const Fixed64 ONE(1);
const Fixed64 CAPACITY(LOCATION_CAPACITY);

// Population & vital rates
const Fixed64 MAX_BIRTH = milli(35);
const Fixed64 MIN_BIRTH = milli(15);
const Fixed64 DEATH_BASE = milli(10);
const Fixed64 CHILD_MORTALITY = milli(400);
const Fixed64 SECURITY_RECOVERY = milli(20);
const Fixed64 SECURITY_FLOOR = milli(500);

// Economy: one elite-owned cultivated parcel fills each location's land
const Fixed64 CULTIVATED_FOOD = milli(2500);
const Fixed64 CULTIVATED_WEALTH = milli(1000);
const Fixed64 CULTIVATED_ELITE_OPPS = milli(20);
const Fixed64 WAGE_K = milli(-10000);
const Fixed64 WAGE_D_CRIT = milli(900);
const Fixed64 W_IMMIS_THRESH = milli(500);
const Fixed64 MIN_WAGE = Fixed64::from_raw(100);

// State fiscal; army base and military positions are per unit of carrying capacity
const Fixed64 TAX_RATE = milli(250);
const Fixed64 COLLECTION_UNREST = milli(500);
const Fixed64 ARMY_BASE = milli(6);
const Fixed64 K_SUPPRESS = milli(200);
const Fixed64 SUPPRESS_REACH = milli(5000);
const Fixed64 SUPPRESS_LEGIT_FLOOR = milli(500);
const Fixed64 MIL_POSITIONS = milli(10);
const Fixed64 K_PATRONAGE = milli(5000);
const Fixed64 K_ABSORB = milli(2000);
const Fixed64 TREASURY_YEARS = milli(5000);

// Legitimacy
const Fixed64 LEGIT_SPEED = milli(50);
const Fixed64 LEGIT_FLOOR = ZERO;
const Fixed64 LEGIT_INSECURITY = milli(150);
const Fixed64 LEGIT_UNREST = milli(300);
const Fixed64 LEGIT_UNPAID_ARMY = milli(500);
const Fixed64 LEGIT_UNPAID_PATRON = milli(250);
const Fixed64 LEGIT_FRAG = milli(300);
const Fixed64 LEGIT_FRAG_TOLERANCE = milli(1500);

// Unrest & phase
const Fixed64 W0_TARGET = milli(500);
const Fixed64 IBN_MARGIN = milli(2000);
const Fixed64 U_FRACTURE_ON = milli(250);
const Fixed64 U_EXIT_THRESH = milli(100);
const Fixed64 OVERPROD_EXIT = milli(1200);

// Elite ledger
const Fixed64 R_E_PREMIUM = milli(20);
const Fixed64 MU_0 = milli(2);
const Fixed64 MU_DOWN = milli(50);
const Fixed64 K_ATTRITION = milli(10);

// Crises
const Fixed64 CRISIS_FRAC_MULT = milli(4000);
const Fixed64 CRISIS_HAZARD_CAP = milli(500);
const Fixed64 K_FAMINE = milli(60);
const Fixed64 REVOLT_UNREST = milli(300);
const Fixed64 CIVILWAR_CULL = milli(10);
const Fixed64 COUP_CULL = milli(40);
const Fixed64 COUP_LEGIT_SHOCK = milli(400);
const Fixed64 WEARY_GAIN = milli(60);
const Fixed64 WEARY_RELAX = milli(20);
const Fixed64 WEARY_HIGH = milli(700);
const Fixed64 WEARY_LOW = milli(400);

struct CrisisConfig {
    Fixed64 base;
    std::uint32_t dmin, dmax, cool;
};

const CrisisConfig CRISIS_CFG[rc::CRISIS_COUNT] = {
    { milli(120), 4, 10, 10 }, // revolt
    { milli(100), 8, 20, 15 }, // civil war
    { milli(80),  1, 3,  12 }, // coup
};

// Keys for draws, so no two draws in a year share one
enum Stream : std::uint64_t {
    STREAM_POPULATION = 0,
    STREAM_ELITES = 1,
    STREAM_CRISIS_ROLL = 2,
    STREAM_CRISIS_DURATION = STREAM_CRISIS_ROLL + rc::CRISIS_COUNT,
    STREAM_CRISIS_COMMONERS = STREAM_CRISIS_DURATION + rc::CRISIS_COUNT,
    STREAM_CRISIS_ELITES = STREAM_CRISIS_COMMONERS + rc::CRISIS_COUNT,
};

// A location's intermediates for this year
struct Site {
    LocationID id;
    Fixed64 workers;      // commoners when the economy ran
    Fixed64 wage;         // Turchin's relative wage w
    Fixed64 immiseration;
    Fixed64 positions;    // local positions plus this location's share of the state's
};

struct Budget {
    Fixed64 baseline_positions;
    Fixed64 overproduction;      // elites over baseline (unfunded) positions
    Fixed64 felt_overproduction; // elites over funded positions
    Fixed64 suppression;
    Fixed64 army_shortfall;
    Fixed64 patronage_shortfall;
};

struct Year {
    rota::core::World& w;
    CountryID country;
    std::vector<Site>& sites;
    std::uint64_t year;

    std::uint64_t key(std::uint64_t stream, std::uint64_t id) const {
        return rota::core::keyed_hash(w.seed, year, stream, id);
    }
};

// Whole people from a fractional count: the fraction becomes one more person with that probability
std::uint32_t to_heads(Fixed64 people, std::uint64_t key, std::int64_t floor) {
    std::int64_t whole = people.raw_value() / Fixed64::SCALE;
    std::int64_t frac = people.raw_value() % Fixed64::SCALE;
    if (frac < 0) {
        whole -= 1;
        frac += Fixed64::SCALE;
    }
    if (static_cast<std::int64_t>(key % Fixed64::SCALE) < frac) whole += 1;
    return static_cast<std::uint32_t>(std::clamp<std::int64_t>(
        whole, floor, std::numeric_limits<std::uint32_t>::max()));
}

bool chance(Fixed64 p, std::uint64_t key) {
    return static_cast<std::int64_t>(key % Fixed64::SCALE) < p.raw_value();
}

Fixed64 heads(std::uint32_t n) { return Fixed64(static_cast<std::int64_t>(n)); }

template <typename F>
Fixed64 weighted_mean(const std::vector<Site>& sites, Fixed64 total_workers, F value) {
    Fixed64 sum;
    for (const Site& s : sites) sum += value(s) * s.workers;
    return sum / total_workers;
}

void step_population(const Year& y, Site& s) {
    auto& cyc = y.w.location_secular_cycle;
    auto& eco = y.w.location_economy;

    Fixed64 security = std::min(cyc.security[s.id] + SECURITY_RECOVERY, ONE - cyc.effective_unrest[s.id]);
    security = std::max(security, SECURITY_FLOOR);
    cyc.security[s.id] = security;

    // high fertility well under cap, smoothstep decline as density climbs; low security shrinks the effective cap
    const Fixed64 pop = heads(eco.commoners[s.id]);
    const Fixed64 rel = pop / (CAPACITY * security);
    Fixed64 t = std::clamp(rel - milli(500), ZERO, ONE);
    t = t * t * (Fixed64(3) - t * 2);
    const Fixed64 birth = std::max(MAX_BIRTH - (MAX_BIRTH - MIN_BIRTH) * t, MIN_BIRTH);
    const Fixed64 death = DEATH_BASE + CHILD_MORTALITY * birth;

    eco.commoners[s.id] = to_heads(pop + (birth - death) * pop, y.key(STREAM_POPULATION, s.id), 0);
}

void step_economy(const Year& y, Site& s) {
    auto& eco = y.w.location_economy;

    const Fixed64 workers = heads(std::max<std::uint32_t>(eco.commoners[s.id], 1));
    const Fixed64 elites = heads(eco.elites[s.id]);
    const Fixed64 jobs = CAPACITY;

    const Fixed64 wage_share = get_wage_share(workers, jobs, WAGE_K, WAGE_D_CRIT);
    const Fixed64 wealth = get_output(jobs, workers, CULTIVATED_WEALTH);
    const Fixed64 food = get_output(jobs, workers, CULTIVATED_FOOD);
    eco.commoner_wealth[s.id] = wealth * wage_share;
    eco.elite_wealth[s.id] = wealth - eco.commoner_wealth[s.id];

    const Fixed64 income_per_head = wealth / (workers + elites);
    s.wage = std::clamp((eco.commoner_wealth[s.id] / workers) / income_per_head, MIN_WAGE, ONE);

    // immiseration = low wage share + food shortfall (food need is 1 per head at every wealth level)
    const Fixed64 wage_penalty = std::max(ZERO, (W_IMMIS_THRESH - wage_share) / W_IMMIS_THRESH);
    const Fixed64 food_penalty = std::max(ZERO, ONE - food / workers);
    s.immiseration = std::min(ONE, wage_penalty + food_penalty);
    s.workers = workers;
}

Budget step_fiscal(const Year& y) {
    auto& state = y.w.country_state;
    const auto& eco = y.w.location_economy;
    const auto& cyc = y.w.location_secular_cycle;
    const CountryID c = y.country;

    Fixed64 total_income, collected, local_positions, elites, unrest_burden;
    for (const Site& s : y.sites) {
        const Fixed64 income = eco.commoner_wealth[s.id] + eco.elite_wealth[s.id];
        total_income += income;
        collected += income * (ONE - cyc.effective_unrest[s.id] * COLLECTION_UNREST);
        local_positions += local_elite_positions(y.w, s.id);
        elites += heads(eco.elites[s.id]);
        unrest_burden += cyc.effective_unrest[s.id] * s.workers;
    }
    const Fixed64 capacity = CAPACITY * static_cast<std::int64_t>(y.sites.size());
    const Fixed64 revenue = TAX_RATE * collected;

    Budget b;
    b.baseline_positions = local_positions + MIL_POSITIONS * capacity;
    b.overproduction = elites / b.baseline_positions;
    const Fixed64 excess_elites = std::max(ZERO, elites - b.baseline_positions);
    const Fixed64 absorb_fraction = ONE / (ONE + K_ABSORB * std::max(ZERO, b.overproduction - ONE)); // patronage can't keep pace
    const Fixed64 desired_patronage = excess_elites * absorb_fraction * K_PATRONAGE;
    // the state pays to hold down mobilization POTENTIAL, so overproduction drains even a solvent treasury
    const Fixed64 army_cost = ARMY_BASE * capacity + K_SUPPRESS * unrest_burden;

    const Fixed64 funds = state.treasury[c] + revenue;
    const Fixed64 army_paid = std::min(army_cost, funds); // army is paid before patronage
    const Fixed64 patronage_paid = std::max(ZERO, std::min(desired_patronage, funds - army_paid));
    const Fixed64 patronage_jobs = patronage_paid / K_PATRONAGE;
    b.army_shortfall = std::max(ZERO, army_cost - army_paid) / army_cost;
    b.patronage_shortfall = desired_patronage > ZERO
        ? std::max(ZERO, desired_patronage - patronage_paid) / desired_patronage
        : ZERO;

    const Fixed64 legit_effect = SUPPRESS_LEGIT_FLOOR + (ONE - SUPPRESS_LEGIT_FLOOR) * state.legitimacy[c];
    b.suppression = SUPPRESS_REACH * army_paid * legit_effect;

    const Fixed64 max_treasury = TREASURY_YEARS * TAX_RATE * total_income;
    state.treasury[c] = std::clamp(state.treasury[c] + revenue - army_paid - patronage_paid, ZERO, max_treasury);

    // felt overproduction is vs FUNDED positions: patronage placates the surplus until it goes unfunded
    state.state_positions[c] = MIL_POSITIONS * capacity + patronage_jobs;
    b.felt_overproduction = elites / (b.baseline_positions + patronage_jobs);

    // state positions are nationally competitive, so each location gets its elites' share
    for (Site& s : y.sites) {
        s.positions = local_elite_positions(y.w, s.id) + state.state_positions[c] * heads(eco.elites[s.id]) / elites;
    }
    return b;
}

void step_legitimacy(const Year& y, const Budget& b, Fixed64 security, Fixed64 effective_unrest) {
    Fixed64& legitimacy = y.w.country_state.legitimacy[y.country];

    const Fixed64 insecurity = std::max(ZERO, ONE - security);
    Fixed64 target = ONE
        - LEGIT_INSECURITY * insecurity
        - LEGIT_UNREST * effective_unrest
        - LEGIT_UNPAID_ARMY * b.army_shortfall
        - LEGIT_UNPAID_PATRON * b.patronage_shortfall
        - LEGIT_FRAG * std::max(ZERO, b.felt_overproduction - LEGIT_FRAG_TOLERANCE);
    target = std::clamp(target, LEGIT_FLOOR, ONE);
    legitimacy = std::clamp(legitimacy + LEGIT_SPEED * (target - legitimacy), ZERO, ONE);
}

void step_unrest(const Year& y, const Budget& b, Fixed64 total_workers) {
    auto& cyc = y.w.location_secular_cycle;
    const auto& eco = y.w.location_economy;
    const bool fracture = y.w.country_state.phase[y.country] == rc::PHASE_FRACTURE;

    for (const Site& s : y.sites) {
        const Fixed64 felt = heads(eco.elites[s.id]) / s.positions; // local felt overproduction
        Fixed64 u = cyc.unrest[s.id];
        if (!fracture) {
            u += milli(100) * (W0_TARGET - s.wage) + milli(100) * (felt - ONE);
        } else {
            u += milli(50) + milli(100) * (W0_TARGET - s.wage)
                + milli(200) * (felt - (cyc.effective_unrest[s.id] * 2 + milli(500)));
            if (felt < milli(1100)) u -= milli(100);
        }
        cyc.unrest[s.id] = std::clamp(u, ZERO, ONE);

        // suppression is per capita, so a big realm's coercion doesn't dwarf the bounded unrest
        cyc.effective_unrest[s.id] = std::clamp(cyc.unrest[s.id] - b.suppression / total_workers, ZERO, ONE);
    }
}

void apply_crisis(const Year& y, rc::CrisisType type, Fixed64 x) {
    auto& state = y.w.country_state;
    auto& eco = y.w.location_economy;
    auto& cyc = y.w.location_secular_cycle;
    const CountryID c = y.country;

    Fixed64 unrest_gain, commoner_loss, elite_loss;
    switch (type) {
        case rc::CRISIS_REVOLT:
            unrest_gain = REVOLT_UNREST * x;
            commoner_loss = milli(100) * K_FAMINE * x;
            elite_loss = milli(300) * CIVILWAR_CULL * x;
            break;
        case rc::CRISIS_CIVIL_WAR:
            unrest_gain = milli(500) * REVOLT_UNREST * x;
            commoner_loss = milli(200) * K_FAMINE * x;
            elite_loss = CIVILWAR_CULL * x;
            state.treasury[c] = state.treasury[c] * (ONE - milli(100) * x);
            break;
        case rc::CRISIS_COUP:
            elite_loss = COUP_CULL * x;
            state.legitimacy[c] = std::max(ZERO, state.legitimacy[c] - COUP_LEGIT_SHOCK * x);
            break;
        default:
            break;
    }

    // Every owned location bears the country's crisis alike
    for (const Site& s : y.sites) {
        cyc.unrest[s.id] = std::min(ONE, cyc.unrest[s.id] + unrest_gain);
        if (commoner_loss > ZERO) {
            eco.commoners[s.id] = to_heads(heads(eco.commoners[s.id]) * (ONE - commoner_loss),
                y.key(STREAM_CRISIS_COMMONERS + type, s.id), 0);
        }
        eco.elites[s.id] = to_heads(heads(eco.elites[s.id]) * (ONE - elite_loss),
            y.key(STREAM_CRISIS_ELITES + type, s.id), 1);
    }
}

void step_crises(const Year& y, const Budget& b, Fixed64 effective_unrest, Fixed64 immiseration) {
    auto& state = y.w.country_state;
    const CountryID c = y.country;
    auto& crises = state.crises[c];
    const std::uint8_t phase = state.phase[c];

    if (phase == rc::PHASE_PROSPERITY) { // no crises in prosperity; wind everything down
        for (rc::Crisis& cr : crises) {
            cr.active = 0;
            if (cr.cooldown > 0) cr.cooldown -= 1;
        }
        state.war_weariness[c] = std::max(ZERO, state.war_weariness[c] - WEARY_RELAX);
        state.in_lull[c] = state.in_lull[c] && state.war_weariness[c] > WEARY_LOW;
        return;
    }

    const Fixed64 op = std::max(ZERO, b.felt_overproduction - ONE);
    const Fixed64 low_legit = ONE - state.legitimacy[c];
    const Fixed64 drivers[rc::CRISIS_COUNT] = {
        milli(400) * immiseration + milli(400) * effective_unrest + milli(300) * low_legit,
        milli(500) * op + milli(500) * effective_unrest,
        milli(500) * op + milli(500) * low_legit,
    };
    const Fixed64 phase_mult = phase == rc::PHASE_STRAIN ? ONE : CRISIS_FRAC_MULT;

    // father-son hysteresis: past weary_high a lull ends active violence and blocks ignition until weariness bleeds below weary_low
    if (!state.in_lull[c] && state.war_weariness[c] >= WEARY_HIGH) {
        state.in_lull[c] = 1;
        for (std::uint8_t t = 0; t < rc::CRISIS_COUNT; t++) {
            if (crises[t].active) {
                crises[t].active = 0;
                crises[t].cooldown = CRISIS_CFG[t].cool;
            }
        }
    } else if (state.in_lull[c] && state.war_weariness[c] <= WEARY_LOW) {
        state.in_lull[c] = 0;
    }

    for (std::uint8_t t = 0; t < rc::CRISIS_COUNT; t++) {
        const rc::CrisisType type = static_cast<rc::CrisisType>(t);
        rc::Crisis& cr = crises[t];
        const CrisisConfig& cfg = CRISIS_CFG[t];
        if (cr.active) {
            cr.age += 1;
            apply_crisis(y, type, cr.intensity);
            if (cr.age >= cr.duration) {
                cr.active = 0;
                cr.cooldown = cfg.cool;
            }
        } else if (cr.cooldown > 0) {
            cr.cooldown -= 1;
        } else if (!state.in_lull[c]) {
            const Fixed64 drive = drivers[t];
            const Fixed64 p = std::min(CRISIS_HAZARD_CAP, cfg.base * drive * phase_mult);
            if (drive > ZERO && chance(p, y.key(STREAM_CRISIS_ROLL + t, c))) {
                cr.active = 1;
                cr.age = 0;
                cr.intensity = std::min(ONE, drive * (phase == rc::PHASE_STRAIN ? milli(500) : ONE)); // milder in strain
                cr.duration = cfg.dmin + static_cast<std::uint32_t>(
                    y.key(STREAM_CRISIS_DURATION + t, c) % (cfg.dmax - cfg.dmin + 1));
                apply_crisis(y, type, cr.intensity);
            }
        }
    }

    Fixed64 violent_load;
    for (const rc::Crisis& cr : crises) {
        if (cr.active) violent_load += cr.intensity;
    }
    state.war_weariness[c] = std::max(ZERO, state.war_weariness[c] - WEARY_RELAX) + WEARY_GAIN * violent_load;
}

void step_elites(const Year& y) {
    auto& eco = y.w.location_economy;
    const auto& cyc = y.w.location_secular_cycle;
    const Fixed64 legitimacy = y.w.country_state.legitimacy[y.country];

    for (const Site& s : y.sites) {
        const Fixed64 elites = heads(eco.elites[s.id]);

        const Fixed64 e_biological = elites * legitimacy * R_E_PREMIUM;

        Fixed64 e_mobility;
        const Fixed64 misery = (W0_TARGET - s.wage) / s.wage;
        if (misery < ZERO) {
            const Fixed64 underprod = std::min(ONE, elites / s.positions); // few elites left -> curb demotion
            e_mobility = elites * MU_DOWN * misery * underprod;
        } else {
            e_mobility = s.workers * MU_0 * misery;
        }

        const Fixed64 u_e = cyc.effective_unrest[s.id];
        const Fixed64 e_attrition = elites * K_ATTRITION * u_e * rota::fmath::sqrt(u_e);

        eco.elites[s.id] = to_heads(elites + e_biological + e_mobility - e_attrition,
            y.key(STREAM_ELITES, s.id), 1);
    }
}

void step_phase(const Year& y, const Budget& b, Fixed64 wage, Fixed64 effective_unrest) {
    auto& state = y.w.country_state;
    const CountryID c = y.country;

    Fixed64 elites;
    for (const Site& s : y.sites) elites += heads(y.w.location_economy.elites[s.id]);

    if (state.phase[c] == rc::PHASE_PROSPERITY) {
        const bool traditional = wage < W0_TARGET && elites > b.baseline_positions;
        const bool ibn_khaldun = b.overproduction > IBN_MARGIN;
        if (traditional || ibn_khaldun) state.phase[c] = rc::PHASE_STRAIN;
    } else if (state.phase[c] == rc::PHASE_STRAIN) {
        const bool fiscal_break = state.treasury[c] <= ZERO && b.army_shortfall > ZERO;
        if (effective_unrest > U_FRACTURE_ON || fiscal_break) state.phase[c] = rc::PHASE_FRACTURE;
    } else if (b.overproduction < OVERPROD_EXIT && effective_unrest < U_EXIT_THRESH) {
        state.phase[c] = rc::PHASE_PROSPERITY;
    }
}

void step_country(const Year& y) {
    const auto& cyc = y.w.location_secular_cycle;

    for (Site& s : y.sites) step_population(y, s);
    for (Site& s : y.sites) step_economy(y, s);

    Fixed64 total_workers;
    for (const Site& s : y.sites) total_workers += s.workers;
    const auto mean = [&](auto value) { return weighted_mean(y.sites, total_workers, value); };
    const Fixed64 wage = mean([](const Site& s) { return s.wage; });
    const Fixed64 immiseration = mean([](const Site& s) { return s.immiseration; });

    const Budget b = step_fiscal(y);
    step_legitimacy(y, b,
        mean([&](const Site& s) { return cyc.security[s.id]; }),
        mean([&](const Site& s) { return cyc.effective_unrest[s.id]; }));

    step_unrest(y, b, total_workers);
    const Fixed64 effective_unrest = mean([&](const Site& s) { return cyc.effective_unrest[s.id]; });

    step_crises(y, b, effective_unrest, immiseration);
    step_elites(y);
    step_phase(y, b, wage, effective_unrest);
}

}

Fixed64 local_elite_positions(const rota::core::World&, LocationID) {
    return CULTIVATED_ELITE_OPPS * CAPACITY;
}

void run_secular_cycle(rota::core::World& world) {
    const CountryID slot_count = static_cast<CountryID>(world.countries.alive.size());
    const LocationID location_count = world.geography.location_count();

    // Unowned land sits out the cycle until a country owns it
    std::vector<std::vector<Site>> sites(slot_count);
    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID owner = world.location_politics.get_owner(loc);
        if (world.countries.is_valid(owner)) sites[owner].push_back(Site{ loc });
    }

    for (CountryID c = 0; c < slot_count; c++) {
        if (sites[c].empty()) continue;
        step_country(Year{ world, c, sites[c], world.calendar.get_year() });
    }
}

}
