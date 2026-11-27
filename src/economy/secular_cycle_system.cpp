#include "economy/secular_cycle_system.h"

#include "core/rng.h"
#include "core/world.h"
#include "countries/civil_war.h"
#include "countries/control.h"
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

const Fixed64 ZERO;
const Fixed64 ONE(1);
const Fixed64 FULL_PRESSURE = milli(1250); // the gauge's 1, in commoners per head of capacity: above 0.8 the land is overfull

// Population & vital rates
const Fixed64 MAX_BIRTH = milli(35);
const Fixed64 MIN_BIRTH = milli(8); // below the deaths it brings, so births alone can shrink a people
const Fixed64 DEATH_BASE = milli(10);
const Fixed64 CHILD_MORTALITY = milli(400);
const Fixed64 FERTILITY_ADJUST = milli(50); // a year's step toward what conditions call for: the parents were born a generation ago
const Fixed64 SECURITY_RECOVERY = milli(50);
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
const Fixed64 COLLECTION_RADICALISM = milli(500);
const Fixed64 ARMY_BASE = milli(6);
const Fixed64 K_SUPPRESS = milli(200);
const Fixed64 SUPPRESS_REACH = milli(2500);
const Fixed64 SUPPRESS_LEGIT_FLOOR = milli(500);
const Fixed64 MIL_POSITIONS = milli(10);
const Fixed64 K_PATRONAGE = milli(5000);
const Fixed64 K_ABSORB = milli(2000);
const Fixed64 TREASURY_YEARS = milli(5000);

// Legitimacy
const Fixed64 LEGIT_SPEED = milli(100);
const Fixed64 LEGIT_FLOOR = ZERO;
const Fixed64 LEGIT_INSECURITY = milli(150);
const Fixed64 LEGIT_RADICALISM = milli(600);
const Fixed64 LEGIT_UNPAID_ARMY = milli(500);
const Fixed64 LEGIT_UNPAID_PATRON = milli(250);
const Fixed64 LEGIT_FRAG = milli(300);
const Fixed64 LEGIT_FRAG_TOLERANCE = milli(1500);
const Fixed64 LEGIT_DIVIDED_ELITES = milli(500); // a state can't regain its standing while its elites are at odds

// Satisfaction falls fast and recovers slowly, more slowly still while its causes persist
const Fixed64 SATISFACTION_FALL = milli(100);
const Fixed64 SATISFACTION_RISE = milli(30);
const Fixed64 ELITE_SATISFACTION_RISE = milli(150); // elites' own: once the surplus has gone, competition for positions ends
const Fixed64 SATISFACTION_STICK = milli(500); // no recovery until the target clears this
const Fixed64 SATISFIED_WAGE = milli(600); // commoners are wholly satisfied at this relative wage or above
const Fixed64 MISERABLE_WAGE = milli(120); // and wholly dissatisfied at this or below
const Fixed64 TURMOIL_DISCONTENT = milli(400); // commoners lose faith in a society growing radical, per unit of radicalism
const Fixed64 ELITE_DISCONTENT = milli(1000); // per elite over each felt position
const Fixed64 DISCONTENT_CAPACITY = milli(300); // share of the land's capacity a wholly dissatisfied people fills

// Radicalism
const Fixed64 RADICALISM_RATE = milli(200);
const Fixed64 RADICALISM_FADE = milli(400); // the rate down, once the grievance has passed: exhaustion
const Fixed64 RADICAL_COMMONERS = milli(250); // each class's weight in the country's discontent: elites carry the power
const Fixed64 RADICAL_ELITES = milli(1000);
const Fixed64 RADICAL_TOLERANCE = milli(400); // discontent below this stays within ordinary politics
const Fixed64 RADICAL_LEGITIMACY = milli(800); // share of the discontent a fully legitimate state keeps from radicalism
const Fixed64 VIOLENCE_ON = milli(400); // below this, political activity is advocacy and protest

