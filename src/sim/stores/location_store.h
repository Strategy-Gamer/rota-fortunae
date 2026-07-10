#pragma once
#include <cstdint>
#include <vector>

inline int pop_idx(uint32_t loc, uint32_t pop_type, uint32_t pop_type_count) {
    return loc * pop_type_count + pop_type;
}

/*
    Data Oriented Design (DOD) stores for the simulation. 
    These are simple structs that store data in a way that is efficient for the simulation to access and modify. 
    They are not meant to be used directly by the game logic, but rather as a way to organize the data for the simulation.
*/
struct LocationStore {
    uint32_t location_count = 0;
    uint32_t pop_type_count = 0;

    std::vector<int32_t> land_area;
    std::vector<int32_t> productivity;
    std::vector<int32_t> common_land;
    std::vector<int32_t> elite_land;
    std::vector<int32_t> wage_share;

    // size = location_count * pop_type_count
    std::vector<int32_t> population;
    std::vector<int32_t> wealth;
    std::vector<int32_t> wealth_level;
    std::vector<int32_t> wealth_level;
    std::vector<int32_t> wealth_level;

};