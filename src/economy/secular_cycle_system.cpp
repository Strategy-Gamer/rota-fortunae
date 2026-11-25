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
};

// One country's year. A location touches all of its owner's fields at once, so they sit together.
struct CountryYear {
    // Summed by the population/economy pass; means are weighted by commoners
    std::int64_t locations = 0;
    std::int64_t workers = 0;
    std::int64_t elites = 0;
    Fixed64 income;
    Fixed64 collected;
    Fixed64 unrest_burden; // last year's U_e x commoners
    Fixed64 wage_sum;
    Fixed64 immiseration_sum;
    Fixed64 security_sum;

    // The budget, handed down to the later passes
    Fixed64 baseline_positions;
    Fixed64 overproduction;      // elites over baseline (unfunded) positions
    Fixed64 felt_overproduction; // elites over funded positions
    Fixed64 army_shortfall;
    Fixed64 suppression_per_worker;
    Fixed64 state_share;         // state positions per elite, since they're nationally competitive
    bool fracture = false;       // last year's phase, read by the unrest pass

    Fixed64 effective_unrest_sum;

    // This year's crises, folded into one effect on every owned location
    Fixed64 commoner_keep = Fixed64(1);
    Fixed64 elite_keep = Fixed64(1);
    Fixed64 unrest_gain;

    std::int64_t elites_after = 0;
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

Fixed64 heads(std::int64_t n) { return Fixed64(n); }

// Heads per unit of a location's carrying capacity
Fixed64 per_capacity(std::int64_t n) { return Fixed64::from_raw(n * Fixed64::SCALE / LOCATION_CAPACITY); }

// The two curves the location passes evaluate every year, sampled once
const rota::fmath::SampledCurve& wage_share_curve() {
    static const rota::fmath::SampledCurve curve(milli(3000), 1200,
        [](Fixed64 pressure) { return get_wage_share(pressure, WAGE_K, WAGE_D_CRIT); });
    return curve;
}

const rota::fmath::SampledCurve& attrition_curve() { // U_e^1.5
    static const rota::fmath::SampledCurve curve(ONE, 1000,
        [](Fixed64 u) { return u * rota::fmath::sqrt(u); });
    return curve;
}

// The divisor of every per-capita mean; FinalSim floors commoners at 1e-8, here at one person
std::int64_t workers_at(const LocationEconomy& eco, LocationID loc) {
    return std::max<std::int64_t>(eco.commoners[loc], 1);
}

struct Pass {
    rota::core::World& w;
    std::vector<CountryYear>& countries;
    std::vector<Fixed64>& wage; // per LocationID: Turchin's relative wage w
    std::uint64_t year;

    std::uint64_t key(std::uint64_t stream, std::uint64_t id) const {
        return rota::core::keyed_hash(w.seed, year, stream, id);
    }