// Phase
const Fixed64 W0_TARGET = milli(500);
const Fixed64 IBN_MARGIN = milli(2000);
const Fixed64 FRACTURE_RADICALISM = milli(250);
const Fixed64 EXIT_RADICALISM = milli(100);
const Fixed64 OVERPROD_EXIT = milli(1200);

// Elite ledger
const Fixed64 R_E_PREMIUM = milli(20);
const Fixed64 MU_0 = Fixed64::from_raw(500);
const Fixed64 MU_DOWN = milli(50);
const Fixed64 K_ATTRITION = milli(10);

// Crises
const Fixed64 CRISIS_FRAC_MULT = milli(1500);
const Fixed64 CRISIS_HAZARD_CAP = milli(500);
const Fixed64 K_FAMINE = milli(60);
const Fixed64 REVOLT_RADICALISM = milli(300);
const Fixed64 CIVILWAR_CULL = milli(10);
const Fixed64 COUP_CULL = milli(20);
const Fixed64 COUP_LEGIT_SHOCK = milli(200);
const Fixed64 WEARY_GAIN = milli(60);
const Fixed64 WEARY_RELAX = milli(20);
const Fixed64 WEARY_HIGH = milli(700);
const Fixed64 WEARY_LOW = milli(400);

struct CrisisConfig {
    Fixed64 base;
    std::uint32_t dmin, dmax, cool;
};

const CrisisConfig CRISIS_CFG[rc::CRISIS_COUNT] = {
    { milli(20),  4, 10, 10 }, // revolt
    { milli(40),  8, 20, 15 }, // civil war
    { milli(10),  1, 3,  12 }, // coup
};

// One country's year. A location touches all of its owner's fields at once, so they sit together.
struct CountryYear {
    // Summed by the population/economy pass; means are weighted by commoners
    std::int64_t locations = 0;
    std::int64_t capacity = 0;
    std::int64_t workers = 0;
    std::int64_t elites = 0;
    Fixed64 reached_elites; // elites weighted by control, who compete for the state's positions
    Fixed64 income;
    Fixed64 collected;
    Fixed64 radicalism_burden; // last year's effective radicalism x commoners
    Fixed64 reached_radicalism; // the same, weighted by control: what the army can be sent to put down
    Fixed64 wage_sum;
    Fixed64 immiseration_sum;
    Fixed64 security_sum;
    Fixed64 elite_discontent_last; // last year's elite dissatisfaction x elites

    // The budget, handed down to the later passes
    Fixed64 baseline_positions;
    Fixed64 overproduction;      // elites over baseline (unfunded) positions
    Fixed64 felt_overproduction; // elites over funded positions
    Fixed64 army_shortfall;
    Fixed64 suppression_per_worker;
    Fixed64 state_share;         // state positions per reached elite: nationally competitive, but the frontier is out of reach

    // Summed by the satisfaction pass, weighted by each class's heads
    Fixed64 commoner_discontent_sum;
    Fixed64 elite_discontent_sum;
    Fixed64 commoner_target_sum;
    Fixed64 elite_target_sum;
    Fixed64 discontent; // both classes' mean dissatisfaction, by their political weight
    Fixed64 radicalism_target;

    Fixed64 effective_radicalism_sum;
    Fixed64 violence_sum;

    // This year's crises, folded into one effect on every owned location
    Fixed64 commoner_keep = Fixed64(1);
    Fixed64 elite_keep = Fixed64(1);

    std::int64_t elites_after = 0;

