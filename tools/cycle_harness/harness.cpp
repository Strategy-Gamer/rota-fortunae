// Native secular-cycle runner, built and driven by cycle.py.
//   harness <map.rgb> <width> <height> <locations.txt> <straits.txt> <seed> <countries> <years> <out.csv> [control ...]
//   harness --knobs
// Writes one row per country with land per year, and prints the final state hash, which matches the
// playtest runner's for the same seed and years while no knob is overridden and no control is given.
//
// Controls are test interventions: radicalism=X@A-B sets every country's radicalism to X at the
// start of each year from A to B, start=X starts every owned location at X commoners per head of
// capacity, and growth=G grows every location's capacity by G a year (a stand-in for economic growth).

#include "knobs.h"

#include "core/world.h"
#include "economy/location_economy.h"
#include "map/map_builder.h"
#include "queries/country_summary.h"
#include "scenario/random_scenario.h"
#include "time/time_progression.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string slurp(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "cannot read %s\n", path);
        std::exit(2);
    }
    std::stringstream s;
    s << file.rdbuf();
    return s.str();
}

double d(rota::Fixed64 x) { return x.to_double(); }

struct Controls {
    bool hold_radicalism = false;
    rota::Fixed64 radicalism;
    std::uint32_t from = 0, to = 0;
    bool set_start = false;
    rota::Fixed64 start;
    rota::Fixed64 growth;
};

Controls parse_controls(int argc, char** argv, int first) {
    Controls c;
    for (int i = first; i < argc; i++) {
        const std::string arg = argv[i];
        char x[32];
        unsigned a, b;
        if (std::sscanf(arg.c_str(), "radicalism=%31[0-9.]@%u-%u", x, &a, &b) == 3) {
            c.hold_radicalism = true;
            c.radicalism = rota::Fixed64::from_raw(knobs::parse("radicalism", x));
            c.from = a;
            c.to = b;
        } else if (std::sscanf(arg.c_str(), "start=%31[0-9.]", x) == 1) {
            c.set_start = true;
            c.start = rota::Fixed64::from_raw(knobs::parse("start", x));
        } else if (std::sscanf(arg.c_str(), "growth=%31[0-9.]", x) == 1) {
            c.growth = rota::Fixed64::from_raw(knobs::parse("growth", x));
        } else {
            std::fprintf(stderr, "unknown control %s\n", argv[i]);
            std::exit(2);
        }
    }
    return c;
}

void apply_controls(const Controls& c, rota::core::World& w) {
    for (auto& capacity : w.location_economy.capacity) {
        capacity += static_cast<std::uint32_t>((c.growth * static_cast<std::int64_t>(capacity)).raw_value() / rota::Fixed64::SCALE);
    }
    const std::uint32_t year = w.calendar.get_year();
    if (c.hold_radicalism && year >= c.from && year < c.to) {
        for (auto& r : w.country_state.radicalism) r = c.radicalism;
    }
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--knobs") {
        knobs::list(stdout);
        return 0;
    }
    if (argc < 10) {
        std::fprintf(stderr, "usage: harness <map.rgb> <width> <height> <locations.txt> <straits.txt> <seed> <countries> <years> <out.csv> [control ...]\n");
        return 2;
    }
    const Controls controls = parse_controls(argc, argv, 10);

    static rota::core::World w; // too big for the stack
    const std::string rgb = slurp(argv[1]);
    w.load_map(std::stoul(argv[2]), std::stoul(argv[3]),
        reinterpret_cast<const std::uint8_t*>(rgb.data()), rgb.size(),
        rota::map::MapBuilder::parse_location_data(slurp(argv[4]), slurp(argv[5])));
    rota::scenario::generate_random_scenario(w, std::stoull(argv[6]), std::stoul(argv[7]));
    if (controls.set_start) {
        auto& eco = w.location_economy;
        for (rota::map::LocationID loc = 0; loc < w.geography.location_count(); loc++) {
            if (!w.countries.is_valid(w.location_politics.owner_country[loc])) continue;
            eco.commoners[loc] = static_cast<std::uint32_t>((controls.start * static_cast<std::int64_t>(eco.capacity[loc])).raw_value() / rota::Fixed64::SCALE);
        }
    }
    apply_controls(controls, w);

    std::FILE* out = std::fopen(argv[9], "w");
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", argv[9]);
        return 2;
    }
    std::fprintf(out, "year,country,generation,locations,occupied,capacity,commoners,elites,phase,"
        "legitimacy,treasury,war_weariness,in_lull,revolt,revolt_new,civil_war,civil_war_new,coup,coup_new,"
        "births,natural_deaths,overshoot_deaths,crisis_deaths,war_deaths,"
        "elite_births,promotions,demotions,attrition,elite_crisis_deaths,elite_war_deaths,"
        "wage,immiseration,security,commoner_satisfaction,commoner_satisfaction_target,elite_satisfaction,elite_satisfaction_target,"
        "radicalism,radicalism_target,effective_radicalism,violence,overproduction,felt_overproduction,army_shortfall\n");

    const std::uint32_t years = std::stoul(argv[8]);
    for (std::uint32_t y = 0; y < years; y++) {
        rota::time::TimeProgression::advance_years(w, 1);
        apply_controls(controls, w);
        const auto summaries = rota::queries::build_country_summaries(w);
        const auto& state = w.country_state;
        for (rota::countries::CountryID c = 0; c < summaries.size(); c++) {
            const auto& s = summaries[c];
            if (!w.countries.is_valid(c) || s.location_count == 0) continue;
            const auto& r = w.cycle_ledger.countries[c];
            const auto& cr = state.crises[c];
            const auto crisis = [&](int t) { return cr[t].active; };
            const auto fresh = [&](int t) { return cr[t].active && cr[t].age == 0 ? 1 : 0; };
            std::fprintf(out, "%u,%u,%u,%u,%u,%llu,%llu,%llu,%u,%.6f,%.3f,%.4f,%u,%u,%d,%u,%d,%u,%d,"
                "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"
                "%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
                w.calendar.get_year(), c, s.generation, s.location_count, s.occupied,
                static_cast<unsigned long long>(s.capacity),
                static_cast<unsigned long long>(s.population - s.elites), static_cast<unsigned long long>(s.elites),
                s.phase, d(state.legitimacy[c]), d(state.treasury[c]), d(state.war_weariness[c]), state.in_lull[c],
                crisis(0), fresh(0), crisis(1), fresh(1), crisis(2), fresh(2),
                d(r.births), d(r.natural_deaths), d(r.overshoot_deaths), d(r.crisis_deaths), d(r.war_deaths),
                d(r.elite_births), d(r.promotions), d(r.demotions), d(r.attrition), d(r.elite_crisis_deaths), d(r.elite_war_deaths),
                d(r.wage), d(r.immiseration), d(r.security), d(r.commoner_satisfaction), d(r.commoner_satisfaction_target),
                d(r.elite_satisfaction), d(r.elite_satisfaction_target), d(r.radicalism), d(r.radicalism_target),
                d(r.effective_radicalism), d(r.violence), d(r.overproduction), d(r.felt_overproduction), d(r.army_shortfall));
        }
    }
    std::fclose(out);
    std::printf("hash %lld year %u\n", static_cast<long long>(rota::core::state_hash(w)), w.calendar.get_year()); // signed, as the playtest runner prints it
    return 0;
}