    // Unowned land sits out the cycle
    bool owned(LocationID loc, CountryID& c) const {
        c = w.location_politics.owner_country[loc];
        return w.countries.is_valid(c);
    }
};

void population_and_economy(const Pass& p) {
    auto& eco = p.w.location_economy;
    auto& cyc = p.w.location_secular_cycle;
    const auto& wage_share_at = wage_share_curve();
    const LocationID location_count = p.w.geography.location_count();

    for (LocationID loc = 0; loc < location_count; loc++) {
        CountryID c;
        if (!p.owned(loc, c)) continue;
        CountryYear& cy = p.countries[c];

        Fixed64 security = std::min(cyc.security[loc] + SECURITY_RECOVERY, ONE - cyc.effective_unrest[loc]);
        security = std::max(security, SECURITY_FLOOR);
        cyc.security[loc] = security;

        // high fertility well under cap, smoothstep decline as density climbs; low security shrinks the effective cap
        const std::int64_t pop = eco.commoners[loc];
        const Fixed64 rel = per_capacity(pop) / security;
        Fixed64 t = std::clamp(rel - milli(500), ZERO, ONE);
        t = t * t * (Fixed64(3) - t * 2);
        const Fixed64 birth = std::max(MAX_BIRTH - (MAX_BIRTH - MIN_BIRTH) * t, MIN_BIRTH);
        const Fixed64 death = DEATH_BASE + CHILD_MORTALITY * birth;
        eco.commoners[loc] = to_heads(heads(pop) + (birth - death) * pop, p.key(STREAM_POPULATION, loc), 0);

        const std::int64_t workers = workers_at(eco, loc);
        const std::int64_t elites = eco.elites[loc];

        const Fixed64 wage_share = wage_share_at(per_capacity(workers)); // jobs = capacity
        const Fixed64 output = get_output(LOCATION_CAPACITY, workers);
        const Fixed64 wealth = output * CULTIVATED_WEALTH;
        eco.commoner_wealth[loc] = wealth * wage_share;
        eco.elite_wealth[loc] = wealth - eco.commoner_wealth[loc];

        // relative wage: commoner income per head over everyone's, which is the wage share scaled by heads
        const Fixed64 w = std::clamp(wage_share * (workers + elites) / workers, MIN_WAGE, ONE);
        p.wage[loc] = w;

        // immiseration = low wage share + food shortfall (food need is 1 per head at every wealth level)
        const Fixed64 wage_penalty = std::max(ZERO, (W_IMMIS_THRESH - wage_share) / W_IMMIS_THRESH);
        const Fixed64 food_penalty = std::max(ZERO, ONE - output * CULTIVATED_FOOD / workers);
        const Fixed64 immiseration = std::min(ONE, wage_penalty + food_penalty);

        const Fixed64 last_unrest = cyc.effective_unrest[loc];
        cy.locations += 1;
        cy.workers += workers;
        cy.elites += elites;
        cy.income += wealth;
        cy.collected += wealth * (ONE - last_unrest * COLLECTION_UNREST);
        cy.unrest_burden += last_unrest * workers;
        cy.wage_sum += w * workers;
        cy.immiseration_sum += immiseration * workers;
        cy.security_sum += security * workers;
    }
}

void fiscal_and_legitimacy(rota::core::World& w, CountryID c, CountryYear& cy) {
    auto& state = w.country_state;

    // tax the wealth base (collection gated by unrest), fund army then patronage, run a capped treasury
    const Fixed64 capacity = CAPACITY * cy.locations;
    const Fixed64 revenue = TAX_RATE * cy.collected;

    cy.baseline_positions = CULTIVATED_ELITE_OPPS * capacity + MIL_POSITIONS * capacity;
    cy.overproduction = cy.elites / cy.baseline_positions;
    const Fixed64 excess_elites = std::max(ZERO, cy.elites - cy.baseline_positions);
    const Fixed64 absorb_fraction = ONE / (ONE + K_ABSORB * std::max(ZERO, cy.overproduction - ONE)); // patronage can't keep pace
    const Fixed64 desired_patronage = excess_elites * absorb_fraction * K_PATRONAGE;
    // the state pays to hold down mobilization POTENTIAL, so overproduction drains even a solvent treasury
    const Fixed64 army_cost = ARMY_BASE * capacity + K_SUPPRESS * cy.unrest_burden;

    const Fixed64 funds = state.treasury[c] + revenue;
    const Fixed64 army_paid = std::min(army_cost, funds); // army is paid before patronage
    const Fixed64 patronage_paid = std::max(ZERO, std::min(desired_patronage, funds - army_paid));
    const Fixed64 patronage_jobs = patronage_paid / K_PATRONAGE;
    cy.army_shortfall = std::max(ZERO, army_cost - army_paid) / army_cost;
    const Fixed64 patronage_shortfall = desired_patronage > ZERO
        ? std::max(ZERO, desired_patronage - patronage_paid) / desired_patronage
        : ZERO;

    const Fixed64 legit_effect = SUPPRESS_LEGIT_FLOOR + (ONE - SUPPRESS_LEGIT_FLOOR) * state.legitimacy[c];
    const Fixed64 suppression = SUPPRESS_REACH * army_paid * legit_effect;
    cy.suppression_per_worker = suppression / cy.workers; // per capita, so a big realm's coercion doesn't dwarf the bounded unrest

    const Fixed64 max_treasury = TREASURY_YEARS * TAX_RATE * cy.income;
    state.treasury[c] = std::clamp(state.treasury[c] + revenue - army_paid - patronage_paid, ZERO, max_treasury);

    // felt overproduction is vs FUNDED positions: patronage placates the surplus until it goes unfunded
    state.state_positions[c] = MIL_POSITIONS * capacity + patronage_jobs;
    cy.state_share = state.state_positions[c] / cy.elites;
    cy.felt_overproduction = cy.elites / (cy.baseline_positions + patronage_jobs);
    cy.fracture = state.phase[c] == rc::PHASE_FRACTURE;

    // legitimacy relaxes toward a target pulled down by insecurity, unrest, unpaid obligations and overproduction
    Fixed64& legitimacy = state.legitimacy[c];
    const Fixed64 insecurity = std::max(ZERO, ONE - cy.security_sum / cy.workers);
    Fixed64 target = ONE
        - LEGIT_INSECURITY * insecurity
        - LEGIT_UNREST * (cy.unrest_burden / cy.workers)
        - LEGIT_UNPAID_ARMY * cy.army_shortfall
        - LEGIT_UNPAID_PATRON * patronage_shortfall
        - LEGIT_FRAG * std::max(ZERO, cy.felt_overproduction - LEGIT_FRAG_TOLERANCE);
    target = std::clamp(target, LEGIT_FLOOR, ONE);
    legitimacy = std::clamp(legitimacy + LEGIT_SPEED * (target - legitimacy), ZERO, ONE);
}

void unrest(const Pass& p) {
    auto& cyc = p.w.location_secular_cycle;
    const auto& eco = p.w.location_economy;
    const LocationID location_count = p.w.geography.location_count();

    for (LocationID loc = 0; loc < location_count; loc++) {
        CountryID c;
        if (!p.owned(loc, c)) continue;
        CountryYear& cy = p.countries[c];

        const std::int64_t elites = eco.elites[loc];
        const Fixed64 felt = elites / (local_elite_positions(p.w, loc) + cy.state_share * elites); // local felt overproduction
        Fixed64 u = cyc.unrest[loc];
        if (!cy.fracture) {
            u += milli(100) * (W0_TARGET - p.wage[loc]) + milli(100) * (felt - ONE);
        } else {
            u += milli(50) + milli(100) * (W0_TARGET - p.wage[loc])
                + milli(200) * (felt - (cyc.effective_unrest[loc] * 2 + milli(500)));
            if (felt < milli(1100)) u -= milli(100);
        }
        cyc.unrest[loc] = std::clamp(u, ZERO, ONE);
        cyc.effective_unrest[loc] = std::clamp(cyc.unrest[loc] - cy.suppression_per_worker, ZERO, ONE);

        cy.effective_unrest_sum += cyc.effective_unrest[loc] * workers_at(eco, loc);
    }
}

// Folds one active crisis-year into the effect every owned location will bear
void apply_crisis(rota::core::World& w, CountryID c, CountryYear& cy, rc::CrisisType type, Fixed64 x) {
    auto& state = w.country_state;
    switch (type) {
        case rc::CRISIS_REVOLT:
            cy.unrest_gain += REVOLT_UNREST * x;
            cy.commoner_keep = cy.commoner_keep * (ONE - milli(100) * K_FAMINE * x);
            cy.elite_keep = cy.elite_keep * (ONE - milli(300) * CIVILWAR_CULL * x);
            break;
        case rc::CRISIS_CIVIL_WAR:
            cy.unrest_gain += milli(500) * REVOLT_UNREST * x;
            cy.commoner_keep = cy.commoner_keep * (ONE - milli(200) * K_FAMINE * x);
            cy.elite_keep = cy.elite_keep * (ONE - CIVILWAR_CULL * x);
            state.treasury[c] = state.treasury[c] * (ONE - milli(100) * x);
            break;
        case rc::CRISIS_COUP:
            cy.elite_keep = cy.elite_keep * (ONE - COUP_CULL * x);
            state.legitimacy[c] = std::max(ZERO, state.legitimacy[c] - COUP_LEGIT_SHOCK * x);
            break;
        default:
            break;
    }
}

void crises(rota::core::World& w, std::uint64_t year, CountryID c, CountryYear& cy) {
    auto& state = w.country_state;
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

    const Fixed64 effective_unrest = cy.effective_unrest_sum / cy.workers;
    const Fixed64 immiseration = cy.immiseration_sum / cy.workers;
    const Fixed64 op = std::max(ZERO, cy.felt_overproduction - ONE);
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
            apply_crisis(w, c, cy, type, cr.intensity);
            if (cr.age >= cr.duration) {
                cr.active = 0;
                cr.cooldown = cfg.cool;
            }
        } else if (cr.cooldown > 0) {
            cr.cooldown -= 1;
        } else if (!state.in_lull[c]) {
            const Fixed64 drive = drivers[t];
            const Fixed64 p = std::min(CRISIS_HAZARD_CAP, cfg.base * drive * phase_mult);
            if (drive > ZERO && chance(p, rota::core::keyed_hash(w.seed, year, STREAM_CRISIS_ROLL + t, c))) {
                cr.active = 1;
                cr.age = 0;
                cr.intensity = std::min(ONE, drive * (phase == rc::PHASE_STRAIN ? milli(500) : ONE)); // milder in strain
                cr.duration = cfg.dmin + static_cast<std::uint32_t>(
                    rota::core::keyed_hash(w.seed, year, STREAM_CRISIS_DURATION + t, c) % (cfg.dmax - cfg.dmin + 1));
                apply_crisis(w, c, cy, type, cr.intensity);
            }
        }
    }

    Fixed64 violent_load;
    for (const rc::Crisis& cr : crises) {
        if (cr.active) violent_load += cr.intensity;
    }
    state.war_weariness[c] = std::max(ZERO, state.war_weariness[c] - WEARY_RELAX) + WEARY_GAIN * violent_load;
}

