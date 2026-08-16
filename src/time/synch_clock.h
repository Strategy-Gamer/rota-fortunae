#pragma once

#include <cstdint>

namespace rota::core { class Hasher; }

// Synchronized Clock

namespace rota::time {

struct SynchClock{
    std::uint64_t tick = 0;

    void step();
    std::uint64_t get_current_tick() const;
    void set_tick(std::uint64_t new_tick);
    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};
}