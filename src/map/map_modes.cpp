#include "map/map_modes.h"

#include "core/world.h"
#include "countries/control.h"
#include "economy/secular_cycle_system.h"

#include <algorithm>
#include <vector>

namespace rota::map {

namespace {

constexpr std::uint32_t rgb(std::uint32_t r, std::uint32_t g, std::uint32_t b) {
    return r | (g << 8U) | (b << 16U);
}

constexpr std::uint32_t UNOWNED_COLOR = rgb(0x50, 0x50, 0x50);
constexpr std::uint32_t SEA_COLOR = rgb(0x2E, 0x4A, 0x6E);
constexpr std::uint32_t IMPASSABLE_COLOR = rgb(0x2A, 0x26, 0x22);

// Phase colors follow the Planning plots: prosperity green, strain yellow, fracture red
constexpr std::uint32_t PHASE_COLORS[] = { rgb(0x3C, 0xA0, 0x46), rgb(0xE6, 0xC8, 0x32), rgb(0xC8, 0x32, 0x28) };

void write_rgb24(std::vector<std::uint8_t>& bytes, std::size_t byte_index, std::uint32_t packed_rgb) {
    bytes[byte_index] = static_cast<std::uint8_t>(packed_rgb & 0xFFU);
    bytes[byte_index + 1] = static_cast<std::uint8_t>((packed_rgb >> 8U) & 0xFFU);
    bytes[byte_index + 2] = static_cast<std::uint8_t>((packed_rgb >> 16U) & 0xFFU);
}

// Presentation only, so floats are fine here
std::uint32_t ramp(Fixed64 value, std::uint32_t low, std::uint32_t high) {
    const double t = std::clamp(value.to_double(), 0.0, 1.0);
    std::uint32_t out = 0;
    for (std::uint32_t shift = 0; shift <= 16; shift += 8) {
        const double a = static_cast<double>((low >> shift) & 0xFFU);
        const double b = static_cast<double>((high >> shift) & 0xFFU);
        out |= static_cast<std::uint32_t>(a + (b - a) * t + 0.5) << shift;
    }
    return out;
}

// Calls color(location, owner) for owned land; paints unowned land, sea and impassable land flat
template <typename F>
RGBImageData paint(const rota::core::World& world, F color) {
    RGBImageData result;
    result.width = world.geography.location_count();
    result.height = 1;
    result.bytes.resize(static_cast<std::size_t>(result.width) * 3);

    for (LocationID loc = 0; loc < result.width; loc++) {
        const rota::countries::CountryID owner = world.location_politics.get_owner(loc);
        std::uint32_t packed;
        switch (world.geography.kind[loc]) {
            case LocationKind::Sea: packed = SEA_COLOR; break;
            case LocationKind::Impassable: packed = IMPASSABLE_COLOR; break;
            default: packed = world.countries.is_valid(owner) ? color(loc, owner) : UNOWNED_COLOR;
        }
        write_rgb24(result.bytes, static_cast<std::size_t>(loc) * 3, packed);
    }
    return result;
}

}

RGBImageData create_map_mode_palette(const rota::core::World& world, MapMode mode) {
    const auto& eco = world.location_economy;
    const auto& state = world.country_state;
    using rota::countries::CountryID;

    switch (mode) {
        case MapMode::Political:
            return paint(world, [&](LocationID, CountryID c) { return world.countries.display_color_rgb[c]; });

        case MapMode::Phase:
            return paint(world, [&](LocationID, CountryID c) { return PHASE_COLORS[state.phase[c]]; });

        case MapMode::Unrest:
            return paint(world, [&](LocationID loc, CountryID) {
                return ramp(world.location_secular_cycle.effective_unrest[loc], rgb(0xEE, 0xE8, 0xDC), rgb(0xA0, 0x10, 0x10));
            });

        case MapMode::PopulationPressure: // density, not the P gauge: the gauge is too gentle to see on a map
            return paint(world, [&](LocationID loc, CountryID) {
                const Fixed64 density = Fixed64(static_cast<std::int64_t>(eco.commoners[loc])) / rota::economy::LOCATION_CAPACITY;
                return ramp((density - Fixed64::from_raw(600'000)) * 5 / 2, rgb(0xE8, 0xF0, 0xFA), rgb(0x14, 0x3C, 0x96));
            });

        case MapMode::EliteOverproduction: {
            const std::vector<Fixed64> control = rota::countries::compute_control(world);
            std::vector<Fixed64> reached_elites(world.countries.alive.size());
            for (LocationID loc = 0; loc < world.geography.location_count(); loc++) {
                const CountryID owner = world.location_politics.get_owner(loc);
                if (world.countries.is_valid(owner)) reached_elites[owner] += control[loc] * static_cast<std::int64_t>(eco.elites[loc]);
            }
            return paint(world, [&](LocationID loc, CountryID c) {
                const Fixed64 positions = rota::economy::elite_positions(world, loc, control[loc], reached_elites[c]);
                const Fixed64 e = rota::economy::elite_overproduction_gauge(static_cast<std::int64_t>(eco.elites[loc]) / positions);
                return ramp(e, rgb(0xF5, 0xF0, 0xE6), rgb(0xD2, 0x64, 0x00));
            });
        }

        case MapMode::Legitimacy:
            return paint(world, [&](LocationID, CountryID c) {
                return ramp(state.legitimacy[c], rgb(0x46, 0x28, 0x28), rgb(0x50, 0xBE, 0x8C));
            });

        case MapMode::Control: {
            const std::vector<Fixed64> control = rota::countries::compute_control(world);
            return paint(world, [&](LocationID loc, CountryID) {
                return ramp(control[loc], rgb(0xF2, 0xEC, 0xDC), rgb(0x1E, 0x46, 0x78));
            });
        }

        case MapMode::LocationColor:
        default: {
            RGBImageData result;
            result.width = world.geography.location_count();
            result.height = 1;
            result.bytes.resize(static_cast<std::size_t>(result.width) * 3);
            for (LocationID loc = 0; loc < result.width; loc++) {
                write_rgb24(result.bytes, static_cast<std::size_t>(loc) * 3, world.geography.display_color_rgb[loc]);
            }
            return result;
        }
    }
}

}
