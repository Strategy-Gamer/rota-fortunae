#include "countries/country_state.h"

#include "core/hash.h"

namespace rota::countries {

void CountryState::reset(CountryID id) {
    if (id >= legitimacy.size()) {
        const size_t slot_count = static_cast<size_t>(id) + 1;
        legitimacy.resize(slot_count);
        radicalism.resize(slot_count);
        treasury.resize(slot_count);
        state_positions.resize(slot_count);
        phase.resize(slot_count);
        war_weariness.resize(slot_count);
        in_lull.resize(slot_count);
        crises.resize(slot_count);
    }

    legitimacy[id] = Fixed64(1);
    radicalism[id] = Fixed64();
    treasury[id] = Fixed64();
    state_positions[id] = Fixed64();
    phase[id] = PHASE_PROSPERITY;
    war_weariness[id] = Fixed64();
    in_lull[id] = 0;
    crises[id] = {};
}

void CountryState::clear() {
    legitimacy.clear();
    radicalism.clear();
    treasury.clear();
    state_positions.clear();
    phase.clear();
    war_weariness.clear();
    in_lull.clear();
    crises.clear();
}

void CountryState::feed_hash(rota::core::Hasher& h) const {
    const size_t slot_count = legitimacy.size();
    h.feed(static_cast<std::uint32_t>(slot_count));
    for (size_t i = 0; i < slot_count; i++) h.feed(legitimacy[i].raw_value());
    for (size_t i = 0; i < slot_count; i++) h.feed(radicalism[i].raw_value());
    for (size_t i = 0; i < slot_count; i++) h.feed(treasury[i].raw_value());
    for (size_t i = 0; i < slot_count; i++) h.feed(state_positions[i].raw_value());
    for (size_t i = 0; i < slot_count; i++) h.feed(phase[i]);
    for (size_t i = 0; i < slot_count; i++) h.feed(war_weariness[i].raw_value());
    for (size_t i = 0; i < slot_count; i++) h.feed(in_lull[i]);
    for (size_t i = 0; i < slot_count; i++) {
        for (const Crisis& c : crises[i]) {
            h.feed(c.active);
            h.feed(c.age);
            h.feed(c.duration);
            h.feed(c.cooldown);
            h.feed(c.intensity.raw_value());
        }
    }
}

}
