#pragma once

#include "countries/countries.h"
#include "map/geography.h"

#include <cstdint>
#include <vector>

namespace rota::countries {

struct LocationPolitics {
    // Indexed by LocationID.
    std::vector<CountryID> owner_country;

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
};

}