    CycleRecord record; // this year's flows, for the ledger
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

// Heads per head of a location's capacity
Fixed64 per_capacity(std::int64_t n, std::int64_t capacity) { return Fixed64::from_raw(n * Fixed64::SCALE / capacity); }

// The two curves the location passes evaluate every year, sampled once
const rota::fmath::SampledCurve& wage_share_curve() {
    static const rota::fmath::SampledCurve curve(milli(3000), 1200,
        [](Fixed64 pressure) { return get_wage_share(pressure, WAGE_K, WAGE_D_CRIT); });
    return curve;
}

const rota::fmath::SampledCurve& attrition_curve() { // violence^1.5
    static const rota::fmath::SampledCurve curve(ONE, 1000,
        [](Fixed64 u) { return u * rota::fmath::sqrt(u); });
    return curve;
}

// Political activity past the threshold of violence: riots, clashes, low-level insurgency
Fixed64 violence(Fixed64 radicalism) {
    return std::max(ZERO, radicalism - VIOLENCE_ON) / (ONE - VIOLENCE_ON);
}

// The divisor of every per-capita mean; FinalSim floors commoners at 1e-8, here at one person
std::int64_t workers_at(const LocationEconomy& eco, LocationID loc) {
    return std::max<std::int64_t>(eco.commoners[loc], 1);
}

struct Pass {
    rota::core::World& w;
    std::vector<CountryYear>& countries;
    std::vector<Fixed64>& wage; // per LocationID: Turchin's relative wage w
    const std::vector<Fixed64>& control; // per LocationID, from this year's start
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

        const Fixed64 last_radicalism = cyc.effective_radicalism[loc];
        Fixed64 security = std::min(cyc.security[loc] + SECURITY_RECOVERY, ONE - violence(last_radicalism));
        security = std::max(security, SECURITY_FLOOR);
        cyc.security[loc] = security;

        // insecurity and dissatisfaction shrink the effective cap, mild dissatisfaction hardly at all; fertility falls
        // by smoothstep as density nears it, and follows a generation late
        const std::int64_t pop = eco.commoners[loc];
        const std::int64_t capacity = eco.capacity[loc];
        const Fixed64 discontent = ONE - cyc.commoner_satisfaction[loc];
        const Fixed64 carried = security * (DISCONTENT_CAPACITY + (ONE - DISCONTENT_CAPACITY) * (ONE - discontent * discontent));
        const Fixed64 rel = per_capacity(pop, capacity) / carried;
        Fixed64 t = std::clamp(rel - milli(500), ZERO, ONE);
        t = t * t * (Fixed64(3) - t * 2);
        Fixed64& fertility = cyc.fertility[loc];
        fertility += FERTILITY_ADJUST * (ONE - t - fertility);
        const Fixed64 birth = MIN_BIRTH + (MAX_BIRTH - MIN_BIRTH) * fertility;
        const Fixed64 natural_death = DEATH_BASE + CHILD_MORTALITY * birth;
        eco.commoners[loc] = to_heads(Fixed64(pop) + (birth - natural_death) * pop, p.key(rota::core::STREAM_POPULATION, loc), 0);
        cy.record.births += birth * pop;
        cy.record.natural_deaths += natural_death * pop;

        const std::int64_t workers = workers_at(eco, loc);
        const std::int64_t elites = eco.elites[loc];

        const Fixed64 wage_share = wage_share_at(per_capacity(workers, capacity)); // jobs = capacity
        const Fixed64 output = get_output(capacity, workers);
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

        cy.locations += 1;
        cy.capacity += capacity;
        cy.workers += workers;
        cy.elites += elites;
        cy.reached_elites += p.control[loc] * elites;
        cy.income += wealth;
        cy.collected += wealth * (ONE - last_radicalism * COLLECTION_RADICALISM) * p.control[loc];
        cy.radicalism_burden += last_radicalism * workers;
        cy.reached_radicalism += last_radicalism * p.control[loc] * workers;
        cy.wage_sum += w * workers;
        cy.immiseration_sum += immiseration * workers;
        cy.security_sum += security * workers;
        cy.elite_discontent_last += (ONE - cyc.elite_satisfaction[loc]) * elites;
    }
}