void crisis_effects_and_elites(const Pass& p) {
    auto& eco = p.w.location_economy;
    auto& cyc = p.w.location_secular_cycle;
    const auto& legitimacy = p.w.country_state.legitimacy;
    const auto& attrition = attrition_curve();
    const LocationID location_count = p.w.geography.location_count();

    for (LocationID loc = 0; loc < location_count; loc++) {
        CountryID c;
        if (!p.owned(loc, c)) continue;
        CountryYear& cy = p.countries[c];

        // workers and positions as the economy and budget saw them, before this year's crises
        const std::int64_t workers = workers_at(eco, loc);
        const std::int64_t elites_before = eco.elites[loc];
        const Fixed64 positions = local_elite_positions(p.w, loc) + cy.state_share * elites_before;

        cyc.unrest[loc] = std::min(ONE, cyc.unrest[loc] + cy.unrest_gain);
        if (cy.commoner_keep < ONE) {
            eco.commoners[loc] = to_heads(cy.commoner_keep * static_cast<std::int64_t>(eco.commoners[loc]),
                p.key(STREAM_CRISIS_COMMONERS, loc), 0);
        }
        const Fixed64 elites = cy.elite_keep * elites_before;

        // build by reproduction + upward mobility, clear by culling + downward mobility
        const Fixed64 e_biological = elites * legitimacy[c] * R_E_PREMIUM;

        Fixed64 e_mobility;
        const Fixed64 misery = (W0_TARGET - p.wage[loc]) / p.wage[loc];
        if (misery < ZERO) {
            const Fixed64 underprod = std::min(ONE, elites / positions); // few elites left -> curb demotion
            e_mobility = elites * MU_DOWN * misery * underprod;
        } else {
            e_mobility = MU_0 * workers * misery;
        }

        const Fixed64 e_attrition = elites * K_ATTRITION * attrition(cyc.effective_unrest[loc]);

        eco.elites[loc] = to_heads(elites + e_biological + e_mobility - e_attrition, p.key(STREAM_ELITES, loc), 1);
        cy.elites_after += eco.elites[loc];
    }
}

