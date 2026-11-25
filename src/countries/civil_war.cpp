#include "countries/civil_war.h"

#include "core/rng.h"
#include "core/world.h"
#include "countries/country_systems.h"

#include <algorithm>
#include <cstdint>

namespace rota::countries {

namespace {

using rota::map::LocationID;
using rota::map::LocationKind;

Fixed64 milli(std::int64_t m) { return Fixed64::from_raw(m * 1000); }

const Fixed64 ZERO;
const Fixed64 ONE(1);

// Per war-year, scaled by the war's intensity. Armies are raised everywhere, so elites fall across
// the whole country, and most of all where a front changes hands.
const Fixed64 WAR_ELITE_LOSS = milli(15);
const Fixed64 SACK_ELITE_LOSS = milli(80);

// Per war-year where the armies are, scaled by the place's effective unrest: fighting and foraging
// are as fierce as the people there are mobilized
const Fixed64 FRONT_COMMONER_LOSS = milli(12);
const Fixed64 FRONT_SECURITY_LOSS = milli(300);
const Fixed64 PILLAGE_COMMONER_LOSS = milli(5); // behind the rebels' lines, where their armies live off the land

const Fixed64 FRONT_TAKE = milli(500); // chance a front falls, times the enemy's share of strength

const std::uint32_t REBEL_COLOR = 0xC0 | (0x20 << 8) | (0x20 << 16); // crimson, packed r | g << 8 | b << 16

// Keys for draws; the secular cycle's streams are all below 100
enum Stream : std::uint64_t {
    STREAM_FRONT_TAKE = 100,
};

bool chance(Fixed64 p, std::uint64_t key) {
    return static_cast<std::int64_t>(key % Fixed64::SCALE) < p.raw_value();
}

bool at_war(const rota::core::World& w, CountryID c) {
    return w.country_state.crises[c][CRISIS_CIVIL_WAR].active != 0;
}

Fixed64 intensity(const rota::core::World& w, CountryID c) {
    return w.country_state.crises[c][CRISIS_CIVIL_WAR].intensity;
}

// Rebels rise in every location more restless (effective unrest) than the country's commoner-weighted
// average, or failing that the most restless one; never in the capital
void ignite(rota::core::World& w, const std::vector<CountryID>& igniting) {
    const auto& politics = w.location_politics;
    const auto& eco = w.location_economy;
    const LocationID location_count = w.geography.location_count();
    const CountryID slot_count = static_cast<CountryID>(w.countries.alive.size());

    std::vector<std::uint8_t> igniting_flag(slot_count, 0);
    for (const CountryID c : igniting) igniting_flag[c] = 1;
    const auto igniting_owner = [&](LocationID loc) {
        const CountryID c = politics.owner_country[loc];
        return w.countries.is_valid(c) && igniting_flag[c] ? c : INVALID_COUNTRY_ID;
    };

    const auto& unrest = w.location_secular_cycle.effective_unrest;
    std::vector<Fixed64> unrest_sum(slot_count);
    std::vector<std::int64_t> workers(slot_count, 0);
    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID c = igniting_owner(loc);
        if (c == INVALID_COUNTRY_ID) continue;
        const std::int64_t heads = std::max<std::int64_t>(eco.commoners[loc], 1);
        unrest_sum[c] += unrest[loc] * heads;
        workers[c] += heads;
    }
    const auto above_average = [&](LocationID loc, CountryID c) {
        return unrest[loc] * workers[c] > unrest_sum[c];
    };

    std::vector<std::uint32_t> rising(slot_count, 0);
    std::vector<LocationID> most_restless(slot_count, rota::map::INVALID_LOCATION_ID);
    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID c = igniting_owner(loc);
        if (c == INVALID_COUNTRY_ID || loc == w.countries.capital[c]) continue;
        if (above_average(loc, c)) rising[c] += 1;
        const LocationID best = most_restless[c];
        if (best == rota::map::INVALID_LOCATION_ID || unrest[loc] > unrest[best]) most_restless[c] = loc;
    }

    std::vector<CountryID> rebels(slot_count, INVALID_COUNTRY_ID);
    for (const CountryID c : igniting) {
        if (most_restless[c] == rota::map::INVALID_LOCATION_ID) continue;
        rebels[c] = create_country(w, w.countries.names[c] + " Rebels", REBEL_COLOR, w.calendar.get_year());
        w.countries.rebels_against[rebels[c]] = c;
    }

    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID c = igniting_owner(loc);
        if (c == INVALID_COUNTRY_ID || rebels[c] == INVALID_COUNTRY_ID || loc == w.countries.capital[c]) continue;
        if (rising[c] > 0 ? above_average(loc, c) : loc == most_restless[c]) {
            w.location_politics.occupier[loc] = rebels[c];
        }
    }
    w.render_dirty |= rota::core::DIRTY_POLITICAL;
}

}

std::vector<CountryID> rebel_sides(const rota::core::World& w) {
    const CountryID slot_count = static_cast<CountryID>(w.countries.alive.size());
    std::vector<CountryID> rebels(slot_count, INVALID_COUNTRY_ID);
    for (CountryID r = 0; r < slot_count; r++) {
        if (w.countries.is_valid(r) && w.countries.rebels_against[r] != INVALID_COUNTRY_ID) {
            rebels[w.countries.rebels_against[r]] = r;
        }
    }
    return rebels;
}