void fiscal_and_legitimacy(rota::core::World& w, CountryID c, CountryYear& cy) {
    auto& state = w.country_state;

    // tax the wealth base (collection gated by radicalism), fund army then patronage, run a capped treasury
    const Fixed64 capacity(cy.capacity);
    const Fixed64 revenue = TAX_RATE * cy.collected;

    cy.baseline_positions = CULTIVATED_ELITE_OPPS * capacity + MIL_POSITIONS * capacity;
    cy.overproduction = cy.elites / cy.baseline_positions;
    const Fixed64 excess_elites = std::max(ZERO, cy.elites - cy.baseline_positions);
    const Fixed64 absorb_fraction = ONE / (ONE + K_ABSORB * std::max(ZERO, cy.overproduction - ONE)); // patronage can't keep pace
    const Fixed64 desired_patronage = excess_elites * absorb_fraction * K_PATRONAGE;
    // the state pays to hold down mobilization POTENTIAL, so overproduction drains even a solvent treasury
    const Fixed64 army_cost = ARMY_BASE * capacity + K_SUPPRESS * cy.reached_radicalism;

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
    cy.suppression_per_worker = suppression / cy.workers; // per capita, so a big realm's coercion doesn't dwarf the bounded radicalism

    const Fixed64 max_treasury = TREASURY_YEARS * TAX_RATE * cy.income;
    state.treasury[c] = std::clamp(state.treasury[c] + revenue - army_paid - patronage_paid, ZERO, max_treasury);

    // felt overproduction is vs FUNDED positions: patronage placates the surplus until it goes unfunded
    state.state_positions[c] = MIL_POSITIONS * capacity + patronage_jobs;
    cy.state_share = cy.reached_elites > ZERO ? state.state_positions[c] / cy.reached_elites : ZERO; // zero without a capital
    cy.felt_overproduction = cy.elites / (cy.baseline_positions + patronage_jobs);

    // legitimacy relaxes toward a target pulled down by insecurity, radicalism, unpaid obligations and overproduction
    Fixed64& legitimacy = state.legitimacy[c];
    const Fixed64 insecurity = std::max(ZERO, ONE - cy.security_sum / cy.workers);
    Fixed64 target = ONE
        - LEGIT_INSECURITY * insecurity
        - LEGIT_RADICALISM * (cy.radicalism_burden / cy.workers)
        - LEGIT_UNPAID_ARMY * cy.army_shortfall
        - LEGIT_UNPAID_PATRON * patronage_shortfall
        - LEGIT_FRAG * std::max(ZERO, cy.felt_overproduction - LEGIT_FRAG_TOLERANCE)
        - LEGIT_DIVIDED_ELITES * (cy.elite_discontent_last / cy.elites);
    target = std::clamp(target, LEGIT_FLOOR, ONE);
    legitimacy = std::clamp(legitimacy + LEGIT_SPEED * (target - legitimacy), ZERO, ONE);
}

// Moves toward the target: falls fast and recovers slowly, the more slowly the worse conditions still are
Fixed64 approach(Fixed64 satisfaction, Fixed64 target, Fixed64 rise) {
    const Fixed64 rate = target < satisfaction
        ? SATISFACTION_FALL
        : rise * std::max(ZERO, target - SATISFACTION_STICK) / (ONE - SATISFACTION_STICK);
    return satisfaction + rate * (target - satisfaction);
}

void satisfaction(const Pass& p) {
    auto& cyc = p.w.location_secular_cycle;
    const auto& eco = p.w.location_economy;
    const LocationID location_count = p.w.geography.location_count();

    for (LocationID loc = 0; loc < location_count; loc++) {
        CountryID c;
        if (!p.owned(loc, c)) continue;
        CountryYear& cy = p.countries[c];

        // commoners judge society by their wage and its turmoil, elites by the positions open to them
        const std::int64_t workers = workers_at(eco, loc);
        const std::int64_t elites = eco.elites[loc];
        const Fixed64 felt = elites / (local_elite_positions(p.w, loc) + cy.state_share * p.control[loc] * elites); // local felt overproduction
        const Fixed64 wage_content = std::clamp((p.wage[loc] - MISERABLE_WAGE) / (SATISFIED_WAGE - MISERABLE_WAGE), ZERO, ONE); // a high wage doesn't buy off turmoil
        const Fixed64 commoner_target = std::max(ZERO, wage_content - TURMOIL_DISCONTENT * p.w.country_state.radicalism[c]);
        const Fixed64 elite_target = std::clamp(ONE - ELITE_DISCONTENT * (felt - ONE), ZERO, ONE);
        cyc.commoner_satisfaction[loc] = approach(cyc.commoner_satisfaction[loc], commoner_target, SATISFACTION_RISE);
        cyc.elite_satisfaction[loc] = approach(cyc.elite_satisfaction[loc], elite_target, ELITE_SATISFACTION_RISE);

        cy.commoner_target_sum += commoner_target * workers;
        cy.elite_target_sum += elite_target * elites;
        cy.commoner_discontent_sum += (ONE - cyc.commoner_satisfaction[loc]) * workers;
        cy.elite_discontent_sum += (ONE - cyc.elite_satisfaction[loc]) * elites;
    }
}