void phase(rota::core::World& w, CountryID c, const CountryYear& cy) {
    auto& state = w.country_state;
    const Fixed64 wage = cy.wage_sum / cy.workers;
    const Fixed64 effective_unrest = cy.effective_unrest_sum / cy.workers;

    if (state.phase[c] == rc::PHASE_PROSPERITY) {
        const bool traditional = wage < W0_TARGET && heads(cy.elites_after) > cy.baseline_positions;
        const bool ibn_khaldun = cy.overproduction > IBN_MARGIN;
        if (traditional || ibn_khaldun) state.phase[c] = rc::PHASE_STRAIN;
    } else if (state.phase[c] == rc::PHASE_STRAIN) {
        const bool fiscal_break = state.treasury[c] <= ZERO && cy.army_shortfall > ZERO;
        if (effective_unrest > U_FRACTURE_ON || fiscal_break) state.phase[c] = rc::PHASE_FRACTURE;
    } else if (cy.overproduction < OVERPROD_EXIT && effective_unrest < U_EXIT_THRESH) {
        state.phase[c] = rc::PHASE_PROSPERITY;
    }
}

}

Fixed64 local_elite_positions(const rota::core::World&, LocationID) {
    return CULTIVATED_ELITE_OPPS * CAPACITY;
}

Fixed64 elite_positions(const rota::core::World& world, LocationID loc, std::int64_t country_elites) {
    const Fixed64 local = local_elite_positions(world, loc);
    const CountryID owner = world.location_politics.owner_country[loc];
    if (!world.countries.is_valid(owner) || country_elites <= 0) return local;
    const std::int64_t elites = world.location_economy.elites[loc];
    return local + world.country_state.state_positions[owner] * elites / country_elites;
}

