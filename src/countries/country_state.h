#pragma once

#include "countries.h"
#include "../utility/FixedDecimal.h"

#include <array>
#include <cstdint>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::countries {

enum Phase : std::uint8_t {
    PHASE_PROSPERITY = 0,
    PHASE_STRAIN = 1,
    PHASE_FRACTURE = 2
};

// FinalSim's live crises, in its iteration order. All three are violent, so all share the lull.
enum CrisisType : std::uint8_t {
    CRISIS_REVOLT = 0,
    CRISIS_CIVIL_WAR = 1,
    CRISIS_COUP = 2,
    CRISIS_COUNT = 3
};

struct Crisis {
    std::uint8_t active = 0;
    std::uint32_t age = 0;
    std::uint32_t duration = 0;
    std::uint32_t cooldown = 0;
    Fixed64 intensity;
};

// Includes gameplay state about countries
struct CountryState{
    // Indexed by CountryID, reset in lockstep with CountryStore slots.

    std::vector<Fixed64> legitimacy;
    std::vector<Fixed64> treasury;
    std::vector<Fixed64> state_positions; // military + funded patronage, from this year's budget
    std::vector<std::uint8_t> phase;
    std::vector<Fixed64> war_weariness;
    std::vector<std::uint8_t> in_lull;
    std::vector<std::array<Crisis, CRISIS_COUNT>> crises;

    void reset(CountryID id); // grows the arrays to cover a newly allocated slot

    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};

}