// Radicalism moves toward the country's discontent, part of which legitimacy keeps in check
void radicalism(rota::core::World& w, CountryID c, CountryYear& cy) {
    auto& state = w.country_state;
    cy.discontent = RADICAL_COMMONERS * (cy.commoner_discontent_sum / cy.workers) + RADICAL_ELITES * (cy.elite_discontent_sum / cy.elites);
    cy.radicalism_target = std::clamp((cy.discontent - RADICAL_TOLERANCE) / (ONE - RADICAL_TOLERANCE), ZERO, ONE)
        * (ONE - RADICAL_LEGITIMACY * state.legitimacy[c]);
    const Fixed64 rate = cy.radicalism_target < state.radicalism[c] ? RADICALISM_FADE : RADICALISM_RATE;
    state.radicalism[c] += rate * (cy.radicalism_target - state.radicalism[c]);
}

// Spread evenly where everyone is content, toward the discontented as discontent grows; the army
// holds it down where it reaches
void local_radicalism(const Pass& p) {
    auto& cyc = p.w.location_secular_cycle;
    const auto& eco = p.w.location_economy;
    const auto& radicalism = p.w.country_state.radicalism;
    const LocationID location_count = p.w.geography.location_count();

    for (LocationID loc = 0; loc < location_count; loc++) {
        CountryID c;
        if (!p.owned(loc, c)) continue;
        CountryYear& cy = p.countries[c];

        const Fixed64 discontent = RADICAL_COMMONERS * (ONE - cyc.commoner_satisfaction[loc]) + RADICAL_ELITES * (ONE - cyc.elite_satisfaction[loc]);
        const Fixed64 here = std::min(ONE, radicalism[c] * (ONE + discontent) / (ONE + cy.discontent));
        cyc.effective_radicalism[loc] = std::clamp(here - cy.suppression_per_worker * p.control[loc], ZERO, ONE);

        const std::int64_t workers = workers_at(eco, loc);
        cy.effective_radicalism_sum += cyc.effective_radicalism[loc] * workers;
        cy.violence_sum += violence(cyc.effective_radicalism[loc]) * workers;
    }
}