Fixed64 population_pressure_gauge(std::int64_t commoners, std::int64_t capacity) {
    if (capacity <= 0) return ZERO;
    const Fixed64 rel = Fixed64::from_raw(commoners * Fixed64::SCALE / capacity);
    return rota::fmath::logistic(rel, Fixed64(3), ONE); // gentle slope, so it tracks real population in a Turchin-like band
}

Fixed64 elite_overproduction_gauge(Fixed64 elites_per_position) {
    return std::clamp((elites_per_position - ONE) / 2, ZERO, ONE); // a surplus of 2x positions reads as 1
}

// FinalSim's step order as linear passes: locations stream through their arrays in LocationID order,
// reading and summing into their owner's CountryYear row, and the countries run between those passes.
void run_secular_cycle(rota::core::World& world) {
    const CountryID slot_count = static_cast<CountryID>(world.countries.alive.size());
    std::vector<CountryYear> countries(slot_count);
    std::vector<Fixed64> wage(world.geography.location_count());
    const Pass pass{ world, countries, wage, world.calendar.get_year() };

    const auto each_country = [&](auto step) {
        for (CountryID c = 0; c < slot_count; c++) {
            if (countries[c].locations > 0) step(c, countries[c]);
        }
    };

    population_and_economy(pass);
    each_country([&](CountryID c, CountryYear& cy) { fiscal_and_legitimacy(world, c, cy); });
    unrest(pass);
    each_country([&](CountryID c, CountryYear& cy) { crises(world, pass.year, c, cy); });
    crisis_effects_and_elites(pass);
    each_country([&](CountryID c, CountryYear& cy) { phase(world, c, cy); });
}

}
