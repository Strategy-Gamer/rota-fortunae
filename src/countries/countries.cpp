#include "countries/countries.h"

namespace rota::countries {

CountryID CountryStore::create_country(
    const std::string& name,
    std::uint32_t display_color
) {
    const CountryID id =
        static_cast<CountryID>(alive.size());

    alive.push_back(1);
    names.push_back(name);
    display_color_rgb.push_back(display_color);

    return id;
}

bool CountryStore::is_valid(CountryID id) const noexcept {
    return id < alive.size() && alive[id] != 0;
}

std::uint32_t CountryStore::count() const noexcept {
    return static_cast<std::uint32_t>(alive.size());
}

void CountryStore::clear() {
    alive.clear();
    names.clear();
    display_color_rgb.clear();
}

}