// Folds one active crisis-year into the effect every owned location will bear
void apply_crisis(rota::core::World& w, CountryID c, CountryYear& cy, rc::CrisisType type, Fixed64 x) {
    auto& state = w.country_state;
    switch (type) {
        case rc::CRISIS_REVOLT:
            state.radicalism[c] = std::min(ONE, state.radicalism[c] + REVOLT_RADICALISM * x);
            cy.commoner_keep = cy.commoner_keep * (ONE - milli(100) * K_FAMINE * x);
            cy.elite_keep = cy.elite_keep * (ONE - milli(300) * CIVILWAR_CULL * x);
            break;
        case rc::CRISIS_CIVIL_WAR: // its deaths come from the fighting on the map (civil_war.cpp)
            state.radicalism[c] = std::min(ONE, state.radicalism[c] + milli(500) * REVOLT_RADICALISM * x);
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

    const Fixed64 effective_radicalism = cy.effective_radicalism_sum / cy.workers;
    const Fixed64 immiseration = cy.immiseration_sum / cy.workers;
    const Fixed64 op = std::max(ZERO, cy.felt_overproduction - ONE);
    const Fixed64 low_legit = ONE - state.legitimacy[c];
    const Fixed64 drivers[rc::CRISIS_COUNT] = {
        milli(400) * immiseration + milli(400) * effective_radicalism + milli(300) * low_legit,
        milli(500) * op + milli(500) * effective_radicalism,
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
            if (drive > ZERO && rota::core::chance(p, rota::core::keyed_hash(w.seed, year, rota::core::STREAM_CRISIS_ROLL + t, c))) {
                cr.active = 1;
                cr.age = 0;
                cr.intensity = std::min(ONE, drive * (phase == rc::PHASE_STRAIN ? milli(500) : ONE)); // milder in strain
                cr.duration = cfg.dmin + static_cast<std::uint32_t>(
                    rota::core::keyed_hash(w.seed, year, rota::core::STREAM_CRISIS_DURATION + t, c) % (cfg.dmax - cfg.dmin + 1));
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

void crisis_effects_and_elites(const Pass& p, const rc::WarYear& war) {
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
        const Fixed64 positions = local_elite_positions(p.w, loc) + cy.state_share * p.control[loc] * elites_before;

        const std::int64_t commoners = eco.commoners[loc];
        const Fixed64 commoner_keep = cy.commoner_keep * war.commoner_keep[loc];
        if (commoner_keep < ONE) {
            eco.commoners[loc] = to_heads(commoner_keep * commoners, p.key(rota::core::STREAM_CRISIS_COMMONERS, loc), 0);
        }
        const Fixed64 elites = cy.elite_keep * war.elite_keep[loc] * elites_before;
        cy.record.crisis_deaths += (ONE - cy.commoner_keep) * commoners;
        cy.record.war_deaths += cy.commoner_keep * (ONE - war.commoner_keep[loc]) * commoners;
        cy.record.elite_crisis_deaths += (ONE - cy.elite_keep) * elites_before;
        cy.record.elite_war_deaths += cy.elite_keep * (ONE - war.elite_keep[loc]) * elites_before;

        // build by reproduction + upward mobility, clear by culling + downward mobility
        // dissatisfied elites raise fewer heirs, as commoners do, mildly dissatisfied ones hardly fewer
        const Fixed64 elite_discontent = ONE - cyc.elite_satisfaction[loc];
        const Fixed64 e_biological = elites * legitimacy[c] * R_E_PREMIUM * (ONE - elite_discontent * elite_discontent);

        Fixed64 e_mobility;
        const Fixed64 misery = (W0_TARGET - p.wage[loc]) / p.wage[loc];
        if (misery < ZERO) {
            const Fixed64 underprod = std::min(ONE, elites / positions); // few elites left -> curb demotion
            e_mobility = elites * MU_DOWN * misery * underprod;
        } else {
            e_mobility = MU_0 * workers * misery;
        }

        const Fixed64 e_attrition = elites * K_ATTRITION * attrition(violence(cyc.effective_radicalism[loc]));

        eco.elites[loc] = to_heads(elites + e_biological + e_mobility - e_attrition, p.key(rota::core::STREAM_ELITES, loc), 1);
        cy.elites_after += eco.elites[loc];
        cy.record.elite_births += e_biological;
        if (e_mobility > ZERO) cy.record.promotions += e_mobility; else cy.record.demotions -= e_mobility;
        cy.record.attrition += e_attrition;
    }
}

void phase(rota::core::World& w, CountryID c, const CountryYear& cy) {
    auto& state = w.country_state;
    const Fixed64 wage = cy.wage_sum / cy.workers;
    const Fixed64 effective_radicalism = cy.effective_radicalism_sum / cy.workers;

    if (state.phase[c] == rc::PHASE_PROSPERITY) {
        const bool traditional = wage < W0_TARGET && Fixed64(cy.elites_after) > cy.baseline_positions;
        const bool ibn_khaldun = cy.overproduction > IBN_MARGIN;
        if (traditional || ibn_khaldun) state.phase[c] = rc::PHASE_STRAIN;
    } else if (state.phase[c] == rc::PHASE_STRAIN) {
        const bool fiscal_break = state.treasury[c] <= ZERO && cy.army_shortfall > ZERO;
        if (effective_radicalism > FRACTURE_RADICALISM || fiscal_break) state.phase[c] = rc::PHASE_FRACTURE;
    } else if (cy.overproduction < OVERPROD_EXIT && effective_radicalism < EXIT_RADICALISM) {
        state.phase[c] = rc::PHASE_PROSPERITY;
    }
}

}

Fixed64 local_elite_positions(const rota::core::World& world, LocationID loc) {
    return CULTIVATED_ELITE_OPPS * static_cast<std::int64_t>(world.location_economy.capacity[loc]);
}

Fixed64 elite_positions(const rota::core::World& world, LocationID loc, Fixed64 control, Fixed64 reached_country_elites) {
    const Fixed64 local = local_elite_positions(world, loc);
    const CountryID owner = world.location_politics.owner_country[loc];
    if (!world.countries.is_valid(owner) || reached_country_elites <= ZERO) return local;
    const std::int64_t elites = world.location_economy.elites[loc];
    return local + world.country_state.state_positions[owner] * (control * elites / reached_country_elites);
}

Fixed64 population_pressure_gauge(std::int64_t commoners, std::int64_t capacity) {
    if (capacity <= 0) return ZERO;
    return std::min(ONE, Fixed64::from_raw(commoners * Fixed64::SCALE / capacity) / FULL_PRESSURE);
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
    const std::vector<Fixed64> control = rota::countries::compute_control(world);
    const Pass pass{ world, countries, wage, control, world.calendar.get_year() };

    const auto each_country = [&](auto step) {
        for (CountryID c = 0; c < slot_count; c++) {
            if (countries[c].locations > 0) step(c, countries[c]);
        }
    };

    population_and_economy(pass);
    each_country([&](CountryID c, CountryYear& cy) { fiscal_and_legitimacy(world, c, cy); });
    satisfaction(pass);
    each_country([&](CountryID c, CountryYear& cy) { radicalism(world, c, cy); });
    local_radicalism(pass);
    each_country([&](CountryID c, CountryYear& cy) { crises(world, pass.year, c, cy); });
    const rc::WarYear war = rc::run_civil_wars(world);
    for (const CountryID c : war.decided) { // won outright, so the crisis ends early
        rc::Crisis& cr = world.country_state.crises[c][rc::CRISIS_CIVIL_WAR];
        cr.active = 0;
        cr.cooldown = CRISIS_CFG[rc::CRISIS_CIVIL_WAR].cool;
    }
    crisis_effects_and_elites(pass, war);
    each_country([&](CountryID c, CountryYear& cy) { phase(world, c, cy); });

    world.cycle_ledger.countries.assign(slot_count, CycleRecord{});
    each_country([&](CountryID c, CountryYear& cy) {
        CycleRecord& r = cy.record;
        r.wage = cy.wage_sum / cy.workers;
        r.immiseration = cy.immiseration_sum / cy.workers;
        r.security = cy.security_sum / cy.workers;
        r.commoner_satisfaction = Fixed64(1) - cy.commoner_discontent_sum / cy.workers;
        r.commoner_satisfaction_target = cy.commoner_target_sum / cy.workers;
        r.elite_satisfaction = Fixed64(1) - cy.elite_discontent_sum / cy.elites;
        r.elite_satisfaction_target = cy.elite_target_sum / cy.elites;
        r.radicalism = world.country_state.radicalism[c];
        r.radicalism_target = cy.radicalism_target;
        r.effective_radicalism = cy.effective_radicalism_sum / cy.workers;
        r.violence = cy.violence_sum / cy.workers;
        r.overproduction = cy.overproduction;
        r.felt_overproduction = cy.felt_overproduction;
        r.army_shortfall = cy.army_shortfall;
        world.cycle_ledger.countries[c] = r;
    });
}

}
