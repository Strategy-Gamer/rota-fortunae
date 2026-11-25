#include "countries/countries.h"

#include "core/hash.h"

namespace rota::countries {

CountryID CountryStore::allocate(
    const std::string& name,
    std::uint32_t display_color,
    std::uint32_t founded
) {
    CountryID id;

    if (!free_list.empty()) {
        id = free_list.back();
        free_list.pop_back();
    } else {
        id = static_cast<CountryID>(alive.size());

        alive.push_back(0);
        marked_destroy.push_back(0);
        generation.push_back(0);
        founded_year.push_back(0);
        names.emplace_back();
        display_color_rgb.push_back(0);
        capital.push_back(rota::map::INVALID_LOCATION_ID);
        location_count.push_back(0);
    }

    alive[id] = 1;
    founded_year[id] = founded;
    names[id] = name;
    display_color_rgb[id] = display_color;

    return id;
}

void CountryStore::free(CountryID id) {
    if (!is_valid(id)) {
        return;
    }

    alive[id] = 0;
    marked_destroy[id] = 0;
    generation[id] += 1; // stale {id, generation} handles stop matching
    founded_year[id] = 0;
    names[id].clear();
    display_color_rgb[id] = 0;
    capital[id] = rota::map::INVALID_LOCATION_ID;
    location_count[id] = 0;

    free_list.push_back(id);
}

bool CountryStore::is_valid(CountryID id) const noexcept {
    return id < alive.size() && alive[id] != 0;
}

std::uint32_t CountryStore::count() const noexcept {
    return static_cast<std::uint32_t>(alive.size() - free_list.size());
}

void CountryStore::clear() {
    alive.clear();
    marked_destroy.clear();
    generation.clear();
    founded_year.clear();
    names.clear();
    display_color_rgb.clear();
    capital.clear();
    location_count.clear();
    free_list.clear();
    dead_countries.clear();
}

void CountryStore::feed_hash(rota::core::Hasher& h) const {
    const size_t slot_count = alive.size();
    h.feed(static_cast<std::uint32_t>(slot_count));
    for (size_t i = 0; i < slot_count; i++) h.feed(alive[i]);
    for (size_t i = 0; i < slot_count; i++) h.feed(marked_destroy[i]);
    for (size_t i = 0; i < slot_count; i++) h.feed(generation[i]);
    for (size_t i = 0; i < slot_count; i++) h.feed(founded_year[i]);
    for (size_t i = 0; i < slot_count; i++) h.feed(display_color_rgb[i]);
    for (size_t i = 0; i < slot_count; i++) h.feed(capital[i]);

    // Order decides which slot the next country takes
    for (CountryID id : free_list) h.feed(id);
}

}
