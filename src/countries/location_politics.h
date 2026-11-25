#pragma once

#include "countries/countries.h"
#include "map/geography.h"

#include <cstdint>
#include <vector>

namespace rota::countries {

struct LocationPolitics {
    // Indexed by LocationID.
    std::vector<CountryID> owner_country;
    std::vector<CountryID> occupier; // who holds it in war, if not its owner; else INVALID_COUNTRY_ID

    void initialize(std::uint32_t location_count);

    void clear();

    [[nodiscard]]
    CountryID get_owner(
        rota::map::LocationID location_id
    ) const noexcept;

    void set_owner(
        rota::map::LocationID location_id,
        CountryID country_id
    );

    // Who actually holds the location: its occupier, else its owner
    [[nodiscard]]
    CountryID controller(rota::map::LocationID location_id) const noexcept {
        return occupier[location_id] != INVALID_COUNTRY_ID ? occupier[location_id] : owner_country[location_id];
    }
};

}