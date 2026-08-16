#pragma once

#include <cstdint>

namespace rota::core { class Hasher; }

// Deterministic in-game calendar.

namespace rota::time {

    // How many accumulated ticks until one day is passed
    // At speed = 1, advance 1 day ~1 real second (assuming 60Hz)
    const std::uint32_t ADVANCE_THRESHOLD = 60;

    // Days in a year. TODO: Leap Years
    const std::uint32_t YEAR_LENGTH = 365;

class Calendar{
public:
    Calendar();

    void clear();

    // Advances time by current speed
    void advance();

    // Returns current year. For those living under rocks.
    std::uint32_t get_year() const;

    // Returns days from Jan 1st of current year.
    std::uint32_t get_day_of_year() const;

    std::uint32_t get_tick_accumulation() const;
    std::uint32_t get_tick_multiplier() const;

    // Returns if calendar progression is paused
    bool is_paused() const;

    // Sets speed multiplier. Cannot be lower than 1.
    void set_tick_multiplier(std::uint32_t new_multiplier);

    // Sets calendar progression paused state
    void set_paused(bool paused_state);

    // Adds years/days to the year count. (Years first). Defaults to nothing happening.
    void add_date(std::uint32_t add_years = 0, std::uint32_t add_days = 0);

    // Sets Year & Day of Year
    void set_date(std::uint32_t new_year, std::uint32_t new_day);

    // Feeds hash data to create hash
    void feed_hash(rota::core::Hasher& h) const;

private:
    // Current year
    std::uint32_t year;

    // How many days from beginning of year has passed (0 = Jan 1st).
    std::uint32_t day_of_year;

    // Sub-day progress units. Resets upon hitting ADVANCE_THRESHOLD.
    std::uint32_t tick_accumulation;

    // Every time advance() is called, how many ticks to add to tick_accumulation
    std::uint32_t tick_multiplier;

    // Determines whether calendar-time progression is paused or not.
    bool paused;
};

}