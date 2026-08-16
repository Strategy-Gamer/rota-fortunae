#include "synch_clock.h"
#include "../core/hash.h"

namespace rota::time {
    
void SynchClock::step(){
    tick += 1;
};

std::uint64_t SynchClock::get_current_tick() const{
    return tick;
};

void SynchClock::set_tick(std::uint64_t new_tick){
    tick = new_tick;
};
    
void SynchClock::clear() {
    tick = 0;
}

void SynchClock::feed_hash(rota::core::Hasher& h) const{
    h.feed(tick);
}

}