// Native secular-cycle runner, built and driven by cycle.py.
//   harness <map.rgb> <width> <height> <locations.txt> <straits.txt> <seed> <countries> <years> <out.csv>
//   harness --knobs
// Writes one row per country with land per year, and prints the final state hash, which matches the
// playtest runner's for the same seed and years while no knob is overridden.

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

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--knobs") {
        knobs::list(stdout);
        return 0;
    }
    if (argc != 10) {
        std::fprintf(stderr, "usage: harness <map.rgb> <width> <height> <locations.txt> <straits.txt> <seed> <countries> <years> <out.csv>\n");
        return 2;
    }

    static rota::core::World w; // too big for the stack
    const std::string rgb = slurp(argv[1]);
    w.load_map(std::stoul(argv[2]), std::stoul(argv[3]),
        reinterpret_cast<const std::uint8_t*>(rgb.data()), rgb.size(),
        rota::map::MapBuilder::parse_location_data(slurp(argv[4]), slurp(argv[5])));
    rota::scenario::generate_random_scenario(w, std::stoull(argv[6]), std::stoul(argv[7]));

    std::FILE* out = std::fopen(argv[9], "w");
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", argv[9]);
        return 2;
    }
    std::fprintf(out, "year,country,generation,locations,occupied,capacity,commoners,elites,phase,"
        "legitimacy,treasury,war_weariness,in_lull,revolt,revolt_new,civil_war,civil_war_new,coup,coup_new,"
        "births,natural_deaths,overshoot_deaths,crisis_deaths,war_deaths,"
        "elite_births,promotions,demotions,attrition,elite_crisis_deaths,elite_war_deaths,"
        "wage,immiseration,security,effective_unrest,overproduction,felt_overproduction,army_shortfall\n");

    const std::uint32_t years = std::stoul(argv[8]);
    for (std::uint32_t y = 0; y < years; y++) {
        rota::time::TimeProgression::advance_years(w, 1);
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
                "%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
                w.calendar.get_year(), c, s.generation, s.location_count, s.occupied,
                static_cast<unsigned long long>(s.location_count) * rota::economy::LOCATION_CAPACITY,
                static_cast<unsigned long long>(s.population - s.elites), static_cast<unsigned long long>(s.elites),
                s.phase, d(state.legitimacy[c]), d(state.treasury[c]), d(state.war_weariness[c]), state.in_lull[c],
                crisis(0), fresh(0), crisis(1), fresh(1), crisis(2), fresh(2),
                d(r.births), d(r.natural_deaths), d(r.overshoot_deaths), d(r.crisis_deaths), d(r.war_deaths),
                d(r.elite_births), d(r.promotions), d(r.demotions), d(r.attrition), d(r.elite_crisis_deaths), d(r.elite_war_deaths),
                d(r.wage), d(r.immiseration), d(r.security), d(r.effective_unrest), d(r.overproduction),
                d(r.felt_overproduction), d(r.army_shortfall));
        }
    }
    std::fclose(out);
    std::printf("hash %lld year %u\n", static_cast<long long>(rota::core::state_hash(w)), w.calendar.get_year()); // signed, as the playtest runner prints it
    return 0;
}