bool is_front(const rota::core::World& w, LocationID loc) {
    const auto& g = w.geography;
    const auto& politics = w.location_politics;
    const CountryID owner = politics.owner_country[loc];
    const CountryID side = politics.controller(loc);
    const auto enemy = [&](LocationID other) {
        return g.kind[other] == LocationKind::Land && politics.owner_country[other] == owner && politics.controller(other) != side;
    };

    for (std::uint32_t i = g.neighbor_offset[loc]; i < g.neighbor_offset[loc + 1]; i++) {
        const LocationID next = g.neighbors[i];
        if (enemy(next)) return true;
        if (g.kind[next] != LocationKind::Sea) continue;
        for (std::uint32_t j = g.neighbor_offset[next]; j < g.neighbor_offset[next + 1]; j++) {
            if (enemy(g.neighbors[j])) return true;
        }
    }
    return false;
}

WarYear run_civil_wars(rota::core::World& w) {
    auto& politics = w.location_politics;
    auto& eco = w.location_economy;
    const LocationID location_count = w.geography.location_count();
    const std::uint64_t year = w.calendar.get_year();

    WarYear out{ std::vector<Fixed64>(location_count, ONE), std::vector<Fixed64>(location_count, ONE), {} };

    // A crisis that ran out (its duration or the war-weariness lull) with both sides still holding
    // land ends in a stalemate: the rebels' land returns when they are swept away at the day's end
    std::vector<CountryID> rebels = rebel_sides(w);
    std::vector<CountryID> igniting;
    for (CountryID c = 0; c < rebels.size(); c++) {
        if (!w.countries.is_valid(c) || w.countries.rebels_against[c] != INVALID_COUNTRY_ID) continue;
        if (rebels[c] != INVALID_COUNTRY_ID && !at_war(w, c)) {
            mark_destroy_country(w, rebels[c]);
        } else if (rebels[c] == INVALID_COUNTRY_ID && at_war(w, c)) {
            igniting.push_back(c);
        }
    }
    ignite(w, igniting);

    rebels = rebel_sides(w);
    const CountryID slot_count = static_cast<CountryID>(rebels.size());
    const auto warring = [&](CountryID c) {
        return w.countries.is_valid(c) && w.countries.rebels_against[c] == INVALID_COUNTRY_ID && at_war(w, c);
    };
    const auto fighting = [&](CountryID c) { return warring(c) && rebels[c] != INVALID_COUNTRY_ID; };

    // Each side fights with the elites of the land it holds
    std::vector<std::int64_t> loyal_strength(slot_count, 0);
    std::vector<std::int64_t> rebel_strength(slot_count, 0);
    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID c = politics.owner_country[loc];
        if (!fighting(c)) continue;
        (politics.occupier[loc] == rebels[c] ? rebel_strength : loyal_strength)[c] += eco.elites[loc];
    }

    std::vector<LocationID> taken;
    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID c = politics.owner_country[loc];
        if (!warring(c)) continue;
        const Fixed64 x = intensity(w, c);
        out.elite_keep[loc] = ONE - WAR_ELITE_LOSS * x;
        if (!fighting(c)) continue; // a war no rebels could rise for is fought at court

        const Fixed64 fierceness = w.location_secular_cycle.effective_unrest[loc];
        const bool rebel_held = politics.occupier[loc] == rebels[c];
        if (!is_front(w, loc)) {
            if (rebel_held) out.commoner_keep[loc] = ONE - PILLAGE_COMMONER_LOSS * fierceness;
            continue;
        }

        out.commoner_keep[loc] = ONE - FRONT_COMMONER_LOSS * fierceness;
        Fixed64& security = w.location_secular_cycle.security[loc];
        security = std::max(ZERO, security - FRONT_SECURITY_LOSS * fierceness);

        const std::int64_t total = loyal_strength[c] + rebel_strength[c];
        if (total <= 0) continue;
        const Fixed64 enemy_share = Fixed64(rebel_held ? loyal_strength[c] : rebel_strength[c]) / total;
        if (chance(FRONT_TAKE * enemy_share, rota::core::keyed_hash(w.seed, year, STREAM_FRONT_TAKE, loc))) {
            taken.push_back(loc);
            out.elite_keep[loc] = out.elite_keep[loc] * (ONE - SACK_ELITE_LOSS * x);
        }
    }
    for (const LocationID loc : taken) { // after every roll, so no front falls twice in a year
        const CountryID c = politics.owner_country[loc];
        politics.occupier[loc] = politics.occupier[loc] == rebels[c] ? INVALID_COUNTRY_ID : rebels[c];
    }
    if (!taken.empty()) w.render_dirty |= rota::core::DIRTY_POLITICAL;

    // The war is decided when the rebels take the capital or hold nothing
    std::vector<std::uint32_t> rebel_held(slot_count, 0);
    for (LocationID loc = 0; loc < location_count; loc++) {
        const CountryID c = politics.owner_country[loc];
        if (fighting(c) && politics.occupier[loc] == rebels[c]) rebel_held[c] += 1;
    }
    for (CountryID c = 0; c < slot_count; c++) {
        if (!fighting(c)) continue;
        const LocationID capital = w.countries.capital[c];
        const bool capital_taken = capital != rota::map::INVALID_LOCATION_ID && politics.occupier[capital] == rebels[c];
        if (capital_taken || rebel_held[c] == 0) {
            mark_destroy_country(w, rebels[c]);
            out.decided.push_back(c);
        }
    }
    return out;
}

}
