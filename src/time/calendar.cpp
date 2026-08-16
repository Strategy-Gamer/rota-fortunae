#include "calendar.h"
#include "../core/hash.h"

// Deterministic in-game calendar.

namespace rota::time {

Calendar::Calendar(){
    clear();
}

void Calendar::clear(){
    year = 1;
    day_of_year = 0;
    tick_accumulation = 0;
    tick_multiplier = 1;
    paused = false;
}

void Calendar::advance(){
    if (paused) return;

    tick_accumulation += tick_multiplier;

    // Should result in floor(tick_acc/THRESHOLD)
    std::uint32_t days_advanced = tick_accumulation / ADVANCE_THRESHOLD;
    if( days_advanced == 0 ) return;
    
    tick_accumulation -= ADVANCE_THRESHOLD * days_advanced;
    add_date(0, days_advanced);
};

std::uint32_t Calendar::get_year() const{
    return year;
};

std::uint32_t Calendar::get_day_of_year() const{
    return day_of_year;
};

std::uint32_t Calendar::get_tick_accumulation() const{
    return tick_accumulation;
}

std::uint32_t Calendar::get_tick_multiplier() const{
    return tick_multiplier;
}

bool Calendar::is_paused() const {
    return paused;
};

void Calendar::set_tick_multiplier(std::uint32_t new_multiplier){
    if(new_multiplier < 1) new_multiplier = 1;
    tick_multiplier = new_multiplier;
};

void Calendar::set_paused(bool paused_state){
    paused = paused_state;
};

void Calendar::add_date(std::uint32_t add_years, std::uint32_t add_days){
    year += add_years;
    
    day_of_year += add_days;

    // TODO: Leap Years
    year += day_of_year / YEAR_LENGTH;
    day_of_year = day_of_year % YEAR_LENGTH;
}

void Calendar::set_date(std::uint32_t new_year, std::uint32_t new_day){
    // Set year before day b/c eventually leap years
    if(new_year < 1) new_year = 1;
    year = new_year;

    if(new_day > YEAR_LENGTH) new_day = new_day % YEAR_LENGTH;
    day_of_year = new_day;
}

void Calendar::feed_hash(rota::core::Hasher& h) const{
    h.feed(year);
    h.feed(day_of_year);
    h.feed(tick_accumulation);
    h.feed(tick_multiplier);
    h.feed(static_cast<std::uint8_t>(paused));
}

};