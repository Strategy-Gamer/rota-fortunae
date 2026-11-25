#include "countries/country_systems.h"

#include "core/world.h"

namespace rota::countries {

CountryID create_country(
    rota::core::World& w,
    const std::string& name,
    std::uint32_t display_color,
    std::uint32_t founded
) {
    const CountryID id = w.countries.allocate(name, display_color, founded);
    w.country_state.reset(id);
    return id;
}

CountryID revive_country(
    rota::core::World& w,
    CountryID id,
    std::uint32_t generation
) {
    for (const DeadCountryRecord& record : w.countries.dead_countries) {
        if (record.id == id && record.generation == generation) {
            return revive_country(w, record);
        }
    }

    return INVALID_COUNTRY_ID;
}

CountryID revive_country(
    rota::core::World& w,
    const DeadCountryRecord& record
) {
    const CountryID id = w.countries.allocate(record.name, record.display_color_rgb, record.founded_year);
    w.country_state.reset(id);
    return id;
}

void mark_destroy_country(rota::core::World& w, CountryID id) {
    if (!w.countries.is_valid(id)) {
        return;
    }

    w.countries.marked_destroy[id] = 1;
}

void destroy_countries(rota::core::World& w) {
    CountryStore& countries = w.countries;
    const CountryID slot_count = static_cast<CountryID>(countries.alive.size());

    for (CountryID id = 0; id < slot_count; id++) {
        if (countries.marked_destroy[id] == 0) {
            continue;
        }

        const std::uint32_t location_count = w.geography.location_count();
        for (rota::map::LocationID loc = 0; loc < location_count; loc++) {
            if (w.location_politics.get_owner(loc) == id) {
                set_location_owner(w, loc, INVALID_COUNTRY_ID);
            }
            if (w.location_politics.occupier[loc] == id) { // its occupations end with it
                w.location_politics.occupier[loc] = INVALID_COUNTRY_ID;
                w.render_dirty |= rota::core::DIRTY_POLITICAL;
            }
        }

        countries.dead_countries.push_back(DeadCountryRecord{
            id,
            countries.generation[id],
            countries.names[id],
            countries.display_color_rgb[id],
            countries.founded_year[id],
            w.calendar.get_year()
        });

        countries.free(id);
        w.country_state.reset(id);
    }
}

void set_location_owner(
    rota::core::World& w,
    rota::map::LocationID location_id,
    CountryID country_id
) {
    const CountryID old_owner = w.location_politics.get_owner(location_id);
    if (old_owner == country_id) {
        return;
    }

    if (w.countries.is_valid(old_owner)) {
        w.countries.location_count[old_owner] -= 1;
        if (w.countries.capital[old_owner] == location_id) {
            w.countries.capital[old_owner] = rota::map::INVALID_LOCATION_ID;
        }
    }
    if (w.countries.is_valid(country_id)) {
        w.countries.location_count[country_id] += 1;
    }

    w.location_politics.set_owner(location_id, country_id);
    w.location_politics.occupier[location_id] = INVALID_COUNTRY_ID; // a change of owner ends any occupation
    w.render_dirty |= rota::core::DIRTY_POLITICAL;
}

}
