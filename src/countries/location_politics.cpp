#include "countries/location_politics.h"

namespace rota::countries {

void LocationPolitics::initialize(
    std::uint32_t location_count
) {
    owner_country.assign(
        location_count,
        INVALID_COUNTRY_ID
    );
}

void LocationPolitics::clear() {
    owner_country.clear();
}

CountryID LocationPolitics::get_owner(
    rota::map::LocationID location_id
) const noexcept {
    if (location_id >= owner_country.size()) {
        return INVALID_COUNTRY_ID;
    }

    return owner_country[location_id];
}

void LocationPolitics::set_owner(
    rota::map::LocationID location_id,
    CountryID country_id
) {
    if (location_id >= owner_country.size()) {
        return;
    }

    owner_country[location_id] = country_id;
}

}